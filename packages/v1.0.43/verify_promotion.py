#!/usr/bin/env python3
"""Verify the metadata-only promotion of pre.15; do not rerun its regressions."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess


BASELINE = 'c021ab112204b2e680eb8f1d1ccc91e5c6ebe7ce'
EVIDENCE = 'source/research/evidence/cursor-fullscreen-presentation-20261107.json'
EVIDENCE_SHA256 = '651c8af02581e3e29986e24a407eb4f9f5f1c28b870b2b2acf09ce981ed0de5f'
METADATA = {'source/shared/source/release_version.h', 'source/BUILD_SOURCE_MANIFEST.json'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()

    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])

    def tree(revision):
        records = {}
        for row in git('ls-tree', '-r', '-z', revision, '--', 'source').split(b'\0'):
            if not row:
                continue
            prefix, raw_name = row.split(b'\t', 1)
            mode, kind, oid = prefix.decode().split()
            name = raw_name.decode()
            require(kind == 'blob', 'Non-blob source object: ' + name)
            records[name] = (mode, oid)
        return records

    def committed_bytes(revision, path):
        return git('show', revision + ':' + path)

    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit,
            'HEAD differs from expected promotion commit')
    require(subprocess.run(['git', '-C', str(repo), 'merge-base', '--is-ancestor',
                            BASELINE, args.expected_commit], check=False).returncode == 0,
            'Promotion does not descend from the pre.15 baseline')
    before, after = tree(BASELINE), tree(args.expected_commit)
    require(len(before) == 3543 and set(before) == set(after),
            'Source membership differs from the 3543-file pre.15 baseline')
    changed = {name for name in before if before[name] != after[name]}
    require(changed == METADATA, 'Source changes exceed or omit release metadata: ' + repr(sorted(changed)))
    require(all(before[name][0] == after[name][0] for name in before), 'Source file modes changed')
    unchanged = sorted(set(after) - METADATA)
    require(len(unchanged) == 3541 and all(before[name] == after[name] for name in unchanged),
            'Non-metadata source blobs differ from pre.15')
    for name, (_, oid) in after.items():
        data = (repo / name).read_bytes()
        require(hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() == oid,
                'Working source differs from promotion commit: ' + name)

    header = committed_bytes(args.expected_commit, 'source/shared/source/release_version.h').decode()
    expected_header = ('#pragma once\n#define RA2_RELEASE_VERSION "1.0.43.0"\n'
                       '#define RA2_RELEASE_VERSION_W L"1.0.43"\n'
                       '#define RA2_RELEASE_VERSION_PARTS 1,0,43,0\n')
    require(header.replace('\r\n', '\n').strip() == expected_header.strip(),
            'Stable release version header differs')
    identity = json.loads(committed_bytes(args.expected_commit, 'source/BUILD_SOURCE_MANIFEST.json'))
    require(identity.get('version') == '1.0.43', 'Stable source identity version differs')

    evidence_bytes = committed_bytes(args.expected_commit, EVIDENCE)
    require(evidence_bytes == committed_bytes(BASELINE, EVIDENCE)
            and sha(evidence_bytes) == EVIDENCE_SHA256, 'Pinned pre.15 evidence changed')
    evidence = json.loads(evidence_bytes)
    require(evidence.get('schema') == 'ra2.cursor-fullscreen-presentation.pre15.v1'
            and evidence.get('version') == '1.0.43-pre.15'
            and evidence.get('passed') is True, 'Baseline evidence did not pass')
    pure = evidence['pure_mapping']
    routes = evidence['production_routes']
    settings = evidence['settings_handler']
    require(pure['build_exit_code'] == 0 and pure['run_exit_code'] == 0
            and pure['result'] == {'scenarios': 26, 'checks': 51, 'failed': 0},
            'Inherited pure-mapping results differ')
    require(routes.get('passed') is True and routes.get('current_checks') == 196,
            'Inherited production-route checks differ')
    for result in routes['results']:
        require(result['exit_code'] == result['expected_exit'], 'Inherited route exit differs: ' + result['name'])
        require(result.get('native_windows_executed') is False and result.get('game_executed') is False,
                'Baseline route overclaims execution')
    require(settings.get('exit_code') == 0 and settings.get('windows_executed') is False,
            'Inherited settings checks differ')
    bindings = dict(pure['files'])
    bindings.update({'source/' + name: digest for name, digest in routes['source_sha256'].items()})
    bindings.update(settings['source_sha256'])
    for name, digest in bindings.items():
        require(name in before and name not in METADATA
                and sha(committed_bytes(args.expected_commit, name)) == digest,
                'Inherited evidence source binding differs: ' + name)

    report = {
        'schema': 'ra2.v1.0.43.promotion-validation.v1',
        'commit': args.expected_commit,
        'version': '1.0.43',
        'file_version': '1.0.43.0',
        'verified_utc': datetime.now(timezone.utc).isoformat(),
        'required_checks_passed': True,
        'source_equivalence_passed': True,
        'baseline_source_commit': BASELINE,
        'source_file_count': len(after),
        'unchanged_source_blobs': len(unchanged),
        'changed_metadata': sorted(changed),
        'checks_inherited_from': BASELINE,
        'checks_reexecuted': False,
        'pure_mapping': pure['result'],
        'controlled_route_checks': routes['current_checks'],
        'settings_handler_exit_code': settings['exit_code'],
        'evidence_path': EVIDENCE,
        'evidence_sha256': EVIDENCE_SHA256,
        'evidence_source_bindings_verified': len(bindings),
        'windows_executed': False,
        'game_tested': False,
        'multiplayer_tested': False,
        'validation_scope': ('Pre.15 checks are inherited, not reexecuted. All 3541 non-metadata source '
                             'blobs match the pinned pre.15 commit; only release metadata changed. '
                             'This check verifies source and recorded evidence. It does not execute '
                             'Windows, the game, a renderer, or multiplayer.'),
    }
    require(git('rev-parse', 'HEAD').decode().strip() == args.expected_commit,
            'HEAD changed during promotion verification')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
