#!/usr/bin/env python3
"""Package a frozen, verified mixed-architecture performance update.

Existing Full resources are retained by the user. This tool accepts only the
four paired outputs of the recorded build and does not publish a release.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import zipfile

from verify_windows_outputs import verify, read_release_version

NAMES = ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe', 'WebView2Loader.dll')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--binaries', required=True, type=Path)
    parser.add_argument('--provenance', required=True, type=Path)
    parser.add_argument('--validation', required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    repo, binaries, out = args.repo.resolve(), args.binaries.resolve(), args.out.resolve()
    git = lambda *command: subprocess.check_output(['git', '-C', str(repo), *command])
    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'Wrong checkout')
    file_version, version = read_release_version()
    require((version, file_version) == ('1.0.44-pre.3', '1.0.44.3'), 'Wrong version')
    provenance_bytes = args.provenance.read_bytes()
    provenance = json.loads(provenance_bytes)
    require(provenance['commit'] == args.expected_commit and provenance['build_returncode'] == 0
            and provenance['static_pe_verification_returncode'] == 0
            and provenance['source_inputs_unchanged_after_build'] is True
            and provenance['git_head_verified_at_finish'] is True
            and provenance['output_directory_was_empty'] is True
            and provenance['build_artifacts_unchanged_after_pe_verification'] is True,
            'Build provenance is incomplete')
    require(Path(provenance['output_dir']).resolve() == binaries, 'Different binary directory')
    for key in ('controlled_windows_checks', 'runtime_tested', 'game_tested', 'multiplayer_tested'):
        require(provenance[key] is False, 'Cross-build overclaims execution')
    source_inputs = {row['path']: row for row in provenance['source_inputs']}

    def tracked(name):
        data = (repo / name).read_bytes()
        expected = git('rev-parse', 'HEAD:' + name).decode().strip()
        require(hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == expected,
                'Uncommitted input: ' + name)
        return data

    for name, row in source_inputs.items():
        data = tracked(name)
        require(len(data) == row['bytes'] and digest(data) == row['sha256'], 'Compiled source changed: ' + name)
    source_tree = git('rev-parse', 'HEAD:source').decode().strip()
    require(source_tree == provenance['source_tree_sha'], 'Source tree differs')
    actual, errors = verify(binaries, host_arch='x64', webview2=True)
    require(not errors, 'PE verification failed: ' + '; '.join(errors))
    require(actual['GameRuntime.dll']['assist_protocol'] == 101, 'Wrong wire protocol')
    validation_bytes = args.validation.read_bytes()
    validation = json.loads(validation_bytes)
    require(validation['commit'] == args.expected_commit and validation['version'] == version
            and validation['required_checks_passed'] is True and validation['checks_reexecuted'] is True,
            'Focused checks are incomplete or belong to another build')
    require(validation['windows_executed'] is False and validation['game_tested'] is False
            and validation['multiplayer_tested'] is False, 'Focused checks overclaim live execution')
    require(validation['checks'], 'No executed checks')
    for check in validation['checks']:
        require(check['exit_code'] == 0 and check['executed_this_revision'] is True, 'Failed/inherited check')
        for group in ('test_source_hashes', 'production_source_hashes'):
            require(check[group], 'Missing source binding: ' + check['name'])
            for name, sha in check[group].items():
                require(name in source_inputs and sha == source_inputs[name]['sha256'] == digest(tracked(name)),
                        'Tested source changed: ' + name)
    current = {name: (binaries / name).read_bytes() for name in NAMES}
    verified = dict(actual)
    verified['WebView2Loader.dll'] = actual['RA2Toolbox.exe']['webview2_loader']
    require(set(provenance['build_artifacts']) == set(NAMES), 'Unpaired artifact set')
    for name, data in current.items():
        record = {'bytes': len(data), 'sha256': digest(data)}
        require(record == provenance['build_artifacts'][name]
                == {key: verified[name][key] for key in record}, 'Packaged bytes differ from build: ' + name)
    current['README.txt'] = tracked('source/README-' + version + '.txt')
    current['Start.cmd'] = b'@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\nstart "" "%~dp0RA2Toolbox.exe"\r\n'
    for name in ('LICENSE.txt', 'NOTICE.txt'):
        current['licenses/WebView2/' + name] = (binaries / 'licenses/WebView2' / name).read_bytes()
    for name, source in {'licenses/LICENSE-GPL.txt': 'LICENSE-GPL',
                         'licenses/THIRD_PARTY_NOTICES.md': 'THIRD_PARTY_NOTICES.md',
                         'licenses/xbrz-License.txt': 'source/third_party/xbrz/License.txt'}.items():
        current[name] = tracked(source)
    out.mkdir(parents=True, exist_ok=True)
    archive_path = out / ('RA2Toolbox-' + version + '-x64-Update.zip')
    records = {}
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, data in sorted(current.items()):
            archive.writestr(name, data)
            records[name] = {'bytes': len(data), 'sha256': digest(data)}
    with zipfile.ZipFile(archive_path) as archive:
        require(archive.testzip() is None and set(archive.namelist()) == set(records), 'ZIP verification failed')
        for name, row in records.items():
            data = archive.read(name)
            require(row == {'bytes': len(data), 'sha256': digest(data)}, 'Archive member differs: ' + name)
    package = {'name': archive_path.name, 'bytes': archive_path.stat().st_size,
               'sha256': digest(archive_path.read_bytes()), 'files': records}
    manifest = {'version': version, 'file_version': file_version, 'source_commit': args.expected_commit,
                'source_tree_sha': source_tree, 'assist_protocol': 101,
                'assist_shared_bytes': actual['GameRuntime.dll']['assist_shared_bytes'],
                'prerelease': True, 'make_latest': False, 'stable_latest_to_preserve': 'v1.0.43',
                'build_method': 'Zig 0.13.0 Windows-target cross compilation on Linux',
                'windows_executed': False, 'game_tested': False, 'multiplayer_tested': False,
                'build_provenance_sha256': digest(provenance_bytes), 'validation_sha256': digest(validation_bytes),
                'actual_pe_verified': actual, 'validation': validation, 'packages': [package]}
    manifest_path = out / 'release-manifest.json'
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
    (out / 'SHA256SUMS.txt').write_text(''.join(digest(path.read_bytes()) + '  ' + path.name + '\n'
                                             for path in (archive_path, manifest_path)))
    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit, 'HEAD changed during packaging')
    print(json.dumps({key: value for key, value in package.items() if key != 'files'}, indent=2))


if __name__ == '__main__':
    main()
