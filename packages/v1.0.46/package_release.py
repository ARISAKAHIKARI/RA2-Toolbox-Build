#!/usr/bin/env python3
"""Package the verified 1.0.46 revised build without generated cursor caches.

Native programs and original cursor resources remain usable without a Python
download. Missing interpolation/preview caches are prepared on demand by the
matching current engine. Build, PE, source and package gates remain mandatory;
Windows/game execution is recorded as false. This script never publishes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys
from urllib.parse import urlparse
import zipfile

sys.dont_write_bytecode = True

BASE_NAME = 'RA2Toolbox-1.0.45-x64-Full.zip'
BASE_SHA = '5ec5c6b92f064148787b53ef69c923b2a6ef85f0e9dd0ac50e0a3cb9230167cf'
BASE_MANIFEST_SHA = '75b938a75aaaa68cfc2da068953a85cf8fe58676cd7bd40e86b7fab7c81f9247'
BASE_COMMIT = '2e4c1443c61312744a1806a29e546ef4fa9a5b56'
BINARIES = ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe', 'WebView2Loader.dll')
GROUPS = {'assets', 'profiles', 'engine', 'licenses'}
BASE_ENGINE = frozenset({'engine/dis_render.py', 'engine/player-template.html',
                         'engine/precompute.py', 'engine/preview_builder.py'})
BOOTSTRAP_ENGINE = frozenset({'engine/prepare_runtime.ps1', 'engine/prepare_runtime.py',
                          'engine/runtime-dependencies.json'})
ENGINE = BASE_ENGINE | BOOTSTRAP_ENGINE
NONSHIPPING_ENGINE_TESTS = frozenset({'engine/test_precompute_cache.py'})
PROFILES = frozenset({'bibata-amber', 'bibata-ice', 'capitaine'})
PROFILE_INPUTS = frozenset({'original.sha', 'palette.bgra', 'groups.json', 'pairs.txt', 'profile.ini'})
REQUIRED = ({'assets/original.sha', 'assets/palette.bgra', 'assets/groups.json',
             'assets/pairs.txt'} | ENGINE |
            {f'profiles/{profile}/{name}' for profile in PROFILES for name in PROFILE_INPUTS})
FOCUSED_CHECKS = frozenset({'value_task_inputs', 'anti_theft_cache', 'subscription_deadlines',
                            'building_metadata', 'production_time', 'lightweight_package'})
REQUIRED_BINDINGS = {
    'source/shared/source/world_frame_transport.h',
    'source/assist/source/value_task.h',
    'source/assist/source/compute_input_copy.h',
    'source/assist/source/compute_frame.h',
    'source/assist/source/main.cpp',
    'source/assist/source/autoload_bridge.h',
    'source/assist/source/missile_bridge.h',
    'source/assist/source/unload_compute_bridge.h',
    'source/assist/source/anti_theft_sample_bridge.h',
    'source/assist/source/external_orders_bridge.h',
    'source/assist/source/build_time_tooltip.h',
    'source/assist/source/build_time_types.h',
    'source/external_reader/source/camera_assets.h',
    'packages/v1.0.46/package_release.py',
    'packages/v1.0.46/test_lightweight_package.py',
} | {'source/' + name for name in ENGINE}
USER_SETTINGS = frozenset({'assist.ini', 'settings.ini', 'toolbox.ini'})
GENERATED_DIRECTORIES = frozenset({'timelines', '.preprocess', 'cache', '.runtime-cache'})
GENERATED_SUFFIXES = frozenset({'.dis', '.tl15'})
DEVELOPMENT_SUFFIXES = frozenset({'.pyc', '.pyo', '.whl', '.pdb', '.obj', '.o',
                                 '.dmp', '.dump', '.sqlite', '.db'})


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + '\n').encode('utf-8')


def safe(name):
    path = PurePosixPath(name)
    require(name and not path.is_absolute() and '\\' not in name and ':' not in name
            and not any(part in {'', '.', '..'} for part in name.split('/'))
            and not any(ord(c) < 32 for c in name), 'Unsafe ZIP path: ' + repr(name))
    return path


def generated_resource(name):
    """Generated interpolation, preview and local caches never ship in either ZIP."""
    path = safe(name)
    return (any(part.casefold() in GENERATED_DIRECTORIES for part in path.parts)
            or path.suffix.casefold() in GENERATED_SUFFIXES
            or path.name.casefold() == 'preview.html')


def include_base_resource(name):
    """Retain pinned original cursor inputs, theme metadata, engine and licenses."""
    path = safe(name)
    return (path.parts[0] in GROUPS and '__pycache__' not in path.parts
            and path.suffix.lower() not in DEVELOPMENT_SUFFIXES
            and not generated_resource(name))


def verify_dependency_manifest(data):
    """Bind the public bootstrap sources and digests into the release receipt."""
    manifest = json.loads(data)
    require(manifest.get('schema_version') == 1, 'Wrong dependency manifest schema')
    python = manifest.get('python', {})
    packages = manifest.get('packages', [])
    require(isinstance(python, dict) and isinstance(packages, list)
            and len(packages) == 3 and all(isinstance(row, dict) for row in packages),
            'Incomplete optional runtime manifest')
    versions = {row.get('id'): row.get('version') for row in packages}
    require(versions == {'numpy': '2.2.6', 'scipy': '1.15.3',
                         'opencv-python-headless': '4.11.0.86'},
            'Optional package versions differ from the validated runtime')
    require(python.get('version') == '3.12.10',
            'Python differs from the validated CPython 3.12.10 runtime')
    for row, host in [(python, 'www.python.org')] + [(row, 'files.pythonhosted.org') for row in packages]:
        url = urlparse(row.get('url', ''))
        require(url.scheme == 'https' and url.hostname == host
                and not url.username and not url.password and url.port in (None, 443)
                and not url.query and not url.fragment,
                'Dependency is not pinned to an expected public HTTPS source')
        filename = row.get('filename', '')
        require(len(safe(filename).parts) == 1 and url.path.endswith('/' + filename),
                'Dependency download filename differs')
        require(isinstance(row.get('bytes'), int) and not isinstance(row['bytes'], bool)
                and 0 < row['bytes'] <= 256 * 1024 * 1024, 'Invalid dependency archive size')
        digest = row.get('sha256', '')
        require(len(digest) == 64 and all(c in '0123456789abcdef' for c in digest),
                'Missing dependency SHA256')
    return manifest


def verify_product_members(names, *, full):
    """The same membership policy is checked before and after ZIP creation."""
    names = list(names)
    require(len(names) == len({name.casefold() for name in names}), 'Package path collision')
    allowed_roots = set(BINARIES) | {'README.txt', 'Start.cmd', 'engine', 'licenses'}
    if full:
        allowed_roots |= {'assets', 'profiles'}
    for name in names:
        path = safe(name)
        require(path.parts[0] in allowed_roots, 'Unexpected product member: ' + name)
        require('__pycache__' not in path.parts and path.suffix.lower() not in DEVELOPMENT_SUFFIXES,
                'Development/cache file in product package: ' + name)
        require(not generated_resource(name), 'Generated cursor resource in product package: ' + name)
        require(not any(part.lower().startswith('gamemd') for part in path.parts),
                'Private game member in product package: ' + name)
        require(path.parts[0].casefold() not in USER_SETTINGS, 'User settings in update')
    names = set(names)
    require({name for name in names if '/' not in name and name.lower().endswith(('.dll', '.exe'))}
            == set(BINARIES), 'Packaged executable set differs')
    require({name for name in names if name.startswith('engine/')} == ENGINE,
            'Packaged preprocessing engine/bootstrap is incomplete or unexpected')
    if full:
        require(REQUIRED <= names, 'Required original cursor inputs or engine absent')
        require({PurePosixPath(name).parts[1] for name in names if name.startswith('profiles/')}
                == PROFILES, 'Bundled cursor profile set differs')
        require({f'profiles/{profile}/{name}' for profile in PROFILES for name in PROFILE_INPUTS} <= names,
                'Bundled cursor profile inputs absent')


def assemble_payload(resources, current, engine):
    require({name for name in resources if name.startswith('engine/')} == ENGINE,
            'Published base engine membership differs')
    require(set(engine) == ENGINE, 'The explicit engine addition plan differs')
    # Ship the complete small engine in Update as well, so its loader, manifest
    # and scripts cannot come from different releases. No deletion instructions
    # or existing runtime, generated cache and user configuration are shipped.
    update = current | engine
    full = resources | update
    verify_product_members(update, full=False)
    verify_product_members(full, full=True)
    return full, update


def source_engine_members(directory):
    """Check the real source directory against the explicit shipping plan."""
    actual = {'engine/' + path.name for path in directory.iterdir()
              if path.is_file() and path.suffix.lower() in {'.py', '.html', '.ps1', '.json'}}
    shipping = actual - NONSHIPPING_ENGINE_TESTS
    require(shipping == ENGINE, 'Unexpected source engine file; update the explicit shipping plan')
    return shipping


def write_zip(path, contents, *, full):
    verify_product_members(contents, full=full)
    records = {}
    with zipfile.ZipFile(path, 'w') as archive:
        for name, data in sorted(contents.items()):
            entry = zipfile.ZipInfo(name, date_time=(2026, 11, 8, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            archive.writestr(entry, data, compresslevel=6)
            records[name] = {'bytes': len(data), 'sha256': sha(data)}
    with zipfile.ZipFile(path) as archive:
        require(archive.testzip() is None, 'Output ZIP CRC failed')
        require(set(archive.namelist()) == set(records), 'Output ZIP member set differs')
        verify_product_members(archive.namelist(), full=full)
        for name, row in records.items():
            data = archive.read(name)
            require(row == {'bytes': len(data), 'sha256': sha(data)}, 'Output member digest failed: ' + name)
    return {'name': path.name, 'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes()), 'files': records}


def verified_build_artifacts(provenance, binaries):
    output = provenance.get('output_dir')
    require(isinstance(output, str) and output and Path(output).resolve() == binaries.resolve(),
            'Binary directory differs from the completed build output')
    require(provenance.get('output_directory_was_empty') is True
            and provenance.get('build_artifacts_unchanged_after_pe_verification') is True,
            'Fresh build output/artifact verification is incomplete')
    records = provenance.get('build_artifacts')
    require(isinstance(records, dict) and set(records) == set(BINARIES), 'Build artifact set differs')
    return records


def verify_artifact_binding(name, built, verified, data=None):
    require(built == {'bytes': verified['bytes'], 'sha256': verified['sha256']},
            'Verified PE differs from completed build: ' + name)
    if data is not None:
        require(built == {'bytes': len(data), 'sha256': sha(data)},
                'Packaged bytes differ from completed build: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--binaries', required=True, type=Path)
    parser.add_argument('--resource-base', required=True, type=Path)
    parser.add_argument('--provenance', required=True, type=Path)
    parser.add_argument('--validation', required=True, type=Path)
    parser.add_argument('--readme', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--source-sync', required=True, type=Path)
    args = parser.parse_args()
    repo, binaries, out = args.repo.resolve(), args.binaries.resolve(), args.out.resolve()
    source = repo / 'source'
    resource_base = args.resource_base.resolve()
    sys.path.insert(0, str(source / 'tools'))
    from verify_windows_outputs import verify, read_release_version

    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])

    tracked_inputs = {}

    def tracked(path):
        name = path.relative_to(repo).as_posix()
        records = [row for row in git('ls-tree', '-z', 'HEAD', '--', name).split(b'\0') if row]
        require(len(records) == 1, 'Expected one tracked file: ' + name)
        prefix, raw_name = records[0].split(b'\t', 1)
        _, kind, expected = prefix.decode().split()
        require(kind == 'blob' and raw_name.decode() == name, 'Wrong tracked file: ' + name)
        data = path.read_bytes()
        require(hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == expected,
                'Working file differs from commit: ' + name)
        tracked_inputs[name] = {'bytes': len(data), 'sha256': sha(data)}
        return data

    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'Wrong checkout HEAD')
    sync_bytes = args.source_sync.read_bytes()
    sync = json.loads(sync_bytes)
    require(sync.get('local_commit') == args.expected_commit
            and sync.get('root_tree_sha') == git('rev-parse', 'HEAD^{tree}').decode().strip()
            and sync.get('remote_root_tree_sha') == sync.get('root_tree_sha')
            and sync.get('remote_tree_verified') is True
            and sync.get('blob_digests_verified') is True,
            'Published GitHub source does not match the complete frozen build tree')
    public_commit = sync.get('remote_commit', '')
    require(len(public_commit) == 40 and all(c in '0123456789abcdef' for c in public_commit),
            'Missing published source commit')
    file_version, version = read_release_version()
    require(version == '1.0.46' and file_version == '1.0.46.3', 'Wrong revised stable release version')
    provenance_bytes = args.provenance.read_bytes()
    provenance = json.loads(provenance_bytes)
    require(provenance['commit'] == args.expected_commit and provenance['build_returncode'] == 0
            and provenance['static_pe_verification_returncode'] == 0
            and provenance['source_inputs_unchanged_after_build'] is True
            and provenance['git_head_verified_at_finish'] is True,
            'Build provenance incomplete or different')
    require(provenance['platform'] == 'linux' and provenance['zig_version'] == '0.13.0', 'Wrong build method')
    built_artifacts = verified_build_artifacts(provenance, binaries)
    for field in ('controlled_windows_checks', 'runtime_tested', 'game_tested', 'multiplayer_tested'):
        require(provenance[field] is False, 'Cross-build provenance overclaims execution')
    for row in provenance['source_inputs']:
        data = tracked(repo / row['path'])
        require(len(data) == row['bytes'] and sha(data) == row['sha256'], 'Compiled source changed: ' + row['path'])
    source_tree = git('rev-parse', 'HEAD:source').decode().strip()
    require(source_tree == provenance['source_tree_sha'], 'Source tree changed after build')
    identity = json.loads(tracked(source / 'BUILD_SOURCE_MANIFEST.json'))
    require(identity['version'] == version, 'Source identity version differs')
    require((binaries / 'BUILD_SOURCE_MANIFEST.json').read_bytes() == tracked(source / 'BUILD_SOURCE_MANIFEST.json'),
            'Compiled source manifest differs')
    actual, errors = verify(binaries, host_arch='x64', webview2=True)
    require(not errors, 'PE verification failed: ' + '; '.join(errors))
    verified_artifacts = dict(actual)
    verified_artifacts['WebView2Loader.dll'] = actual['RA2Toolbox.exe']['webview2_loader']
    for name in BINARIES:
        verify_artifact_binding(name, built_artifacts[name], verified_artifacts[name])
    abi = actual['GameRuntime.dll']
    require(abi['assist_protocol'] == 101 and identity['build_protocol'] == 101, 'Wrong Assist protocol')
    target = json.loads((binaries / 'BUILD_TARGET.json').read_text())
    require(tuple(target[key] for key in ('host', 'runtime', 'broker', 'assist_protocol', 'cursor_protocol', 'runtime_protocol', 'console_ui'))
            == ('x64', 'x86', 'x86', 101, 155, 4, 'webview2'), 'Wrong compiled architecture/profile')
    validation_bytes = args.validation.read_bytes()
    validation = json.loads(validation_bytes)
    require(validation.get('commit') == args.expected_commit, 'Validation evidence belongs to another commit')
    require(validation.get('version') == version, 'Validation evidence version differs')
    require(validation.get('required_checks_passed') is True, 'Required focused checks did not pass')
    require(validation.get('game_tested') is False and validation.get('multiplayer_tested') is False
            and validation.get('windows_executed') is False,
            'Release validation overclaims execution')
    require(validation.get('checks_reexecuted') is True,
            'This release requires focused checks against the repaired source')
    checks = validation.get('focused_checks', {})
    require(set(checks) == FOCUSED_CHECKS
            and all(row.get('passed') is True for row in checks.values()),
            'A performance or package check lacks passing evidence')
    bindings = validation.get('source_bindings', [])
    require(bindings and len({row['path'] for row in bindings}) == len(bindings),
            'Validation source bindings absent or duplicated')
    require(REQUIRED_BINDINGS <= {row['path'] for row in bindings},
            'Validation did not bind every changed performance or packaging input')
    for row in bindings:
        relative = safe(row['path'])
        require(relative.parts[0] == 'source' or relative.parts[:2] == ('packages', 'v1.0.46'),
                'Validation binding is outside release source')
        require(sha(tracked(repo / row['path'])) == row['sha256'],
                'Validated source differs from packaged source: ' + row['path'])

    base_manifest_bytes = (resource_base / 'release-manifest.json').read_bytes()
    require(sha(base_manifest_bytes) == BASE_MANIFEST_SHA, 'Published base manifest digest differs')
    base_manifest = json.loads(base_manifest_bytes)
    require(base_manifest['source_commit'] == BASE_COMMIT and base_manifest['version'] == '1.0.45',
            'Published resource base identity differs')
    matches = [row for row in base_manifest['packages'] if row['name'] == BASE_NAME]
    require(len(matches) == 1 and matches[0]['sha256'] == BASE_SHA, 'Base archive manifest membership differs')
    expected_base = matches[0]
    base_path = resource_base / BASE_NAME
    require(base_path.stat().st_size == expected_base['bytes'] and sha(base_path.read_bytes()) == BASE_SHA,
            'Published base archive digest differs')
    resources = {}
    excluded_runtime = {'files': 0, 'uncompressed_bytes': 0, 'compressed_payload_bytes': 0}
    excluded_generated = {'files': 0, 'uncompressed_bytes': 0, 'compressed_payload_bytes': 0,
                          'preview_files': 0, 'timeline_files': 0}
    with zipfile.ZipFile(base_path) as archive:
        require(archive.testzip() is None, 'Published base ZIP CRC failed')
        members = archive.infolist()
        names = [item.filename for item in members]
        require(len(names) == len(set(n.casefold() for n in names)) and set(names) == set(expected_base['files']),
                'Published base ZIP/manifest membership differs')
        for item in members:
            name = item.filename
            path = safe(name)
            require(not item.is_dir() and not stat.S_ISLNK(item.external_attr >> 16), 'Invalid base member')
            data = archive.read(item)
            expected = expected_base['files'][name]
            require(expected == {'bytes': len(data), 'sha256': sha(data)}, 'Base member digest differs: ' + name)
            require(not any(part.lower().startswith('gamemd') for part in path.parts), 'Private game member in base')
            if path.parts[0] == 'runtime':
                excluded_runtime['files'] += 1
                excluded_runtime['uncompressed_bytes'] += item.file_size
                excluded_runtime['compressed_payload_bytes'] += item.compress_size
            if generated_resource(name):
                excluded_generated['files'] += 1
                excluded_generated['uncompressed_bytes'] += item.file_size
                excluded_generated['compressed_payload_bytes'] += item.compress_size
                excluded_generated['preview_files'] += path.name.casefold() == 'preview.html'
                excluded_generated['timeline_files'] += path.suffix.casefold() in GENERATED_SUFFIXES
            if not include_base_resource(name):
                continue
            resources[name] = data
    require(excluded_runtime['files'] == 0, 'Pinned slim base unexpectedly includes an optional runtime')
    require(excluded_generated['files'] == 392 and excluded_generated['preview_files'] == 4
            and excluded_generated['timeline_files'] == 388,
            'Pinned base generated-resource membership differs')

    current = {name: (binaries / name).read_bytes() for name in BINARIES}
    for name in BINARIES:
        verify_artifact_binding(name, built_artifacts[name], verified_artifacts[name], current[name])
    for name in ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe'):
        require(sha(current[name]) != expected_base['files'][name]['sha256'],
                'Old product executable must not be reused: ' + name)
    for name in ('LICENSE.txt', 'NOTICE.txt'):
        current['licenses/WebView2/' + name] = (binaries / 'licenses/WebView2' / name).read_bytes()
    current['licenses/LICENSE-GPL.txt'] = tracked(repo / 'LICENSE-GPL')
    current['licenses/THIRD_PARTY_NOTICES.md'] = tracked(repo / 'THIRD_PARTY_NOTICES.md')
    current['licenses/xbrz-License.txt'] = tracked(source / 'third_party/xbrz/License.txt')
    current['README.txt'] = tracked(args.readme.resolve())
    current['Start.cmd'] = b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
    source_engine_members(source / 'engine')
    engine = {name: tracked(source / name) for name in sorted(ENGINE)}
    # The on-demand workflow intentionally changes engine behavior. Bind every
    # shipped byte to the current frozen source and current validation above;
    # never mix those files with the older base engine or claim old tests reran.
    changed_engine = sorted(name for name in ENGINE if engine[name] != resources[name])
    dependency_manifest = verify_dependency_manifest(engine['engine/runtime-dependencies.json'])
    full_contents, current = assemble_payload(resources, current, engine)

    out.mkdir(parents=True, exist_ok=True)
    allowed = {f'RA2Toolbox-{version}-x64-Full.zip', f'RA2Toolbox-{version}-x64-Update.zip', 'release-manifest.json', 'SHA256SUMS.txt'}
    require({path.name for path in out.iterdir()} <= allowed, 'Unexpected output file')
    update = write_zip(out / f'RA2Toolbox-{version}-x64-Update.zip', current, full=False)
    full = write_zip(out / f'RA2Toolbox-{version}-x64-Full.zip', full_contents, full=True)
    require(all(full['files'][name] == update['files'][name] for name in current), 'Full/Update current files differ')
    report = {'version': version, 'file_version': file_version,
              'source_commit': public_commit, 'local_build_commit': args.expected_commit,
              'source_tree_sha': source_tree, 'complete_git_tree_sha': sync['root_tree_sha'],
              'source_identity': sync, 'source_sync_receipt_sha256': sha(sync_bytes),
              'assist_protocol': abi['assist_protocol'], 'assist_shared_bytes': abi['assist_shared_bytes'],
              'build_platform': 'Linux', 'build_method': 'Zig 0.13.0 cross compilation',
              'build_provenance_sha256': sha(provenance_bytes),
              'source_files_verified': len(provenance['source_inputs']),
              'controlled_windows_checks': False, 'current_release_binaries_executed_on_windows': False,
              'full_game_tested': False, 'multiplayer_tested': False,
              'prerelease': False, 'make_latest': True, 'revises_same_release': 'v1.0.46',
              'validation': validation, 'validation_evidence_sha256': sha(validation_bytes),
              'actual_pe_verified': actual,
              'resources': {'source_release': 'v1.0.45', 'source_commit': BASE_COMMIT,
                            'archive': BASE_NAME, 'sha256': BASE_SHA,
                            'manifest_sha256': BASE_MANIFEST_SHA,
                            'verified_members': len(expected_base['files']),
                            'old_root_programs_reused': False,
                            'excluded_optional_runtime': excluded_runtime, 'base_already_slim': True,
                            'excluded_generated_cursor_resources': excluded_generated,
                            'generated_cursor_resources_bundled': False,
                            'existing_cursor_cache_preserved_by_update': True},
              'optional_runtime': {
                  'bundled': False,
                  'download_trigger': 'requested_cursor_preprocessing_or_preview_only',
                  'native_startup_requires_download': False,
                  'existing_valid_cursor_cache_requires_download': False,
                  'original_cursor_frame_playback_requires_download': False,
                  'existing_runtime_preserved_by_update': True,
                  'manifest_path': 'engine/runtime-dependencies.json',
                  'manifest_sha256': sha(engine['engine/runtime-dependencies.json']),
                  'dependencies': dependency_manifest,
                  'engine_upgrade': {'retained': sorted(ENGINE), 'added': [],
                                     'removed': [], 'updated': changed_engine,
                                     'source': 'current_frozen_source',
                                     'update_contains_complete_engine': True}},
              'packages': [full, update]}
    (out / 'release-manifest.json').write_bytes(encoded(report))
    sums = [out / full['name'], out / update['name'], out / 'release-manifest.json']
    (out / 'SHA256SUMS.txt').write_text(''.join(sha(path.read_bytes()) + '  ' + path.name + '\n' for path in sums), encoding='ascii')
    require({path.name for path in out.iterdir()} == allowed, 'Distribution file membership differs')
    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'HEAD changed during packaging')
    for name, row in list(tracked_inputs.items()):
        data = tracked(repo / name)
        require(row == {'bytes': len(data), 'sha256': sha(data)}, 'Source changed during packaging: ' + name)
    subprocess.run(['sha256sum', '-c', 'SHA256SUMS.txt'], cwd=out, check=True)
    print(json.dumps({'version': version, 'commit': args.expected_commit,
                      'controlled_windows_checks': False,
                      'packages': [{key: value for key, value in row.items() if key != 'files'} for row in (full, update)],
                      'output': str(out)}, indent=2))


if __name__ == '__main__':
    raise SystemExit(main())
