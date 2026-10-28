"""Actual ordinary v17 clamp plus supplied Runtime under controlled x86/Win32.

Maps the official ordinary DLL with PE relocations and executes its replacement
clamp. Does not load Syringe, a Windows graphics device or multiplayer peers.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import importlib.util
import json
import struct

import pefile
from unicorn import UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--game', type=Path, required=True)
parser.add_argument('--runtime', type=Path, required=True)
parser.add_argument('--spawner', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
official_sha = '58f198973bd0fd3b0e1b2c42fc6c545efe1fd399680abc2e8d6e9e189751614d'
assert hashlib.sha256(args.spawner.read_bytes()).hexdigest() == official_sha, 'Requires official CnCNetYR ordinary v0.0.0.17'
ordinary_base = 0x18000000
camera_site = 0x6D8640
clamp_rva = 0x2A50
# This public ordinary jump target was independently resolved by executing its
# actual CRT patch constructor, not inferred from uninitialized file data.
patch = b'\xe9' + struct.pack('<i', ordinary_base + clamp_rva - camera_site - 5)
rows = []
for initial_small in (False, True):
    spec = importlib.util.spec_from_file_location('ordinary_smallmap_fixture', args.repo / 'source/assist/tests/entry_runtime_regression.py')
    m = importlib.util.module_from_spec(spec)
    m.fixture_paths = [str(args.game), str(args.runtime)]
    spec.loader.exec_module(m)
    ordinary = pefile.PE(str(args.spawner))
    ordinary.relocate_image(ordinary_base)
    m.u.mem_map(ordinary_base, (ordinary.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
    m.u.mem_write(ordinary_base, ordinary.get_memory_mapped_image())
    m.images.append(ordinary)
    m.u.mem_write(camera_site, patch)
    assert camera_site + 5 + struct.unpack('<i', patch[1:])[0] == ordinary_base + clamp_rva
    s = m.Shared.from_buffer_copy(m.u.mem_read(m.SH, C.sizeof(m.Shared)))
    s.flags = 0
    s.externalDisplay = 1
    m.u.mem_write(m.SH, bytes(s))
    m.w(0xA8B238, 3)

    def geometry(small):
        values = (10, 0, 0, 10, 10, 1280, 960) if small else (200, 2, 4, 190, 190, 1120, 960)
        for address, value in zip((0x87F8DC, 0x87F8E4, 0x87F8E8, 0x87F8EC, 0x87F8F0, 0x886FA8, 0x886FAC), values):
            m.w(address, value)
        return values

    def expected(values, point):
        map_width, x, y, width, height, view_width, view_height = values
        low_x = view_width // 2 + 30 * (2 * x - map_width)
        high_x = low_x + 60 * width - view_width
        low_y = view_height // 2 + 15 * (2 * y + map_width - 5)
        high_y = low_y + 30 * height - view_height + 135

        def bound(value, low, high):
            return int((low + high) / 2) if high < low else min(max(value, low), high)

        return bound(point[0], low_x, high_x), bound(point[1], low_y, high_y)

    point = m.alloc(8)
    calls = [0]

    def native_clamp(values):
        for xy in ((-1000, -1000), (0, 0), (100000, 100000)):
            m.u.mem_write(point, struct.pack('<2i', *xy))
            m.run(camera_site, (point,), m.r(0x887324))
            actual = struct.unpack('<2i', m.u.mem_read(point, 8))
            assert actual == expected(values, xy), (values, xy, actual, expected(values, xy))
            # Native C++ bool return defines AL, not all of EAX.
            assert bool(m.u.reg_read(UC_X86_REG_EAX) & 255) == (actual != xy)
            m.run(camera_site, (point,), m.r(0x887324))
            assert (m.u.reg_read(UC_X86_REG_EAX) & 255) == 0
            calls[0] += 2

    values = geometry(initial_small)
    native_clamp(values)
    writes = []
    m.u.hook_add(UC_HOOK_MEM_WRITE, lambda u, access, address, size, value, user: writes.append((address, size)) if camera_site <= address < camera_site + 5 else None)
    stages = []
    for label, small in [('initial', initial_small), ('small', True), ('large', False), ('small_again', True)]:
        values = geometry(small)
        out = m.hook()
        native_clamp(values)
        assert bytes(m.u.mem_read(camera_site, 5)) == patch
        assert not writes
        assert out.data.status == 1 and out.data.hookInstalled == 0
        stages.append({'stage': label, 'small': small, 'hook_conflicts': out.data.hookConflicts, 'hook_installed': out.data.hookInstalled})
    s = m.Shared.from_buffer_copy(m.u.mem_read(m.SH, C.sizeof(m.Shared)))
    s.stop = 1
    m.u.mem_write(m.SH, bytes(s))
    m.hook()
    native_clamp(values)
    assert bytes(m.u.mem_read(camera_site, 5)) == patch and not writes
    rows.append({'initial_small': initial_small, 'ordinary_base': hex(ordinary_base), 'ordinary_clamp_rva': hex(clamp_rva), 'stages': stages, 'clamp_calls': calls[0], 'site_writes': len(writes), 'stop_preserves_ordinary_patch': True, 'status': 'PASS'})
    print('PASS', args.game.name, 'initial_small=' + str(initial_small), calls[0], 'ordinary clamp calls')
report = {'scope': __doc__, 'runtime_sha256': hashlib.sha256(args.runtime.read_bytes()).hexdigest(), 'ordinary_sha256': official_sha, 'game_sha256': hashlib.sha256(args.game.read_bytes()).hexdigest(), 'real_multiplayer': False, 'results': rows}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
