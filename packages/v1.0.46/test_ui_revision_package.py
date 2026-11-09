#!/usr/bin/env python3
"""Check UI-release provenance rejection and the actual pinned resource base.

Receipt/log fixtures below are synthetic negative controls, never release
evidence. The real packager independently reads the executed UI logs and every
frozen source and output byte.
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
SPEC = importlib.util.spec_from_file_location('ui_revision_package', HERE / 'package_ui_revision.py')
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
ROOT = HERE.parents[1]
BASE_FULL = BASE_MANIFEST = None


class UIRevisionPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='ra2-ui-package-policy-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.commit = 'a' * 40
        paths = set().union(*(tests | production for tests, production in PACKAGE.CHECK_BINDINGS.values()))
        self.sources = {path: ('fixture:' + path).encode() for path in paths}
        self.inputs = {path: {'sha256': PACKAGE.sha(data)} for path, data in self.sources.items()}
        logs = {'console_core': '35 console model tests passed\n',
                'console_debug': '16 debug console core scenarios passed\n',
                'console_browser': 'PASS browser fixture: synthetic gate input only\n',
                'console_model': 'console_model: 10 checks passed\n',
                'console_metadata': json.dumps({'scope': 'synthetic gate input', 'status': 'PASS',
                                               'result': {'checks': 10, 'groups': 15,
                                                          'assertions_enabled': True}})}
        checks = []
        for name, (tests, production) in PACKAGE.CHECK_BINDINGS.items():
            log = self.directory / (name + '.log')
            log.write_text(logs[name], encoding='utf-8')
            checks.append({'name': name, 'exit_code': 0, 'executed_this_revision': True,
                           'command': ['synthetic-package-gate-fixture', name],
                           'log_path': log.name, 'log_sha256': PACKAGE.sha(log.read_bytes()),
                           'test_source_hashes': {path: self.inputs[path]['sha256'] for path in tests},
                           'production_source_hashes': {path: self.inputs[path]['sha256'] for path in production}})
        self.receipt = {'schema': 'ra2.ui-revision-validation.v1',
                        'version': PACKAGE.VERSION, 'file_version': PACKAGE.FILE_VERSION,
                        'commit': self.commit, 'checks_reexecuted': True,
                        'required_checks_passed': True, 'windows_executed': False,
                        'game_tested': False, 'multiplayer_tested': False, 'checks': checks}

    def verify(self, receipt=None):
        return PACKAGE.validate_ui_evidence(self.receipt if receipt is None else receipt,
                                             self.directory / 'receipt.json', self.commit,
                                             self.inputs, self.sources.__getitem__)

    def test_actual_published_base_and_current_engine_are_identical(self):
        resources, record = PACKAGE.pinned_base(BASE_FULL, BASE_MANIFEST)
        self.assertEqual(len(record['files']), 53)
        self.assertEqual(len(resources), 47)
        self.assertEqual(sum(name.startswith(('assets/', 'profiles/')) for name in resources), 32)
        for name in PACKAGE.POLICY.ENGINE:
            self.assertEqual((ROOT / 'source' / name).read_bytes(), resources[name], name)

    def test_tampered_archive_or_manifest_cannot_supply_resources(self):
        bad_archive = self.directory / BASE_FULL.name
        changed = bytearray(BASE_FULL.read_bytes())
        changed[-1] ^= 1
        bad_archive.write_bytes(changed)
        with self.assertRaisesRegex(ValueError, 'archive differs'):
            PACKAGE.pinned_base(bad_archive, BASE_MANIFEST)
        bad_manifest = self.directory / 'manifest.json'
        bad_manifest.write_bytes(BASE_MANIFEST.read_bytes() + b'\n')
        with self.assertRaisesRegex(ValueError, 'manifest differs'):
            PACKAGE.pinned_base(BASE_FULL, bad_manifest)

    def test_wrapped_metadata_receipt_is_accepted_but_compile_only_is_rejected(self):
        self.assertEqual(len(self.verify()), 5)
        self.assertFalse(PACKAGE.success_log('console_metadata', 'compiled successfully'))
        self.assertFalse(PACKAGE.success_log('console_metadata', json.dumps({
            'result': {'checks': 221, 'groups': 15, 'assertions_enabled': False}})))
        self.assertFalse(PACKAGE.success_log('console_metadata', json.dumps({
            'status': 'FAIL', 'result': {'checks': 221, 'groups': 15, 'assertions_enabled': True}})))

    def test_core_only_log_cannot_be_relabelled_as_browser_validation(self):
        browser = next(row for row in self.receipt['checks'] if row['name'] == 'console_browser')
        path = self.directory / browser['log_path']
        path.write_text('35 console model tests passed\n', encoding='utf-8')
        browser['log_sha256'] = PACKAGE.sha(path.read_bytes())
        with self.assertRaisesRegex(ValueError, 'success marker'):
            self.verify()

    def test_changed_source_or_log_rejects_previously_passing_evidence(self):
        stale = copy.deepcopy(self.receipt)
        stale['checks'][0]['production_source_hashes']['source/suite/frontend/app.js'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'Tested UI source differs'):
            self.verify(stale)
        path = self.directory / self.receipt['checks'][0]['log_path']
        path.write_bytes(path.read_bytes() + b'changed after receipt\n')
        with self.assertRaisesRegex(ValueError, 'log is empty or changed'):
            self.verify()

    def test_missing_inherited_or_wrong_revision_evidence_is_rejected(self):
        for change in ({'file_version': '1.0.46.3'}, {'commit': 'b' * 40},
                       {'checks_reexecuted': False}, {'windows_executed': True}):
            with self.subTest(change=change), self.assertRaises(ValueError):
                self.verify(self.receipt | change)
        with self.assertRaisesRegex(ValueError, 'five current UI checks'):
            self.verify(self.receipt | {'checks': self.receipt['checks'][:-1]})
        inherited = copy.deepcopy(self.receipt)
        inherited['checks'][0]['executed_this_revision'] = False
        with self.assertRaisesRegex(ValueError, 'did not execute'):
            self.verify(inherited)


def main():
    global BASE_FULL, BASE_MANIFEST
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-full', required=True, type=Path)
    parser.add_argument('--base-manifest', required=True, type=Path)
    args = parser.parse_args()
    BASE_FULL, BASE_MANIFEST = args.base_full.resolve(), args.base_manifest.resolve()
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(UIRevisionPackageTests)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
