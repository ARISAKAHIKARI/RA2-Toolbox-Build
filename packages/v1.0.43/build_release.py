#!/usr/bin/env python3
"""Cross-build a pinned RA2 Toolbox checkout without editing its sources."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys


ARTIFACT_NAMES = ('RA2Toolbox.exe', 'GameRuntime.dll', 'GameRuntimeHost32.exe', 'WebView2Loader.dll')


def require_empty_output(output):
    if any(output.iterdir()):
        raise RuntimeError('Build output directory must be empty: ' + str(output))


def build_artifacts(output):
    records = {}
    for name in ARTIFACT_NAMES:
        data = (output / name).read_bytes()
        if not data:
            raise RuntimeError('Build output is empty: ' + name)
        records[name] = {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
    return records


def now():
    return datetime.now(timezone.utc).isoformat()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--logs', required=True, type=Path)
    parser.add_argument('--expected-commit', required=True)
    parser.add_argument('--zig', required=True, type=Path)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--cache', required=True, type=Path)
    parser.add_argument('--cpus', type=int, default=6)
    args = parser.parse_args()
    args.zig, args.sdk, args.cache = args.zig.resolve(), args.sdk.resolve(), args.cache.resolve()
    repo, output, logs = args.repo.resolve(), args.output.resolve(), args.logs.resolve()
    source = repo / 'source'
    output.mkdir(parents=True, exist_ok=True)
    require_empty_output(output)
    logs.mkdir(parents=True, exist_ok=True)

    def git(*command):
        return subprocess.check_output(['git', '-C', str(repo), *command])

    def head():
        return git('rev-parse', 'HEAD').decode().strip()

    def source_inputs():
        records = []
        for raw in git('ls-tree', '-r', '-z', '--full-tree', 'HEAD', '--', 'source').split(b'\0'):
            if not raw:
                continue
            prefix, name = raw.split(b'\t', 1)
            _, kind, expected = prefix.decode().split()
            path = name.decode()
            if kind != 'blob':
                raise RuntimeError('Unsupported source object: ' + path)
            data = (repo / path).read_bytes()
            actual = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
            if actual != expected:
                raise RuntimeError('Tracked input differs from pinned commit: ' + path)
            records.append({'path': path, 'git_blob_sha': actual,
                            'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)})
        untracked = git('ls-files', '--others', '--exclude-standard', '-z', '--', 'source').split(b'\0')
        untracked = [name.decode() for name in untracked if name and Path(name.decode()).suffix.lower() in {'.h', '.hpp', '.cpp', '.c', '.rc', '.py', '.sh', '.js', '.mjs', '.html'}]
        if untracked:
            raise RuntimeError('Untracked source inputs require review/commit: ' + ', '.join(untracked))
        if not records:
            raise RuntimeError('No source inputs found')
        return records

    if head() != args.expected_commit:
        raise RuntimeError('Checkout HEAD differs from requested build commit')
    before = source_inputs()
    zig_version = subprocess.check_output([str(args.zig), 'version'], text=True).strip()
    if zig_version != '0.13.0':
        raise RuntimeError('Build requires Zig 0.13.0')
    subprocess.run([sys.executable, str(source / 'tools/prepare_console_sdk.py'),
                    '--output', str(args.sdk), '--offline'], check=True)
    env = os.environ.copy()
    env.update(ZIG=str(args.zig), RA2_WEBVIEW2_SDK=str(args.sdk),
               RA2_OUTPUT_DIR=str(output), RA2_CONSOLE_UI='webview2',
               RA2_PYTHON=sys.executable, PYTHONDONTWRITEBYTECODE='1',
               GITHUB_SHA=args.expected_commit, GITHUB_REPOSITORY='ARISAKAHIKARI/RA2-Toolbox-Build',
               ZIG_GLOBAL_CACHE_DIR=str(args.cache / 'global'),
               ZIG_LOCAL_CACHE_DIR=str(args.cache / 'local'))
    affinity = sorted(os.sched_getaffinity(0))[:max(1, args.cpus)]
    os.sched_setaffinity(0, affinity)
    report = {'commit': args.expected_commit,
              'source_tree_sha': git('rev-parse', 'HEAD:source').decode().strip(),
              'started_utc': now(), 'platform': sys.platform,
              'cpu_affinity': affinity, 'zig_version': zig_version,
              'webview2_sdk': '1.0.4314.35', 'output_dir': str(output),
              'output_directory_was_empty': True,
              'controlled_windows_checks': False, 'runtime_tested': False,
              'game_tested': False, 'multiplayer_tested': False,
              'source_inputs': before}
    report_path = logs / 'build-provenance.json'

    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')

    save()
    script = source / 'build-zig-x64.sh'
    with (logs / 'cross-build.log').open('w') as log:
        # Decode CRLF in memory only; tracked bytes remain unchanged.
        result = subprocess.run(['bash', '-x', '-c', script.read_text(), str(script)],
                                cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT)
    report['build_returncode'] = result.returncode
    save()
    if result.returncode:
        print('Build failed:', logs / 'cross-build.log', flush=True)
        return result.returncode
    report['build_artifacts'] = build_artifacts(output)
    report['build_artifacts_unchanged_after_pe_verification'] = False
    save()
    with (logs / 'pe-verification.log').open('w') as log:
        result = subprocess.run([sys.executable, str(source / 'tools/verify_windows_outputs.py'),
                                 '--root', str(output), '--host-arch', 'x64', '--webview2'],
                                cwd=source, env=env, stdout=log, stderr=subprocess.STDOUT)
    report['static_pe_verification_returncode'] = result.returncode
    report['build_artifacts_unchanged_after_pe_verification'] = build_artifacts(output) == report['build_artifacts']
    report['completed_utc'] = now()
    report['source_inputs_unchanged_after_build'] = source_inputs() == before
    report['git_head_verified_at_finish'] = head() == args.expected_commit
    save()
    if not report['source_inputs_unchanged_after_build'] or not report['git_head_verified_at_finish']:
        raise RuntimeError('Source or HEAD changed during build')
    if not report['build_artifacts_unchanged_after_pe_verification']:
        raise RuntimeError('Build artifacts changed during PE verification')
    print(json.dumps({k: v for k, v in report.items() if k != 'source_inputs'}, indent=2))
    print('Build provenance:', report_path, flush=True)
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
