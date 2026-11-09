#!/usr/bin/env python3
"""Package the frozen 1.0.47-pre.1 optimization build; never build or publish.

Use the verified 1.0.46.4 originals, themes, licenses and seven-file engine.
Only prepare_runtime.py may change, with a seventh current dependency check.
Bind all current outputs and executed checks to one frozen source tree.
No optional Python runtime or generated cursor cache ships.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import tempfile
import zipfile

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('ra2_146_product_policy',
                                             HERE.parent / 'v1.0.46/package_release.py')
POLICY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(POLICY)
require, sha, encoded = POLICY.require, POLICY.sha, POLICY.encoded

VERSION, FILE_VERSION = '1.0.47-pre.1', '1.0.47.1'
BASE_NAME = 'RA2Toolbox-1.0.46-x64-Full.zip'
BASE_BYTES = 4428436
BASE_SHA = 'd8d5d871e0358579b49c1a2dedbfca48c601ea22119bf897e18593ae7233533c'
BASE_MANIFEST_BYTES = 61217
BASE_MANIFEST_SHA = '1d30a70514dcba77b26a330db130150850fe24c6fd962c513212542db10dc26b'
BASE_COMMIT = 'f452d1cb95c57f465356b55a3d7b83fb7403a70a'
APPROVED_ENGINE_CHANGES = frozenset({'engine/prepare_runtime.py'})
CHECK_BINDINGS = {
    'missile_allocator': (
        {'source/assist/tests/missile_allocation_equivalence.cpp'},
        {'source/assist/source/missile_compute.h', 'source/assist/source/missile_bridge.h',
         'source/assist/source/main.cpp'}),
    'snapshot_reuse': (
        {'source/assist/tests/world_frame_reuse.cpp', 'source/assist/tests/world_frame_reuse.py'},
        {'source/shared/source/world_frame.h', 'source/shared/source/world_frame_pool.h',
         'source/shared/source/world_frame_transport.h'}),
    'tech_capture': (
        {'source/assist/tests/tech_enhancement_capture.cpp',
         'source/assist/tests/tech_enhancement_capture.py'},
        {'source/assist/source/tech_ui_breakpoint_bridge.h',
         'source/assist/source/tech_enhancement.h', 'source/assist/source/tech_parameter_lookup.h'}),
    'voxel_recovery': (
        {'source/assist/tests/voxel_part_capture.cpp', 'source/assist/tests/voxel_part_capture.py'},
        {'source/external_reader/source/camera_assets.h', 'source/external_reader/source/radar_scene.h'}),
    'idle_aircraft_display': (
        {'source/assist/tests/idle_aircraft_display.cpp'},
        {'source/external_reader/source/idle_aircraft.h',
         'source/external_reader/source/tactical_sampler.h',
         'source/assist/source/aircraft_indicator.h', 'source/assist/source/main.cpp'}),
    'terrain_reuse': (
        {'source/assist/tests/terrain_storage_pool.cpp'},
        {'source/external_reader/source/radar_map.h'}),
}
DEPENDENCY_BINDINGS = (
    {'source/tools/test_dependency_probes.py'},
    {'source/engine/prepare_runtime.py', 'source/engine/runtime-dependencies.json'},
)
PACKAGE_INPUTS = (
    'packages/v1.0.43/build_release.py', 'packages/v1.0.46/package_release.py',
    'packages/v1.0.47/package_release.py', 'packages/v1.0.47/test_package_release.py',
    'packages/v1.0.47/README.txt', 'packages/v1.0.47/PACKAGING.md',
)


def pinned_base(archive_path, manifest_path):
    """Check the pinned public archive and manifest before using any resource."""
    archive_bytes, manifest_bytes = archive_path.read_bytes(), manifest_path.read_bytes()
    require(len(archive_bytes) == BASE_BYTES and sha(archive_bytes) == BASE_SHA,
            'Published 1.0.46.4 archive differs from its pinned digest')
    require(len(manifest_bytes) == BASE_MANIFEST_BYTES and sha(manifest_bytes) == BASE_MANIFEST_SHA,
            'Published 1.0.46.4 manifest differs from its pinned digest')
    manifest = json.loads(manifest_bytes)
    require((manifest.get('version'), manifest.get('file_version'), manifest.get('source_commit'))
            == ('1.0.46', '1.0.46.4', BASE_COMMIT), 'Published resource identity differs')
    matches = [row for row in manifest['packages'] if row['name'] == BASE_NAME]
    require(len(matches) == 1, 'Expected one published Full package')
    record = matches[0]
    require((record['bytes'], record['sha256']) == (BASE_BYTES, BASE_SHA),
            'Published archive manifest membership differs')
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
    return resources, record, manifest


def success_log(name, text):
    """Recognize existing test output; never append or synthesize a PASS marker."""
    objects, texts = [], [text]

    def visit(value):
        if isinstance(value, dict):
            objects.append(value)
            for key, child in value.items():
                if key in {'stdout', 'stderr'} and isinstance(child, str):
                    texts.append(child)
                elif isinstance(child, (dict, list)):
                    visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    for candidate in (text, *text.splitlines()):
        try:
            visit(json.loads(candidate))
        except (json.JSONDecodeError, TypeError):
            pass
    body = '\n'.join(texts)
    if re.search(r'^FAIL\b', body, re.M):
        return False
    if name == 'missile_allocator':
        return any(type(row.get('equivalent_scenarios')) is int
                   and row['equivalent_scenarios'] >= 8063
                   and row.get('same_workspace_reused') is True for row in objects)
    if name == 'snapshot_reuse':
        return any(row.get('case') == 'seqlock_interleavings' and row.get('passed') is True
                   and type(row.get('checks')) is int and row['checks'] > 0 for row in objects)
    if name == 'tech_capture':
        return any(row.get('case') == 'summary' and row.get('passed') is True
                   and type(row.get('checks')) is int and row['checks'] > 0 for row in objects)
    if name == 'voxel_recovery':
        return (re.search(r'\brecovered_scenes=[1-9]\d* pixel_mismatches=0\b', body) is not None
                and re.search(r'\bchecks=[1-9]\d* failures=0\b', body) is not None
                and not any(row.get('expected_failure') is True for row in objects))
    if name == 'idle_aircraft_display':
        return re.search(r'^PASS paused same-source aircraft display;', body, re.M) is not None
    if name == 'terrain_reuse':
        return re.search(r'^PASS actual terrain storage:', body, re.M) is not None
    if name == 'dependency_probes':
        return any(row.get('passed') is True and type(row.get('tests_run')) is int
                   and row['tests_run'] > 0 and type(row.get('failures')) is int and row['failures'] == 0
                   and type(row.get('errors')) is int and row['errors'] == 0
                   for row in objects)
    return False


def validate_evidence(validation, receipt_path, commit, source_tree, source_inputs, tracked, engine_changes=()):
    require(validation.get('schema') == 'ra2.optimization-validation.v1', 'Wrong validation schema')
    require((validation.get('version'), validation.get('file_version'), validation.get('commit'),
             validation.get('source_tree_sha')) == (VERSION, FILE_VERSION, commit, source_tree),
            'Validation belongs to another frozen source revision')
    require(validation.get('required_checks_passed') is True, 'Required checks are incomplete')
    for key in ('windows_executed', 'game_tested', 'gpu_tested', 'multiplayer_tested'):
        require(validation.get(key) is False, 'Validation overclaims platform execution: ' + key)
    require(validation.get('engine_changes') == list(engine_changes),
            'Validation engine change plan differs from current source')
    bindings_by_check = dict(CHECK_BINDINGS)
    if engine_changes:
        bindings_by_check['dependency_probes'] = DEPENDENCY_BINDINGS
    checks = validation.get('checks')
    require(isinstance(checks, list) and len(checks) == len(bindings_by_check)
            and all(isinstance(row, dict) for row in checks)
            and {row.get('name') for row in checks} == set(bindings_by_check),
            'Require exactly the current optimization checks and any required dependency check')
    logs = {}
    for check in checks:
        name = check['name']
        require(type(check.get('exit_code')) is int and check['exit_code'] == 0
                and check.get('executed_this_revision') is True,
                'Check did not execute successfully this revision: ' + name)
        command = check.get('command')
        require(isinstance(command, list) and command
                and all(isinstance(value, str) and value for value in command),
                'Missing executed command argv: ' + name)
        raw_path = check.get('log_path')
        require(isinstance(raw_path, str) and raw_path, 'Missing execution log: ' + name)
        path = Path(raw_path)
        path = (path if path.is_absolute() else receipt_path.parent / path).resolve()
        data = path.read_bytes()
        require(data and sha(data) == check.get('log_sha256'), 'Execution log is empty or changed: ' + name)
        require(success_log(name, data.decode('utf-8', errors='replace')),
                'Execution log lacks the current check result: ' + name)
        logs[str(path)] = {'bytes': len(data), 'sha256': sha(data)}
        for key, required in zip(('test_source_hashes', 'production_source_hashes'), bindings_by_check[name]):
            bindings = check.get(key)
            require(isinstance(bindings, dict) and required <= set(bindings),
                    'Incomplete ' + key + ': ' + name)
            for path, digest in bindings.items():
                require(POLICY.safe(path).parts[0] == 'source' and path in source_inputs,
                        'Validated input is outside the completed build source: ' + path)
                require(digest == source_inputs[path]['sha256'] == sha(tracked(path)),
                        'Tested source differs from compiled source: ' + path)
    return logs


def engine_change_plan(resources, engine):
    require(set(engine) == POLICY.ENGINE, 'Engine member set differs')
    changed = sorted(name for name in engine if engine[name] != resources[name])
    require(set(changed) <= APPROVED_ENGINE_CHANGES, 'Engine change is outside the reviewed plan')
    return changed


def assemble(resources, current, engine):
    """Preserve pinned resources with only the explicit current engine change."""
    engine_change_plan(resources, engine)
    update = current | {name: data for name, data in resources.items() if name.startswith('licenses/')}
    full, update = POLICY.assemble_payload(resources, update, engine)
    require(len(full) == 53 and len(update) == 21, 'Optimization package membership differs')
    require(all(full[name] == engine.get(name, data) for name, data in resources.items()),
            'Pinned resource bytes changed')
    return full, update


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('repo', 'binaries', 'provenance', 'validation', 'base-full', 'base-manifest', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--source-sync', type=Path, help='Optional already-verified source push receipt')
    args = parser.parse_args()
    repo, binaries, out = args.repo.resolve(), args.binaries.resolve(), args.out.resolve()
    source = repo / 'source'
    require(HERE == repo / 'packages/v1.0.47', 'Packager must belong to the selected checkout')
    require(re.fullmatch(r'[0-9a-f]{40}', args.expected_commit) is not None, 'Use a full pinned commit SHA')
    require(not args.out.is_symlink() and not out.exists(), 'Output directory must not already exist')

    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])

    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'Wrong checkout HEAD')
    root_tree = git('rev-parse', 'HEAD^{tree}').decode().strip()
    source_tree = git('rev-parse', 'HEAD:source').decode().strip()
    tree, tracked_inputs = {}, {}
    for row in git('ls-tree', '-r', '-z', '--full-tree', 'HEAD').split(b'\0'):
        if row:
            prefix, name = row.split(b'\t', 1)
            tree[name.decode()] = tuple(prefix.decode().split())

    def tracked(name):
        POLICY.safe(name)
        require(name in tree and tree[name][0] in {'100644', '100755'} and tree[name][1] == 'blob',
                'Expected regular tracked input: ' + name)
        path = repo / name
        require(not path.is_symlink(), 'Symbolic source input: ' + name)
        data = path.read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        require(blob == tree[name][2], 'Working input differs from frozen commit: ' + name)
        tracked_inputs[name] = {'bytes': len(data), 'sha256': sha(data)}
        return data

    for name in PACKAGE_INPUTS:
        tracked(name)
    readme = tracked('packages/v1.0.47/README.txt')
    require(VERSION.encode() in readme and FILE_VERSION.encode() in readme, 'README version differs')
    provenance_bytes, validation_bytes = args.provenance.read_bytes(), args.validation.read_bytes()
    provenance, validation = json.loads(provenance_bytes), json.loads(validation_bytes)
    require(provenance.get('commit') == args.expected_commit
            and type(provenance.get('build_returncode')) is int and provenance['build_returncode'] == 0
            and type(provenance.get('static_pe_verification_returncode')) is int
            and provenance['static_pe_verification_returncode'] == 0
            and provenance.get('source_inputs_unchanged_after_build') is True
            and provenance.get('git_head_verified_at_finish') is True, 'Incomplete build provenance')
    require(provenance.get('platform') == 'linux' and provenance.get('zig_version') == '0.13.0'
            and provenance.get('webview2_sdk') == '1.0.4314.35', 'Wrong pinned build toolchain')
    for key in ('controlled_windows_checks', 'runtime_tested', 'game_tested', 'multiplayer_tested'):
        require(provenance.get(key) is False, 'Cross-build provenance overclaims execution')
    built = POLICY.verified_build_artifacts(provenance, binaries)
    rows = provenance.get('source_inputs', [])
    source_inputs = {row['path']: row for row in rows}
    require(len(rows) == len(source_inputs)
            and set(source_inputs) == {name for name in tree if name.startswith('source/')},
            'Build provenance must bind every source blob exactly once')
    for name, row in source_inputs.items():
        data = tracked(name)
        require(row['git_blob_sha'] == tree[name][2]
                and row['bytes'] == len(data) and row['sha256'] == sha(data), 'Compiled source changed: ' + name)
    require(source_tree == provenance.get('source_tree_sha'), 'Compiled source tree differs')
    resources, base_record, base_manifest = pinned_base(args.base_full, args.base_manifest)
    POLICY.source_engine_members(source / 'engine')
    engine = {name: tracked('source/' + name) for name in POLICY.ENGINE}
    changed_engine = engine_change_plan(resources, engine)
    log_bindings = validate_evidence(validation, args.validation.resolve(), args.expected_commit,
                                     source_tree, source_inputs, tracked, changed_engine)

    sync, sync_bytes = None, None
    public_commit = args.expected_commit
    if args.source_sync:
        sync_bytes = args.source_sync.read_bytes()
        sync = json.loads(sync_bytes)
        require(sync.get('local_commit') == args.expected_commit
                and sync.get('root_tree_sha') == root_tree and sync.get('remote_root_tree_sha') == root_tree
                and sync.get('remote_tree_verified') is True and sync.get('blob_digests_verified') is True,
                'Source sync receipt differs from the frozen complete tree')
        public_commit = sync.get('remote_commit', '')
        require(re.fullmatch(r'[0-9a-f]{40}', public_commit) is not None, 'Missing synced source commit')

    sys.path.insert(0, str(source / 'tools'))
    from verify_windows_outputs import verify, read_release_version
    file_version, version = read_release_version(source / 'shared/source/release_version.h')
    require((version, file_version) == (VERSION, FILE_VERSION), 'Wrong prerelease version')
    identity_bytes = tracked('source/BUILD_SOURCE_MANIFEST.json')
    identity = json.loads(identity_bytes)
    require(identity.get('version') == VERSION and identity.get('build_protocol') == 101,
            'Source identity or Assist protocol differs')
    require((binaries / 'BUILD_SOURCE_MANIFEST.json').read_bytes() == identity_bytes, 'Built source identity differs')
    target_bytes = (binaries / 'BUILD_TARGET.json').read_bytes()
    target = json.loads(target_bytes)
    require(tuple(target.get(key) for key in ('host', 'runtime', 'broker', 'assist_protocol',
                                             'cursor_protocol', 'runtime_protocol', 'console_ui'))
            == ('x64', 'x86', 'x86', 101, 155, 4, 'webview2'), 'Wrong completed build profile')
    actual, errors = verify(binaries, host_arch='x64', webview2=True)
    require(not errors, 'PE verification failed: ' + '; '.join(errors))
    verified = dict(actual)
    verified['WebView2Loader.dll'] = actual['RA2Toolbox.exe']['webview2_loader']
    current = {name: (binaries / name).read_bytes() for name in POLICY.BINARIES}
    for name, data in current.items():
        POLICY.verify_artifact_binding(name, built[name], verified[name], data)

    require(actual['GameRuntime.dll']['assist_shared_bytes'] == base_manifest['assist_shared_bytes'],
            'Shared layout changed despite unchanged protocol')
    for name in ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe'):
        require(sha(current[name]) != base_record['files'][name]['sha256'], 'Previous product program reused: ' + name)
    dependency_manifest = POLICY.verify_dependency_manifest(engine['engine/runtime-dependencies.json'])
    for filename in ('LICENSE.txt', 'NOTICE.txt'):
        name = 'licenses/WebView2/' + filename
        data = (binaries / name).read_bytes()
        require(data == resources[name] and sha(data) == verified['WebView2Loader.dll']['licenses'][name],
                'Verified loader license differs from pinned base: ' + name)
    for name, path in {'licenses/LICENSE-GPL.txt': 'LICENSE-GPL',
                       'licenses/THIRD_PARTY_NOTICES.md': 'THIRD_PARTY_NOTICES.md',
                       'licenses/xbrz-License.txt': 'source/third_party/xbrz/License.txt'}.items():
        require(tracked(path) == resources[name], 'License changed from pinned base: ' + name)
    current['README.txt'] = readme
    current['Start.cmd'] = b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
    full_contents, update_contents = assemble(resources, current, engine)
    require(set(full_contents) == set(base_record['files']), 'Full package path set changed')

    # Stage locally; only expose a complete package set after all final checks.
    out.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.ra2-147-package-', dir=out.parent) as temporary:
        stage = Path(temporary) / 'complete'
        stage.mkdir()
        update = POLICY.write_zip(stage / f'RA2Toolbox-{VERSION}-x64-Update.zip', update_contents, full=False)
        full = POLICY.write_zip(stage / f'RA2Toolbox-{VERSION}-x64-Full.zip', full_contents, full=True)
        require(all(full['files'][name] == update['files'][name] for name in update_contents),
                'Full and Update paired members differ')
        report = {
            'schema': 'ra2.optimization-release.v1', 'version': VERSION, 'file_version': FILE_VERSION,
            'source_commit': public_commit, 'local_build_commit': args.expected_commit,
            'source_tree_sha': source_tree, 'complete_git_tree_sha': root_tree,
            'source_published': sync is not None, 'source_identity': sync,
            'source_sync_receipt_sha256': sha(sync_bytes) if sync_bytes is not None else None,
            'build_method': 'Zig 0.13.0 Windows-target cross compilation on Linux',
            'build_provenance_sha256': sha(provenance_bytes), 'source_files_verified': len(source_inputs),
            'packaging_inputs': {name: tracked_inputs[name] for name in PACKAGE_INPUTS},
            'assist_protocol': 101, 'cursor_protocol': 155, 'runtime_protocol': 4,
            'assist_shared_bytes': actual['GameRuntime.dll']['assist_shared_bytes'],
            'actual_pe_verified': actual, 'validation': validation,
            'validation_evidence_sha256': sha(validation_bytes), 'verified_validation_logs': log_bindings,
            'controlled_windows_checks': False, 'current_release_binaries_executed_on_windows': False,
            'full_game_tested': False, 'physical_gpu_tested': False, 'multiplayer_tested': False,
            'prerelease': True, 'make_latest': False,
            'deferred_work': ['cursor_mixed_canvas_fix'] + ([] if changed_engine else ['dependency_probe_optimization']),
            'resources': {'source_release': 'v1.0.46', 'source_file_version': '1.0.46.4',
                          'source_commit': BASE_COMMIT, 'archive': BASE_NAME, 'bytes': BASE_BYTES,
                          'sha256': BASE_SHA, 'manifest_sha256': BASE_MANIFEST_SHA,
                          'verified_members': 53,
                          'unchanged_members': sorted(set(resources) - set(changed_engine)),
                          'old_product_programs_reused': False, 'generated_cursor_resources_bundled': False},
            'optional_runtime': {'bundled': False, 'native_startup_requires_download': False,
                                 'existing_runtime_and_cache_preserved_by_update': True,
                                 'engine_unchanged_from_base': not changed_engine,
                                 'engine_changed_members': changed_engine, 'complete_engine_in_update': True,
                                 'engine_source': 'current_frozen_source_with_explicit_change_plan',
                                 'manifest_path': 'engine/runtime-dependencies.json',
                                 'manifest_sha256': sha(resources['engine/runtime-dependencies.json']),
                                 'dependencies': dependency_manifest},
            'packages': [full, update],
        }
        (stage / 'release-manifest.json').write_bytes(encoded(report))
        sums = [stage / full['name'], stage / update['name'], stage / 'release-manifest.json']
        (stage / 'SHA256SUMS.txt').write_text(''.join(sha(path.read_bytes()) + '  ' + path.name + '\n'
                                                   for path in sums), encoding='ascii')
        require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'HEAD changed during packaging')
        for name, record in list(tracked_inputs.items()):
            require(record == {'bytes': len(data := tracked(name)), 'sha256': sha(data)},
                    'Source changed during packaging: ' + name)
        for name in POLICY.BINARIES:
            require(built[name] == {'bytes': len(data := (binaries / name).read_bytes()), 'sha256': sha(data)},
                    'Built product changed during packaging: ' + name)
        for path, record in log_bindings.items():
            require(record == {'bytes': len(data := Path(path).read_bytes()), 'sha256': sha(data)},
                    'Validation log changed during packaging: ' + path)
        receipts = [(args.provenance, provenance_bytes), (args.validation, validation_bytes),
                    (binaries / 'BUILD_TARGET.json', target_bytes),
                    (binaries / 'BUILD_SOURCE_MANIFEST.json', identity_bytes)]
        if args.source_sync:
            receipts.append((args.source_sync, sync_bytes))
        for path, before in receipts:
            require(path.read_bytes() == before, 'Evidence changed during packaging: ' + str(path))
        require({path.name for path in stage.iterdir()} == {path.name for path in sums} | {'SHA256SUMS.txt'},
                'Distribution member set differs')
        require(not out.exists(), 'Output appeared during packaging')
        os.rename(stage, out)
    print(json.dumps({'version': VERSION, 'file_version': FILE_VERSION, 'commit': args.expected_commit,
                      'source_published': sync is not None, 'checks': [row['name'] for row in validation['checks']],
                      'windows_executed': False, 'gpu_tested': False,
                      'packages': [{key: value for key, value in row.items() if key != 'files'}
                                   for row in (full, update)], 'output': str(out)}, indent=2))


if __name__ == '__main__':
    main()
