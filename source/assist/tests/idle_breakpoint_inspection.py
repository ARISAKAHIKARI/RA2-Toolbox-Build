#!/usr/bin/env python3
"""Compare actual idle bridge maintenance and complete DR Service, with Win32/INT3 boundaries controlled."""
import argparse
import hashlib
import json
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
    parser.add_argument('--baseline-ref', default='88812eda0d23d065d61d6f742315b7d0581fd1a8')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[3]
    tests = Path(__file__).resolve().parent
    header = repo / 'source/assist/source/tech_ui_breakpoint_bridge.h'
    core = header.with_name('tech_ui_breakpoint_core.h')
    boundary = tests / 'breakpoint_owner_rendezvous.cpp'
    fixture = Path(__file__).with_suffix('.cpp')
    paths = [header, core, core.with_name('tech_ui_breakpoint_hot.h'),
             repo / 'source/shared/source/runtime_seh_scope.h',
             repo / 'source/shared/source/hardware_slot_budget.h',
             boundary, fixture, Path(__file__).resolve()]
    hashes = {str(p.relative_to(repo)): digest(p) for p in paths}
    before = subprocess.check_output(['git', 'show', args.baseline_ref + ':' + str(header.relative_to(repo))], cwd=repo, text=True)
    # The before/after comparison changes only the bridge. Refuse to silently
    # label a newer DR Service or dependency as the released baseline.
    baseline_hashes = {str(header.relative_to(repo)): hashlib.sha256(before.encode()).hexdigest()}
    for path in paths[1:5]:
        baseline = subprocess.check_output(['git', 'show', args.baseline_ref + ':' + str(path.relative_to(repo))], cwd=repo)
        baseline_hashes[str(path.relative_to(repo))] = hashlib.sha256(baseline).hexdigest()
        assert baseline == path.read_bytes(), 'Service dependency differs from baseline: ' + str(path)
    core_text = core.read_text().replace('#include <windows.h>', '')
    for name in ('ownerRendezvousTrap', 'ownerRendezvousWitness'):
        core_text, count = re.subn(r'static __declspec\(naked\) void ' + name + r'\(\)\{__asm \{.*?\}\}',
                                  'static void ' + name + '();', core_text, flags=re.S)
        assert count == 1
    core_text = re.sub(r'#include "([^"]+)"', lambda m: '#include "' + str((core.parent / m[1]).resolve()) + '"', core_text)
    boundary_text = boundary.read_text().split('static uint32_t sites[SiteCount]')[0]
    # These counters are confined to the existing explicit Win32 API seam.
    boundary_text = boundary_text.replace('PVOID AddVectoredExceptionHandler(unsigned,Handler fn){',
        'static unsigned registrationCalls=0,removalCalls=0;\nPVOID AddVectoredExceptionHandler(unsigned,Handler fn){++registrationCalls;')
    boundary_text = boundary_text.replace('unsigned RemoveVectoredExceptionHandler(PVOID){',
        'unsigned RemoveVectoredExceptionHandler(PVOID){++removalCalls;')
    assert '++registrationCalls' in boundary_text and '++removalCalls' in boundary_text
    cases = ['idle_hidden', 'idle_visible', 'visibility_transition', 'requested_hidden', 'active_audit_hidden',
             'disabled_cleanup_hidden', 'stop_cleanup_hidden', 'fault_cleanup_hidden', 'foreign_cleanup_hidden',
             'pending_cleanup_hidden', 'failed_cleanup_hidden', 'fault_visible', 'stop_visible',
             'no_house_visible', 'not_running_visible', 'clock_wrap_visible']
    report = {'schema': 'ra2.idle-breakpoint-inspection.v1', 'baseline_ref': args.baseline_ref,
              'baseline_production_sha256': baseline_hashes,
              'source_sha256': hashes, 'windows_executed': False, 'game_executed': False,
              'scope': __doc__, 'before': [], 'after': [], 'boundaries': [
                  'Win32 handles and exception delivery use the existing owner-rendezvous fixture seam.',
                  'Actual private INT3/NOP instructions are replaced by controlled CONTEXT delivery; no OS exception latency is claimed.',
                  'Debug UI publication, signature-policy update and FPS admission are separate unchanged boundaries.',
                  'The virtual clock measures work counts, not elapsed Windows time or game FPS.']}
    for phase, production in [('before', before), ('after', header.read_text())]:
        work = args.out / phase
        work.mkdir(parents=True, exist_ok=True)
        (work / 'production_core.h').write_text(core_text)
        (work / 'idle-inspection-boundary.inc').write_text(boundary_text)
        globals_text = production[production.index('static int failure=0,'):production.index('static volatile LONG fullExceptions=')]
        # enhancementTags() needs this state declared before its definition.
        (work / 'idle-inspection-globals.inc').write_text(globals_text)
        functions = ['static bool healthy(', 'static bool ready(', 'static bool refreshControlEligibility(', 'static bool configure(']
        (work / 'idle-inspection-configure.inc').write_text(''.join(block(production, f) for f in functions))
        (work / 'idle-inspection-maintain.inc').write_text(block(production, 'static void maintainTechParameters('))
        exe = work / 'idle-breakpoint-inspection'
        command = ['g++', '-std=c++17', '-O2', '-UNDEBUG', '-fsanitize=undefined', '-fno-sanitize-recover=all',
                   '-I', str(work.resolve()), str(fixture), '-o', str(exe)]
        built = subprocess.run(command, capture_output=True, text=True)
        (work / 'build.log').write_text(built.stdout + built.stderr)
        if built.returncode:
            raise RuntimeError(built.stdout + built.stderr)
        report[phase + '_command'] = command
        report[phase + '_extracted_sha256'] = {p.name: digest(p) for p in sorted(work.glob('*.inc'))}
        for case in (cases[:2] if phase == 'before' else cases):
            run = subprocess.run([str(exe), case], capture_output=True, text=True, timeout=30)
            (work / (case + '.log')).write_text(run.stdout + run.stderr)
            if run.returncode:
                raise RuntimeError(f'{phase}/{case}: ' + run.stdout + run.stderr)
            report[phase].append(json.loads(run.stdout))
    old, new = report['before'][0], report['after'][0]
    for key in ['private_rendezvous', 'handler_registrations', 'handler_removals',
                'thread_handles_duplicated', 'thread_handles_closed']:
        assert old[key] == 20 and new[key] == 0, (key, old[key], new[key])
    assert old['debug_register_writes'] == new['debug_register_writes'] == 0
    for key in ['private_rendezvous', 'handler_registrations', 'handler_removals', 'debug_register_writes']:
        assert report['before'][1][key] == report['after'][1][key]
    report['source_unchanged'] = hashes == {str(p.relative_to(repo)): digest(p) for p in paths}
    report['passed'] = report['source_unchanged'] and all(row['passed'] for row in report['after'])
    report['after_cases'] = len(report['after'])
    report['after_checks'] = sum(row['checks'] for row in report['after'])
    (args.out / 'receipt.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: report[k] for k in ['passed', 'after_cases', 'after_checks', 'windows_executed', 'game_executed']}))
    print(json.dumps({'idle_hidden_before': old, 'idle_hidden_after': new}))
    assert report['passed']


if __name__ == '__main__':
    main()
