#!/usr/bin/env python3
"""Package the frozen 1.0.46.4 UI revision and its verified paired Windows outputs.

Original resources come only from the SHA256-pinned published 1.0.46.3 Full
archive and manifest. The current engine must be byte-identical to that base;
this UI revision records five freshly executed UI checks, not inherited
performance tests. This script does not publish or execute Windows programs.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
import stat
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('ra2_146_package_policy', HERE / 'package_release.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
require, sha, encoded = POLICY.require, POLICY.sha, POLICY.encoded

VERSION, FILE_VERSION = '1.0.46', '1.0.46.4'
BASE_NAME = 'RA2Toolbox-1.0.46-x64-Full.zip'
BASE_BYTES = 4427616
BASE_SHA = 'c5e1ee4d2bcdb727e35e5c81ca95119db6d5f20edc4f65394c9fbd013dbd7c78'
BASE_MANIFEST_BYTES = 261267
BASE_MANIFEST_SHA = '888d2334d5c00a02f2f6859b04e667fdfd8729bd6ebe753d6765ef00e9d42920'
BASE_COMMIT = 'cecb87411d75cbc739f243710f8c85b53bb3d355'
CHECK_BINDINGS = {
    'console_core': ({'source/suite/tests/console_store_test.mjs'},
                     {'source/suite/frontend/app.js'}),
    'console_debug': ({'source/suite/tests/console_debug_test.mjs'},
                      {'source/suite/frontend/app.js'}),
    'console_browser': ({'source/suite/tests/console_store_test.mjs'},
                        {'source/suite/frontend/app.js', 'source/suite/frontend/app.css',
                         'source/suite/frontend/index.html'}),
    'console_model': ({'source/suite/tests/console_model_test.cpp'},
                      {'source/suite/source/console_model.h'}),
    'console_metadata': ({'source/suite/tests/console_metadata.cpp'},
                         {'source/assist/source/main.cpp'}),
}


def pinned_base(archive_path, manifest_path):
    """Verify the real published archive, its identity, and every member."""
    archive_bytes, manifest_bytes = archive_path.read_bytes(), manifest_path.read_bytes()
    require(len(archive_bytes) == BASE_BYTES and sha(archive_bytes) == BASE_SHA,
            'Published 1.0.46.3 Full archive differs from its pinned digest')
    require(len(manifest_bytes) == BASE_MANIFEST_BYTES and sha(manifest_bytes) == BASE_MANIFEST_SHA,
            'Published 1.0.46.3 manifest differs from its pinned digest')
    manifest = json.loads(manifest_bytes)
    require((manifest.get('version'), manifest.get('file_version'), manifest.get('source_commit'))
            == (VERSION, '1.0.46.3', BASE_COMMIT), 'Published resource base identity differs')
    matches = [row for row in manifest['packages'] if row['name'] == BASE_NAME]
    require(len(matches) == 1, 'Expected exactly one Full archive in the base manifest')
    record = matches[0]
    require((record['bytes'], record['sha256']) == (BASE_BYTES, BASE_SHA),
            'Published base archive membership differs')
    resources = {}
    with zipfile.ZipFile(io.BytesIO(archive_bytes)) as archive:
        require(archive.testzip() is None, 'Published base ZIP CRC failed')
        items = archive.infolist()
        names = [item.filename for item in items]
        require(len(names) == 53 and set(names) == set(record['files']),
                'Published base ZIP member set differs')
        POLICY.verify_product_members(names, full=True)
        for item in items:
            require(not item.is_dir() and not stat.S_ISLNK(item.external_attr >> 16),
                    'Directory or symbolic link in published base')
            data = archive.read(item)
            require(record['files'][item.filename] == {'bytes': len(data), 'sha256': sha(data)},
                    'Published base member digest differs: ' + item.filename)
            if POLICY.include_base_resource(item.filename):
                resources[item.filename] = data
    return resources, record


def success_log(name, text):
    """A successful exit alone must not label a core-only run as browser QA."""
    if re.search(r'^FAIL\b', text, re.M):
        return False
    patterns = {
        'console_core': r'\b[1-9]\d* console model tests passed\b',
        'console_debug': r'\b[1-9]\d* debug console core scenarios passed\b',
        'console_browser': r'^PASS browser fixture:',
        'console_model': r'\bconsole_model: [1-9]\d* checks passed\b',
    }
    if name in patterns:
        return re.search(patterns[name], text, re.M) is not None
    candidates = [text, *text.splitlines()]
    for candidate in candidates:
        try:
            value = json.loads(candidate)
        except (json.JSONDecodeError, TypeError):
            continue
        if not isinstance(value, dict) or value.get('status', 'PASS') != 'PASS':
            continue
        result = value.get('result', value)
        if (isinstance(result, dict) and result.get('assertions_enabled') is True
                and type(result.get('checks')) is int and result['checks'] > 0
                and result.get('groups') == 15):
            return True
    return False


def validate_ui_evidence(validation, receipt_path, commit, source_inputs, tracked):
    require(validation.get('schema') == 'ra2.ui-revision-validation.v1', 'Wrong UI evidence schema')
    require((validation.get('version'), validation.get('file_version'), validation.get('commit'))
            == (VERSION, FILE_VERSION, commit), 'UI evidence belongs to another source revision')
    require(validation.get('checks_reexecuted') is True
            and validation.get('required_checks_passed') is True, 'UI checks are incomplete')
    for key in ('windows_executed', 'game_tested', 'multiplayer_tested'):
        require(validation.get(key) is False, 'UI evidence overclaims Windows/game execution')
    checks = validation.get('checks')
    require(isinstance(checks, list) and len(checks) == len(CHECK_BINDINGS)
            and all(isinstance(row, dict) for row in checks)
            and {row.get('name') for row in checks} == set(CHECK_BINDINGS),
            'Require exactly the five current UI checks, with no inherited performance results')
    log_bindings = {}
    for check in checks:
        name = check['name']
        require(type(check.get('exit_code')) is int and check['exit_code'] == 0
                and check.get('executed_this_revision') is True,
                'UI check did not execute successfully: ' + name)
        command = check.get('command')
        require(isinstance(command, list) and command
                and all(isinstance(item, str) and item for item in command),
                'Missing executed UI command: ' + name)
        raw_log = check.get('log_path')
        require(isinstance(raw_log, str) and raw_log, 'Missing UI log path: ' + name)
        log_path = Path(raw_log)
        if not log_path.is_absolute():
            log_path = receipt_path.parent / log_path
        log_path = log_path.resolve()
        log_bytes = log_path.read_bytes()
        require(log_bytes and sha(log_bytes) == check.get('log_sha256'),
                'UI log is empty or changed: ' + name)
        require(success_log(name, log_bytes.decode('utf-8', errors='replace')),
                'UI log lacks the executed check success marker: ' + name)
        log_bindings[str(log_path)] = {'bytes': len(log_bytes), 'sha256': sha(log_bytes)}
        for key, required in zip(('test_source_hashes', 'production_source_hashes'), CHECK_BINDINGS[name]):
            bindings = check.get(key)
            require(isinstance(bindings, dict) and required <= set(bindings),
                    'Incomplete ' + key + ': ' + name)
            for path, expected_sha in bindings.items():
                relative = POLICY.safe(path)
                require(relative.parts[0] == 'source' and path in source_inputs,
                        'UI evidence input is outside the completed build source: ' + path)
                data = tracked(path)
                require(expected_sha == source_inputs[path]['sha256'] == sha(data),
                        'Tested UI source differs from compiled source: ' + path)
    return log_bindings


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'binaries', 'provenance', 'validation', 'base-full',
                 'base-manifest', 'readme', 'out', 'source-sync'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    args = parser.parse_args()
    repo, binaries, out = args.repo.resolve(), args.binaries.resolve(), args.out.resolve()
    source = repo / 'source'
    require(HERE == repo / 'packages/v1.0.46', 'Packager must belong to the selected checkout')

    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])

    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'Wrong checkout HEAD')
    tree = {}
    for row in git('ls-tree', '-r', '-z', '--full-tree', 'HEAD').split(b'\0'):
        if row:
            prefix, raw_name = row.split(b'\t', 1)
            mode, kind, digest = prefix.decode().split()
            tree[raw_name.decode()] = (mode, kind, digest)
    tracked_inputs = {}

    def tracked(name):
        POLICY.safe(name)
        require(name in tree and tree[name][0] in {'100644', '100755'} and tree[name][1] == 'blob',
                'Expected a regular tracked input: ' + name)
        path = repo / name
        require(not path.is_symlink(), 'Symbolic link is not a frozen source input: ' + name)
        data = path.read_bytes()
        actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        require(actual == tree[name][2], 'Working input differs from frozen commit: ' + name)
        tracked_inputs[name] = {'bytes': len(data), 'sha256': sha(data)}
        return data

    for name in ('packages/v1.0.46/package_ui_revision.py', 'packages/v1.0.46/package_release.py',
                 'packages/v1.0.43/build_release.py'):
        tracked(name)
    require(args.readme.resolve() == repo / 'packages/v1.0.46/README-ui.txt', 'Wrong product README')
    readme = tracked('packages/v1.0.46/README-ui.txt')
    require(FILE_VERSION.encode() in readme, 'Product README omits the UI revision file version')

    sync_bytes = args.source_sync.read_bytes()
    sync = json.loads(sync_bytes)
    root_tree = git('rev-parse', 'HEAD^{tree}').decode().strip()
    require(sync.get('local_commit') == args.expected_commit and sync.get('root_tree_sha') == root_tree
            and sync.get('remote_root_tree_sha') == root_tree and sync.get('remote_tree_verified') is True
            and sync.get('blob_digests_verified') is True, 'Published source does not match the frozen tree')
    public_commit = sync.get('remote_commit', '')
    require(re.fullmatch(r'[0-9a-f]{40}', public_commit) is not None, 'Missing published source commit')

    provenance_bytes = args.provenance.read_bytes()
    provenance = json.loads(provenance_bytes)
    require(provenance.get('commit') == args.expected_commit
            and type(provenance.get('build_returncode')) is int and provenance['build_returncode'] == 0
            and type(provenance.get('static_pe_verification_returncode')) is int
            and provenance['static_pe_verification_returncode'] == 0
            and provenance.get('source_inputs_unchanged_after_build') is True
            and provenance.get('git_head_verified_at_finish') is True, 'Build provenance is incomplete')
    require(provenance.get('platform') == 'linux' and provenance.get('zig_version') == '0.13.0'
            and provenance.get('webview2_sdk') == '1.0.4314.35', 'Wrong pinned build toolchain')
    for key in ('controlled_windows_checks', 'runtime_tested', 'game_tested', 'multiplayer_tested'):
        require(provenance.get(key) is False, 'Cross-build provenance overclaims execution')
    built_artifacts = POLICY.verified_build_artifacts(provenance, binaries)
    rows = provenance.get('source_inputs', [])
    source_inputs = {row['path']: row for row in rows}
    expected_paths = {name for name in tree if name.startswith('source/')}
    require(len(rows) == len(source_inputs) and set(source_inputs) == expected_paths,
            'Build provenance must bind every source blob exactly once')
    for name, row in source_inputs.items():
        data = tracked(name)
        require(row['git_blob_sha'] == tree[name][2]
                and row['bytes'] == len(data) and row['sha256'] == sha(data),
                'Compiled source changed: ' + name)
    source_tree = git('rev-parse', 'HEAD:source').decode().strip()
    require(source_tree == provenance.get('source_tree_sha'), 'Compiled source tree differs')

    sys.path.insert(0, str(source / 'tools'))
    from verify_windows_outputs import verify, read_release_version
    file_version, version = read_release_version(source / 'shared/source/release_version.h')
    require((version, file_version) == (VERSION, FILE_VERSION), 'Wrong UI revision version')
    identity_bytes = tracked('source/BUILD_SOURCE_MANIFEST.json')
    identity = json.loads(identity_bytes)
    require(identity.get('version') == VERSION and identity.get('build_protocol') == 101,
            'Source identity or Assist protocol differs')
    require((binaries / 'BUILD_SOURCE_MANIFEST.json').read_bytes() == identity_bytes,
            'Built source identity differs')
    target = json.loads((binaries / 'BUILD_TARGET.json').read_bytes())
    require(tuple(target.get(key) for key in ('host', 'runtime', 'broker', 'assist_protocol',
                                             'cursor_protocol', 'runtime_protocol', 'console_ui'))
            == ('x64', 'x86', 'x86', 101, 155, 4, 'webview2'), 'Wrong completed build profile')
    actual, errors = verify(binaries, host_arch='x64', webview2=True)
    require(not errors, 'PE validation failed: ' + '; '.join(errors))
    verified = dict(actual)
    verified['WebView2Loader.dll'] = actual['RA2Toolbox.exe']['webview2_loader']
    current = {name: (binaries / name).read_bytes() for name in POLICY.BINARIES}
    for name, data in current.items():
        POLICY.verify_artifact_binding(name, built_artifacts[name], verified[name], data)

    validation_bytes = args.validation.read_bytes()
    validation = json.loads(validation_bytes)
    log_bindings = validate_ui_evidence(validation, args.validation.resolve(), args.expected_commit,
                                       source_inputs, tracked)
    resources, base_record = pinned_base(args.base_full, args.base_manifest)
    for name in ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe'):
        require(sha(current[name]) != base_record['files'][name]['sha256'],
                'A previous product program was reused: ' + name)
    POLICY.source_engine_members(source / 'engine')
    engine = {name: tracked('source/' + name) for name in sorted(POLICY.ENGINE)}
    require(all(engine[name] == resources[name] for name in engine),
            'This UI-only revision requires the validated 1.0.46.3 engine unchanged')
    dependency_manifest = POLICY.verify_dependency_manifest(engine['engine/runtime-dependencies.json'])
    for filename in ('LICENSE.txt', 'NOTICE.txt'):
        name = 'licenses/WebView2/' + filename
        data = (binaries / name).read_bytes()
        require(sha(data) == verified['WebView2Loader.dll']['licenses'][name],
                'Verified WebView2 license changed: ' + name)
        current[name] = data
    for name, path in {'licenses/LICENSE-GPL.txt': 'LICENSE-GPL',
                       'licenses/THIRD_PARTY_NOTICES.md': 'THIRD_PARTY_NOTICES.md',
                       'licenses/xbrz-License.txt': 'source/third_party/xbrz/License.txt'}.items():
        current[name] = tracked(path)
    current['README.txt'] = readme
    current['Start.cmd'] = b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
    full_contents, update_contents = POLICY.assemble_payload(resources, current, engine)
    require(len(full_contents) == 53 and len(update_contents) == 18, 'UI package membership differs')
    original_names = {name for name in resources if name.startswith(('assets/', 'profiles/'))}
    require(original_names == {name for name in full_contents if name.startswith(('assets/', 'profiles/'))}
            and all(full_contents[name] == resources[name] for name in original_names),
            'Original cursor resources changed')

    out.mkdir(parents=True, exist_ok=True)
    allowed = {f'RA2Toolbox-{VERSION}-x64-Full.zip', f'RA2Toolbox-{VERSION}-x64-Update.zip',
               'release-manifest.json', 'SHA256SUMS.txt'}
    require(all(path.name in allowed and path.is_file() and not path.is_symlink()
                for path in out.iterdir()), 'Unexpected or symbolic output member')
    update = POLICY.write_zip(out / f'RA2Toolbox-{VERSION}-x64-Update.zip', update_contents, full=False)
    full = POLICY.write_zip(out / f'RA2Toolbox-{VERSION}-x64-Full.zip', full_contents, full=True)
    require(all(full['files'][name] == update['files'][name] for name in update_contents),
            'Full and Update paired members differ')
    reused = {name: base_record['files'][name] for name in sorted(resources)
              if name not in update_contents}
    report = {
        'schema': 'ra2.ui-revision-release.v1', 'version': VERSION, 'file_version': FILE_VERSION,
        'source_commit': public_commit, 'local_build_commit': args.expected_commit,
        'source_tree_sha': source_tree, 'complete_git_tree_sha': root_tree,
        'source_identity': sync, 'source_sync_receipt_sha256': sha(sync_bytes),
        'build_method': 'Zig 0.13.0 Windows-target cross compilation on Linux',
        'build_provenance_sha256': sha(provenance_bytes), 'source_files_verified': len(source_inputs),
        'assist_protocol': 101, 'cursor_protocol': 155, 'runtime_protocol': 4,
        'assist_shared_bytes': actual['GameRuntime.dll']['assist_shared_bytes'],
        'actual_pe_verified': actual, 'validation': validation,
        'validation_evidence_sha256': sha(validation_bytes), 'verified_ui_logs': log_bindings,
        'historical_performance_checks_reexecuted': False,
        'controlled_windows_checks': False, 'current_release_binaries_executed_on_windows': False,
        'full_game_tested': False, 'multiplayer_tested': False,
        'prerelease': False, 'make_latest': True, 'revises_same_release': 'v1.0.46',
        'resources': {'source_release': 'v1.0.46', 'source_file_version': '1.0.46.3',
                      'source_commit': BASE_COMMIT, 'archive': BASE_NAME,
                      'bytes': BASE_BYTES, 'sha256': BASE_SHA,
                      'manifest_sha256': BASE_MANIFEST_SHA, 'verified_members': 53,
                      'reused_members': reused, 'original_cursor_members_preserved': len(original_names),
                      'old_product_programs_reused': False, 'generated_cursor_resources_bundled': False},
        'optional_runtime': {'bundled': False, 'native_startup_requires_download': False,
                             'existing_runtime_and_cache_preserved_by_update': True,
                             'engine_unchanged_from_base': True,
                             'engine_source': 'current_frozen_source',
                             'manifest_path': 'engine/runtime-dependencies.json',
                             'manifest_sha256': sha(engine['engine/runtime-dependencies.json']),
                             'dependencies': dependency_manifest},
        'packages': [full, update],
    }
    (out / 'release-manifest.json').write_bytes(encoded(report))
    sums = [out / full['name'], out / update['name'], out / 'release-manifest.json']
    (out / 'SHA256SUMS.txt').write_text(''.join(sha(path.read_bytes()) + '  ' + path.name + '\n'
                                             for path in sums), encoding='ascii')
    require({path.name for path in out.iterdir()} == allowed, 'Distribution member set differs')
    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'HEAD changed during packaging')
    for name, record in list(tracked_inputs.items()):
        data = tracked(name)
        require(record == {'bytes': len(data), 'sha256': sha(data)}, 'Source changed during packaging: ' + name)
    for name in POLICY.BINARIES:
        data = (binaries / name).read_bytes()
        require(built_artifacts[name] == {'bytes': len(data), 'sha256': sha(data)},
                'Built product changed during packaging: ' + name)
    for path, record in log_bindings.items():
        data = Path(path).read_bytes()
        require(record == {'bytes': len(data), 'sha256': sha(data)}, 'UI log changed during packaging')
    for path, before in ((args.provenance, provenance_bytes), (args.validation, validation_bytes),
                         (args.source_sync, sync_bytes)):
        require(path.read_bytes() == before, 'Evidence changed during packaging: ' + str(path))
    subprocess.run(['sha256sum', '-c', 'SHA256SUMS.txt'], cwd=out, check=True)
    print(json.dumps({'version': VERSION, 'file_version': FILE_VERSION, 'source_commit': public_commit,
                      'ui_checks': list(CHECK_BINDINGS), 'windows_executed': False,
                      'original_cursor_resources_preserved': len(original_names),
                      'packages': [{key: value for key, value in row.items() if key != 'files'}
                                   for row in (full, update)], 'output': str(out)}, indent=2))


if __name__ == '__main__':
    raise SystemExit(main())
