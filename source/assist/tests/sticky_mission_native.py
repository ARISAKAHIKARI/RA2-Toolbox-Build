"""Execute native QueueMission, Unit::ReadyToNextMission and NextMission.

The readiness function and both mission transitions are original executable
instructions. Only idle locomotor/radio/map queries are test doubles. This is
not a whole-game movement, weapon selection, or multiplayer test.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP

p = argparse.ArgumentParser()
p.add_argument('game')
p.add_argument('--output', required=True)
a = p.parse_args()
pe = pefile.PE(a.game)
u = Uc(UC_ARCH_X86, UC_MODE_32)
u.mem_map(0x400000, 0x1000000)
u.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
u.mem_map(0x20000000, 0x100000)
unit, loco, vt, stop, stack = 0x20000000, 0x20002000, 0x20003000, 0x20004000, 0x200f0000
def w(at, value):
    u.mem_write(at, struct.pack('<I', value & 0xffffffff))
def r(at):
    return struct.unpack('<I', u.mem_read(at, 4))[0]
def ret(value, count):
    sp = u.reg_read(UC_X86_REG_ESP)
    u.reg_write(UC_X86_REG_EAX, value)
    u.reg_write(UC_X86_REG_EIP, r(sp))
    u.reg_write(UC_X86_REG_ESP, sp + 4 + count*4)
queries = {stop+16:(0,1), 0x4a51d0:(1,0), 0x65ad40:(0,1), 0x565730:(unit+0x1000,1), 0x47c520:(0,0)}
def hook(uc, address, size, unused):
    if address == stop:
        uc.emu_stop()
    elif address in queries:
        ret(*queries[address])
u.hook_add(UC_HOOK_CODE, hook)
def run(address, *args):
    w(stack, stop)
    for i, value in enumerate(args):
        w(stack+4+i*4, value)
    u.reg_write(UC_X86_REG_ESP, stack)
    u.reg_write(UC_X86_REG_ECX, unit)
    u.emu_start(address, stop+1, count=10000)
    assert u.reg_read(UC_X86_REG_EIP) == stop
    return u.reg_read(UC_X86_REG_EAX) & 255
rows = []
for current in (5, 6):
    for requested in (1, 2, 5, 13):
        u.mem_write(unit, bytes(0x2000))
        w(unit, 0x7f5c70)
        w(unit+0x674, loco)
        w(loco, vt)
        w(vt+0x80, stop+16)
        w(unit+0xac, current)
        w(unit+0xb4, -1)
        w(0xa8ed84, 100)
        run(0x5b35e0, requested, 0)
        assert r(unit+0xac) == current
        # Queueing Guard when already guarding is intentionally a no-op.
        expected = 0xffffffff if current == requested else requested
        assert r(unit+0xb4) == expected
        ready = run(0x744270)
        assert ready == int(current != 6), (current, requested, ready)
        if ready:
            run(0x5b3570)
            assert r(unit+0xac) == requested and r(unit+0xb4) == 0xffffffff
        else:
            # Time passing and a second normal attack do not bypass Sticky.
            w(0xa8ed84, 10000)
            run(0x5b35e0, 1, 1)
            assert r(unit+0xac) == 6 and r(unit+0xb4) == 1
        rows.append({'initial':current, 'requested':requested, 'ready':bool(ready), 'status':'PASS'})
out = Path(a.output)
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({'scope':__doc__, 'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(), 'results':rows}, indent=2)+'\n')
print('PASS: original Unit readiness blocks Sticky and accepts Guard; native queued transitions verified', len(rows))
