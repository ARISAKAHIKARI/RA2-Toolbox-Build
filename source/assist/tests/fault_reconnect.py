"""Compare exact old/current bridge lifecycle routes with bounded transport.

Resource completion, Win32 calls and portable exception transport are substitutes;
no game, real hooks, Windows SEH, hardware debug registers or multiplayer behavior
is exercised. Current input branches and the production scoped helper are also
extracted; the frozen baseline contains the pre-fix bridge lifecycle routes.
The current publication/depth helpers are checked alongside both route sets.
"""
import argparse
import hashlib
import json
import os
import subprocess
from pathlib import Path


def block(text, marker):
    start = text.index(marker)
    at = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[at] == '{') - (text[at] == '}')
        at += 1
    return text[start:at]


def routes(text):
    # Stop at ordinary feature admission. Everything preceding this counter,
    # including mapping, fault/stop gates and cleanup, is production source.
    tick = text[text.index('static void tick(){'):
                text.index(' if((snap.techOpen||techPending||techDataPending())')]
    tick += '\n ++ordinaryCalls;\n}'
    markers = (
        'static void restoreFrameDriver(){}',
        'static void maintainScheduler(){',
        'static void dispatchFromTrigger(dispatchrouter::Trigger trigger,DWORD now,const MSG* message){',
        'extern "C" void AssistTick(){', 'extern "C" void AssistFault(){',
        'extern "C" void AssistRuntimeWatchdog(){',
        'extern "C" bool AssistRuntimeDetached(){',
    )
    return '\n\n'.join([block(text, 'static void disconnect(){'), tick] +
                        [block(text, marker) for marker in markers]) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--zig', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    source = here.parent / 'source/bridge.cpp'
    inputs = here.parent / 'source/native_message_policy.h'
    shared = here.parents[1] / 'shared/source'
    client = shared / 'game_entry_client.h'
    baseline = here / 'fixtures/fault_reconnect_baseline'
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = json.loads((baseline / 'manifest.json').read_text())
    assert sha(baseline / 'routes.inc') == manifest['routes_inc_sha256']
    tracked = [source, inputs, client, shared / 'runtime_seh_scope.h',
               here / 'fault_reconnect.cpp', Path(__file__),
               baseline / 'routes.inc', baseline / 'manifest.json']
    before = {str(path): sha(path) for path in tracked}
    current = out / 'current.inc'
    bridge = source.read_text()
    current.write_text(routes(bridge))
    helpers = out / 'unwind-helpers.inc'
    helpers.write_text('\n\n'.join(block(bridge, marker) for marker in (
        'template<class Fn>static void withNativeQueueCallDepth(',
        'template<class Fn>static void withInputPublication(')) + '\n')
    assert bridge.count('withNativeQueueCallDepth(nativeMegaQueueCallDepth,') == 1
    assert bridge.count('withNativeQueueCallDepth(nativeObjectQueueCallDepth,') == 1
    assert bridge.count('withInputPublication(sh,match,now,frame,externalInputFrame,') == 1
    scoped = out / 'scoped.inc'
    scoped.write_text(block(client.read_text(), 'template<class Fn>bool scoped(') + '\n')
    input_route = out / 'input.inc'
    input_route.write_text(block(inputs.read_text(), 'extern "C" void AssistRuntimeFilter(') + '\n')
    results = {}
    for phase, include in [('baseline', baseline / 'routes.inc'), ('current', current)]:
        exe = out / (phase + ('.exe' if os.name == 'nt' else ''))
        compiler = [str(args.zig.resolve()), 'c++'] if args.zig else [args.cxx]
        command = [*compiler, '-std=c++17', '-O2', '-UNDEBUG',
                   '-DFAULT_RECONNECT_ROUTES="' + include.resolve().as_posix() + '"',
                   '-DGAMEENTRY_SCOPED_ROUTE="' + scoped.as_posix() + '"',
                   '-DINPUT_FILTER_ROUTE="' + input_route.as_posix() + '"',
                   '-DBRIDGE_UNWIND_HELPERS="' + helpers.as_posix() + '"',
                   str(here / 'fault_reconnect.cpp'), '-o', str(exe)]
        build = subprocess.run(command, capture_output=True, text=True)
        (out / (phase + '-build.log')).write_text(build.stdout + build.stderr)
        if build.returncode:
            raise RuntimeError(build.stdout + build.stderr)
        run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
        (out / (phase + '.log')).write_text(run.stdout + run.stderr)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        results[phase] = {'executable_sha256': sha(exe),
                          'routes_sha256': sha(include), **json.loads(run.stdout)}
    old, new = results['baseline'], results['current']
    expected_old_failures = {
        'attached_fault_stays_latched_until_explicit_stop',
        'null_ipc_fault_retains_ownership_then_reconnect',
        'stop_or_dead_owner_retirement_then_fresh_host',
        'forwarded_unwind_releases_only_owned_scheduler_and_entry',
    }
    assert {name for name, passed in old['cases'].items() if not passed} == expected_old_failures, old
    assert not old['pass'] and new['pass'] and all(new['cases'].values()), results
    assert before == {str(path): sha(path) for path in tracked}, 'source changed during test'
    report = {'pass': True, 'scope': __doc__, 'source_sha256': before,
              'production_routes_extracted': True, 'baseline': manifest,
              'results': results, 'game_process_tested': False,
              'windows_hooks_tested': False, 'multiplayer_tested': False}
    (out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'pass': True, 'baseline': old['cases'], 'current': new['cases']}, indent=2))


if __name__ == '__main__':
    main()
