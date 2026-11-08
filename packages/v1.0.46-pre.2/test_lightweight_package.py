#!/usr/bin/env python3
"""Exercise the actual package policy without generated cursor caches or a Windows run.

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
        self.current.update({'README.txt': b'1.0.46-pre.2 fixture', 'Start.cmd': b'@echo off\r\n'})
        self.current.update({name: ('current license fixture: ' + name).encode() for name in (
            'licenses/LICENSE-GPL.txt', 'licenses/THIRD_PARTY_NOTICES.md', 'licenses/xbrz-License.txt',
            'licenses/WebView2/LICENSE.txt', 'licenses/WebView2/NOTICE.txt')})
        self.engine = {name: ('new engine fixture: ' + name).encode() for name in PACKAGE.ENGINE}

    def test_keeps_slim_base_and_original_cursor_inputs(self):
        omitted = {name for name in BASE_MEMBERS if name.startswith('runtime/')}
        self.assertEqual(len(omitted), 0)
        self.assertTrue(all(not PACKAGE.include_base_resource(name) for name in omitted))
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        self.assertFalse(any(name.casefold().startswith('runtime/') for name in full))
        self.assertFalse(any(name.casefold().startswith('runtime/') for name in update))
        original = {name for name in BASE_MEMBERS if name.startswith(('assets/', 'profiles/'))
                    and not PACKAGE.generated_resource(name)}
        self.assertEqual({name for name in full if name.startswith(('assets/', 'profiles/'))}, original)
        self.assertTrue(all(full[name] == self.resources[name] for name in original))
        self.assertTrue(PACKAGE.REQUIRED <= set(full))

    def test_removes_every_generated_timeline_and_preview_from_full_and_update(self):
        generated = {name for name in BASE_MEMBERS if PACKAGE.generated_resource(name)}
        self.assertEqual(len(generated), 392)
        self.assertEqual(sum(name.endswith('.dis') for name in generated), 388)
        self.assertEqual(sum(name.endswith('/preview.html') for name in generated), 4)
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        self.assertEqual(len(full), 53)
        self.assertEqual(len(update), 18)
        self.assertFalse(generated & set(full))
        self.assertFalse(any(PACKAGE.generated_resource(name) for name in full))
        self.assertFalse(any(PACKAGE.generated_resource(name) for name in update))

    def test_rejects_generated_resources_under_every_allowed_root(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        invalid = ['assets/timelines/0-1.dis', 'assets/TIMELINES/cache.bin',
                   'profiles/bibata-amber/timelines/0-1.dis', 'assets/orphan.DIS',
                   'assets/preview.html', 'profiles/bibata-amber/PREVIEW.HTML',
                   'engine/preview.html', 'engine/timelines/0-1.dis',
                   'assets/foo.tl15', 'assets/.preprocess/progress.ini',
                   'profiles/bibata-ice/cache/generated.bin']
        for name in invalid:
            with self.subTest(name=name):
                self.assertFalse(PACKAGE.include_base_resource(name))
                with self.assertRaises(ValueError):
                    PACKAGE.verify_product_members(full | {name: b'generated'}, full=True)
                with self.assertRaises(ValueError):
                    PACKAGE.verify_product_members(update | {name: b'generated'}, full=False)

    def test_both_packages_require_the_complete_matching_engine(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        self.assertEqual({name for name in update if name.startswith('engine/')}, PACKAGE.ENGINE)
        self.assertTrue(all(full[name] == update[name] == self.engine[name] for name in PACKAGE.ENGINE))
        # Current engine bytes intentionally differ from the pinned base;
        # all seven current files must replace that older set coherently.
        self.assertTrue(all(self.engine[name] != self.resources[name] for name in PACKAGE.ENGINE))
        for name in PACKAGE.ENGINE:
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

    def test_rejects_missing_original_inputs_or_native_programs(self):
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

    def test_zip_roundtrip_and_overlays_preserve_existing_runtime_caches_and_settings(self):
        full, update = PACKAGE.assemble_payload(self.resources, self.current, self.engine)
        with tempfile.TemporaryDirectory(prefix='ra2-slim-policy-') as temp:
            folder = Path(temp)
            full_result = PACKAGE.write_zip(folder / 'Full.zip', full, full=True)
            update_result = PACKAGE.write_zip(folder / 'Update.zip', update, full=False)
            self.assertEqual(full_result['sha256'], digest(folder / 'Full.zip'))
            self.assertEqual(update_result['sha256'], digest(folder / 'Update.zip'))
            self.assertTrue(all(full_result['files'][name] == row
                                for name, row in update_result['files'].items()))
            preserved = {'runtime/python.exe': b'existing runtime', 'Assist.ini': b'existing settings',
                         'settings.ini': b'existing cursor settings', 'Toolbox.ini': b'existing console settings',
                         'profiles/user-custom/original.sha': b'user cursor', 'cache/user.dat': b'user cache',
                         'assets/timelines/0-1.dis': b'existing default interpolation',
                         'profiles/bibata-amber/timelines/0-1.dis': b'existing theme interpolation',
                         'assets/preview.html': b'existing default preview',
                         'profiles/bibata-amber/preview.html': b'existing theme preview',
                         'cache/timelines/0-1.tl15': b'existing native playback cache'}
            for package, contents in [('Update', update), ('Full', full)]:
                installed = folder / package
                for name, data in preserved.items():
                    path = installed / name
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_bytes(data)
                with zipfile.ZipFile(folder / (package + '.zip')) as archive:
                    archive.extractall(installed)
                with self.subTest(package=package):
                    self.assertTrue(all((installed / name).read_bytes() == data for name, data in preserved.items()))
                    self.assertTrue(all((installed / name).read_bytes() == data for name, data in contents.items()))


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
        runtime = [item for item in infos if item.filename.startswith('runtime/')]
        generated = [item for item in infos if PACKAGE.generated_resource(item.filename)]
        retained = [item for item in infos if not item.filename.startswith('runtime/')
                    and not PACKAGE.generated_resource(item.filename)]
        size_without_generated = 22 + sum(item.compress_size + 76 + 2 * len(item.filename.encode('utf-8'))
                                       + 2 * len(item.extra) + len(item.comment) for item in retained)
        BASE_SUMMARY = {'name': args.base.name, 'sha256': PACKAGE.BASE_SHA,
                        'archive_bytes': args.base.stat().st_size, 'members': len(infos),
                        'retained_existing_members': len(retained),
                        'removed_runtime_members': len(runtime),
                        'removed_generated_members': len(generated),
                        'removed_generated_compressed_payload_bytes': sum(item.compress_size for item in generated),
                        'same_bytes_zip_estimate_without_generated': size_without_generated,
                        'estimate_excludes_new_binary_and_bootstrap_differences': True}
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(SlimPackagePolicyTests)
    names = [test.id().rsplit('.', 1)[-1] for test in suite]
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    report = {'schema_version': 1, 'suite': 'lightweight_package', 'passed': result.wasSuccessful(),
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
