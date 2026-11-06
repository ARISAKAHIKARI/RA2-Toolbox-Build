"""Run extracted production native consumers against synthetic calls and memory."""
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
    sidebar = (source / 'sidebar.h').read_text()
    extracts = {
        'STARTUP_HELPERS': block(bridge, 'static bool startupSignatureStrict(') + '\n' +
            block(bridge, 'static bool startupSignature(') + '\n' +
            bridge[bridge.index('static nativeentry::TimedResult nativeDependencyCache'):
                   bridge.index('static void notifyHost(')],
        'STARTUP_GATE': '\n'.join(block(bridge, marker) for marker in (
            'if(!checked&&(!signatureRetryAt', 'if(!good){snap.commandStatus=3;')),
        'CONSUMER_SUBMIT': block(bridge, 'static bool submitConstructed('),
        'CONSUMER_SELECTION': '\n'.join(block(bridge, marker) for marker in (
            'static int selectFilter(', 'static int findIdle(')),
        'CONSUMER_SUPER': block(bridge, 'static int superSelect('),
        'CONSUMER_DISPLAY': '\n'.join(block(bridge, marker) for marker in (
            'static void scanCrates(', 'static void scan(', 'static bool scanSummary(')),
        'CONSUMER_SIDEBAR': '\n'.join(block(sidebar, marker) for marker in (
            'static bool reservationCanBuild(', 'static int typeTab(',
            'static uint32_t techFactory(', 'static bool ensureTechCameo(',
            'static bool techNativeDependencies(')),
    }
    definitions = []
    for name, content in extracts.items():
        target = out / (name.lower() + '.inc')
        target.write_text(content + '\n')
        definitions.append('-D' + name + '="' + target.as_posix() + '"')
    inputs = [source / name for name in (
        'bridge.cpp', 'sidebar.h', 'superweapon_sidebar_bridge.h', 'sidebar_policy.h',
        'native_entry_policy.h', 'native_entry_runtime.h', 'startup_dependency_policy.h',
        'startup_signature_compat.h', 'signatures.h', 'constructor_adapter.h',
        'command_intent.h', 'vanilla_constructor_catalog.h', 'entry_gate.h',
        'command_gateway.h', 'unit_task_arbiter.h', 'order_policy.h',
        'technology_mode_policy.h')]
    inputs += [here / 'native_dependency_consumers.cpp', here / 'native_entry_runtime.cpp',
               here / 'startup_dependencies.py', Path(__file__).resolve()]
    inputs += [source.parents[1] / 'shared/source' / name for name in (
        'debug_entry_types.h', 'debug_entry_catalog.h', 'debug_entry_policy.h')]
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): sha(path) for path in inputs}
    exe = out / 'native-dependency-consumers'
    command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', *definitions,
               str(here / 'native_dependency_consumers.cpp'), '-o', str(exe)]
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
        'pass': True, 'scope': __doc__, 'source_sha256': hashes,
        'executable_sha256': sha(exe), 'stdout': run.stdout,
        'game_process_tested': False, 'windows_hooks_tested': False,
        'multiplayer_tested': False,
    }, indent=2) + '\n')


if __name__ == '__main__':
    main()
