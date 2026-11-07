#!/usr/bin/env python3
"""Read-only supported-PE checks with synthetic House/Type/Building data.

The existing predicate fixture supplies allocator and Building mission mocks.
AddCameo and House post-refresh UI/update calls are explicit sinks here. The
actual CanBuild, FindFactory and construction-option enumeration execute.
No user process, Windows program, private executable writes or native UI run.
"""
import argparse
import hashlib
import json
from pathlib import Path
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--exe', type=Path, required=True)
parser.add_argument('--repo', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
fixture = args.repo / 'source/tools/audit_tech_data_native.py'
fixture_text = fixture.read_text()
prefix = fixture_text.split('\nrows=[]', 1)[0]
original_argv = sys.argv
sys.argv = [str(fixture), '--exe', str(args.exe), '--output', str(args.output)]
scope = {'__file__': str(fixture), '__name__': 'native_predicate_setup'}
exec(compile(prefix, str(fixture), 'exec'), scope)
sys.argv = original_argv

Case = scope['Case']
H, T, HT, B, BT, VT, COUNTS, LIST, STOP, STACK = [scope[x] for x in ('H', 'T', 'HT', 'B', 'BT', 'VT', 'COUNTS', 'LIST', 'STOP', 'STACK')]
EIP, EAX, ECX, ESP, ESI = [scope['UC_X86_REG_' + x] for x in ('EIP', 'EAX', 'ECX', 'ESP', 'ESI')]


class NativeCase(Case):
    def __init__(self, kind, insignificant, level=1):
        self.cameos = []
        self.calls = []
        self.post_updates = []
        super().__init__()
        self.w(T, 0x7EB610 if kind == 16 else 0x7F6218)
        self.w(T + 0x634, level)
        self.w(T + 0x232, insignificant, 1)
        self.w(T + 0x6CC, 1)
        self.w(BT + 0xEB8, kind)
        self.w(B + 0x41B, 1, 1)
        self.w(B + 0x90, 1, 1)
        self.w(B + 0x6C, 100)
        self.w(VT + 0x400 + 0x4E0, 0x4456D0)
        self.w(0xA83D4C, H)
        for array in (0xA8E348, 0xA83CE0, 0xA8B218):
            self.w(array + 4, LIST + 0x800)
            self.w(array + 16, 0)
        array = 0xA8E348 if kind == 16 else 0xA83CE0
        self.w(array + 16, 1)
        self.w(LIST + 0x800, T)
        self.w(H + 0x1FC, 1, 1)

    def code(self, u, address, size, data):
        if address == 0x4F7870:
            sp = u.reg_read(ESP)
            self.calls.append({'type': self.r(sp + 4), 'build_limit_only': self.r(sp + 8), 'include_production': self.r(sp + 12)})
        if address == 0x6A6300:
            sp = u.reg_read(ESP)
            self.cameos.append([self.r(sp + 4), self.r(sp + 8)])
            self.mock.append('AddCameo sink')
            self.ret(1, 8)
            return
        if address in (0x6A7D20, 0x50AF10, 0x50B1D0):
            self.post_updates.append(hex(address))
            self.mock.append('House post-refresh update sink ' + hex(address))
            self.ret(0)
            return
        if address == 0x4F9302:
            u.emu_stop()
            return
        super().code(u, address, size, data)

    def house_refresh(self):
        self.writes = []
        self.u.reg_write(ESP, STACK)
        self.u.reg_write(ESI, H)
        self.u.emu_start(0x4F9265, STOP, count=100000)
        assert self.u.reg_read(EIP) == 0x4F9302, hex(self.u.reg_read(EIP))
        assert self.u.reg_read(ESP) == STACK


rows = []
for name, kind in (('PRES', 16), ('BUS', 40)):
    for insignificant in (0, 1):
        for level in (-1, 1):
            c = NativeCase(kind, insignificant, level)
            if name == 'BUS':
                c.vector(T + 0x638, [0])
                c.count(0x5550, 1)
            original = bytes(c.u.mem_read(T, 0x2000))
            code_hash = hashlib.sha256(c.u.mem_read(0x401000, 0x3C0000)).hexdigest()
            build = c.run(0x4F7870, [T, 0, 1])
            factory = c.run(0x5F7900, [1, 0, 0, H])
            c.run(0x4456D0, [], this=B)
            direct_cameos = list(c.cameos)
            c.cameos.clear()
            c.house_refresh()
            assert bytes(c.u.mem_read(T, 0x2000)) == original
            assert hashlib.sha256(c.u.mem_read(0x401000, 0x3C0000)).hexdigest() == code_hash
            expected = 1 if level == 1 else 0
            assert build == expected, (name, insignificant, level, 'CanBuild', build)
            assert factory == B, (name, insignificant, level, 'FindFactory', factory)
            wanted = [[kind, 0]] if expected else []
            assert direct_cameos == wanted, (name, insignificant, level, 'Building', direct_cameos)
            assert c.cameos == wanted, (name, insignificant, level, 'House', c.cameos)
            assert int.from_bytes(c.u.mem_read(H + 0x1FC, 1), 'little') == 0
            rows.append({'type': name, 'kind': kind, 'insignificant': insignificant, 'tech_level': level,
                         'full_canbuild': build, 'find_factory_present': factory == B,
                         'direct_building_addcameo_calls': direct_cameos,
                         'house_recheck_addcameo_calls': c.cameos,
                         'house_recheck_flag_cleared': True, 'type_bytes_unchanged': True,
                         'code_hash_unchanged': True, 'native_canbuild_calls': len(c.calls),
                         'explicit_mocks': sorted(set(c.mock))})

report = {'status': 'PASS', 'cases': len(rows), 'private_executable_sha256': hashlib.sha256(args.exe.read_bytes()).hexdigest(),
          'base_fixture_path': str(fixture.relative_to(args.repo)), 'base_fixture_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
          'script_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
          'game_tested': False, 'windows_executed': False, 'native_ui_executed': False,
          'boundary': __doc__,
          'synthetic_fields': 'Real type vtables and factory kinds; abstract owner group 1 for matching Type/factory, no PRES prerequisite; BUS uses synthetic prerequisite index 0 with owned count 1. Not current platform rules.',
          'actual_functions': ['0x4F7870 CanBuild', '0x5F7900 FindFactory', '0x4456D0 Building::UpdateConstructionOptions', '0x4F9265..0x4F9302 House RecheckTechTree slice'],
          'rows': rows}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'status': report['status'], 'cases': len(rows), 'rows': [{k:r[k] for k in ('type','insignificant','tech_level','full_canbuild','direct_building_addcameo_calls','house_recheck_addcameo_calls')} for r in rows]}))
