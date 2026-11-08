#!/usr/bin/env python3
"""Package the verified 1.0.44 stable cross-build with published 1.0.43 resources.

This stable release route preserves the existing build and package
gates and records Windows/game execution as false. It never publishes remotely.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import stat
import subprocess
import sys
import zipfile

sys.dont_write_bytecode = True

BASE_NAME = 'RA2Toolbox-1.0.43-x64-Full.zip'
BASE_SHA = '231a8ed43a05320ab19fa95f3ae3c1d8a3df3a163368a120be15d43755d09b26'
BASE_MANIFEST_SHA = 'a351bba2e2212317f33d87d4169381a95721f6af171fcc4aabee9e4f048f7142'
BASE_COMMIT = 'bf3fbe88faa96513649f2998aa0ab6e71935f4a8'
BINARIES = ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe', 'WebView2Loader.dll')
GROUPS = {'assets', 'profiles', 'engine', 'runtime', 'licenses'}
REQUIRED = {'runtime/python.exe', 'runtime/python312.dll', 'runtime/python312.zip',
            'runtime/python312._pth', 'runtime/Lib/site-packages/cv2/cv2.pyd',
            'assets/original.sha', 'assets/palette.bgra', 'assets/groups.json',
            'engine/precompute.py', 'engine/dis_render.py'}


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
    require(version == '1.0.44' and file_version == '1.0.44.0', 'Wrong stable release version')
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
    require(set(checks) == {'broker', 'construction', 'asset_replacement'}
            and all(row.get('passed') is True for row in checks.values()),
            'A requested repair lacks passing focused evidence')
    bindings = validation.get('source_bindings', [])
    require(bindings and len({row['path'] for row in bindings}) == len(bindings),
            'Validation source bindings absent or duplicated')
    for row in bindings:
        relative = safe(row['path'])
        require(relative.parts[0] == 'source', 'Validation binding is outside source')
        require(sha(tracked(repo / row['path'])) == row['sha256'],
                'Validated source differs from packaged source: ' + row['path'])

    base_manifest_bytes = (resource_base / 'release-manifest.json').read_bytes()
    require(sha(base_manifest_bytes) == BASE_MANIFEST_SHA, 'Published base manifest digest differs')
    base_manifest = json.loads(base_manifest_bytes)
    require(base_manifest['source_commit'] == BASE_COMMIT and base_manifest['version'] == '1.0.43',
            'Published resource base identity differs')
    matches = [row for row in base_manifest['packages'] if row['name'] == BASE_NAME]
    require(len(matches) == 1 and matches[0]['sha256'] == BASE_SHA, 'Base archive manifest membership differs')
    expected_base = matches[0]
    base_path = resource_base / BASE_NAME
    require(base_path.stat().st_size == expected_base['bytes'] and sha(base_path.read_bytes()) == BASE_SHA,
            'Published base archive digest differs')
    resources = {}
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
            if path.parts[0] not in GROUPS:
                continue
            require(not any(part.lower().startswith('gamemd') for part in path.parts), 'Private game member in base')
            if '__pycache__' in path.parts or path.suffix.lower() in {'.pyc', '.pyo', '.whl', '.pdb', '.obj', '.o', '.dmp', '.dump', '.sqlite', '.db'}:
                continue
            resources[name] = data

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
    engine = {}
    for path in sorted((source / 'engine').rglob('*')):
        if path.is_file() and '__pycache__' not in path.parts and path.suffix in {'.py', '.html'}:
            name = 'engine/' + path.relative_to(source / 'engine').as_posix()
            engine[name] = tracked(path)
    require(set(engine) == {name for name in resources if name.startswith('engine/')},
            'Engine member removals/additions need an explicit upgrade plan')
    for name, data in engine.items():
        if resources[name] != data:
            current[name] = data
    full_contents = resources | engine | current

    def write_zip(path, contents):
        records = {}
        seen = set()
        with zipfile.ZipFile(path, 'w') as archive:
            for name, data in sorted(contents.items()):
                safe(name)
                require(name.casefold() not in seen, 'Package path collision: ' + name)
                seen.add(name.casefold())
                require(not name.startswith(('source/', 'docs/', 'verification/', 'third-party/')), 'Development content in product package')
                entry = zipfile.ZipInfo(name, date_time=(2026, 11, 8, 0, 0, 0))
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, data, compresslevel=6)
                records[name] = {'bytes': len(data), 'sha256': sha(data)}
        with zipfile.ZipFile(path) as archive:
            require(archive.testzip() is None, 'Output ZIP CRC failed')
            require(set(archive.namelist()) == set(records), 'Output ZIP member set differs')
            for name, row in records.items():
                data = archive.read(name)
                require(row == {'bytes': len(data), 'sha256': sha(data)}, 'Output member digest failed: ' + name)
        require({name for name in records if '/' not in name and name.lower().endswith(('.dll', '.exe'))} == set(BINARIES),
                'Packaged executable set differs')
        return {'name': path.name, 'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes()), 'files': records}

    out.mkdir(parents=True, exist_ok=True)
    allowed = {f'RA2Toolbox-{version}-x64-Full.zip', f'RA2Toolbox-{version}-x64-Update.zip', 'release-manifest.json', 'SHA256SUMS.txt'}
    require({path.name for path in out.iterdir()} <= allowed, 'Unexpected output file')
    update = write_zip(out / f'RA2Toolbox-{version}-x64-Update.zip', current)
    full = write_zip(out / f'RA2Toolbox-{version}-x64-Full.zip', full_contents)
    require(REQUIRED <= full['files'].keys(), 'Required offline resources absent')
    require(len([name for name in full['files'] if name.startswith('profiles/') and name.endswith('/profile.ini')]) == 3,
            'Bundled cursor profiles differ')
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
              'prerelease': False, 'make_latest': True,
              'validation': validation, 'validation_evidence_sha256': sha(validation_bytes),
              'actual_pe_verified': actual,
              'resources': {'source_release': 'v1.0.43', 'source_commit': BASE_COMMIT,
                            'archive': BASE_NAME, 'sha256': BASE_SHA,
                            'manifest_sha256': BASE_MANIFEST_SHA,
                            'verified_members': len(expected_base['files']),
                            'old_root_programs_reused': False},
              'packages': [full, update]}
    (out / 'release-manifest.json').write_bytes(encoded(report))
    sums = [out / full['name'], out / update['name'], out / 'release-manifest.json']
    (out / 'SHA256SUMS.txt').write_text(''.join(sha(path.read_bytes()) + '  ' + path.name + '\n' for path in sums), encoding='ascii')
    require({path.name for path in out.iterdir()} == allowed, 'Distribution file membership differs')
    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'HEAD changed during packaging')
    for row in provenance['source_inputs']:
        require(sha(tracked(repo / row['path'])) == row['sha256'], 'Source changed during packaging')
    subprocess.run(['sha256sum', '-c', 'SHA256SUMS.txt'], cwd=out, check=True)
    print(json.dumps({'version': version, 'commit': args.expected_commit,
                      'controlled_windows_checks': False,
                      'packages': [{key: value for key, value in row.items() if key != 'files'} for row in (full, update)],
                      'output': str(out)}, indent=2))


if __name__ == '__main__':
    raise SystemExit(main())
