"""Windows x86 production entry leases/FP cleanup under a foreign outer SEH handler."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--zig', required=True, type=Path)
p.add_argument('--out', required=True, type=Path)
p.add_argument('--compile-only', action='store_true')
p.add_argument('--entry-ref', help='compare an archived entry client/manager/Policy with the same current SEH fixture')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
here = Path(__file__).resolve().parent
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
paths = ['source/cursor/source/resource_guard.cpp', 'source/assist/source/guard.cpp', 'source/assist/source/exception_policy.h',
         'source/shared/source/runtime_seh_scope.h', 'source/shared/source/game_entry_client.h',
         'source/shared/source/game_entry_manager.cpp', 'source/shared/source/entry_policy.h',
         'source/shared/source/game_entry_validation.h',
         'source/shared/source/runtime_fp_scope.h', 'source/assist/tests/exception_policy_windows.cpp',
         'source/assist/tests/exception_policy_seh_boundary.cpp', 'source/assist/tests/runtime_scope_windows.cpp',
         'source/assist/tests/runtime_scope_seh_outer.cpp', 'source/assist/tests/runtime_scope_windows.py']
sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
before = {path: sha(root/path) for path in paths}
records = []
entry_override = None
manager_source = root/'source/shared/source/game_entry_manager.cpp'
fixture_source = here/'runtime_scope_windows.cpp'
if a.entry_ref:
    overlay = out/'entry-comparison'
    overlay.mkdir(exist_ok=True)
    shared = root/'source/shared/source'
    names = ('game_entry_client.h', 'game_entry_manager.cpp', 'entry_policy.h')
    archived = {name: subprocess.check_output(['git','show',a.entry_ref+':source/shared/source/'+name],cwd=root).decode() for name in names}
    def includes(text, original):
        def target(match):
            name = match.group(1)
            path = overlay/name if original == shared and name in names else (original/name).resolve()
            return '#include "'+path.as_posix()+'"'
        return re.sub(r'^#include "([^"\n]+)"',target,text,flags=re.M)
    for name,text in archived.items():
        (overlay/name).write_text(includes(text,shared),encoding='utf-8')
    fixture_source = out/'scope-entry-comparison.cpp'
    fixture = (here/'runtime_scope_windows.cpp').read_text()
    fixture = fixture.replace('"../../shared/source/game_entry_client.h"','"'+(overlay/'game_entry_client.h').as_posix()+'"')
    fixture_source.write_text(includes(fixture,here),encoding='utf-8')
    manager_source = overlay/'game_entry_manager.cpp'
    entry_override = {'commit':subprocess.check_output(['git','rev-parse',a.entry_ref],cwd=root,text=True).strip(),
                      'archived_sha256':{name:hashlib.sha256(text.encode()).hexdigest() for name,text in archived.items()},
                      'adaptation':'Only quoted include locations rewritten; current guard and SEH fixture unchanged.'}

def run(command):
    proc = subprocess.run([str(x) for x in command], text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    records.append({'command': [str(x) for x in command], 'exit_code': proc.returncode, 'stdout': proc.stdout})
    if proc.stdout:
        print(proc.stdout, flush=True)
    return proc.returncode == 0

zig = a.zig.resolve()
msvc = [zig, 'c++', '-target', 'x86-windows-msvc', '-nostdlib', '-nostdinc', '-std=c++17', '-O2',
        '-fms-extensions', '-fno-stack-protector', '-c']
gnu = [zig, 'c++', '-target', 'x86-windows-gnu', '-std=c++17', '-O2', '-UNDEBUG',
       '-fms-extensions', '-fasm-blocks', '-fno-exceptions', '-fno-rtti', '-c']
steps = [
    [*msvc, root/'source/assist/source/guard.cpp', '-o', out/'guard.o'],
    [*msvc, root/'source/cursor/source/resource_guard.cpp', '-o', out/'cursor-resource-guard.o'],
    [*msvc, here/'exception_policy_seh_boundary.cpp', '-o', out/'policy-outer.o'],
    [*msvc, here/'runtime_scope_seh_outer.cpp', '-o', out/'scope-outer.o'],
    [*gnu, '-Dmain=PolicyFixtureUnusedMain', here/'exception_policy_windows.cpp', '-o', out/'guard-callback-stubs.o'],
    [*gnu, manager_source, '-o', out/'manager.o'],
    [*gnu, fixture_source, '-o', out/'scope.o'],
    [zig, 'c++', '-target', 'x86-windows-gnu', out/'guard.o', out/'cursor-resource-guard.o', out/'policy-outer.o', out/'scope-outer.o',
     out/'guard-callback-stubs.o', out/'manager.o', out/'scope.o', '-o', out/'runtime-scope-windows.exe'],
]
passed = True
for command in steps:
    if not run(command):
        passed = False
        break
executed = False
if passed and not a.compile_only:
    executed = True
    passed = run([out/'runtime-scope-windows.exe'])
stable = before == {path: sha(root/path) for path in paths}
result = {'pass': passed and stable, 'compile_only': a.compile_only,
          'entry_override': entry_override,
          'scope': __doc__, 'source_sha256': before, 'source_hash_stable': stable,
          'windows_test_process_executed': executed, 'game_process_executed': False, 'commands': records}
(out/'result.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
raise SystemExit(not result['pass'])
