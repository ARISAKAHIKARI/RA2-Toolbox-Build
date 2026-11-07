#!/usr/bin/env python3
"""Execute exact Host connection actions; the old button is a negative control."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    end, depth = brace + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--baseline-main', type=Path)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    source = here.parent / 'source/main.cpp'
    output = args.out.resolve()
    output.mkdir(parents=True, exist_ok=True)
    before = sha(source)
    text = source.read_text()
    assert 'else if(id==431){requestGameReconnect();}' in text
    assert 'RA2.Console.StatusText' in text and 'reinterpret_cast<HANDLE>(&statusText)' in text
    assert 'L" runtimeEnabled="' in text and 'L" hostEnabled="' in text
    assert '!pid?L"no-target":!hook?L"not-subscribed"' in text
    variants = [('current', '\n'.join(function(text, name) for name in (
        'static void requestGameReconnect()', 'static void serviceGameTargetPreference()',
        'static void logDiscovery(')), [])]
    if args.baseline_main:
        legacy = function(args.baseline_main.read_text(), 'else if(id==431){')
        body = legacy[legacy.index('{'):]
        variants.insert(0, ('baseline', 'static void requestGameReconnect()' + body,
                            ['-DLEGACY_NEGATIVE_CONTROL']))
    results = {}
    for name, routes, flags in variants:
        include = output / (name + '-production.inc')
        include.write_text(routes + '\n')
        binary = output / name
        command = [args.cxx, '-std=c++17', '-O2', '-UNDEBUG', '-DRA2_UNIFIED', *flags,
                   '-DHOST_CONNECTION_ROUTES="' + include.as_posix() + '"',
                   str(here / 'host_connection_recovery.cpp'), '-o', str(binary)]
        build = subprocess.run(command, capture_output=True, text=True)
        (output / (name + '-build.log')).write_text(build.stdout + build.stderr)
        if build.returncode:
            raise SystemExit(build.stdout + build.stderr)
        run = subprocess.run([str(binary)], capture_output=True, text=True)
        (output / (name + '.log')).write_text(run.stdout + run.stderr)
        report = json.loads(run.stdout.strip().splitlines()[-1])
        if name == 'baseline':
            assert run.returncode == 1 and report['failures'] == 1, run.stdout + run.stderr
            assert 'FAIL: connect action must not pause an enabled host' in run.stdout
        else:
            assert run.returncode == 0 and report['failures'] == 0, run.stdout + run.stderr
        results[name] = dict(report, exit_code=run.returncode, routes_sha256=sha(include))
    assert sha(source) == before
    sources = [source, here / 'host_connection_recovery.cpp', Path(__file__).resolve()]
    report = {'schema': 'ra2.host-connection-recovery.v1', 'passed': True,
              'checks': results['current']['checks'], 'negative_controls': 'baseline' in results,
              'real_windows_tested': False, 'game_tested': False, 'results': results,
              'source_sha256': {p.relative_to(repo).as_posix(): sha(p) for p in sources}}
    (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report, ensure_ascii=False))


if __name__ == '__main__':
    main()
