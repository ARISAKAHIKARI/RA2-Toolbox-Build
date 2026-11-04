"""Check all Input defaults and compare old/new Windows x86 reset stack usage."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def run(command, log):
    result = subprocess.run(command, capture_output=True, text=True, timeout=120)
    log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result.stdout


def assembly_function(text, name):
    match = re.search(r'^_' + re.escape(name) + r':[^\n]*\n', text, re.M)
    if not match:
        raise AssertionError(f'missing code-generation wrapper {name}')
    end = text.find('# -- End function', match.end())
    if end < 0:
        raise AssertionError(f'missing function boundary for {name}')
    return text[match.start():end]


def stack_details(text):
    amounts = [int(n) for n in re.findall(r'\.cv_fpo_stackalloc\s+(\d+)', text)]
    amounts += [int(n, 0) for n in re.findall(r'subl?\s+\$(0x[0-9a-fA-F]+|\d+),\s*%esp', text)]
    return {
        'largest_explicit_stack_allocation': max(amounts, default=0),
        'stack_probe': bool(re.search(r'calll?\s+_{1,3}(?:alloca|chkstk\w*)\b', text)),
        'memcpy': bool(re.search(r'calll?\s+_memcpy\b', text)),
        'calls': re.findall(r'calll?\s+(\S+)', text),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--zig', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    source = here / 'missile_input_reset.cpp'
    header = here.parent / 'source/missile_compute.h'
    paths = [source, header, Path(__file__).resolve()]
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): sha(path) for path in paths}
    exe = out / 'missile-input-reset'
    flags = ['-std=c++17', '-O2', '-UNDEBUG']
    run([args.cxx, *flags, str(source), '-o', str(exe)], out / 'build.log')
    stdout = run([str(exe)], out / 'run.log')
    print(stdout, end='')
    semantics = json.loads(stdout)
    assert semantics['pass'] and semantics['passes'] == 6
    assert semantics['heap_inputs'] == 2 and not semantics['padding_compared']
    assert semantics['input_bytes'] == 1066008
    report = {'pass': True, 'scope': __doc__, 'source_sha256': hashes,
              'semantic_flags': flags, 'semantic_result': semantics,
              'executable_sha256': sha(exe), 'windows_executed': False,
              'game_process_tested': False, 'multiplayer_tested': False}
    if args.zig:
        assembly = out / 'missile-input-reset-x86-windows.s'
        target_flags = ['c++', '-target', 'x86-windows-gnu', '-std=c++17', '-O2',
                        '-fno-exceptions', '-fno-rtti', '-S',
                        '-DMISSILE_RESET_CODEGEN=1']
        run([str(args.zig.resolve()), *target_flags, str(source), '-o', str(assembly)],
            out / 'windows-codegen.log')
        text = assembly.read_text()
        old = stack_details(assembly_function(text, 'missileResetOldAggregate'))
        new = stack_details(assembly_function(text, 'missileResetProduction'))
        assert old['largest_explicit_stack_allocation'] >= semantics['input_bytes'], old
        assert old['stack_probe'] and old['memcpy'], old
        assert new['largest_explicit_stack_allocation'] <= 256, new
        assert not new['stack_probe'] and not new['memcpy'], new
        # Do not hide a large frame in an out-of-line constructor/reset call.
        assert all(callee == '_memset' for callee in new['calls']), new
        codegen = {'pass': True, 'flags': target_flags, 'old_aggregate': old,
                   'production_helper': new, 'assembly_sha256': sha(assembly)}
        report['windows_x86_codegen'] = codegen
        print(json.dumps({'windows_x86_codegen': codegen}, separators=(',', ':')))
    assert hashes == {str(path): sha(path) for path in paths}
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
