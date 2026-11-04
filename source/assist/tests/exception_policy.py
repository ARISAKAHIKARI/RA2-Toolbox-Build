"""Focused portable policy and real Windows x86 production guard SEH regression."""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--cxx', default='g++')
p.add_argument('--zig', type=Path)
p.add_argument('--out', required=True, type=Path)
p.add_argument('--compile-only', action='store_true')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
paths = ['source/cursor/source/resource_guard.cpp', 'source/assist/source/guard.cpp', 'source/assist/source/exception_policy.h',
         'source/shared/source/runtime_fault.h', 'source/assist/source/pause_view_guard.h',
         'source/assist/source/sidebar_view_guard.h', 'source/assist/source/view_zoom_native_guard.h',
         'source/assist/source/tech_ui_breakpoint_guard.h', 'source/assist/tests/exception_policy.cpp',
         'source/assist/tests/exception_policy_seh_boundary.cpp', 'source/assist/tests/exception_policy_windows.cpp',
         'source/assist/tests/exception_policy.py']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {path: sha(root/path) for path in paths}
records = []
executed = False

def run(command):
    proc = subprocess.run([str(x) for x in command], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    records.append({'command': [str(x) for x in command], 'exit_code': proc.returncode, 'stdout': proc.stdout})
    print(proc.stdout, flush=True)
    return proc.returncode == 0

if a.zig:
    zig = a.zig.resolve()
    flags = [zig, 'c++', '-target', 'x86-windows-msvc', '-nostdlib', '-nostdinc', '-std=c++17', '-O2', '-fms-extensions', '-fno-stack-protector', '-c']
    guard = out/'production-guard.o'
    outer = out/'outer-handler.o'
    cursor = out/'cursor-resource-guard.o'
    exe = out/'exception-policy-windows.exe'
    passed = run([*flags, root/'source/assist/source/guard.cpp', '-o', guard])
    if passed:
        passed = run([*flags, here/'exception_policy_seh_boundary.cpp', '-o', outer])
    if passed:
        passed = run([*flags, root/'source/cursor/source/resource_guard.cpp', '-o', cursor])
    if passed:
        passed = run([zig, 'c++', '-target', 'x86-windows-gnu', '-std=c++17', '-O2', '-UNDEBUG',
                      here/'exception_policy_windows.cpp', guard, outer, cursor, '-o', exe])
    if passed and not a.compile_only:
        executed = True
        passed = run([exe])
    scope = 'Real Windows x86 SEH from whole production guard.cpp and cursor resource_guard.cpp; own process and no game; callback models raise debug/control exceptions or cause a real AV.'
else:
    exe = out/('exception-policy.exe' if os.name == 'nt' else 'exception-policy')
    passed = run([a.cxx, '-std=c++17', '-O2', '-UNDEBUG', here/'exception_policy.cpp', '-o', exe])
    if passed and not a.compile_only:
        executed = True
        passed = run([exe])
    scope = 'Portable production exception decision policy; no Windows SEH or game execution.'
stable = before == {path: sha(root/path) for path in paths}
result = {'pass': passed and stable, 'compile_only': a.compile_only, 'scope': scope, 'source_sha256': before, 'source_hash_stable': stable,
          'windows_test_process_executed': executed, 'game_process_executed': False,
          'commands': records}
(out/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
raise SystemExit(not result['pass'])
