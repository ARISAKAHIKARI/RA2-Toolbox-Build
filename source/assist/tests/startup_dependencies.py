"""Exercise production startup and optional command guards in synthetic memory."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def block(text, marker):
    start = text.index(marker)
    opening = text.index('{', start)
    at, depth = opening + 1, 1
    while depth:
        depth += (text[at] == '{') - (text[at] == '}')
        at += 1
    return text[start:at]


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
    placement = (source / 'placement_bridge.h').read_text()
    extracts = {
        'STARTUP_HELPERS': '\n'.join(block(bridge, marker) for marker in (
            'static bool startupSignature(', 'static bool revealSignature(',
            'static bool signatures(')),
        'OPTIONAL_COMMANDS': bridge[bridge.index('else if(c==ClearBeacons)'):
                                    bridge.index('else if(c==OpenTech)')],
        'STARTUP_GATE': '\n'.join(block(bridge, marker) for marker in (
            'if(!checked&&(!signatureRetryAt', 'if(!good){snap.commandStatus=3;')),
        'RENDER_REVEAL': '\n'.join(block(placement, marker) for marker in (
            'static bool directMapRevealReady(', 'static void renderRevealTick(')),
        'SUSPENDED_OPTIONS': block(bridge, 'static void suspendedOptions('),
    }
    paths = [source / name for name in (
        'bridge.cpp', 'placement_bridge.h', 'direct_modes_bridge.h',
        'startup_dependency_policy.h', 'startup_signature_compat.h', 'signatures.h',
        'native_entry_policy.h', 'native_entry_runtime.h')]
    paths += [source.parents[1] / 'shared/source/direct_patch_policy.h',
              here / 'startup_dependencies.cpp', Path(__file__).resolve()]
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): sha(path) for path in paths}
    definitions = []
    for name, text in extracts.items():
        target = out / (name.lower() + '.inc')
        target.write_text(text + '\n')
        definitions.append('-D' + name + '="' + target.as_posix() + '"')
    exe = out / 'startup-dependencies'
    command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', *definitions,
               str(here / 'startup_dependencies.cpp'), '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        raise RuntimeError(run.stderr or f'fixture exited {run.returncode}')
    assert hashes == {str(path): sha(path) for path in paths}
    (out / 'result.json').write_text(json.dumps({
        'pass': True, 'scope': __doc__, 'source_sha256': hashes,
        'executable_sha256': sha(exe), 'stdout': run.stdout,
        'game_process_tested': False, 'windows_hooks_tested': False,
        'multiplayer_tested': False,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
