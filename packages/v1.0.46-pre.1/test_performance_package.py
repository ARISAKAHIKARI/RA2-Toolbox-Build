#!/usr/bin/env python3
"""Exercise the actual slim-package policy without claiming a Windows run.

Member names come from the SHA256-verified published 1.0.45 archive. Tiny
synthetic payloads are used for round-trip/overlay checks; the release packager
separately verifies every real executable, resource and final ZIP member.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('slim_release', Path(__file__).with_name('package_release.py'))
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)
BASE_MEMBERS = []
BASE_SUMMARY = {}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def public_manifest_fixture():
    python = {'version': '3.12.10', 'filename': 'python-3.12.10-embed-amd64.zip',
              'url': 'https://www.python.org/ftp/python/3.12.10/python-3.12.10-embed-amd64.zip',
              'bytes': 1000, 'sha256': 'a' * 64}
    packages = []
    for name, version in [('numpy', '2.2.6'), ('scipy', '1.15.3'), ('opencv-python-headless', '4.11.0.86')]:
        filename = name.replace('-', '_') + '-' + version + '-fixture.whl'
        packages.append({'id': name, 'version': version, 'filename': filename,
                         'url': 'https://files.pythonhosted.org/packages/fixture/' + filename,
                         'bytes': 1000, 'sha256': 'b' * 64})
    return {'schema_version': 1, 'python': python, 'packages': packages}


class SlimPackagePolicyTests(unittest.TestCase):
    def setUp(self):
        self.resources = {name: ('resource: ' + name).encode() for name in BASE_MEMBERS
                          if PACKAGE.include_base_resource(name)}
        self.current = {name: ('new build fixture: ' + name).encode() for name in PACKAGE.BINARIES}
        self.current.update({'README.txt': b'1.0.46-pre.1 fixture', 'Start.cmd': b'@echo off\r\n'})
        self.engine = {name: ('new engine fixture: ' + name).encode() for name in PACKAGE.ENGINE}

    def test_keeps_slim_base_and_prepared_resources(self):
        omitted = {name for name in BASE_MEMBERS if name.startswith('runtime/')}
        self.assertEqual(len(omitted), 0)
        self.assertTrue(all(not PACKAGE.include_base_resource(name) for name in omitted))
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        self.assertFalse(any(name.casefold().startswith('runtime/') for name in full))
        self.assertFalse(any(name.casefold().startswith('runtime/') for name in update))
        prepared = {name for name in BASE_MEMBERS if name.startswith(('assets/', 'profiles/'))}
        self.assertEqual({name for name in full if name.startswith(('assets/', 'profiles/'))}, prepared)
        self.assertTrue(all(full[name] == self.resources[name] for name in prepared))

    def test_both_packages_require_the_complete_matching_engine(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        self.assertEqual({name for name in update if name.startswith('engine/')}, PACKAGE.ENGINE)
        self.assertTrue(all(full[name] == update[name] == self.engine[name] for name in PACKAGE.ENGINE))
        for name in PACKAGE.BOOTSTRAP_ENGINE:
            missing = self.engine.copy()
            missing.pop(name)
            with self.subTest(missing=name), self.assertRaises(ValueError):
                PACKAGE.assemble_payload(self.resources, self.current, missing)
        unexpected = self.engine | {'engine/tests/test_bootstrap.py': b'do not ship'}
        with self.assertRaises(ValueError):
            PACKAGE.assemble_payload(self.resources, self.current, unexpected)

    def test_rejects_runtime_download_cache_settings_and_development_files(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        invalid = ['runtime/python.exe', 'RUNTIME/python.exe', '.runtime-cache/python.zip',
                   'Assist.ini', 'settings.ini', 'Toolbox.ini', 'assets/cache.pyc',
                   'assets/gamemdAres.exe', 'source/game_runtime/runtime.cpp']
        for name in invalid:
            with self.subTest(name=name), self.assertRaises(ValueError):
                PACKAGE.verify_product_members(full | {name: b'bad'}, full=True)
        with self.assertRaises(ValueError):
            PACKAGE.verify_product_members(update | {'profiles/user-123/profile.ini': b'bad'}, full=False)

    def test_rejects_unsafe_paths_and_case_collisions(self):
        full, _ = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        invalid = ['../escape', '/absolute', 'C:/absolute', 'assets\\escape', 'assets/../escape',
                   'assets//double', 'assets/./dot', 'assets/bad\nname']
        for name in invalid:
            with self.subTest(name=name), self.assertRaises(ValueError):
                PACKAGE.verify_product_members(full | {name: b'bad'}, full=True)
        with self.assertRaises(ValueError):
            PACKAGE.verify_product_members(full | {'ra2toolbox.exe': b'duplicate'}, full=True)

    def test_rejects_missing_prepared_resources_or_native_programs(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        for name in PACKAGE.REQUIRED | set(PACKAGE.BINARIES) | {'profiles/capitaine/profile.ini'}:
            incomplete = full.copy()
            incomplete.pop(name)
            with self.subTest(missing=name), self.assertRaises(ValueError):
                PACKAGE.verify_product_members(incomplete, full=True)
        for name in PACKAGE.BINARIES:
            incomplete = update.copy()
            incomplete.pop(name)
            with self.subTest(update_missing=name), self.assertRaises(ValueError):
                PACKAGE.verify_product_members(incomplete, full=False)

    def test_dependency_manifest_pins_public_https_size_hash_and_versions(self):
        valid = public_manifest_fixture()
        self.assertEqual(PACKAGE.verify_dependency_manifest(json.dumps(valid)), valid)
        changes = [('url', 'http://www.python.org/python.zip'),
                   ('url', 'https://www.python.org.attacker.invalid/python.zip'),
                   ('url', 'https://user:pass@www.python.org/python.zip'),
                   ('url', valid['python']['url'] + '?alternate=1'),
                   ('sha256', ''), ('sha256', 'g' * 64), ('bytes', -1), ('bytes', '1000')]
        for key, value in changes:
            bad = copy.deepcopy(valid)
            bad['python'][key] = value
            with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                PACKAGE.verify_dependency_manifest(json.dumps(bad))
        bad = copy.deepcopy(valid)
        bad['packages'][0]['version'] = 'unvalidated-latest'
        with self.assertRaises(ValueError):
            PACKAGE.verify_dependency_manifest(json.dumps(bad))

    def test_zip_roundtrip_and_update_overlay_preserve_existing_runtime_and_settings(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        with tempfile.TemporaryDirectory(prefix='ra2-slim-policy-') as temp:
            folder = Path(temp)
            full_result = PACKAGE.write_zip(folder / 'Full.zip', full, full=True)
            update_result = PACKAGE.write_zip(folder / 'Update.zip', update, full=False)
            self.assertEqual(full_result['sha256'], digest(folder / 'Full.zip'))
            self.assertEqual(update_result['sha256'], digest(folder / 'Update.zip'))
            self.assertTrue(all(full_result['files'][name] == row
                                for name, row in update_result['files'].items()))
            installed = folder / 'installed'
            preserved = {'runtime/python.exe': b'existing runtime', 'Assist.ini': b'existing settings',
                         'settings.ini': b'existing cursor settings', 'Toolbox.ini': b'existing console settings',
                         'profiles/user-custom/original.sha': b'user cursor', 'cache/user.dat': b'user cache'}
            for name, data in preserved.items():
                path = installed / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            with zipfile.ZipFile(folder / 'Update.zip') as archive:
                archive.extractall(installed)
            self.assertTrue(all((installed / name).read_bytes() == data for name, data in preserved.items()))
            self.assertTrue(all((installed / name).read_bytes() == data for name, data in update.items()))


def main():
    global BASE_MEMBERS, BASE_SUMMARY
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', type=Path, required=True)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if digest(args.base) != PACKAGE.BASE_SHA:
        raise SystemExit('Published base archive SHA256 differs')
    with zipfile.ZipFile(args.base) as archive:
        infos = archive.infolist()
        BASE_MEMBERS = [item.filename for item in infos]
        if len(BASE_MEMBERS) != len(set(BASE_MEMBERS)):
            raise SystemExit('Duplicate base members')
        retained = [item for item in infos if not item.filename.startswith('runtime/')]
        size_without_runtime = 22 + sum(item.compress_size + 76 + 2 * len(item.filename.encode('utf-8'))
                                       + 2 * len(item.extra) + len(item.comment) for item in retained)
        BASE_SUMMARY = {'name': args.base.name, 'sha256': PACKAGE.BASE_SHA,
                        'archive_bytes': args.base.stat().st_size, 'members': len(infos),
                        'retained_existing_members': len(retained),
                        'removed_runtime_members': len(infos) - len(retained),
                        'same_bytes_zip_estimate_without_runtime': size_without_runtime,
                        'estimate_excludes_new_binary_and_bootstrap_differences': True}
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SlimPackagePolicyTests)
    names = [test.id().rsplit('.', 1)[-1] for test in suite]
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {'schema_version': 1, 'suite': 'performance_package', 'passed': result.wasSuccessful(),
              'tests_run': result.testsRun, 'tests': names, 'base': BASE_SUMMARY,
              'test_payloads': 'small synthetic bytes; complete member set from SHA256-verified published base',
              'final_release_zip_validation': 'performed separately by package_release.py after frozen build',
              'windows_executed': False, 'game_tested': False, 'multiplayer_tested': False,
              'source_bindings': [{'path': path.relative_to(ROOT).as_posix(), 'sha256': digest(path)}
                                  for path in (Path(__file__).resolve(), Path(__file__).with_name('package_release.py'))]}
    output = json.dumps(report, ensure_ascii=False, indent=2) + '\n'
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(output, encoding='utf-8')
    print(output, end='')
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
