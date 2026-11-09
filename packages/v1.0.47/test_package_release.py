#!/usr/bin/env python3
"""Small packaging gates, using synthetic receipts and the real pinned base.

Synthetic successful receipts below only exercise acceptance/rejection logic.
They are never the six production checks or evidence for a release build.
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
ROOT = HERE.parents[1]
BASE_FULL = BASE_MANIFEST = None


class PackageGates(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ra2-147-package-gates-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.commit, self.tree = 'a' * 40, 'b' * 40
        paths = set().union(*(tests | production for tests, production in PACKAGE.CHECK_BINDINGS.values()))
        self.sources = {name: ('synthetic input: ' + name).encode() for name in paths}
        self.inputs = {name: {'sha256': PACKAGE.sha(data)} for name, data in self.sources.items()}
        logs = {
            'missile_allocator': '{"equivalent_scenarios":8063,"same_workspace_reused":true}\n',
            'snapshot_reuse': '{"case":"seqlock_interleavings","passed":true,"checks":10}\n',
            'tech_capture': '{"case":"summary","passed":true,"checks":10}\n',
            'voxel_recovery': 'recovered_scenes=8 pixel_mismatches=0\nchecks=160 failures=0\n',
            'idle_aircraft_display': 'PASS paused same-source aircraft display; synthetic gate fixture\n',
            'terrain_reuse': 'PASS actual terrain storage: synthetic gate fixture\n',
        }
        checks = []
        for name, (tests, production) in PACKAGE.CHECK_BINDINGS.items():
            path = self.directory / (name + '.log')
            path.write_text(logs[name], encoding='utf-8')
            checks.append({'name': name, 'command': ['synthetic-gate-fixture', name],
                           'exit_code': 0, 'executed_this_revision': True,
                           'log_path': path.name, 'log_sha256': PACKAGE.sha(path.read_bytes()),
                           'test_source_hashes': {p: self.inputs[p]['sha256'] for p in tests},
                           'production_source_hashes': {p: self.inputs[p]['sha256'] for p in production}})
        self.receipt = {'schema': 'ra2.optimization-validation.v1', 'commit': self.commit,
                        'source_tree_sha': self.tree, 'version': PACKAGE.VERSION,
                        'file_version': PACKAGE.FILE_VERSION, 'required_checks_passed': True,
                        'windows_executed': False, 'game_tested': False,
                        'gpu_tested': False, 'multiplayer_tested': False,
                        'engine_changes': [], 'checks': checks}

    def verify(self, receipt=None):
        return PACKAGE.validate_evidence(receipt or self.receipt, self.directory / 'validation.json',
                                         self.commit, self.tree, self.inputs, self.sources.__getitem__)

    def test_current_receipt_shape_accepts_six_distinct_frozen_bindings(self):
        self.assertEqual(len(self.verify()), 6)

    def test_old_missing_duplicated_and_unexecuted_checks_are_rejected(self):
        for change in ({'commit': 'c' * 40}, {'source_tree_sha': 'd' * 40},
                       {'version': '1.0.46'}, {'file_version': '1.0.46.4'},
                       {'required_checks_passed': False}, {'windows_executed': True},
                       {'gpu_tested': True}, {'game_tested': True}, {'multiplayer_tested': True}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.verify(self.receipt | change)
        for checks in (self.receipt['checks'][:-1], [self.receipt['checks'][0]] * 6):
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

    def test_real_published_resources_and_engine_are_pinned(self):
        resources, record, manifest = PACKAGE.pinned_base(BASE_FULL, BASE_MANIFEST)
        self.assertEqual((len(resources), len(record['files']), manifest['file_version']), (47, 53, '1.0.46.4'))
        for name in PACKAGE.POLICY.ENGINE:
            if name not in PACKAGE.APPROVED_ENGINE_CHANGES:
                self.assertEqual((ROOT / 'source' / name).read_bytes(), resources[name], name)

    def test_engine_change_requires_explicit_plan_and_seventh_bound_check(self):
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
        self.assertEqual(len(PACKAGE.validate_evidence(receipt, self.directory / 'validation.json',
                         self.commit, self.tree, self.inputs, self.sources.__getitem__, changed)), 7)
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
