"""Run actual ordinary-entry admission/consumers against pinned loader/PE fixtures.

The negative control compiles pre.8 production code and fails the same three
ordinary-call consumer expectations. Native execution boundaries are substitutes;
neither Phobos nor game code is executed by this test.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

from startup_dependencies import block


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--baseline-ref', default='2051e5c7da24c0df27e9f7bc59978d8994525bdb')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    source = here.parent / 'source'
    repo = here.parents[2]
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    inputs = [source / name for name in (
        'bridge.cpp', 'sidebar.h', 'native_entry_policy.h', 'native_entry_runtime.h',
        'native_platform_profile.h', 'signatures.h', 'startup_dependency_policy.h',
        'startup_signature_compat.h')]
    inputs += [here / name for name in (
        'native_platform_profile.cpp', 'native_platform_profile.py',
        'native_entry_runtime.cpp', 'startup_dependencies.py')]
    inputs += [repo / 'source/shared/source' / name for name in (
        'debug_entry_policy.h', 'debug_entry_types.h', 'debug_entry_catalog.h')]
    hashes = {str(p): sha(p) for p in inputs}
    baseline = subprocess.check_output(
        ['git', 'rev-parse', '--verify', args.baseline_ref + '^{commit}'],
        cwd=repo, text=True).strip()

    def old(path):
        return subprocess.check_output(['git', 'show', baseline + ':source/assist/source/' + path], cwd=repo)

    before = out / 'before'
    before.mkdir(exist_ok=True)
    for name in ('native_entry_policy.h', 'native_entry_runtime.h'):
        (before / name).write_bytes(old(name))
    baseline_inputs = {name: hashlib.sha256(old(name)).hexdigest() for name in (
        'bridge.cpp', 'sidebar.h', 'native_entry_policy.h', 'native_entry_runtime.h')}

    def compile_fixture(folder, bridge, sidebar, prior=False):
        folder.mkdir(exist_ok=True)
        helpers = block(bridge, 'static bool startupSignatureStrict(') + '\n'
        if 'static bool nativeVerifiedCanBuildDependency(' in bridge:
            helpers += block(bridge, 'static bool nativeVerifiedCanBuildDependency(') + '\n'
        helpers += block(bridge, 'static bool startupSignature(') + '\n'
        helpers += bridge[bridge.index('static nativeentry::TimedResult nativeDependencyCache'):
                          bridge.index('static void notifyHost(')]
        gates = '\n'.join(block(bridge, marker) for marker in (
            'if(!checked&&(!signatureRetryAt', 'if(!good){snap.commandStatus=3;'))
        unselect = block(bridge, 'static void unselect(')
        call_boundary = '((void(__cdecl*)())0x48DC90)()'
        assert unselect.count(call_boundary) == 1
        unselect = unselect.replace(call_boundary, 'fakeNativeUnselect()')
        consumers = '\n'.join((block(bridge, 'static bool selection('), unselect,
                              block(sidebar, 'static bool reservationCanBuild(')))
        definitions = []
        for name, text in {'STARTUP_HELPERS': helpers, 'STARTUP_GATE': gates,
                           'PROFILE_CONSUMERS': consumers}.items():
            path = folder / (name.lower() + '.inc')
            path.write_text(text + '\n')
            definitions.append('-D' + name + '="' + path.as_posix() + '"')
        if prior:
            definitions += ['-DPROFILE_BASELINE=1',
                            '-DNATIVE_ENTRY_RUNTIME_HEADER="' + (before / 'native_entry_runtime.h').as_posix() + '"']
        exe = folder / 'native-platform-profile'
        command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', *definitions,
                   str(here / 'native_platform_profile.cpp'), '-o', str(exe)]
        result = subprocess.run(command, capture_output=True, text=True)
        (folder / 'build.log').write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        return exe

    previous = compile_fixture(before, old('bridge.cpp').decode(), old('sidebar.h').decode(), True)
    negatives = []
    for i, name in enumerate(('CanBuild +7', 'UnselectAll entry', 'Select +3')):
        result = subprocess.run([str(previous), str(i)], capture_output=True, text=True, timeout=30)
        (before / ('negative-' + str(i) + '.log')).write_text(result.stdout + result.stderr)
        assert result.returncode != 0 and 'native entry check' in result.stderr, (i, result)
        negatives.append({'case': name, 'expected_failure_observed': True,
                          'returncode': result.returncode, 'successful_checks_counted': 0})
    after = out / 'after'
    exe = compile_fixture(after, (source / 'bridge.cpp').read_text(), (source / 'sidebar.h').read_text())
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=120)
    (after / 'run.log').write_text(result.stdout + result.stderr)
    print(result.stdout, end='')
    if result.returncode:
        raise RuntimeError(result.stderr or f'fixture exited {result.returncode}')
    match = re.search(r'PASS: (\d+) checks, (\d+) scenarios', result.stdout)
    assert match
    assert hashes == {str(p): sha(p) for p in inputs}, 'source changed while validating'
    receipt = {
        'pass': True, 'schema': 'ra2.native-platform-profile.validation.v1',
        'checks': int(match[1]), 'scenarios': int(match[2]),
        'source_sha256': hashes, 'baseline_commit': baseline,
        'baseline_source_sha256': baseline_inputs, 'old_negative_controls': negatives,
        'executable_sha256': sha(exe), 'baseline_executable_sha256': sha(previous),
        'stdout': result.stdout,
        'upstream_contract': {'phobos': 'ce864b03e56b15f4c59f78955aee87079e54ce76',
                              'syringeex': '7dbb780dfa7441f8d9b2ae89300540f1cf5f854e'},
        'coverage': ['three actual production admission/consumer paths',
                     'actual module allocation/PE32/declaration/export ownership',
                     'full 73-byte wrapper ABI and replay/continuation ranges',
                     'all fixed wrapper byte corruptions', 'unknown additional callback rejection',
                     'CanBuild companion ownership and self-retirement lifecycle',
                     'Ares/Phobos coexistence and raw DR separation',
                     'profile error to original/skip recovery',
                     'malformed metadata, page/bounds, changed-during-inspection'],
        'native_boundaries': ['Win32 memory and module APIs are synthetic',
                              'call<T> records original entry/receiver/arguments',
                              'one direct unselect function-pointer invocation replaced by explicit native boundary substitute'],
        'real_phobos_dll_hash_verified': False, 'entire_callback_body_verified': False,
        'game_process_tested': False, 'windows_hooks_tested': False,
        'multiplayer_tested': False,
    }
    (out / 'result.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n')
    print('Negative controls: pre.8 actual consumers fail all three expected admission cases; not counted as passes.')


if __name__ == '__main__':
    main()
