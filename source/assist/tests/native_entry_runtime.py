"""Run production native-call admission and startup gate with Runtime(10) E9 entries."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from startup_dependencies import block


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    source = here.parent / 'source'
    bridge = (source / 'bridge.cpp').read_text()
    extracts = {
        'STARTUP_HELPERS': block(bridge, 'static bool startupSignature(') + '\n' +
            bridge[bridge.index('static nativeentry::TimedResult nativeDependencyCache'):
                   bridge.index('static void notifyHost(')],
        'STARTUP_GATE': '\n'.join(block(bridge, marker) for marker in (
            'if(!checked&&(!signatureRetryAt', 'if(!good){snap.commandStatus=3;')),
    }
    definitions = []
    for name, content in extracts.items():
        target = out / (name.lower() + '.inc')
        target.write_text(content + '\n')
        definitions.append('-D' + name + '="' + target.as_posix() + '"')
    inputs = [source / name for name in ('bridge.cpp', 'native_entry_policy.h',
              'native_entry_runtime.h', 'startup_dependency_policy.h',
              'startup_signature_compat.h', 'signatures.h')]
    inputs += [here / 'native_entry_runtime.cpp', Path(__file__).resolve(),
               here / 'startup_dependencies.py']
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): sha(path) for path in inputs}
    exe = out / 'native-entry-runtime'
    command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', *definitions,
               str(here / 'native_entry_runtime.cpp'), '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        raise RuntimeError(run.stderr or f'fixture exited {run.returncode}')
    assert hashes == {str(path): sha(path) for path in inputs}
    (out / 'result.json').write_text(json.dumps({
        'pass': True, 'source_sha256': hashes, 'executable_sha256': sha(exe),
        'stdout': run.stdout, 'runtime10_entry_bytes': True,
        'target_stub_bodies': 'synthetic; not supplied by Runtime(10)',
        'game_process_tested': False, 'windows_hooks_tested': False,
        'multiplayer_tested': False,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
