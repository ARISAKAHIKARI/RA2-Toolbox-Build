"""Execute actual ordinary v17 FP repair helpers and supplied Runtime callbacks.

Controlled x86/Win32 fixture; no actual Syringe scheduling, driver or multiplayer.
The ordinary rate-limited logging counter is set to its natural cap (10) because
this fixture does not initialize ordinary CRT logging. Repair code is unmodified.
"""
from pathlib import Path
import argparse
import ctypes as C
import hashlib
import importlib.util
import json

import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_MXCSR, UC_X86_REG_FPCW, UC_X86_REG_EAX

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
spec = importlib.util.spec_from_file_location('fp_ordinary_fixture', args.repo / 'source/assist/tests/entry_runtime_regression.py')
m = importlib.util.module_from_spec(spec)
m.fixture_paths = [str(args.game), str(args.runtime)]
spec.loader.exec_module(m)
ordinary = pefile.PE(str(args.spawner))
ordinary.relocate_image(ordinary_base)
m.u.mem_map(ordinary_base, (ordinary.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095)
m.u.mem_write(ordinary_base, ordinary.get_memory_mapped_image())
m.images.append(ordinary)
m.w(ordinary_base + 0x380F0, 10)
s = m.Shared.from_buffer_copy(m.u.mem_read(m.SH, C.sizeof(m.Shared)))
s.flags = 0
s.externalDisplay = 1
m.u.mem_write(m.SH, bytes(s))
m.w(0xA8B238, 3)
inside, chained = [], []
for name, target in [('GetCurrentThreadId', inside), ('CallNextHookEx', chained)]:
    entry = next(address for address, imported_name in m.imports.items() if imported_name == name)
    m.u.hook_add(UC_HOOK_CODE, lambda u, address, size, user, target=target: target.append(u.reg_read(UC_X86_REG_MXCSR)), begin=entry, end=entry)
rows = []
for name in ('SetVideoMode_RepairFPState', 'LogicClass_Update_RepairFPState'):
    rva = next(export.address for export in ordinary.DIRECTORY_ENTRY_EXPORT.symbols if export.name == name.encode())
    for incoming in (0x1F80, 0x1FA0, 0x3F80, 0x5F80, 0x7FA0, 0x9FC0):
        expected = 0x1F80 if incoming & 0xE040 else incoming
        for order in ('ordinary_then_runtime', 'runtime_then_ordinary'):
            m.u.reg_write(UC_X86_REG_MXCSR, incoming)
            m.u.reg_write(UC_X86_REG_FPCW, 0x27F)
            inside.clear()
            chained.clear()

            def repair():
                m.run(ordinary_base + rva)
                assert m.u.reg_read(UC_X86_REG_MXCSR) == expected
                assert m.u.reg_read(UC_X86_REG_EAX) == 0

            if order == 'ordinary_then_runtime':
                repair()
            callback_input = m.u.reg_read(UC_X86_REG_MXCSR)
            m.hook()
            assert inside and all(value == 0x1F80 for value in inside), inside
            assert chained and all(value == callback_input for value in chained), chained
            assert m.u.reg_read(UC_X86_REG_MXCSR) == callback_input
            assert m.u.reg_read(UC_X86_REG_FPCW) == 0x27F
            if order == 'runtime_then_ordinary':
                repair()
            assert m.u.reg_read(UC_X86_REG_MXCSR) == expected
            rows.append({'ordinary_export': name, 'ordinary_rva': hex(rva), 'incoming_mxcsr': hex(incoming), 'expected_mxcsr': hex(expected), 'order': order, 'callback_inside_mxcsr': '0x1f80', 'chained_mxcsr': hex(callback_input), 'x87_control_restored': True, 'status': 'PASS'})
report = {'scope': __doc__, 'ordinary_sha256': official_sha, 'runtime_sha256': hashlib.sha256(args.runtime.read_bytes()).hexdigest(), 'game_sha256': hashlib.sha256(args.game.read_bytes()).hexdigest(), 'real_multiplayer': False, 'results': rows}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print('PASS', len(rows), 'actual ordinary repair / Runtime sequential compositions')
