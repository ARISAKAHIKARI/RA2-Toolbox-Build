"""Execute the original tactical terrain gate and camera clamp on controlled inputs.

The 6D521C gate and its 568300 CoordinatesLegal callee run unchanged. Cell
lookup (5657A0) and draw (47EC90) are boundary doubles that record invocation.
578540 independently classifies gameplay usability; it is not substituted into
the gate. This proves admission to drawing, not final pixels or full visibility.
6D8640 is compared with the compiled production clamp for ordinary viewports.
The tool's midpoint behavior on an undersized map is a separately named policy.
Private capture bytes, palettes and images are never written to the report.
"""
import argparse
from collections import Counter
import hashlib
import itertools
import json
import math
from pathlib import Path
import random
import struct
import subprocess
import tempfile

from native_bib_capture import Capture, SUPPORTED_SHA
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP,
    UC_X86_REG_ESP,
)

RANGES = {
    'CoordinatesLegal': (0x568300, 0x568345),
    'IsWithinUsableArea': (0x578540, 0x5785F0),
    'tactical_terrain_gate': (0x6D521C, 0x6D526A),
    'camera_clamp': (0x6D8640, 0x6D86FE),
}


class Native:
    MAP = 0x87F7E8
    CELL = 0x2000000
    STACK = 0x2010000
    STOP = 0x2020000

    def __init__(self, capture):
        self.u = Uc(UC_ARCH_X86, UC_MODE_32)
        pages = {start & ~4095 for start, _ in RANGES.values()}
        pages.update((0x565000, 0x47E000, 0x87F000, 0x880000, 0x886000,
                      self.CELL, self.STACK, self.STOP))
        for page in pages:
            self.u.mem_map(page, 4096)
        for start, end in RANGES.values():
            self.u.mem_write(start, capture.read(start, end - start))
        self.u.mem_write(0x5657A0, b'\xcc')
        self.u.mem_write(0x47EC90, b'\xcc')
        self.u.mem_write(self.STOP, b'\x90')
        self.calls = []
        self.expected_cell = None
        self.u.hook_add(UC_HOOK_CODE, self.boundary)

    def read(self, address):
        return struct.unpack('<I', self.u.mem_read(address, 4))[0]

    def write(self, address, value):
        self.u.mem_write(address, struct.pack('<I', value & 0xFFFFFFFF))

    def bounds(self, bounds):
        self.u.mem_write(self.MAP + 0xF4, struct.pack('<6i', *bounds))

    def returned(self, value, cleanup):
        sp = self.u.reg_read(UC_X86_REG_ESP)
        self.u.reg_write(UC_X86_REG_EAX, value)
        self.u.reg_write(UC_X86_REG_ESP, sp + 4 + cleanup)
        self.u.reg_write(UC_X86_REG_EIP, self.read(sp))

    def boundary(self, _u, address, _size, _data):
        if address == 0x568300:
            self.calls.append('CoordinatesLegal')
        elif address == 0x578540:
            self.calls.append('IsWithinUsableArea')
        elif address == 0x5657A0:
            sp = self.u.reg_read(UC_X86_REG_ESP)
            assert self.u.reg_read(UC_X86_REG_ECX) == self.MAP
            actual = struct.unpack('<hh', self.u.mem_read(self.read(sp + 4), 4))
            assert actual == self.expected_cell
            self.calls.append('lookup')
            self.returned(self.CELL, 4)
        elif address == 0x47EC90:
            sp = self.u.reg_read(UC_X86_REG_ESP)
            assert self.u.reg_read(UC_X86_REG_ECX) == self.CELL
            assert self.read(sp + 8) == 0x886FA0
            self.calls.append('draw')
            self.returned(1, 12)

    def predicate(self, address, bounds, x, y, level, slope):
        self.bounds(bounds)
        self.u.mem_write(self.CELL + 0x24, struct.pack('<hh', x, y))
        self.u.mem_write(self.CELL + 0x11B, struct.pack('<bB', level, slope))
        pointer = self.CELL + 0x24 if address == 0x568300 else self.CELL
        sp = self.STACK + 2048
        self.u.mem_write(sp, struct.pack('<III', self.STOP, pointer, 1))
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.reg_write(UC_X86_REG_ECX, self.MAP)
        self.u.emu_start(address, self.STOP, count=200)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.STOP
        return bool(self.u.reg_read(UC_X86_REG_EAX) & 255)

    def draw_gate(self, bounds, x, y):
        self.bounds(bounds)
        sp = self.STACK + 1024
        self.u.mem_write(sp, bytes(128))
        self.u.mem_write(sp + 0x28, struct.pack('<hh', x, y))
        self.write(sp + 0x30, 160)
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.reg_write(UC_X86_REG_EDX, sp + 0x28)
        self.u.reg_write(UC_X86_REG_EAX, 320)
        self.expected_cell = (x, y)
        self.calls = []
        self.u.emu_start(0x6D521C, 0x6D526A, count=300)
        assert self.u.reg_read(UC_X86_REG_EIP) == 0x6D526A
        assert self.u.reg_read(UC_X86_REG_ESP) == sp
        assert self.calls in (['CoordinatesLegal'], ['CoordinatesLegal', 'lookup', 'draw'])
        return self.calls[-1] == 'draw'

    def camera(self, bounds, viewport, x, y):
        self.bounds(bounds)
        self.u.mem_write(0x886FA8, struct.pack('<ii', *viewport))
        point = self.CELL + 0x400
        self.u.mem_write(point, struct.pack('<ii', x, y))
        sp = self.STACK + 2048
        self.u.mem_write(sp, struct.pack('<II', self.STOP, point))
        self.u.reg_write(UC_X86_REG_ESP, sp)
        self.u.emu_start(0x6D8640, self.STOP, count=300)
        assert self.u.reg_read(UC_X86_REG_EIP) == self.STOP
        assert self.u.reg_read(UC_X86_REG_ESP) == sp + 8
        return struct.unpack('<ii', self.u.mem_read(point, 8))


