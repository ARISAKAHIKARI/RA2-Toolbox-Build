#!/usr/bin/env python3
"""Verify exact Runtime memory wrappers: native byte semantics, target codegen and paired benchmark."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess


def block(text, marker):
    start = text.index(marker)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end] + '\n'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--zig', required=True, type=Path)
    parser.add_argument('--baseline-ref', default='88812eda0d23d065d61d6f742315b7d0581fd1a8')
    args = parser.parse_args()
    out = args.out.resolve();out.mkdir(parents=True, exist_ok=True)
    repo = Path(__file__).resolve().parents[3]
    bridge = repo / 'source/assist/source/bridge.cpp'
    helper = repo / 'source/shared/source/runtime_memory.h'
    fixture = Path(__file__).with_suffix('.cpp')
    production = bridge.read_text()
    before = subprocess.check_output(['git', 'show', args.baseline_ref + ':' + str(bridge.relative_to(repo))], cwd=repo, text=True)
    wrappers = lambda text: ''.join(block(text, f'extern "C" void* __cdecl {name}(') for name in ['memset', 'memcpy'])
    current = wrappers(production);baseline = wrappers(before)
    # Other agents may change tick() concurrently. Pin the exact owned wrapper
    # blocks and helper; the complete bridge hash remains an informative stamp.
    hashes = {str(p.relative_to(repo)): digest(p) for p in [helper, fixture, Path(__file__).resolve()]}
    prefix = '#include <stddef.h>\n#include <stdint.h>\n#ifndef _WIN32\n#define __cdecl\n#endif\n'
    def renamed(text, label):
        return text.replace(' __cdecl memset(', f' __cdecl {label}_fill(').replace(' __cdecl memcpy(', f' __cdecl {label}_copy(')
    paths = {}
    for name, text in [('baseline', prefix + renamed(baseline, 'baseline')),
                       ('subject', prefix + f'#include "{helper}"\n' + renamed(current, 'subject')),
                       ('production', prefix + f'#include "{helper}"\n' + current)]:
        paths[name] = out / (name + '.cpp');paths[name].write_text(text)
    commands = []
    def run(command, label, timeout=120, environment=None):
        commands.append([str(part) for part in command])
        result = subprocess.run(command, capture_output=True, text=True, timeout=timeout,
                                env=None if environment is None else {**os.environ, **environment})
        (out / (label + '.log')).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(label + ': ' + result.stdout + result.stderr)
        return result.stdout
    common = ['-std=c++17', '-O2', '-fno-builtin', '-fno-stack-protector', '-fno-exceptions', '-fno-rtti']
    sanitizer = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    semantic = out / 'runtime-memory-semantics'
    run(['g++', *common, *sanitizer, paths['subject'], fixture, '-o', semantic], 'semantics-build')
    # LeakSanitizer needs /proc task access unavailable in the managed runner.
    # Address/UB checks and real inaccessible-page checks remain enabled.
    sanitizer_environment = {'ASAN_OPTIONS': 'detect_leaks=0'}
    semantics = json.loads(run([semantic], 'semantics-run', environment=sanitizer_environment))
    codegen = []
    for target in ['x86-windows-gnu', 'x86_64-windows-gnu']:
        obj = out / (target + '.o')
        run([args.zig.resolve(), 'c++', '-target', target, *common, '-fms-extensions', '-fasm-blocks', '-c', paths['production'], '-o', obj], target + '-build')
        assembly = run(['objdump', '-dr', '-Mintel', obj], target + '-disassembly')
        undefined = run(['nm', '-u', obj], target + '-undefined')
        assert not undefined.strip(), (target, undefined)
        assert not re.search(r'\b(?:call|callq|xmm\d+|ymm\d+|zmm\d+|mm[0-7])\b', assembly), target
        assert len(re.findall(r'\bcld\b', assembly)) == 2, target
        assert 'rep movs' in assembly and 'rep stos' in assembly, target
        codegen.append({'target': target, 'object_sha256': digest(obj), 'undefined_symbols': [],
                        'calls': 0, 'simd_registers': 0, 'bulk_rep_copy': True, 'bulk_rep_fill': True})
    benchmark = out / 'runtime-memory-benchmark'
    run(['g++', *common, '-DRUNTIME_MEMORY_BENCHMARK', paths['baseline'], paths['subject'], fixture, '-o', benchmark], 'benchmark-build')
    measurements = [json.loads(line) for line in run([benchmark], 'benchmark-run').splitlines()]
    report = {'schema': 'ra2.runtime-memory.v1', 'passed': semantics['passed'], 'baseline_ref': args.baseline_ref,
              'scope': __doc__, 'source_sha256': hashes, 'bridge_sha256_at_start': hashlib.sha256(production.encode()).hexdigest(),
              'baseline_wrapper_sha256': hashlib.sha256(baseline.encode()).hexdigest(),
              'production_wrapper_sha256': hashlib.sha256(current.encode()).hexdigest(),
              'generated_translation_units_sha256': {name: digest(path) for name, path in paths.items()},
              'commands': commands, 'sanitizer_environment': sanitizer_environment,
              'semantics': semantics, 'target_codegen': codegen, 'benchmark': measurements,
              'windows_executed': False, 'game_executed': False, 'limits': [
                  'Native x86_64 Linux executes exact wrappers with symbol-only renaming under ASan/UBSan and inaccessible guard pages.',
                  'LeakSanitizer is disabled because the managed runner does not permit its /proc task inspection.',
                  'Windows x86 and x64 target objects are compiled and disassembled; this is not Windows execution.',
                  'Benchmark uses the released scalar wrapper bodies and current wrapper bodies in separate translation units, alternating order across seven rounds.',
                  'Linux host timings may vary with concurrent workload, CPU and memory state; no Windows latency or game FPS claim.',
                  'The byte fallback on non-GNU/non-x86 compilers is not executed here. Overlap is outside the memcpy contract.']}
    report['source_unchanged'] = (hashes == {str(p.relative_to(repo)): digest(p) for p in [helper, fixture, Path(__file__).resolve()]}
                                  and current == wrappers(bridge.read_text()))
    report['passed'] = report['passed'] and report['source_unchanged']
    (out / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'passed': report['passed'], 'semantics': semantics, 'target_codegen': codegen}))
    for row in measurements:
        if row['size_bytes'] in [4096, 6340000, 5128*4096]:
            print(json.dumps({key: row[key] for key in ['size_bytes', 'operation', 'before_median_ns', 'after_median_ns']}))
    assert report['passed']


if __name__ == '__main__':
    main()
