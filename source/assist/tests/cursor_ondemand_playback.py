#!/usr/bin/env python3
"""Execute the actual Cursor playback-bank acceptance and raster-cache types."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path

from runtime_broker_retirement import function


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    repo = here.parents[2]
    cursor = repo / 'source/cursor/source'
    source = cursor / 'main.cpp'
    fixture = here / 'cursor_ondemand_playback.cpp'
    inputs = [source, cursor / 'playback.h', cursor / 'motion.h', cursor / 'rotation.h',
              fixture, Path(__file__), here / 'runtime_broker_retirement.py']
    bindings = {str(path.relative_to(repo)): digest(path) for path in inputs}
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    text = source.read_text(encoding='utf-8')
    extracted = function(text, 'struct PlaybackBank {') + ';\n\n'
    extracted += function(text, 'static void acceptPlaybackBank(PlaybackBank&&bank){') + '\n'
    route = out / 'production-playback.inc'
    route.write_text(extracted, encoding='utf-8')
    poll = function(text, 'static void motionPoll(){')
    assert 'if(playbackEpoch==motionEpoch)' in poll
    assert 'acceptPlaybackBank(std::move(bank));' in poll
    exe = out / 'cursor-ondemand-playback'
    command = [args.cxx, '-std=c++17', '-O1', '-g', '-fsanitize=undefined',
               '-fno-sanitize-recover=all', '-DPLAYBACK_PRODUCTION="' + str(route) + '"',
               str(fixture), '-o', str(exe)]
    build = subprocess.run(command, text=True, capture_output=True)
    (out / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    if build.returncode:
        raise SystemExit(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], text=True, capture_output=True)
    (out / 'run.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    if run.returncode:
        raise SystemExit(run.stdout + run.stderr)
    result = json.loads(run.stdout.strip().splitlines()[-1])
    if any(digest(path) != bindings[str(path.relative_to(repo))] for path in inputs):
        raise SystemExit('Playback test inputs changed during verification')
    receipt = {
        'schema': 'ra2.cursor-ondemand-playback.v1',
        'passed': result['failures'] == 0,
        'result': result,
        'source_bindings': [{'path': path, 'sha256': value} for path, value in bindings.items()],
        'production_route_sha256': digest(route),
        'build_command': command,
        'build_exit_code': build.returncode,
        'run_exit_code': run.returncode,
        'windows_executed': False,
        'game_tested': False,
        'multiplayer_tested': False,
        'scope': [
            'PlaybackBank and acceptPlaybackBank are extracted verbatim from production main.cpp; playback.h and its frame/timeline/raster implementations are included unchanged.',
            'Full replacement, empty full replacement, incremental addition/replacement, capacity rejection and exact-cap admission execute with real bank ownership and pixel data.',
            'Frame destruction and reconstruction at the same legal storage address deterministically reproduce stale raster pixels before invalidation and fresh pixels after actual acceptance.',
            'Replacement clears active ownership and restores lastFrame/lastPhase submission sentinels; additions and rejected changes preserve valid cached pixels and active animations.',
        ],
        'limits': [
            'Capacity cases alter the real Timeline resident-byte charge instead of allocating 512 MiB; bank arithmetic and ownership execute unchanged.',
            'The Windows UI, frame submission, asynchronous scheduling, disk decoder and real-game rendering are not executed by this focused fixture.',
        ],
    }
    (out / 'receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))
    print('PASS actual playback acceptance and raster lifetime; no Windows/game execution.')


if __name__ == '__main__':
    main()
