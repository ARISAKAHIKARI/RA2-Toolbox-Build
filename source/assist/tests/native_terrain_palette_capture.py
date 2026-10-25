"""Verify ore-mine terrain palette/light/position branches in original x86 code.

Input is a private capture accepted by native_bib_capture.Capture. The output
contains code hashes, source choices and aggregate counters, not game assets,
raw palettes, executable bytes or a framebuffer. Selection slices execute
unchanged; bootstrap file/allocator/Convert construction boundaries are doubled.
No live game, rasterizer, Windows display or multiplayer update is executed.
"""
import argparse
from collections import Counter
import hashlib
import itertools
import json
import struct

from native_bib_capture import Capture, SUPPORTED_SHA
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESI,
    UC_X86_REG_ESP,
)


class Probe:
    STACK = 0x70000000
    FIXTURE = 0x71000000
    OBJECT = FIXTURE
    TYPE = FIXTURE + 0x1000
    CELL = FIXTURE + 0x2000
    RESOURCE = FIXTURE + 0x3000
    ISO = FIXTURE + 0x4000
    FILE = FIXTURE + 0x6000

    def __init__(self, capture):
        self.u = capture.emulator()
        self.u.mem_map(self.STACK, 0x10000)
        self.u.mem_map(self.FIXTURE, 0x10000)
        self.write(self.OBJECT + 0xC8, self.TYPE)
        self.write(self.CELL + 0x34, self.ISO)
        self.write(0x87F6BC, self.RESOURCE)

    def read(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def write(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def select(self, path, flag, object_light, terrain_light, y):
        u = self.u
        sp = self.STACK + 0x8000
        u.mem_write(self.TYPE + 0x2B1, bytes([flag]))
        u.mem_write(self.CELL + 0x10A, struct.pack('<hh', object_light, terrain_light))
        u.reg_write(UC_X86_REG_ESP, sp)
        u.reg_write(UC_X86_REG_ESI, self.OBJECT)
        if path == 'draw':
            begin, end, position, brightness = 0x71C280, 0x71C2BC, sp + 0x1C, UC_X86_REG_EBP
            u.reg_write(UC_X86_REG_EBP, self.CELL)
        else:
            begin, end, position, brightness = 0x71C40F, 0x71C44B, sp + 0x18, UC_X86_REG_ESI
            u.reg_write(UC_X86_REG_EBX, self.CELL)
        u.mem_write(position, struct.pack('<ii', 320, y))
        u.emu_start(begin, end, count=100)
        assert u.reg_read(UC_X86_REG_EIP) == end
        actual = (u.reg_read(UC_X86_REG_EDX), u.reg_read(brightness),
                  struct.unpack('<ii', u.mem_read(position, 8)))
        expected = (self.RESOURCE if flag else self.ISO,
                    object_light if flag else terrain_light,
                    (320, y - 16 if flag else y))
        assert actual == expected, (path, flag, object_light, terrain_light, y, actual, expected)

    def bootstrap(self):
        """Execute original filename/6-bit expansion/alias logic on synthetic RGB."""
        u = self.u
        source = bytes((i * 7 + 3) % 64 for i in range(768))
        u.mem_write(self.FILE, source)
        calls = []

        def returned(value, cleanup=0):
            sp = u.reg_read(UC_X86_REG_ESP)
            ret = self.read(sp)
            u.reg_write(UC_X86_REG_EAX, value)
            u.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
            u.reg_write(UC_X86_REG_EIP, ret)

        def boundary(_u, address, _size, _data):
            sp = u.reg_read(UC_X86_REG_ESP)
            if address == 0x5B40B0:
                filename = bytes(u.mem_read(u.reg_read(UC_X86_REG_ECX), 32)).split(b'\0')[0].decode('ascii')
                calls.append({'file': filename})
                returned(self.FILE)
            elif address == 0x7CA090:
                destination, source_at, size = struct.unpack('<III', u.mem_read(sp + 4, 12))
                assert source_at == self.FILE and size == 768
                u.mem_write(destination, bytes(u.mem_read(source_at, size)))
                returned(destination)
            elif address == 0x7C8E17:
                assert self.read(sp + 4) == 0x188
                returned(self.RESOURCE)
            elif address == 0x48E740:
                palette, palette8, surface, shades, skip = struct.unpack('<IIIII', u.mem_read(sp + 4, 20))
                assert u.reg_read(UC_X86_REG_ECX) == self.RESOURCE
                assert bytes(u.mem_read(palette, 768)) == bytes(v * 4 for v in source)
                assert palette8 == 0x885780 and surface and shades == 53 and skip == 0
                calls.append({'constructor': '48E740', 'expanded_synthetic_channels': 768, 'shade_count': shades})
                returned(self.RESOURCE, 20)

        hook = u.hook_add(UC_HOOK_CODE, boundary)
        try:
            u.reg_write(UC_X86_REG_ESP, self.STACK + 0x8000)
            u.reg_write(UC_X86_REG_EBX, 0)
            u.emu_start(0x52BFCE, 0x52C082, count=20000)
            assert u.reg_read(UC_X86_REG_EIP) == 0x52C082
            assert self.read(0x87F6B4) == self.RESOURCE
            self.write(0x87F6BC, 0)
            u.emu_start(0x52C1EA, 0x52C200, count=30)
            assert u.reg_read(UC_X86_REG_EIP) == 0x52C200
            assert self.read(0x87F6BC) == self.RESOURCE
        finally:
            u.hook_del(hook)
        assert calls[0] == {'file': 'UNITSNO.PAL'} and len(calls) == 2
        return {'file': calls[0]['file'], 'shade_count': 53,
                'expanded_synthetic_channels': 768, 'resource_aliases_unit_converter': True}


def inspect(capture):
    types, instances = {}, Counter()
    for pointer in capture.vector(0xA8E988):
        if capture.word(pointer) != 0x7F522C:
            continue
        terrain_type = capture.word(pointer + 0xC8)
        flag = capture.read(terrain_type + 0x2B1, 1)[0]
        name = capture.name(terrain_type + 0x24, 24)
        if flag:
            instances[name] += 1
            types[name] = {'spawns_tiberium': flag,
                           'animated': capture.read(terrain_type + 0x2B3, 1)[0],
                           'frames': capture.shape(capture.word(terrain_type + 0xA4)).get('frames')}
    convert = capture.word(0x87F6BC)
    middle = capture.word(convert + 0x174)
    colors = capture.read(middle, 512)
    probe = Probe(capture)
    cases = 0
    for args in itertools.product(('draw', 'alternate_draw'), (0, 1, 2),
                                  ((0, 2000), (825, 1100), (1000, 900), (2000, 0)),
                                  (-32, 160)):
        path, flag, levels, y = args
        probe.select(path, flag, *levels, y)
        cases += 1
    return {
        'theater': capture.word(0x822CF8),
        'ore_mines': dict(sorted(instances.items())), 'types': types,
        'resource_converter': {
            'vtable': f'{capture.word(convert):08X}',
            'bytes_per_pixel': capture.word(convert + 4),
            'shade_count': capture.word(convert + 0x16C),
            'same_as_unit_converter': convert == capture.word(0x87F6B4),
            'middle_rgb565_sha256': hashlib.sha256(colors).hexdigest(),
        },
        'native_selection_cases': cases, 'selection_mismatches': 0,
        'bootstrap': probe.bootstrap(),
        'code_sha256': {
            f'{start:08X}-{end:08X}': hashlib.sha256(capture.read(start, end - start)).hexdigest()
            for start, end in ((0x71C1B0, 0x71C35D), (0x71C360, 0x71C4CD),
                               (0x52BFCE, 0x52C082), (0x52C1EA, 0x52C200))
        },
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    results = {stage: inspect(Capture(args.capture, stage))
               for stage in ('01-off', '02-on', '03-off')}
    result = {'supported_executable_sha256': SUPPORTED_SHA.lower(), 'scope': __doc__,
              'native_selection_cases': sum(r['native_selection_cases'] for r in results.values()),
              'selection_mismatches': 0, 'stages': results,
              'game_tested': False, 'windows_rendering_tested': False, 'multiplayer_tested': False}
    with open(args.output, 'w') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print(f"PASS {result['native_selection_cases']} original terrain source/brightness/offset cases; "
          'three native bootstrap slices select UNITSNO.PAL and alias its converter')
