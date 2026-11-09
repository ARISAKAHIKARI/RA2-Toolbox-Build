#!/usr/bin/env python3
"""Small packaging gates, using synthetic receipts and the real pinned base.

Synthetic successful receipts below only exercise acceptance/rejection logic.
They are never production test runs or evidence for a release build.
"""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('optimization_package', HERE / 'package_release.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
ENTRY_FIXTURE_SPEC = importlib.util.spec_from_file_location('entry_policy_fixtures', HERE / 'entry_policy_fixtures.py')
ENTRY_FIXTURES = importlib.util.module_from_spec(ENTRY_FIXTURE_SPEC)
ENTRY_FIXTURE_SPEC.loader.exec_module(ENTRY_FIXTURES)
ROOT = HERE.parents[1]
BASE_FULL = BASE_MANIFEST = None
FUNCTIONAL_LOGS = {
    'console_receipt_order': ''.join('PASS synthetic case ' + str(i) + '\n' for i in range(24))
        + '{"suite":"console_receipt_order","checks":24,"failures":0,"passed":true}\n',
    'catalogue_recovery': '{"checks":93,"failures":0,"scenarios":21,"recovered_catalogues":20}\n',
    'planner_batch_delivery': (
        'batch turns=20 combat=0 duplicate_far=0 first_yields=20 next_yields=0 attempts=20\n'
        'batch turns=20 combat=32 duplicate_far=0 first_yields=16 next_yields=4 attempts=20\n'
        'batch turns=40 combat=0 duplicate_far=0 first_yields=32 next_yields=8 attempts=40\n'
        'batch turns=32 combat=32 duplicate_far=0 first_yields=16 next_yields=16 attempts=32\n'
        'batch turns=20 combat=32 duplicate_far=1 first_yields=16 next_yields=4 attempts=20\n'
        'duplicate_turn frame=500 yields=1 attempts=1\nduplicate_turn frame=501 yields=0 attempts=1\n'
        'duplicate_turn frame=529 yields=0 attempts=1\nduplicate_turn frame=530 yields=1 attempts=2\n'
        'duplicate_turn frame=559 yields=0 attempts=2\nduplicate_turn frame=560 yields=1 attempts=3\n'
        'duplicate_turn frame=589 yields=0 attempts=3\nduplicate_turn frame=590 yields=0 attempts=3\n'
        'receipt kind=0 emitted=3 attempts=3\nreceipt kind=1 emitted=3 attempts=3\n'
        'receipt kind=2 emitted=3 attempts=3\ntask first_yields=16 next_yields=4 reset_yields=16\n'
        'PASS: 136 checks, 0 failures; bounded final-batch delivery and retry lifecycle\n'),
    'scene_extra_recovery': (
        ''.join(f'synthetic-{i} failure_ready=0 fault_hits=1 recovery_ready=1 pixel_mismatches=0\n'
                for i in range(9))
        + 'layer_recoveries=9 pixel_mismatches=0\n'
        + ''.join(f'scene_synthetic-{i} variant={variant} failure_scene=0 recovery_scene=1 pixel_mismatches=0\n'
                  for variant in range(2) for i in range(6))
        + 'scene_recoveries=12 pixel_mismatches=0\nhealthy_digest=0123456789abcdef checks=109 failures=0\n'
        'PASS transactional frozen body/animation palette and depth caches, same-cache recovery, '
        'legal absent/wide/flat paths and actual SceneRenderer output\n'),
}
FEATURE_LOGS = {
    'planner_defense_delivery': (
        'omitted first_defense=0 next_defense=1\n'
        'returned drops=15 miners=20 duplicate=0 first=1 early=0 due=1\n'
        'returned drops=0 miners=0 duplicate=0 first=1 early=0 due=1\n'
        'returned drops=0 miners=20 duplicate=1 first=1 early=0 due=1\n'
        'host_task first=0 next=1\n'
        'PASS: 21 checks, 0 failures; real Planner final-batch defense lifecycle\n'),
    'missile_manual_lifecycle': (
        'dead_history count=1 new_owned=1\n'
        'PASS: 16 checks, 0 failures; extracted production manual-ownership helpers\n'),
    'missile_manual_capacity': (
        'PASS: 23 checks, 0 failures; exact production ownership/delivery/Host dispatch paths\n'),
    'patrol_capacity': (
        'width=2 count=4 bind=1\nwidth=4 count=8 bind=1\nwidth=8 count=16 bind=1\n'
        'width=10 count=16 bind=1\nwidth=20 count=16 bind=1\nwidth=40 count=16 bind=1\n'
        'width=80 count=16 bind=1\n'
        'PASS: 31 checks, 0 failures; real patrol to route, synthetic graph only\n'),
    'patrol_small_buffer': (
        'buffer count=16 digest=1280\n'
        'PASS: 1 checks, 0 failures; real patrol to route, synthetic graph only\n'),
    'selection_catalogue_identity': '23 checks, 0 failures\n',
    'toast_clock_wrap': '15 checks, 0 failures\n',
    'console_composition': (
        ''.join(f'PASS synthetic composition case {i}\n' for i in range(16))
        + '{"suite":"console_composition","checks":16,"failures":0,'
        '"level":"production listeners with controlled DOM event fixture"}\n'),
    'console_hotkey_presentation': (
        ''.join(f'PASS synthetic key-name case {i}\n' for i in range(8))
        + '{"suite":"console_hotkey_presentation","checks":8,"failures":0,'
        '"level":"actual production encodeHotkey and keyName"}\n'),
    'secondary_drag_bounds': 'PASS secondary drag: 7 boundary cases + 900 range cases\n',
    'secondary_render_retry': 'PASS secondary retry: 8 submission and recovery cases\n',
    'icon_cache_reopen': (
        'PASS failed scope -> same-path reconnect -> cache persistence/restore; '
        'healthy reconnect keeps generation\n'),
    'hotkey_input_diagnostic': (
        'PASS hotkey input diagnostics: 14 scenarios, 83 checks; '
        'pure observation, no Win32/game execution\n'),
    'input_ownership': (
        'PASS input_ownership: 61 checks; epoch, zero sentinel, 200ms leases, '
        'and normalized wrap boundary\n'),
    'superweapon_result_feedback': '{"checks":15,"failures":0,"passed":true}\n',
    'pause_key_pairing': '{"checks":45,"failures":0,"passed":true}\n',
    'native_mouse_pairing': '{"checks":69,"failures":0,"passed":true}\n',
    'passenger_frame_sampling': (
        'first passenger paint: bubbles=1 live_RPM=0\n'
        'PASS 1000 paints of one logic frame: zero live RPM, immutable contents/position\n'
        'PASS missing/failed passenger page: no live fallback, 120ms lease never renewed, next complete frame recovers\n'
        'PASS no token/unconfirmed/wrong PID/match/frame/tick/retired publication/disabled/chat: closed without RPM\n'
        'PASS inspect disappearance/danger-only/shared demand routing: no rate reduction or alternate live scanner\n'
        'PASS paused frame independent confirmation and confirmation expiry\n'
        'passenger frame sampling: 2169 checks passed\n'),
}

# Synthetic report shapes exercise packaging rejection, never a product run.
HOTKEY_REPORT_FIXTURES = {'hotkey_command_context': {'passed': True,
                            'checks': 67,
                            'scenarios': 22,
                            'negative_control': {'requested': True,
                                                 'baseline_commit': '93f02787d42b61ff5c30e450317f99ba00673149',
                                                 'reproduced': True,
                                                 'cases': {'registration': {'passed': False,
                                                                            'checks': 2,
                                                                            'scenarios': 1,
                                                                            'failure': 'display failure '
                                                                                       'must retain '
                                                                                       'command hotkey '
                                                                                       'mode',
                                                                            'returncode': 2},
                                                           'message': {'passed': False,
                                                                       'checks': 1,
                                                                       'scenarios': 1,
                                                                       'failure': 'WM_HOTKEY must dispatch '
                                                                                  'AutoBoard after display '
                                                                                  'failure',
                                                                       'returncode': 2},
                                                           'service': {'passed': False,
                                                                       'checks': 3,
                                                                       'scenarios': 1,
                                                                       'failure': 'pending hotkey must '
                                                                                  'dispatch AutoBoard '
                                                                                  'after display failure',
                                                                       'returncode': 2}},
                                                 'scope': 'baseline main.cpp/hotkey_host.h functions; '
                                                          'current Registry shared with production case',
                                                 'extracted_source_sha256': {'source/assist/source/main.cpp': '34178e444c44be457f71c2b38df73124e60448cd3d88901219ca8bce2290026a',
                                                                             'source/assist/source/hotkey_host.h': '69dc30de77236285069f3234014fad1d231b209e264b120a52e6d6647cf81a75'}},
                            'scope': 'production Host identity, registration and hotkey dispatch functions '
                                     'with simulated Win32 and command sink; not real Windows or game '
                                     'execution'},
 'hotkey_message_recovery': {'test': 'hotkey_message_recovery',
                             'passed': True,
                             'case_count': 38,
                             'check_count': 161,
                             'production_run': {'exit_code': 0,
                                                'stdout': '{"case_count":38,"check_count":161,"passed":true}',
                                                'stderr': ''},
                             'current_missing_message_probe': {'exit_code': 0,
                                                               'stdout': 'PASS ordinary_without_wm '
                                                                         'expected=1 actual=1',
                                                               'stderr': ''},
                             'baseline_negative_control': {'commit': '93f02787d42b61ff5c30e450317f99ba00673149',
                                                           'sha256': '5f44086a95f720b361d34a11b31093ddfbaf9692b379e181fe7dc0d9fc5995ce',
                                                           'expected_failure': True,
                                                           'exit_code': 1,
                                                           'stdout': 'FAIL ordinary_without_wm expected=1 '
                                                                     'actual=0',
                                                           'stderr': ''},
                             'scope': 'Production Registry with synthetic key states and message '
                                      'timestamps; not native Windows, a game process, GPU or multiplayer '
                                      'validation.'}}
HOTKEY_REPORT_FIXTURES['hotkey_command_context']['negative_control']['fixture_source_sha256'] = {
    'source/assist/tests/fixtures/hotkey_pre1473/main.cpp': 'c824b03b81a7ee2cef824db5b04498dd4ca7c7935c956960ccc10ee0280b1b1f',
    'source/assist/tests/fixtures/hotkey_pre1473/hotkey_host.h': '5650d38558132a364500360848e45afd55953d3691c4b8cd581d13f01b34ad6c',
}
for _name, _report in HOTKEY_REPORT_FIXTURES.items():
    _report['sources' if _name == 'hotkey_command_context' else 'source_hashes'] = {
        path: PACKAGE.sha(('synthetic input: ' + path).encode())
        for path in set().union(*PACKAGE.HOTKEY_BINDINGS[_name])}
    FEATURE_LOGS[_name] = json.dumps(_report, separators=(',', ':')) + '\n'


# All values here are synthetic policy fixtures, never executed product results.
LIVE_REPORT_FIXTURE = {
    'suite': 'hotkey_live_observation', 'passed': True, 'scenarios': 10, 'checks': 40,
    'scope': PACKAGE.LIVE_OBSERVATION_SCOPE,
    'sources': {path: PACKAGE.sha(('synthetic input: ' + path).encode())
                for path in set().union(*PACKAGE.INPUT_INTEGRATION_BINDINGS['hotkey_live_observation'])},
    'compile_command': ['synthetic-policy-compiler', '-UNDEBUG'],
    'compile_exit_code': 0, 'run_exit_code': 0, 'windows_executed': False, 'game_tested': False,
}
FEATURE_LOGS['hotkey_live_observation'] = (
    ''.join(f'PASS hotkey_live_observation::synthetic policy case {i}\n' for i in range(10))
    + json.dumps({key: LIVE_REPORT_FIXTURE[key] for key in ('suite', 'passed', 'scenarios', 'checks', 'scope')},
                 separators=(',', ':')) + '\n')


def formation_policy_fixture():
    """Model the report contract solely to exercise package acceptance/rejection."""
    scenarios = [
        ('low_uptime_poll', 1), ('unset_high_uptime_poll', 1),
        ('old_match_active_preview_poll', 1), ('old_match_lease_poll', 1),
        ('unset_high_uptime_wm', 1), ('current_active_preview_poll', 0),
        ('current_commit_lease_poll', 0), ('current_expired_lease_poll', 1),
        ('current_wrap_lease_poll', 0), ('current_normalized_zero_lease_poll', 0),
        ('normalized_zero_lease_expired_poll', 1), ('ancient_deadline_poll', 1),
        ('current_active_preview_wm', 0),
    ]
    report = {'passed': True, 'scope': PACKAGE.FORMATION_SCOPE,
              'source_sha256': {path: PACKAGE.sha(('synthetic input: ' + path).encode())
                               for path in set().union(*PACKAGE.INPUT_INTEGRATION_BINDINGS['formation_input_ownership'])},
              'baseline_kind': PACKAGE.FORMATION_BASELINE_KIND,
              'baseline_expression': PACKAGE.FORMATION_BASELINE_EXPRESSION,
              'case_count': 13, 'check_count': 78,
              'expected_baseline_failures': sorted(PACKAGE.FORMATION_BASELINE_FAILURES)}
    headers = {'source/assist/source/input_ownership.h', 'source/assist/source/hotkey_routes.h',
               'source/assist/source/control_link.h', 'source/assist/source/protocol.h',
               'source/shared/source/key_gesture.h'}
    for branch, dirname in (('current', 'current'), ('baseline', 'baseline-expression')):
        cases = []
        for name, expected in scenarios:
            failed = branch == 'baseline' and name in PACKAGE.FORMATION_BASELINE_FAILURES
            sent = 0 if failed else expected
            message = int(name.endswith('_wm'))
            cases.append({'scenario': name, 'expected_send': expected, 'sent': sent,
                          'owned': int(not sent), 'mode': 2, 'messages': message,
                          'sampled': int(not message and bool(sent)), 'actions': int(bool(sent)),
                          'blocked': int(message and not sent), 'passed': not failed})
        summary = {'passed': branch == 'current', 'scenarios': 13, 'checks': 78,
                   'failures': 0 if branch == 'current' else 5}
        stdout = ''.join(json.dumps(record, separators=(',', ':')) + '\n' for record in cases + [summary])
        stderr = ''.join('FAIL ' + case['scenario'] + '\n' for case in cases if not case['passed'])
        report[branch] = {
            'compile_command': ['synthetic-policy-compiler', '-UNDEBUG'], 'compile_exit': 0,
            'run_exit': 0 if branch == 'current' else 1, 'cases': cases, 'summary': summary,
            'stdout': stdout, 'stderr': stderr,
            'directory': '/synthetic-package-policy/formation_input_ownership-artifacts/' + dirname,
            'generated_cpp_sha256': PACKAGE.sha(('synthetic policy C++ ' + branch).encode()),
            'run_log_sha256': PACKAGE.sha((stdout + stderr).encode()),
            'production_header_sha256': {
                '/synthetic-package-policy/' + path: PACKAGE.sha(('synthetic input: ' + path).encode())
                for path in headers},
        }
    return report


FEATURE_LOGS['formation_input_ownership'] = json.dumps(formation_policy_fixture(), separators=(',', ':')) + '\n'


def cold_start_policy_fixture():
    """Synthetic cold-start report; no C++ or historical binary is executed here."""
    names = ['manual_only', 'default_own_ground', 'legacy_advanced_settings', 'auto_repair',
             'already_connected', 'manual_restart', 'missing_reader', 'query_only_reader',
             'disabled_host', 'delayed_connection', 'delayed_world_publication']
    report = {'suite': 'hotkey_cold_start', 'passed': True, 'scope': PACKAGE.COLD_START_SCOPE,
              'scenarios': 11, 'checks': 77, 'baseline_callsite_sha256': PACKAGE.COLD_START_BASELINE_SHA,
              'negative_control_reproduced': True, 'windows_executed': False, 'game_tested': False,
              'sources': {path: PACKAGE.sha(('synthetic input: ' + path).encode())
                          for path in set().union(*PACKAGE.INPUT_INTEGRATION_BINDINGS['hotkey_cold_start'])}}
    for branch in ('current', 'baseline'):
        cases, lines = [], []
        for name in names:
            failed = branch == 'baseline' and name in PACKAGE.COLD_START_FAILURES
            ready = name not in {'missing_reader', 'query_only_reader', 'disabled_host'} and not failed
            case = {'case': name, 'passed': not failed, 'failures': 6 if failed else 0,
                    'feature_demand': int(name in {'default_own_ground', 'legacy_advanced_settings', 'auto_repair'}),
                    'bridge_attempts': 3 if ready and name == 'delayed_connection' else int(ready),
                    'world_scope_failures': int(failed), 'sampler_calls': int(ready),
                    'mode': 2 if ready else 0, 'autoboard_registered': int(ready)}
            cases.append(case)
            if failed:
                lines += [f'CHECK_FAILED synthetic policy guard {i}' for i in range(6)]
            lines.append(json.dumps(case, separators=(',', ':')))
        summary = {'suite': 'hotkey_cold_start', 'passed': branch == 'current',
                   'scenarios': 11, 'checks': 77, 'failures': 0 if branch == 'current' else 18}
        lines.append(json.dumps(summary, separators=(',', ':')))
        stdout = '\n'.join(lines) + '\n'
        report[branch] = {'summary': summary, 'cases': cases,
                          'compile_command': ['synthetic-policy-compiler', '-UNDEBUG', '-DRA2_UNIFIED=1'],
                          'compile_exit_code': 0, 'exit_code': 0 if branch == 'current' else 1,
                          'stdout': stdout, 'stderr': '', 'run_log_sha256': PACKAGE.sha(stdout.encode()),
                          'callsite_sha256': PACKAGE.COLD_START_BASELINE_SHA if branch == 'baseline'
                              else PACKAGE.sha(b'synthetic policy current callsite')}
    return report


FEATURE_LOGS['hotkey_cold_start'] = json.dumps(cold_start_policy_fixture(), separators=(',', ':')) + '\n'


class PackageGates(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ra2-147-package-gates-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.commit, self.tree = 'a' * 40, 'b' * 40
        paths = set().union(*(tests | production for tests, production in PACKAGE.CHECK_BINDINGS.values()))
        self.sources = {name: ('synthetic input: ' + name).encode() for name in paths}
        for name in paths:
            if any(prefix in name for prefix in ('fixtures/entry_pre1475/', 'fixtures/read_only_pre1475/', 'fixtures/read_only_pre_cooperative/', 'fixtures/read_only_pre_pairing/')):
                self.sources[name] = (ROOT / name).read_bytes()
        self.inputs = {name: {'sha256': PACKAGE.sha(data)} for name, data in self.sources.items()}
        logs = {
            'missile_allocator': '{"equivalent_scenarios":8063,"same_workspace_reused":true}\n',
            'snapshot_reuse': '{"case":"seqlock_interleavings","passed":true,"checks":10}\n',
            'tech_capture': '{"case":"summary","passed":true,"checks":10}\n',
            'voxel_recovery': 'recovered_scenes=8 pixel_mismatches=0\nchecks=160 failures=0\n',
            'idle_aircraft_display': 'PASS paused same-source aircraft display; synthetic gate fixture\n',
            'terrain_reuse': 'PASS actual terrain storage: synthetic gate fixture\n',
        } | FUNCTIONAL_LOGS | FEATURE_LOGS
        entry_reports = {name: ENTRY_FIXTURES.make(PACKAGE, name, self.sources) for name in PACKAGE.ENTRY_BINDINGS}
        logs.update({name: value[0] for name, value in entry_reports.items()})
        checks = []
        for name, (tests, production) in PACKAGE.CHECK_BINDINGS.items():
            path = self.directory / name / 'result.json' if name in PACKAGE.ENTRY_BINDINGS else self.directory / (name + '.log')
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(logs[name], encoding='utf-8')
            checks.append({'name': name, 'command': ['synthetic-gate-fixture', name],
                           'exit_code': 0, 'executed_this_revision': True,
                           'log_path': str(path.relative_to(self.directory)), 'log_sha256': PACKAGE.sha(path.read_bytes()),
                           'test_source_hashes': {p: self.inputs[p]['sha256'] for p in tests},
                           'production_source_hashes': {p: self.inputs[p]['sha256'] for p in production}})
            if name in PACKAGE.ENTRY_BINDINGS:
                supporting = {}
                for relative, raw in entry_reports[name][1].items():
                    raw_path = path.parent / relative
                    raw_path.parent.mkdir(parents=True, exist_ok=True)
                    raw_path.write_bytes(raw)
                    supporting[str(raw_path)] = {'bytes':len(raw), 'sha256':PACKAGE.sha(raw)}
                if supporting:
                    checks[-1]['supporting_logs'] = supporting
            if name == 'patrol_small_buffer':
                checks[-1].update(command=['synthetic-gate-fixture', '--small-buffer'],
                                  compile_command=['synthetic-compiler', '-fsanitize=address',
                                                   '-fno-omit-frame-pointer'], compile_exit_code=0)
            if name == 'hotkey_command_context':
                report = json.loads(logs[name])
                results = {'production-all.log':
                           {key: report[key] for key in ('passed', 'checks', 'scenarios')}}
                results.update({'negative-control-' + key + '.log':
                                {field: value for field, value in case.items() if field != 'returncode'}
                                for key, case in report['negative_control']['cases'].items()})
                supporting = {}
                for filename, result in results.items():
                    raw_path = self.directory / filename
                    raw_path.write_text(json.dumps(result, separators=(',', ':')) + '\n')
                    data = raw_path.read_bytes()
                    supporting[str(raw_path)] = {'bytes': len(data), 'sha256': PACKAGE.sha(data)}
                checks[-1]['supporting_logs'] = supporting
            if name == 'hotkey_live_observation':
                report_path = path.parent / 'result.json'
                report_path.write_text(json.dumps(LIVE_REPORT_FIXTURE) + '\n')
                raw = report_path.read_bytes()
                checks[-1]['supporting_logs'] = {
                    str(report_path): {'bytes': len(raw), 'sha256': PACKAGE.sha(raw)}}
            if name == 'formation_input_ownership':
                report = json.loads(logs[name])
                supporting = {}
                for branch, dirname in (('current', 'current'), ('baseline', 'baseline-expression')):
                    directory = path.parent / (path.stem + '-artifacts') / dirname
                    directory.mkdir(parents=True)
                    report[branch]['directory'] = str(directory)
                    raw_path = directory / 'run.log'
                    raw = (report[branch]['stdout'] + report[branch]['stderr']).encode()
                    raw_path.write_bytes(raw)
                    supporting[str(raw_path)] = {'bytes': len(raw), 'sha256': PACKAGE.sha(raw)}
                path.write_text(json.dumps(report, separators=(',', ':')) + '\n')
                checks[-1]['log_sha256'] = PACKAGE.sha(path.read_bytes())
                checks[-1]['supporting_logs'] = supporting
            if name == 'hotkey_cold_start':
                report = json.loads(logs[name])
                supporting = {}
                for branch in ('current', 'baseline'):
                    directory = path.parent / branch
                    directory.mkdir()
                    raw_path = directory / 'run.log'
                    raw = (report[branch]['stdout'] + report[branch]['stderr']).encode()
                    raw_path.write_bytes(raw)
                    supporting[str(raw_path)] = {'bytes': len(raw), 'sha256': PACKAGE.sha(raw)}
                checks[-1]['supporting_logs'] = supporting
        self.receipt = {'schema': 'ra2.optimization-validation.v1', 'commit': self.commit,
                        'source_tree_sha': self.tree, 'version': PACKAGE.VERSION,
                        'file_version': PACKAGE.FILE_VERSION, 'required_checks_passed': True,
                        'windows_executed': False, 'game_tested': False,
                        'gpu_tested': False, 'multiplayer_tested': False,
                        'engine_changes': [], 'checks': checks}

    def verify(self, receipt=None):
        return PACKAGE.validate_evidence(receipt or self.receipt, self.directory / 'validation.json',
                                         self.commit, self.tree, self.inputs, self.sources.__getitem__)

    def expected_log_paths(self, receipt):
        paths = set()
        for row in receipt['checks']:
            path = Path(row['log_path'])
            paths.add(str((path if path.is_absolute() else self.directory / path).resolve()))
            paths.update(row.get('supporting_logs', {}))
        return paths

    def test_current_receipt_requires_all_legacy_and_feature_bindings(self):
        legacy = {'missile_allocator', 'snapshot_reuse', 'tech_capture', 'voxel_recovery',
                  'idle_aircraft_display', 'terrain_reuse', 'console_receipt_order',
                  'catalogue_recovery', 'planner_batch_delivery', 'scene_extra_recovery'}
        self.assertEqual(set(PACKAGE.CHECK_BINDINGS), legacy | set(FEATURE_LOGS) | set(PACKAGE.ENTRY_BINDINGS))
        self.assertEqual(set(PACKAGE.FEATURE_CHECKS), set(FEATURE_LOGS) | set(PACKAGE.ENTRY_BINDINGS))
        self.assertEqual(set(self.verify()), self.expected_log_paths(self.receipt))
        self.assertEqual(len(PACKAGE.CHECK_BINDINGS), 38)
        self.assertEqual((PACKAGE.VERSION, PACKAGE.FILE_VERSION), ('1.0.47-pre.9', '1.0.47.9'))

    def test_shared_transition_accepts_only_reviewed_protocol_103_tail(self):
        self.assertEqual(PACKAGE.ASSIST_PROTOCOL, 103)
        PACKAGE.verify_shared_transition(23162528, 23162552)
        for base, current in ((23162528, 23162528), (23162528, 23162544),
                              (23162528, 23162548), (23162528, 23162560),
                              (23162544, 23162552), (23162520, 23162552),
                              ('23162528', 23162552), (23162528, '23162552'),
                              (True, 23162552), (23162528, True)):
            with self.subTest(base=base, current=current), self.assertRaisesRegex(ValueError, 'Shared layout differs'):
                PACKAGE.verify_shared_transition(base, current)

    def test_old_missing_duplicated_and_unexecuted_checks_are_rejected(self):
        for change in ({'commit': 'c' * 40}, {'source_tree_sha': 'd' * 40},
                       {'version': '1.0.46'}, {'file_version': '1.0.46.4'},
                       {'required_checks_passed': False}, {'windows_executed': True},
                       {'gpu_tested': True}, {'game_tested': True}, {'multiplayer_tested': True}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.verify(self.receipt | change)
        for checks in (self.receipt['checks'][:-1],
                       [self.receipt['checks'][0]] * len(PACKAGE.CHECK_BINDINGS)):
            with self.assertRaisesRegex(ValueError, 'current optimization checks'):
                self.verify(self.receipt | {'checks': checks})
        for key, value in (('executed_this_revision', False), ('exit_code', 1), ('exit_code', False),
                           ('command', [])):
            changed = copy.deepcopy(self.receipt)
            changed['checks'][0][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                self.verify(changed)

    def test_missing_or_changed_source_hash_cannot_relabel_a_previous_run(self):
        for key in ('test_source_hashes', 'production_source_hashes'):
            changed = copy.deepcopy(self.receipt)
            first = next(iter(changed['checks'][0][key]))
            changed['checks'][0][key][first] = '0' * 64
            with self.assertRaisesRegex(ValueError, 'Tested source differs'):
                self.verify(changed)
            del changed['checks'][0][key][first]
            with self.assertRaisesRegex(ValueError, 'Incomplete'):
                self.verify(changed)

    def test_tampered_empty_or_compile_only_log_is_rejected(self):
        row = self.receipt['checks'][0]
        path = self.directory / row['log_path']
        path.write_bytes(path.read_bytes() + b'changed after receipt\n')
        with self.assertRaisesRegex(ValueError, 'log is empty or changed'):
            self.verify()
        for data in (b'', b'compiled successfully\n'):
            path.write_bytes(data)
            row['log_sha256'] = PACKAGE.sha(data)
            with self.assertRaises(ValueError):
                self.verify()

    def test_voxel_gate_accepts_new_check_count_but_rejects_failed_or_expected_failure(self):
        output = 'recovered_scenes=9 pixel_mismatches=0\nchecks=200 failures=0\n'
        self.assertTrue(PACKAGE.success_log('voxel_recovery', output))
        self.assertTrue(PACKAGE.success_log('voxel_recovery', json.dumps({'stdout': output, 'exit_code': 0})))
        for bad in (output.replace('failures=0', 'failures=1'),
                    output.replace('pixel_mismatches=0', 'pixel_mismatches=1'),
                    output + 'FAIL recovered candidate\n',
                    json.dumps({'stdout': output, 'expected_failure': True})):
            self.assertFalse(PACKAGE.success_log('voxel_recovery', bad))

    def test_new_groups_cannot_be_omitted_or_relabelled_as_unexecuted(self):
        for name in PACKAGE.FUNCTIONAL_CHECKS | PACKAGE.FEATURE_CHECKS:
            changed = copy.deepcopy(self.receipt)
            changed['checks'] = [row for row in changed['checks'] if row['name'] != name]
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'current optimization checks'):
                self.verify(changed)
            for key, value in (('executed_this_revision', False), ('exit_code', 1), ('exit_code', False),
                               ('command', [])):
                changed = copy.deepcopy(self.receipt)
                next(row for row in changed['checks'] if row['name'] == name)[key] = value
                with self.subTest(name=name, key=key), self.assertRaises(ValueError):
                    self.verify(changed)

    def test_new_groups_require_present_unchanged_run_logs(self):
        for name in PACKAGE.FUNCTIONAL_CHECKS | PACKAGE.FEATURE_CHECKS:
            row = next(row for row in self.receipt['checks'] if row['name'] == name)
            path = self.directory / row['log_path']
            original = path.read_bytes()
            path.unlink()
            with self.subTest(name=name, reason='missing'), self.assertRaises(FileNotFoundError):
                self.verify()
            for data in (b'', original + b'changed after execution\n'):
                path.write_bytes(data)
                with self.subTest(name=name, reason='changed'), self.assertRaisesRegex(ValueError, 'log is empty or changed'):
                    self.verify()
            path.write_bytes(original)

    def test_new_groups_require_each_test_and_production_fingerprint(self):
        for name in PACKAGE.FUNCTIONAL_CHECKS | PACKAGE.FEATURE_CHECKS:
            for key, required in zip(('test_source_hashes', 'production_source_hashes'), PACKAGE.CHECK_BINDINGS[name]):
                for source in required:
                    for missing in (False, True):
                        changed = copy.deepcopy(self.receipt)
                        row = next(row for row in changed['checks'] if row['name'] == name)
                        if missing:
                            del row[key][source]
                        else:
                            row[key][source] = '0' * 64
                        with self.subTest(name=name, source=source, missing=missing), self.assertRaises(ValueError):
                            self.verify(changed)
                    original = self.sources[source]
                    self.sources[source] += b'changed after execution\n'
                    with self.subTest(name=name, source=source), self.assertRaisesRegex(ValueError, 'Tested source differs'):
                        self.verify()
                    self.sources[source] = original

    def test_new_groups_reject_claimed_pass_wrappers_duplicates_and_failure_output(self):
        for name, good in (FUNCTIONAL_LOGS | FEATURE_LOGS).items():
            row = next(row for row in self.receipt['checks'] if row['name'] == name)
            path = self.directory / row['log_path']
            original = path.read_bytes()
            for bad in ('{"passed":true}\n', 'compiled successfully\n', good + good,
                        good + 'FAIL synthetic negative control\n',
                        json.dumps({'passed': True, 'expected_failure': True, 'stdout': good}),
                        json.dumps({'passed': True, 'stdout': good})):
                path.write_text(bad, encoding='utf-8')
                row['log_sha256'] = PACKAGE.sha(path.read_bytes())
                with self.subTest(name=name, bad=bad[:50]), self.assertRaisesRegex(ValueError, 'lacks the current check result'):
                    self.verify()
            path.write_bytes(original)
            row['log_sha256'] = PACKAGE.sha(original)

    def test_new_group_result_counts_and_both_pixel_recoveries_are_mandatory(self):
        changes = {
            'console_receipt_order': [('"checks":24', '"checks":23'), ('"failures":0', '"failures":1'),
                                      ('"failures":0', '"failures":false'), ('"passed":true', '"passed":false')],
            'catalogue_recovery': [('"checks":93', '"checks":92'), ('"failures":0', '"failures":1'),
                                   ('"failures":0', '"failures":false'), ('"scenarios":21', '"scenarios":20'),
                                   ('"recovered_catalogues":20', '"recovered_catalogues":19')],
            'planner_batch_delivery': [('136 checks', '135 checks'), ('136 checks', '80 checks'),
                                       ('0 failures', '1 failures'),
                                       ('duplicate_far=1 first_yields=16 next_yields=4 attempts=20',
                                        'duplicate_far=1 first_yields=16 next_yields=0 attempts=20'),
                                       ('frame=590 yields=0 attempts=3', 'frame=590 yields=1 attempts=4')],
            'scene_extra_recovery': [('checks=109', 'checks=108'), ('failures=0', 'failures=1'),
                                     ('layer_recoveries=9', 'layer_recoveries=0'),
                                     ('scene_recoveries=12', 'scene_recoveries=0'),
                                     ('layer_recoveries=9 pixel_mismatches=0', 'layer_recoveries=9 pixel_mismatches=1'),
                                     ('scene_recoveries=12 pixel_mismatches=0', 'scene_recoveries=12 pixel_mismatches=1'),
                                     ('pixel_mismatches=0', 'pixel_mismatches=1')],
        }
        for name, variants in changes.items():
            good = FUNCTIONAL_LOGS[name]
            self.assertTrue(PACKAGE.success_log(name, good), name)
            for old, new in variants:
                bad = good.replace(old, new, 1)
                with self.subTest(name=name, change=(old, new)):
                    self.assertNotEqual(good, bad)
                    self.assertFalse(PACKAGE.success_log(name, bad))

    def test_feature_counts_and_lifecycle_results_cannot_regress(self):
        changes = {
            'planner_defense_delivery': [('21 checks', '20 checks'), ('0 failures', '1 failures'),
                                         ('next_defense=1', 'next_defense=0'),
                                         ('early=0 due=1', 'early=1 due=0'),
                                         ('host_task first=0 next=1', 'host_task first=0 next=0')],
            'missile_manual_lifecycle': [('16 checks', '14 checks'), ('0 failures', '1 failures'),
                                         ('count=1 new_owned=1', 'count=1024 new_owned=0')],
            'missile_manual_capacity': [('23 checks', '22 checks'), ('0 failures', '1 failures')],
            'patrol_capacity': [('31 checks', '30 checks'), ('0 failures', '1 failures'),
                                ('width=80 count=16 bind=1', 'width=80 count=32 bind=0')],
            'patrol_small_buffer': [('1 checks', '0 checks'), ('0 failures', '1 failures'),
                                    ('count=16', 'count=32')],
            'selection_catalogue_identity': [('23 checks', '22 checks'), ('0 failures', '1 failures')],
            'toast_clock_wrap': [('15 checks', '14 checks'), ('0 failures', '1 failures')],
            'console_composition': [('"checks":16', '"checks":15'), ('"failures":0', '"failures":1'),
                                    ('"failures":0', '"failures":false')],
            'console_hotkey_presentation': [('"checks":8', '"checks":7'), ('"failures":0', '"failures":1'),
                                            ('"failures":0', '"failures":false')],
            'secondary_drag_bounds': [('7 boundary cases', '6 boundary cases'), ('900 range cases', '899 range cases')],
            'secondary_render_retry': [('8 submission', '7 submission')],
            'icon_cache_reopen': [('healthy reconnect keeps generation', 'healthy reconnect loses generation')],
            'hotkey_input_diagnostic': [('14 scenarios', '13 scenarios'), ('83 checks', '82 checks'),
                                        ('no Win32/game execution', 'Windows/game execution')],
            'input_ownership': [('61 checks', '60 checks'), ('zero sentinel', 'no sentinel')],
            'hotkey_live_observation': [('"scenarios":10', '"scenarios":9'),
                                        ('"checks":40', '"checks":39'), ('"passed":true', '"passed":1'),
                                        ('no Windows/game execution', 'Windows/game execution')],
            'formation_input_ownership': [('"case_count":13', '"case_count":12'),
                                          ('"check_count":78', '"check_count":77'),
                                          ('"compile_exit":0', '"compile_exit":1'),
                                          ('"run_exit":1', '"run_exit":0'),
                                          ('explicit former owner expression only', 'historical product execution')],
            'hotkey_cold_start': [('"scenarios":11', '"scenarios":10'),
                                  ('"checks":77', '"checks":76'),
                                  ('"negative_control_reproduced":true', '"negative_control_reproduced":false'),
                                  ('"compile_exit_code":0', '"compile_exit_code":1'),
                                  ('"exit_code":1', '"exit_code":0'),
                                  (PACKAGE.COLD_START_BASELINE_SHA, '0' * 64)],
            'superweapon_result_feedback': [('"checks":15', '"checks":14'), ('"failures":0', '"failures":1'),
                                             ('"passed":true', '"passed":false'), ('"failures":0', '"failures":false')],
            'pause_key_pairing': [('"checks":45', '"checks":44'), ('"failures":0', '"failures":1'),
                                  ('"passed":true', '"passed":false'), ('"failures":0', '"failures":false')],
            'native_mouse_pairing': [('"checks":69', '"checks":68'), ('"failures":0', '"failures":1'),
                                     ('"passed":true', '"passed":false'), ('"failures":0', '"failures":false')],
            'passenger_frame_sampling': [('2158 checks', '2157 checks'), ('live_RPM=0', 'live_RPM=14'),
                                         ('120ms lease never renewed', '120ms lease renewed')],
            'hotkey_command_context': [('"checks":67', '"checks":66'),
                                       ('"scenarios":22', '"scenarios":21'),
                                       ('"reproduced":true', '"reproduced":false'),
                                       ('"returncode":2', '"returncode":0'),
                                       ('"requested":true', '"requested":false')],
            'hotkey_message_recovery': [('"case_count":38', '"case_count":37'),
                                        ('"check_count":161', '"check_count":160'),
                                        ('"expected_failure":true', '"expected_failure":false'),
                                        ('"exit_code":1', '"exit_code":0'),
                                        ('actual=0', 'actual=1')],
        }
        self.assertEqual(set(changes), set(FEATURE_LOGS))
        for name, variants in changes.items():
            good = FEATURE_LOGS[name]
            self.assertTrue(PACKAGE.success_log(name, good), name)
            for old, new in variants:
                with self.subTest(name=name, change=(old, new)):
                    bad = good.replace(old, new, 1)
                    self.assertNotEqual(good, bad)
                    self.assertFalse(PACKAGE.success_log(name, bad))
            for missing in range(len(good.splitlines())):
                lines = good.splitlines()
                del lines[missing]
                with self.subTest(name=name, missing_line=missing):
                    self.assertFalse(PACKAGE.success_log(name, '\n'.join(lines)))

    def test_feature_json_rejects_ambiguous_fields_and_duplicate_cases(self):
        for name in ('console_composition', 'console_hotkey_presentation',
                     'superweapon_result_feedback', 'pause_key_pairing', 'native_mouse_pairing'):
            good = FEATURE_LOGS[name]
            for bad in (good.replace('"failures":0', '"failures":1,"failures":0'),
                        good.replace('"failures":0', '"failures":0,"unknown":true'),
                        good.replace('"checks":', '"checks":true,"ignored":')):
                with self.subTest(name=name, bad=bad):
                    self.assertFalse(PACKAGE.success_log(name, bad))
        for name in ('console_composition', 'console_hotkey_presentation'):
            lines = FEATURE_LOGS[name].splitlines()
            lines[1] = lines[0]
            self.assertFalse(PACKAGE.success_log(name, '\n'.join(lines)))

    def test_hotkey_reports_require_typed_results_and_pinned_negative_controls(self):
        for name in PACKAGE.HOTKEY_CHECKS:
            good = FEATURE_LOGS[name]
            for bad in (good.replace('"passed":true', '"passed":false,"passed":true', 1),
                        good.replace('"passed":true', '"passed":true,"unknown":true', 1),
                        good.replace('"passed":true', '"passed":1', 1),
                        good.replace(PACKAGE.HOTKEY_BASELINE_COMMIT, '0' * 40, 1)):
                with self.subTest(name=name, bad=bad[:100]):
                    self.assertFalse(PACKAGE.success_log(name, bad))
        context = json.loads(FEATURE_LOGS['hotkey_command_context'])
        for path, value in ((('checks',), True), (('scenarios',), True),
                            (('negative_control', 'cases', 'registration', 'returncode'), False),
                            (('negative_control', 'cases', 'message', 'failure'), 'C++ compilation failed')):
            changed = copy.deepcopy(context)
            target = changed
            for part in path[:-1]:
                target = target[part]
            target[path[-1]] = value
            self.assertFalse(PACKAGE.success_log('hotkey_command_context', json.dumps(changed)))
        recovery = json.loads(FEATURE_LOGS['hotkey_message_recovery'])
        for path, value in ((('case_count',), True), (('check_count',), True),
                            (('production_run', 'exit_code'), False),
                            (('baseline_negative_control', 'exit_code'), True),
                            (('baseline_negative_control', 'stderr'), 'compilation failed')):
            changed = copy.deepcopy(recovery)
            target = changed
            for part in path[:-1]:
                target = target[part]
            target[path[-1]] = value
            self.assertFalse(PACKAGE.success_log('hotkey_message_recovery', json.dumps(changed)))

    def test_input_integration_reports_reject_untyped_or_unexecuted_results(self):
        live = FEATURE_LOGS['hotkey_live_observation']
        for bad in (live.replace('"passed":true', '"passed":false,"passed":true'),
                    live.replace('"checks":40', '"checks":true'),
                    live.replace('"passed":true', '"passed":true,"extra":1'),
                    live.replace('synthetic policy case 1\n', 'synthetic policy case 0\n'),
                    live.replace('PASS hotkey_live_observation::', 'PASS other_suite::', 1)):
            with self.subTest(kind='live', bad=bad[:70]):
                self.assertFalse(PACKAGE.success_log('hotkey_live_observation', bad))
        formation = formation_policy_fixture()
        self.assertTrue(PACKAGE.success_log('formation_input_ownership', json.dumps(formation)))
        for branch, blocked_names in (('current', {'current_active_preview_wm'}),
                                      ('baseline', {'current_active_preview_wm', 'unset_high_uptime_wm'})):
            self.assertEqual({case['scenario'] for case in formation[branch]['cases'] if case['blocked']},
                             blocked_names)
            for index, case in enumerate(formation[branch]['cases']):
                for wrong in (1 - case['blocked'], True, False, -1, 2, '0'):
                    changed = copy.deepcopy(formation)
                    row = changed[branch]
                    row['cases'][index]['blocked'] = wrong
                    # Rebind the synthetic stdout and hash so this rejection
                    # proves the exact counter/type rule, not a stale raw log.
                    row['stdout'] = ''.join(json.dumps(record, separators=(',', ':')) + '\n'
                                            for record in row['cases'] + [row['summary']])
                    row['run_log_sha256'] = PACKAGE.sha((row['stdout'] + row['stderr']).encode())
                    with self.subTest(kind='formation-blocked', branch=branch,
                                      scenario=case['scenario'], wrong=wrong):
                        self.assertFalse(PACKAGE.success_log('formation_input_ownership', json.dumps(changed)))
        for path, value in ((('passed',), 1), (('case_count',), True), (('check_count',), True),
                            (('current', 'compile_exit'), False), (('baseline', 'compile_exit'), 1),
                            (('baseline', 'run_exit'), True), (('baseline', 'run_exit'), 0),
                            (('current', 'summary', 'passed'), 1),
                            (('baseline', 'summary', 'failures'), 4),
                            (('current', 'cases', 0, 'passed'), 1)):
            changed = copy.deepcopy(formation)
            target = changed
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
            with self.subTest(kind='formation', path=path):
                self.assertFalse(PACKAGE.success_log('formation_input_ownership', json.dumps(changed)))

    def test_live_input_report_and_stdout_are_bound_and_coverage_can_grow(self):
        row = next(row for row in self.receipt['checks'] if row['name'] == 'hotkey_live_observation')
        raw_path = self.directory / row['log_path']
        report_path = self.directory / 'result.json'
        report = json.loads(report_path.read_text())
        original = copy.deepcopy(report)

        def save_report(value):
            report_path.write_text(json.dumps(value))
            data = report_path.read_bytes()
            row['supporting_logs'][str(report_path)] = {'bytes': len(data), 'sha256': PACKAGE.sha(data)}

        for field, value in (('checks', 41), ('compile_exit_code', False), ('run_exit_code', 1),
                              ('windows_executed', True), ('game_tested', True)):
            save_report(original | {field: value})
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'differs from actual run'):
                self.verify()
        report['sources'][next(iter(report['sources']))] = '0' * 64
        save_report(report)
        with self.assertRaisesRegex(ValueError, 'report source differs'):
            self.verify()
        save_report(original)
        report_path.write_text(report_path.read_text() + '\n')
        with self.assertRaisesRegex(ValueError, 'report changed after run'):
            self.verify()
        grown = original | {'checks': 41, 'scenarios': 11}
        save_report(grown)
        lines = raw_path.read_text().splitlines()
        lines.insert(-1, 'PASS hotkey_live_observation::additional synthetic policy case')
        lines[-1] = json.dumps({key: grown[key] for key in ('suite', 'passed', 'scenarios', 'checks', 'scope')})
        raw_path.write_text('\n'.join(lines) + '\n')
        row['log_sha256'] = PACKAGE.sha(raw_path.read_bytes())
        self.verify()
        row['supporting_logs'] = {}
        with self.assertRaisesRegex(ValueError, 'Missing live input diagnostic report binding'):
            self.verify()

    def test_formation_report_sources_compiler_inputs_and_raw_runs_are_bound(self):
        row = next(row for row in self.receipt['checks'] if row['name'] == 'formation_input_ownership')
        report_path = self.directory / row['log_path']
        original = json.loads(report_path.read_text())

        def save_report(value):
            report_path.write_text(json.dumps(value))
            row['log_sha256'] = PACKAGE.sha(report_path.read_bytes())

        changed = copy.deepcopy(original)
        changed['source_sha256'][next(iter(changed['source_sha256']))] = '0' * 64
        save_report(changed)
        with self.assertRaisesRegex(ValueError, 'report source differs'):
            self.verify()
        changed = copy.deepcopy(original)
        header = next(iter(changed['current']['production_header_sha256']))
        for branch in ('current', 'baseline'):
            changed[branch]['production_header_sha256'][header] = '0' * 64
        save_report(changed)
        with self.assertRaisesRegex(ValueError, 'Formation compiler input differs'):
            self.verify()
        save_report(original)
        for raw_name in list(row['supporting_logs']):
            path = Path(raw_name)
            raw = path.read_bytes()
            path.write_bytes(raw + b'changed after execution\n')
            with self.subTest(path=raw_name), self.assertRaisesRegex(ValueError, 'execution log differs'):
                self.verify()
            path.write_bytes(raw)
        row['supporting_logs'].pop(next(iter(row['supporting_logs'])))
        with self.assertRaisesRegex(ValueError, 'Missing current/negative formation input execution logs'):
            self.verify()

    def test_cold_start_requires_compiled_control_and_typed_case_failures(self):
        original = cold_start_policy_fixture()
        for path, value in ((('passed',), 1), (('scenarios',), True), (('checks',), True),
                            (('negative_control_reproduced',), 1), (('windows_executed',), True),
                            (('current', 'compile_exit_code'), False), (('baseline', 'compile_exit_code'), 1),
                            (('baseline', 'exit_code'), True), (('current', 'summary', 'passed'), 1),
                            (('baseline', 'summary', 'failures'), 17),
                            (('baseline', 'cases', 0, 'passed'), True),
                            (('current', 'cases', 0, 'feature_demand'), False)):
            changed = copy.deepcopy(original)
            target = changed
            for key in path[:-1]:
                target = target[key]
            target[path[-1]] = value
            with self.subTest(path=path):
                self.assertFalse(PACKAGE.success_log('hotkey_cold_start', json.dumps(changed)))
        text = json.dumps(original)
        self.assertFalse(PACKAGE.success_log('hotkey_cold_start',
            text.replace('"passed": true', '"passed": false, "passed": true', 1)))

    def test_cold_start_report_sources_and_both_raw_executions_are_bound(self):
        row = next(row for row in self.receipt['checks'] if row['name'] == 'hotkey_cold_start')
        report_path = self.directory / row['log_path']
        original = report_path.read_bytes()
        changed = json.loads(original)
        changed['sources'][next(iter(changed['sources']))] = '0' * 64
        report_path.write_text(json.dumps(changed))
        row['log_sha256'] = PACKAGE.sha(report_path.read_bytes())
        with self.assertRaisesRegex(ValueError, 'report source differs'):
            self.verify()
        report_path.write_bytes(original)
        row['log_sha256'] = PACKAGE.sha(original)
        for raw_name in list(row['supporting_logs']):
            raw_path = Path(raw_name)
            raw = raw_path.read_bytes()
            raw_path.write_bytes(raw + b'changed after execution\n')
            with self.subTest(path=raw_name), self.assertRaisesRegex(ValueError, 'Cold-start execution log differs'):
                self.verify()
            raw_path.write_bytes(raw)
        row['supporting_logs'].pop(next(iter(row['supporting_logs'])))
        with self.assertRaisesRegex(ValueError, 'Missing current/negative cold-start execution logs'):
            self.verify()

    def test_hotkey_raw_report_hashes_must_match_the_executed_source(self):
        for name in PACKAGE.HOTKEY_CHECKS:
            row = next(row for row in self.receipt['checks'] if row['name'] == name)
            path = self.directory / row['log_path']
            original = path.read_bytes()
            report = json.loads(original)
            key = 'sources' if name == 'hotkey_command_context' else 'source_hashes'
            report[key][next(iter(report[key]))] = '0' * 64
            path.write_text(json.dumps(report))
            row['log_sha256'] = PACKAGE.sha(path.read_bytes())
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'Hotkey raw report source differs'):
                self.verify()
            path.write_bytes(original)
            row['log_sha256'] = PACKAGE.sha(original)

    def test_hotkey_context_coverage_can_grow_but_stdout_must_match_report(self):
        row = next(row for row in self.receipt['checks'] if row['name'] == 'hotkey_command_context')
        report_path = self.directory / row['log_path']
        report = json.loads(report_path.read_text())
        report.update(checks=104, scenarios=35)
        report_path.write_text(json.dumps(report))
        row['log_sha256'] = PACKAGE.sha(report_path.read_bytes())
        self.assertTrue(PACKAGE.success_log('hotkey_command_context', report_path.read_text()))
        with self.assertRaisesRegex(ValueError, 'execution log result differs from report'):
            self.verify()
        path = self.directory / 'production-all.log'
        path.write_text(json.dumps({key: report[key] for key in ('passed', 'checks', 'scenarios')}))
        with self.assertRaisesRegex(ValueError, 'execution log changed after run'):
            self.verify()
        data = path.read_bytes()
        row['supporting_logs'][str(path)] = {'bytes': len(data), 'sha256': PACKAGE.sha(data)}
        self.verify()
        del row['supporting_logs'][str(path)]
        with self.assertRaisesRegex(ValueError, 'Missing current/negative hotkey execution log bindings'):
            self.verify()

    def test_patrol_buffer_requires_actual_sanitizer_compile_and_buffer_command(self):
        variants = (
            {'compile_command': []}, {'compile_command': ['g++', '-fno-omit-frame-pointer']},
            {'compile_command': ['g++', '-fsanitize=address']},
            {'compile_command': ['g++', '-fsanitize=address', '-fno-omit-frame-pointer', '-fno-sanitize=all']},
            {'compile_exit_code': 1}, {'compile_exit_code': False},
            {'command': ['synthetic-gate-fixture']},
        )
        for change in variants:
            changed = copy.deepcopy(self.receipt)
            next(row for row in changed['checks'] if row['name'] == 'patrol_small_buffer').update(change)
            with self.subTest(change=change), self.assertRaisesRegex(ValueError, 'executed ASan build'):
                self.verify(changed)

    def test_real_published_resources_and_engine_are_pinned(self):
        resources, record, manifest = PACKAGE.pinned_base(BASE_FULL, BASE_MANIFEST)
        self.assertEqual((len(resources), len(record['files']), manifest['file_version']), (47, 53, '1.0.46.4'))
        for name in PACKAGE.POLICY.ENGINE:
            if name not in PACKAGE.APPROVED_ENGINE_CHANGES:
                self.assertEqual((ROOT / 'source' / name).read_bytes(), resources[name], name)

    def test_engine_change_requires_explicit_plan_and_additional_bound_check(self):
        resources, _, _ = PACKAGE.pinned_base(BASE_FULL, BASE_MANIFEST)
        engine = {name: resources[name] for name in PACKAGE.POLICY.ENGINE}
        self.assertEqual(PACKAGE.engine_change_plan(resources, engine), [])
        engine['engine/prepare_runtime.py'] += b'\n# synthetic gate change\n'
        changed = PACKAGE.engine_change_plan(resources, engine)
        self.assertEqual(changed, ['engine/prepare_runtime.py'])
        with self.assertRaisesRegex(ValueError, 'engine change plan'):
            PACKAGE.validate_evidence(self.receipt, self.directory / 'validation.json', self.commit,
                                      self.tree, self.inputs, self.sources.__getitem__, changed)
        receipt = copy.deepcopy(self.receipt)
        receipt['engine_changes'] = changed
        with self.assertRaisesRegex(ValueError, 'required dependency check'):
            PACKAGE.validate_evidence(receipt, self.directory / 'validation.json', self.commit,
                                      self.tree, self.inputs, self.sources.__getitem__, changed)
        tests, production = PACKAGE.DEPENDENCY_BINDINGS
        for path in tests | production:
            self.sources[path] = ('synthetic input: ' + path).encode()
            self.inputs[path] = {'sha256': PACKAGE.sha(self.sources[path])}
        log = self.directory / 'dependency.json'
        log.write_text(json.dumps({'passed': True, 'tests_run': 6, 'failures': 0, 'errors': 0}))
        receipt['checks'].append({'name': 'dependency_probes', 'command': ['synthetic-dependency-fixture'],
                                  'exit_code': 0, 'executed_this_revision': True,
                                  'log_path': log.name, 'log_sha256': PACKAGE.sha(log.read_bytes()),
                                  'test_source_hashes': {p: self.inputs[p]['sha256'] for p in tests},
                                  'production_source_hashes': {p: self.inputs[p]['sha256'] for p in production}})
        self.assertEqual(set(PACKAGE.validate_evidence(receipt, self.directory / 'validation.json',
                         self.commit, self.tree, self.inputs, self.sources.__getitem__, changed)),
                         self.expected_log_paths(receipt))
        engine['engine/precompute.py'] += b'\n# unreviewed fixture change\n'
        with self.assertRaisesRegex(ValueError, 'outside the reviewed plan'):
            PACKAGE.engine_change_plan(resources, engine)

    def test_tampered_published_archive_or_manifest_is_rejected(self):
        archive = self.directory / 'changed.zip'
        data = bytearray(BASE_FULL.read_bytes())
        data[-1] ^= 1
        archive.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'archive differs'):
            PACKAGE.pinned_base(archive, BASE_MANIFEST)
        manifest = self.directory / 'changed.json'
        manifest.write_bytes(BASE_MANIFEST.read_bytes() + b'\n')
        with self.assertRaisesRegex(ValueError, 'manifest differs'):
            PACKAGE.pinned_base(BASE_FULL, manifest)

    def test_product_payload_has_full_resources_and_paired_complete_small_update(self):
        resources, _, _ = PACKAGE.pinned_base(BASE_FULL, BASE_MANIFEST)
        current = {name: b'synthetic, not a PE or release artifact' for name in PACKAGE.POLICY.BINARIES}
        current |= {'README.txt': b'synthetic README', 'Start.cmd': b'synthetic launcher'}
        engine = {name: resources[name] for name in PACKAGE.POLICY.ENGINE}
        full, update = PACKAGE.assemble(resources, current, engine)
        self.assertEqual((len(full), len(update)), (53, 21))
        self.assertTrue(all(full[name] == data for name, data in resources.items()))
        self.assertTrue(all(full[name] == data for name, data in update.items()))
        self.assertEqual(len([name for name in update if name.startswith('licenses/')]), 8)
        for name in ('runtime/python.exe', 'assets/timelines/a.dis', 'profiles/capitaine/preview.html',
                     'source/main.cpp', 'settings.ini', 'engine/extra.py', 'licenses/debug.pdb',
                     '../outside', 'engine\\precompute.py', 'ra2toolbox.exe'):
            with self.subTest(name=name), self.assertRaises(ValueError):
                PACKAGE.POLICY.verify_product_members(list(full) + [name], full=True)

    def test_changed_binary_bytes_cannot_match_completed_build_receipt(self):
        data = b'synthetic verified bytes'
        row = {'bytes': len(data), 'sha256': PACKAGE.sha(data)}
        PACKAGE.POLICY.verify_artifact_binding('synthetic.exe', row, row, data)
        with self.assertRaises(ValueError):
            PACKAGE.POLICY.verify_artifact_binding('synthetic.exe', row, row, data + b'changed')


def main():
    global BASE_FULL, BASE_MANIFEST
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-full', required=True, type=Path)
    parser.add_argument('--base-manifest', required=True, type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    BASE_FULL, BASE_MANIFEST = args.base_full.resolve(), args.base_manifest.resolve()
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(PackageGates))
    report = {'schema': 'ra2.packaging-gates.v1', 'scope': 'packaging_policy_only',
              'synthetic_receipt_fixtures': True, 'product_build_performed': False,
              'windows_executed': False, 'game_tested': False, 'gpu_tested': False, 'multiplayer_tested': False,
              'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
              'passed': result.wasSuccessful(),
              'base_archive_sha256': PACKAGE.sha(BASE_FULL.read_bytes()),
              'base_manifest_sha256': PACKAGE.sha(BASE_MANIFEST.read_bytes()),
              'gate_inputs': {name: PACKAGE.sha((ROOT / name).read_bytes())
                              for name in PACKAGE.PACKAGE_INPUTS}}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_bytes(PACKAGE.encoded(report))
    print(json.dumps(report, indent=2))
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
