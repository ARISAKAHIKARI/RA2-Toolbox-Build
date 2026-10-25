"""Compare production resource/TMP selection with captured original x86.

Private capture bytes remain local. The replay executes the original resource
lookup, flat/slope pool selection, TMP coordinate selection and linked-variant
dispatch. Synthetic cells/types/tables and a GetImage boundary isolate these
decisions; no game update, original rasterizer, Windows or multiplayer runs.
"""
import argparse
import hashlib
import itertools
import json
from pathlib import Path
import struct
import subprocess
import tempfile

from native_bib_capture import Capture, SUPPORTED_SHA
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP)


class Native:
    STACK = 0x70000000
    FIX = 0x71000000
    CELL = FIX
    LIST = FIX + 0x1000
    RESOURCE_LIST = FIX + 0x2000
    RESOURCE = FIX + 0x3000
    TYPES = FIX + 0x10000
    TILE = FIX + 0x40000
    SHAPE = FIX + 0x60000
    FRAME = FIX + 0x61000
    STOP = FIX + 0x70000

    def __init__(self, capture):
        self.capture = capture
        self.u = capture.emulator()
        self.u.mem_map(self.STACK, 0x10000)
        self.u.mem_map(self.FIX, 0x80000)
        self.put(0xA83D84, self.LIST)
        self.put(0xB0F4EC, self.RESOURCE_LIST)
        self.put(0xB0F4F8, 1)
        self.put(self.RESOURCE_LIST, self.RESOURCE)
        self.put(self.RESOURCE + 0x98, 0)
        for i in range(128):
            p = self.TYPES + i * 0x400
            self.put(self.LIST + i * 4, p)
            self.put(p + 0x294, i)
            self.u.mem_write(p + 0x2A9, b'\1')
        self.put(0xA8ED2C, self.RESOURCE_LIST + 0x100)
        self.put(self.RESOURCE_LIST + 0x100, self.TILE)
        for i in range(8):
            self.put(self.TILE + i * 0x1000, 0x71079000)
            self.put(self.TILE + i * 0x1000 + 0x2BC, self.TILE + (i + 1) * 0x1000)
        # GetImage is the only intercepted call in the TMP selection path.
        self.get_image = 0x7107A000
        self.put(0x71079000 + 0x9C, self.get_image)
        for i in range(16):
            self.put(0x81CCA8 + i * 4, ((i * 3) ^ (i >> 1)) & 3)
        for i in range(64):
            self.put(0x89E620 + i * 4, (i * 5 + (i >> 3)) & 7)
        self.u.mem_write(0x89E7C7, b'\1')

        def boundary(u, address, size, data):
            if address == self.get_image:
                sp = u.reg_read(UC_X86_REG_ESP)
                ret = self.word(sp)
                u.reg_write(UC_X86_REG_EAX, self.SHAPE)
                u.reg_write(UC_X86_REG_ESP, sp + 4)
                u.reg_write(UC_X86_REG_EIP, ret)
        self.u.hook_add(UC_HOOK_CODE, boundary)

    def put(self, p, value):
        self.u.mem_write(p, struct.pack('<I', value & 0xFFFFFFFF))

    def word(self, p):
        return struct.unpack('<I', self.u.mem_read(p, 4))[0]

    def overlay(self, first, flat, slopes, stored, x, y, slope):
        u = self.u
        self.put(self.RESOURCE + 0xE0, self.TYPES + first * 0x400)
        self.put(self.RESOURCE + 0xE8, flat)
        self.put(self.RESOURCE + 0xEC, slopes)
        u.mem_write(self.CELL + 0x24, struct.pack('<hh', x, y))
        u.mem_write(self.CELL + 0x11C, bytes([slope]))
        sp = self.STACK + 0x8000
        self.put(sp, self.STOP)
        u.reg_write(UC_X86_REG_ESP, sp)
        u.reg_write(UC_X86_REG_ECX, stored)
        u.emu_start(0x5FDD20, self.STOP, count=10000)
        assert u.reg_read(UC_X86_REG_EIP) == self.STOP
        assert u.reg_read(UC_X86_REG_ESP) == sp + 4
        assert u.reg_read(UC_X86_REG_EAX) == 0
        u.reg_write(UC_X86_REG_ESI, self.CELL)
        u.reg_write(UC_X86_REG_ESP, sp)
        u.emu_start(0x47F871, 0x47F900, count=1000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x47F900
        chosen = u.reg_read(UC_X86_REG_ECX)
        return (chosen - self.TYPES) // 0x400

    def tile(self, count, sub, x, y, flags, broken, width, height):
        u = self.u
        self.put(self.TILE + 0x2F0, count)
        self.put(self.SHAPE, width)
        self.put(self.SHAPE + 4, height)
        for i in range(width * height):
            self.put(self.SHAPE + 16 + i * 4, self.FRAME + i * 0x100)
            self.put(self.FRAME + i * 0x100 + 36, 4 if broken else 0)
        self.put(self.CELL + 0x38, 0)
        self.put(self.CELL + 0x140, flags)
        u.mem_write(self.CELL + 0x24, struct.pack('<hh', x, y))
        u.mem_write(self.CELL + 0x11A, bytes([sub]))
        sp = self.STACK + 0x8000
        u.reg_write(UC_X86_REG_ESP, sp)
        u.reg_write(UC_X86_REG_ESI, self.CELL)
        u.reg_write(UC_X86_REG_EBX, 0)
        u.emu_start(0x480389, 0x480403, count=10000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x480403
        assert u.reg_read(UC_X86_REG_ESP) == sp
        variation = self.word(sp + 0x28)
        u.reg_write(UC_X86_REG_EAX, variation)
        u.reg_write(UC_X86_REG_ESI, self.TILE)
        u.emu_start(0x547D15, 0x547D68, count=1000)
        assert u.reg_read(UC_X86_REG_EIP) == 0x547D68
        return (u.reg_read(UC_X86_REG_ESI) - self.TILE) // 0x1000


def cases():
    result = []
    for first, flat, slopes in ((0, 12, 8), (30, 12, 0), (50, 5, 12)):
        for x, y, slope in itertools.product((0, 1, 7, 31, 511), (0, 2, 13, 127, 511), range(5) if slopes else (0,)):
            result.append(('O', (first, flat, slopes, first + min(2, flat - 1), x, y, slope)))
    for count, sub, x, y, broken, damaged, dims in itertools.product(
            (1, 2, 4, 8), (0, 1, 3, 7), (0, 1, 7, 31, 511),
            (0, 2, 13, 511), (0, 1), (0, 1), ((1, 1), (2, 2))):
        result.append(('T', (count, sub, x, y, damaged << 13, broken, *dims)))
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    fixture = Path(__file__).with_name('art_variants.cpp')
    requests = cases()
    with tempfile.TemporaryDirectory(prefix='ra2-art-variants-') as temp:
        binary = Path(temp) / 'production'
        subprocess.run(['g++', '-std=c++17', '-O2', str(fixture), '-o', str(binary)], check=True)
        lines = '\n'.join(k + ' ' + ' '.join(map(str, a)) for k, a in requests) + '\n'
        output = subprocess.check_output([str(binary), '--probe'], input=lines.encode())
        production = list(map(int, output.split()))
    assert len(production) == len(requests)
    stages = {}
    for stage in ('01-off', '02-on', '03-off'):
        capture = Capture(args.capture, stage)
        native = Native(capture)
        for n, (kind, values) in enumerate(requests):
            original = native.overlay(*values) if kind == 'O' else native.tile(*values)
            assert production[n] == original, (stage, kind, values, production[n], original)
        pools = []
        for p in capture.vector(0xB0F4E8):
            pools.append({'name': capture.name(p + 0x24), 'first_type': capture.name(capture.word(p + 0xE0) + 0x24),
                'flat': capture.signed(p + 0xE8), 'slopes': capture.signed(p + 0xEC)})
        ranges = ((0x5FDD20, 0x5FDDDE), (0x47F871, 0x47F900), (0x480389, 0x480403),
                  (0x4814F0, 0x48166A), (0x5471F0, 0x547224), (0x547D15, 0x547D68))
        stages[stage] = {'cases': len(requests), 'mismatches': 0, 'resource_pools': pools,
            'code_sha256': {f'{a:08X}-{b:08X}': hashlib.sha256(capture.read(a, b-a)).hexdigest() for a, b in ranges}}
        del native, capture
    result = {'supported_executable_sha256': SUPPORTED_SHA.lower(), 'scope': __doc__,
        'cases': len(requests) * len(stages), 'mismatches': 0, 'stages': stages,
        'game_tested': False, 'windows_rendering_tested': False, 'multiplayer_tested': False}
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    Path(args.output).write_text(json.dumps(result, indent=2) + '\n')
    print(f"PASS {result['cases']} original/production art selection cases; no rasterizer or game simulation executed")