def camera_limits(bounds, viewport):
    width, _height, left, top, columns, rows = bounds
    vw, vh = viewport
    return (30 * (2 * left - width) + vw // 2,
            30 * (2 * (left + columns) - width) - (vw - vw // 2),
            15 * (2 * top + width - 5) + vh // 2,
            15 * (2 * (top + rows) + width + 4) - (vh - vh // 2))


def portable(cases, source):
    driver = r'''
#include "@HEADER@"
#include <iomanip>
#include <iostream>
int main(){char mode;external::mapbounds::Bounds b;std::cout<<std::setprecision(17);
 while(std::cin>>mode>>b.width>>b.height>>b.left>>b.top>>b.columns>>b.rows){
  if(mode=='R'){int x,y,level,slope;std::cin>>x>>y>>level>>slope;std::cout<<b.legal(x,y)<<' '<<b.usable(x,y,level,slope)<<'\n';}
  else{double vw,vh,x,y;std::cin>>vw>>vh>>x>>y;bool valid=b.clampCamera(x,y,vw,vh);std::cout<<valid<<' '<<x<<' '<<y<<'\n';}
 }
}
'''
    with tempfile.TemporaryDirectory(prefix='ra2-render-bounds-') as temp:
        cpp, exe = Path(temp) / 'driver.cpp', Path(temp) / 'driver'
        cpp.write_text(driver.replace('@HEADER@', str(source / 'external_reader/source/map_bounds.h')))
        subprocess.run(['g++', '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
        result = subprocess.run([str(exe)], input=''.join(' '.join(map(str, row)) + '\n' for row in cases),
                                text=True, capture_output=True, check=True)
    return [tuple(map(float, line.split())) for line in result.stdout.splitlines()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    capture = Capture(args.capture, '02-on')
    native = Native(capture)
    rng = random.Random(1386)
    bounds_cases = [(131, 146, 3, 5, 124, 135), (64, 64, 2, 2, 60, 56),
                    (64, 64, 12, 15, 24, 20), (32, 64, 0, 0, 32, 64)]
    cases, expected = [], []
    categories = Counter()
    examples = []
    for bounds in bounds_cases:
        width, height, left, top, columns, rows = bounds
        marks = (-1, 0, 1, width // 2, width, height, width + height, 511, 512)
        points = set(itertools.product(marks, repeat=2))
        points.update((rng.randrange(0, 512), rng.randrange(0, 512)) for _ in range(160))
        for y in sorted({1, width // 2, width, height, width + height - 1, width + height + 1}):
            edges = (width - y, width + y, y - width, width + 2 * height - y,
                     2 * left - width + y, 2 * (left + columns) - width + y,
                     width + 2 * top - y, width + 2 * (top + rows) + 2 - y)
            points.update((edge + delta, y) for edge in edges for delta in (-1, 0, 1)
                          if -32768 <= edge + delta < 32768)
        example_cells = set()
        for (x, y), level, slope in itertools.product(sorted(points), (0, 4, 14), (0, 1)):
            legal = native.predicate(0x568300, bounds, x, y, level, slope)
            usable = legal and native.predicate(0x578540, bounds, x, y, level, slope)
            drawn = native.draw_gate(bounds, x, y)
            assert drawn == legal, (bounds, x, y, legal, usable, drawn)
            label = 'legal_usable_drawn' if usable else 'legal_outside_usable_drawn' if legal else 'physical_illegal_not_drawn'
            categories[label] += 1
            if legal and not usable and (x, y) not in example_cells and len(example_cells) < 3:
                examples.append({'bounds': bounds, 'cell': [x, y], 'level': level, 'slope': slope,
                                 'coordinates_legal': True, 'gameplay_usable': False, 'draw_called': True})
                example_cells.add((x, y))
            cases.append(('R', *bounds, x, y, level, slope))
            expected.append((int(legal), int(usable)))
    render_count = len(cases)
    camera_count = odd_count = skipped_small = 0
    for bounds, viewport in itertools.product(bounds_cases, ((640, 512), (800, 600), (801, 601), (1024, 768), (1025, 769))):
        xlo, xhi, ylo, yhi = camera_limits(bounds, viewport)
        if xhi < xlo or yhi < ylo:
            skipped_small += 1
            continue
        xs = {-50000, 50000, xlo - 1, xlo, xlo + 1, (xlo + xhi) // 2, xhi - 1, xhi, xhi + 1}
        ys = {-50000, 50000, ylo - 1, ylo, ylo + 1, (ylo + yhi) // 2, yhi - 1, yhi, yhi + 1}
        for x, y in itertools.product(sorted(xs), sorted(ys)):
            destination = native.camera(bounds, viewport, x, y)
            cases.append(('C', *bounds, *viewport, x, y))
            expected.append((1, *destination))
            camera_count += 1
            odd_count += bool(viewport[0] % 2 or viewport[1] % 2)
    policy_count = 0
    for bounds, viewport, point in itertools.product(
            ((64, 64, 25, 25, 4, 4), (64, 64, 2, 2, 8, 60)),
            ((640, 512), (801, 601)), ((-50000, -50000), (0, 0), (50000, 50000))):
        xlo, xhi, ylo, yhi = camera_limits(bounds, viewport)
        def policy(value, lo, hi):
            return math.trunc((lo + hi) / 2) if hi < lo else max(lo, min(hi, value))
        cases.append(('P', *bounds, *viewport, *point))
        expected.append((1, policy(point[0], xlo, xhi), policy(point[1], ylo, yhi)))
        policy_count += 1
    actual = portable(cases, Path(__file__).resolve().parents[2])
    assert len(actual) == len(cases)
    mismatches = [{'input': case, 'expected': want, 'actual': got}
                  for case, want, got in zip(cases, expected, actual) if want != got]
    assert not mismatches, mismatches[:8]
    assert all(categories[name] for name in ('legal_usable_drawn', 'legal_outside_usable_drawn', 'physical_illegal_not_drawn'))
    report = {
        'supported_executable_sha256': SUPPORTED_SHA.lower(), 'capture_stage_executed': '02-on',
        'scope': __doc__,
        'code_sha256': {name: {'begin': f'{start:08X}', 'end_exclusive': f'{end:08X}',
                              'sha256': hashlib.sha256(capture.read(start, end - start)).hexdigest()}
                        for name, (start, end) in RANGES.items()},
        'render_gate': {'cases': render_count, 'mismatches': 0, 'categories': dict(categories),
                        'examples': examples, 'lookup_boundary': '005657A0', 'draw_boundary': '0047EC90',
                        'usable_predicate_is_not_called_by_gate': True},
        'camera_native_comparison': {'cases': camera_count, 'odd_viewport_cases': odd_count,
                                     'mismatches': 0, 'undersized_viewport_combinations_excluded': skipped_small},
        'tool_midpoint_policy': {'cases': policy_count, 'mismatches': 0,
                                 'native_equivalence_claimed': False,
                                 'rule': 'Use truncated midpoint only for an inverted per-axis camera range; otherwise clamp.'},
        'compiled_production_header': 'external_reader/source/map_bounds.h',
        'game_tested': False, 'full_pixel_renderer_tested': False, 'multiplayer_tested': False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS {render_count} original terrain gate cases {dict(categories)}; '
          f'{camera_count} native camera comparisons ({odd_count} odd viewports); '
          f'{policy_count} separate tool midpoint policy cases')


if __name__ == '__main__':
    main()
