"""Read-only private-PE audit. Execute original x86 leaves in Unicorn only.

No game is started/injected/written. Output contains addresses, outcomes and
dependency labels, never executable bytes or a disassembly dump.
Requires pefile and unicorn. Only the recorded supported SHA is used.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EAX

SUPPORTED = '268a81b7422b3bb855e1b4a162aadba4d807f88fb9f70feb99118211d8fbb8d0'
HOUSE, TYPE, TYPE_VT, OBJECT, BASE_TYPE = 0x20010000, 0x20020000, 0x20030000, 0x20040000, 0x20050000
FOUNDATION, POS, CELL, FACTORY, BUILDER = 0x20060000, 0x20060100, 0x20070000, 0x20080000, 0x20090000
OBJECT_VT, BUILDER_VT, EVENT, HOUSES = 0x20031000, 0x20032000, 0x200A0000, 0x200B0000
KIND, FOOTPRINT, FIND_FACTORY, RADIO, STOP = 0x20100000, 0x20100010, 0x20100020, 0x20100030, 0x20101000

class Harness:
    def __init__(self, pe):
        self.mu = Uc(UC_ARCH_X86, UC_MODE_32)
        self.mu.mem_map(pe.OPTIONAL_HEADER.ImageBase, (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
        self.mu.mem_write(pe.OPTIONAL_HEADER.ImageBase, pe.get_memory_mapped_image())
        self.mu.mem_map(0x20000000, 0x200000)
        self.mu.mem_map(0x30000000, 0x100000)
        self.calls = {}
        self.cell = (0, 0)
        self.ground = True
        self.complete = True
        self.stop_at_legality = False
        self.legality_result = None
        self.put(0xA83D4C, HOUSE)
        self.put(HOUSE + 0x30, 3)
        self.put(0xA8022C, HOUSES)
        self.put(HOUSES + 3 * 4, HOUSE)
        self.put(HOUSE + 0x53BC, FACTORY)
        self.put(0xA8ED6B, 0, 'B')
        self.put(0xA8B264, 0, 'B')
        self.put(0x8A03F8, 0x7FFF7FFF)
        self.put(0xB0EB58, 0x7FFF7FFF)
        self.put(TYPE, TYPE_VT)
        self.put(TYPE + 0xE08, 0)
        self.put(TYPE + 0xEB4, 1)
        self.put(TYPE_VT + 0x2C, KIND)
        self.put(TYPE_VT + 0x90, FOOTPRINT)
        self.put(TYPE_VT + 0xA8, 0x464AC0)
        self.put(FOUNDATION, 0)
        self.put(FOUNDATION + 4, 0x7FFF7FFF)
        self.put(POS, 50 | (60 << 16))
        self.put(OBJECT, OBJECT_VT)
        self.put(OBJECT + 0x21C, HOUSE)
        self.put(OBJECT + 0x520, TYPE)
        self.put(OBJECT_VT + 0x190, FIND_FACTORY)
        self.put(OBJECT_VT + 0xD8, 0x440580)
        self.put(OBJECT_VT + 0x1AC, 0x449440)
        self.put(BUILDER, BUILDER_VT)
        self.put(BUILDER_VT + 0x278, RADIO)
        self.put(BASE_TYPE + 0x154F, 1, 'B')
        self.put(0x200C0000 + 0x21C, HOUSE)
        self.put(0x200C0000 + 0x520, BASE_TYPE)
        self.mu.hook_add(UC_HOOK_CODE, self.hook)

    def put(self, at, value, fmt='I'):
        self.mu.mem_write(at, struct.pack('<' + fmt, value))

    def word(self, at):
        return struct.unpack('<I', self.mu.mem_read(at, 4))[0]

    def ret(self, value, arguments=0):
        sp = self.mu.reg_read(UC_X86_REG_ESP)
        self.mu.reg_write(UC_X86_REG_EAX, value)
        self.mu.reg_write(UC_X86_REG_ESP, sp + 4 + arguments)
        self.mu.reg_write(UC_X86_REG_EIP, self.word(sp))

    def hook(self, mu, at, size, data):
        sp = mu.reg_read(UC_X86_REG_ESP)
        if at == STOP:
            mu.emu_stop()
            return
        if at in (0x4A8EB0, 0x4A9070, 0x4C70E1, 0x4FB0E0, 0x440580, 0x449440, 0x464AC0, 0x716150):
            self.calls[hex(at)] = self.calls.get(hex(at), 0) + 1
        if at == 0x4405AB and self.stop_at_legality:
            self.legality_result = mu.reg_read(UC_X86_REG_EAX)
            mu.emu_stop()
        elif at == KIND:
            self.ret(7)
        elif at == FOOTPRINT:
            self.ret(FOUNDATION, 4)
        elif at == 0x45ECA0:
            self.ret(1, 4)
        elif at == 0x45EC90:
            self.ret(1)
        elif at == 0x5657A0:
            self.cell = struct.unpack('<hh', mu.mem_read(self.word(sp + 4), 4))
            self.put(CELL + 0x24, (self.cell[0] & 65535) | ((self.cell[1] & 65535) << 16))
            self.ret(CELL, 4)
        elif at == 0x565730:
            x, y = struct.unpack('<ii', mu.mem_read(self.word(sp + 4), 8))
            self.cell = (x // 256, y // 256)
            self.put(CELL + 0x24, (self.cell[0] & 65535) | ((self.cell[1] & 65535) << 16))
            self.ret(CELL, 4)
        elif at == 0x47C520:
            self.ret(0x200C0000 if self.cell == (50, 50) else 0)
        elif at == 0x47C620:
            self.calls['cell_legality_substitute'] = self.calls.get('cell_legality_substitute', 0) + 1
            self.ret(int(self.ground), 12)
        elif at == 0x48DCD0:
            self.ret(TYPE)
        elif at == 0x4CA130:
            self.calls['factory_complete_substitute'] = self.calls.get('factory_complete_substitute', 0) + 1
            self.ret(int(self.complete))
        elif at == 0x4CA160:
            self.ret(OBJECT)
        elif at == FIND_FACTORY:
            self.ret(BUILDER, 8)
        elif at == RADIO:
            self.ret(0, 8)

    def run(self, entry, this, args):
        sp = 0x300F0000
        self.mu.mem_write(sp, struct.pack('<' + 'I' * (1 + len(args)), STOP, *args))
        self.mu.reg_write(UC_X86_REG_ESP, sp)
        self.mu.reg_write(UC_X86_REG_ECX, this)
        self.mu.emu_start(entry, STOP, count=1000000)
        assert self.mu.reg_read(UC_X86_REG_EIP) == STOP or self.legality_result is not None
        return self.mu.reg_read(UC_X86_REG_EAX) & 255

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    raw = args.exe.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    assert digest == SUPPORTED, 'Unverified executable; no address substitution'
    pe = pefile.PE(data=raw)
    rows = []
    for label, y, adjacent, place_anywhere, expected in [('near',52,1,0,1),('far',60,1,0,0),('far_expanded_adjacent',60,12,0,1),('far_PlaceAnywhere_only',60,1,1,0)]:
        h = Harness(pe)
        h.put(POS, 50 | y << 16)
        h.put(TYPE + 0xEB4, adjacent)
        h.put(TYPE + 0x1703, place_anywhere, 'B')
        value = h.run(0x4A8EB0, 0x87F7E8, [TYPE,3,FOUNDATION,POS])
        assert value == expected
        rows.append(dict(case=label,returned=value,calls=h.calls))
    for flag in (0,1):
        for ground in (False,True):
            h = Harness(pe)
            h.ground = ground
            h.put(TYPE + 0x1703,flag,'B')
            value = h.run(0x464AC0,TYPE,[POS,HOUSE])
            assert value == int(flag or ground)
            assert h.calls.get('cell_legality_substitute',0) == int(not flag)
            rows.append(dict(case='CanPlaceHere',place_anywhere=flag,ground_substitute=ground,returned=value,calls=h.calls))
    for complete,ground,flag in [(False,False,0),(True,False,0),(True,True,0),(True,False,1)]:
        h = Harness(pe)
        h.complete,h.ground,h.stop_at_legality = complete,ground,True
        h.put(TYPE + 0x1703,flag,'B')
        h.put(EVENT,0x0B,'B');h.put(EVENT+2,3,'B')
        for offset,value in [(7,7),(11,12),(15,0),(19,50|(60<<16))]:
            h.put(EVENT+offset,value)
        h.run(0x4C6CB0,EVENT,[])
        expected = None if not complete else 0 if flag or ground else 7
        assert h.legality_result == expected
        assert not h.calls.get('0x4a8eb0') and not h.calls.get('0x4a9070')
        rows.append(dict(case='Place_event_to_Unlimbo_initial_legality',factory_complete_substitute=complete,ground_substitute=ground,place_anywhere=flag,move_result=h.legality_result,calls=h.calls))
    text = next(s for s in pe.sections if s.Characteristics & 0x20000000)
    data = text.get_data();base = pe.OPTIONAL_HEADER.ImageBase + text.VirtualAddress
    refs = {hex(target): [hex(base+i) for i in range(len(data)-5) if data[i] == 0xE8 and base+i+5+struct.unpack_from('<i',data,i+1)[0] == target] for target in (0x4A8EB0,0x4A9070,0x4C6AE0)}
    result = dict(executable_sha256=digest,private_executable_modified=False,game_executed=False,engine_code_modified_in_emulator=False,checks=len(rows),direct_call_candidates=refs,cases=rows,
                  boundary='Original x86 functions; factory/object/geometry services and Cell legality are explicit substitutes. Event execution stops after Unlimbo initial legality. No completed building, Windows UI, platform or netplay claim.')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(checks=len(rows),status='PASS',sha256=digest),ensure_ascii=False))

if __name__ == '__main__':
    main()
