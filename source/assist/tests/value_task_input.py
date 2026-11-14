#!/usr/bin/env python3
"""Compare actual Host/Runtime input transfers and actual worker ownership."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from runtime_broker_retirement import function


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--baseline', default='HEAD')
    p.add_argument('--cxx', default='g++')
    p.add_argument('--current-only', action='store_true', help='verify current transfer/worker paths without historical allocation comparisons')
    a = p.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    source = repo / 'source/assist/source'
    out = a.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = subprocess.check_output(['git', 'rev-parse', a.baseline], cwd=repo, text=True).strip()
    old = lambda name: subprocess.check_output(['git', 'show', baseline + ':source/assist/source/' + name], cwd=repo).decode()
    baseline_task = out / 'baseline-value-task.h'
    baseline_task.write_text(old('value_task.h').replace('namespace compute {', 'namespace baselinecompute {', 1).replace('"../../shared/source/latest_worker.h"', '"' + str(repo / 'source/shared/source/latest_worker.h') + '"'))
    baseline_frame = out / 'baseline-compute-frame.h'
    baseline_frame.write_text(old('compute_frame.h').replace('namespace compute {', 'namespace baselinehistory {', 1).replace('copyInput(p->value,input);', 'assist::compute::copyInput(p->value,input);'))
    extracted = {}
    main_source = (source / 'main.cpp').read_text()
    for label, text in [('current', main_source), ('baseline', old('main.cpp'))]:
        route = function(text, 'template<class Input,class Output>static void serviceValueTask(')
        if label == 'baseline':
            route = route.replace('serviceValueTask(', 'serviceValueTaskBaseline(', 1).replace('compute::ValueTask<', 'baselinecompute::ValueTask<', 1)
        path = out / (label + '-service.h')
        path.write_text(route + '\n')
        extracted[label + '_service'] = path
    for label, text in [('current', (source / 'unload_compute_bridge.h').read_text()), ('baseline', old('unload_compute_bridge.h'))]:
        route = function(text, 'static void publishUnloadCalculation(){')
        if label == 'baseline':
            route = route.replace('publishUnloadCalculation()', 'publishUnloadCalculationBaseline()', 1).replace('unloadFrames', 'baselineUnloadFrames')
        path = out / (label + '-unload-publish.h')
        path.write_text(route + '\n')
        extracted[label + '_unload_publish'] = path
    inputs = [source / name for name in ('main.cpp', 'value_task.h', 'compute_input_copy.h', 'compute_frame.h',
              'compute_tasks.h', 'autoload.h', 'autoload_bridge.h', 'missile_compute.h', 'missile_bridge.h',
              'unload_compute.h', 'unload_compute_bridge.h', 'protocol.h', 'plan_freshness.h')]
    inputs += [repo / 'source/shared/source/latest_worker.h', here / 'value_task_input.cpp', Path(__file__), here / 'runtime_broker_retirement.py']
    bindings = {str(path.relative_to(repo)): digest(path) for path in inputs}
    # The other Runtime capture/publication routes must use the same tested
    # transfer helper; existing tactical-frame fixtures execute their collectors.
    missile = (source / 'missile_bridge.h').read_text()
    assert 'compute::copyInput(missileInput,i);' in missile
    assert 'compute::copyInput(sh->missileInput,i);' in missile
    assert 'missilecompute::reset(i);' in missile
    load = (source / 'autoload_bridge.h').read_text()
    assert 'compute::copyInput(loadCaptureCohort,sh->loadInput);' in load
    assert 'compute::copyInput(sh->loadInput,in);' in load
    command = [a.cxx, '-std=c++17', '-O2', '-I' + str(source), '-pthread', '-UNDEBUG', '-U_FORTIFY_SOURCE', '-D_FORTIFY_SOURCE=0', '-fno-builtin-memcpy',
               '-Wl,--wrap=memcpy', '-fsanitize=undefined', '-fno-sanitize-recover=all',
               '-DBASELINE_VALUE_TASK="' + str(baseline_task) + '"',
               '-DBASELINE_FRAME_HISTORY="' + str(baseline_frame) + '"']
    if a.current_only:
        command.append('-DVALUE_TASK_CURRENT_ONLY=1')
    command += ['-D' + name.upper() + '="' + str(path) + '"' for name, path in extracted.items()]
    exe = out / 'value-task-input'
    command += [str(here / 'value_task_input.cpp'), '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=60)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    print(run.stdout + run.stderr, end='')
    if run.returncode:
        raise SystemExit(run.returncode)
    records = [{'kind': line.split(' ', 1)[0], **json.loads(line.split(' ', 1)[1])} for line in run.stdout.splitlines()]
    if any(digest(path) != bindings[str(path.relative_to(repo))] for path in inputs):
        raise SystemExit('Input-transfer sources changed during checking')
    report = {'schema': 'ra2.value-task-input.v1', 'passed': True, 'baseline_commit': baseline,
              'records': records, 'current_only': a.current_only, 'build_command': command, 'run_exit_code': run.returncode,
              'source_bindings': [{'path': path, 'sha256': value} for path, value in bindings.items()],
              'extracted_routes': {name: digest(path) for name, path in extracted.items()},
              'baseline_value_task_sha256': digest(baseline_task), 'baseline_history_sha256': digest(baseline_frame),
              'windows_executed': False, 'game_tested': False, 'multiplayer_tested': False,
              'scope': [
                  ('Current serviceValueTask and publishUnloadCalculation execute with controlled Win32 seams; historical allocation comparisons are omitted.' if a.current_only else 'Actual current/baseline serviceValueTask and publishUnloadCalculation execute with controlled Win32 timing/barrier/notification boundaries.'),
                  'Actual current/baseline ValueTask and Latest threads run; one calculation is held to measure the established newest-pending queue without changing its execution semantics.',
                  'Actual InputCopy, FrameHistory, unload/missile/load calculators compare counted payloads and results with complete source copies; load mutation and future composition type rows are retained.',
                  'Pinned leases, bounded idle reuse with allocation fallback, allocation failure, generation invalidation and return after ValueTask destruction execute.',
              ],
              'limits': [
                  'Allocation counters cover the Host caller thread only. The unchanged worker Result allocation remains; 500 small caller-side Request allocations remain per 500 submissions.',
                  'Timing is a Linux UBSan fixture measurement under shared load, not Windows CPU or game FPS evidence.',
                  'Unused tails are not copied for valid counts; calculations and provenance use counted ranges. Invalid counts retain the bounded full-copy fallback.',
                  'This fixture executes unload Runtime publication directly; missile/load Runtime routing is source-bound here and their collectors are covered by the existing tactical-frame fixture.',
              ]}
    (out / 'receipt.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')


if __name__ == '__main__':
    main()
