#!/usr/bin/env python3
"""Run actual Cursor source adoption and immutable-cache routes with I/O seams."""
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
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--cxx', default='c++')
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    source = here.parents[1]
    cursor = source / 'cursor/source'
    main_cpp = cursor / 'main.cpp'
    fixture = here / 'cursor_ondemand_source.cpp'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source_bytes = main_cpp.read_bytes()
    body = source_bytes.decode('utf-8').replace('\r\n', '\n')
    signatures = (
        'static motion::Image frameCanvas(int n){',
        'static void rebuildDisplay(){',
        'static unsigned pairKey(int a,int b){',
        'static void readPreparedPairs(){',
        'static void syncDefaultDisplay(){',
        'static void resetPrepared(bool stopCurrent){',
        'static void pollOriginalResource(){',
        'static void materializeWorkProfile(){',
    )
    extracted = '\n\n'.join(function(body, name) for name in signatures) + '\n'
    adaptations = {
        'std::filesystem::create_directories(std::filesystem::path(workProfile));':
            'fixtureCreateDirectories(workProfile);',
        'std::ifstream f(std::filesystem::path(workProfile)/L"pairs.txt");':
            'auto f=fixturePreparedPairStream();',
    }
    for before, after in adaptations.items():
        assert extracted.count(before) == 1, 'Expected one filesystem seam: ' + before
        extracted = extracted.replace(before, after, 1)
    routes = out / 'production-source.inc'
    routes.write_text(extracted, encoding='utf-8')
    include = out / 'include'
    include.mkdir(exist_ok=True)
    (include / 'windows.h').write_text(
        '#pragma once\n#include <cstdint>\n'
        'using DWORD=uint32_t;using LONG=int32_t;using BYTE=uint8_t;'
        'using BOOL=int;using HANDLE=void*;\n'
        'constexpr BOOL FALSE=0,TRUE=1;constexpr DWORD MOVEFILE_WRITE_THROUGH=8;\n'
        'int wsprintfW(wchar_t*,const wchar_t*,...);\n', encoding='utf-8')
    inputs = [main_cpp, fixture, Path(__file__), here / 'runtime_broker_retirement.py']
    inputs += [cursor / (name + '.h') for name in (
        'core', 'ondemand', 'bridge', 'playback', 'motion', 'rotation',
        'super_hotspot', 'snapshot_latch', 'sequence')]
    hashes = {str(path.relative_to(source.parent)): digest(path) for path in inputs}
    if hashlib.sha256(source_bytes).hexdigest() != hashes[str(main_cpp.relative_to(source.parent))]:
        raise SystemExit('Cursor source changed during extraction')
    exe = out / 'cursor-ondemand-source'
    command = [args.cxx, '-std=c++17', '-O1', '-g', '-fsanitize=undefined',
               '-fno-sanitize-recover=all', '-I' + str(include),
               '-DSOURCE_PRODUCTION="' + str(routes) + '"', str(fixture), '-o', str(exe)]
    build = subprocess.run(command, text=True, capture_output=True)
    (out / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8')
    if build.returncode:
        raise SystemExit(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], text=True, capture_output=True)
    (out / 'run.log').write_text(run.stdout + run.stderr, encoding='utf-8')
    if run.returncode:
        raise SystemExit(run.stdout + run.stderr)
    result = json.loads(run.stdout.strip().splitlines()[-1])
    if any(digest(path) != hashes[str(path.relative_to(source.parent))] for path in inputs):
        raise SystemExit('Test inputs changed while checking')
    receipt = {
        'schema': 'ra2.cursor-ondemand-source.v1',
        'pass': result['failures'] == 0,
        'result': result,
        'build_command': command,
        'build_exit_code': build.returncode,
        'run_exit_code': run.returncode,
        'source_sha256': hashes,
        'production_routes_sha256': digest(routes),
        'production_functions': list(signatures),
        'adaptations': adaptations,
        'windows_executed': False,
        'game_tested': False,
        'network_downloads_executed': False,
        'scope': [
            'The actual pollOriginalResource/syncDefaultDisplay/resetPrepared/readPreparedPairs/materializeWorkProfile functions execute; only two filesystem expressions are redirected.',
            'Real core.h SHA encode/decode, ondemand.h content identity, actual frameCanvas/rebuildDisplay, palette premultiplication and playback frame packing execute unchanged.',
            '450/517 visible source adoption, palette-only identity changes, identical-byte reuse, custom theme preservation, invalid/transparent/uncommitted/torn resource rejection are exercised.',
            'All three immutable inputs are compared byte-for-byte; engine-derived pairs rewrites survive another epoch, and 100 hot-epoch materializations make zero filesystem calls.',
        ],
        'limits': [
            'Shared resource delivery uses the actual Shared layout with controlled bytes/sequence; MemoryBarrier injects a deterministic sequence change, not a live cross-process race.',
            'File reads/writes, directory creation, MoveFileExW/DeleteFileW, timing and hook wake/stop/cancel boundaries are deterministic substitutes; disk permissions, crash durability and Windows visibility are not proven.',
            'This fixture does not run a game, PowerShell, Python interpolation or asynchronous playback completion; those have separate focused checks.',
        ],
    }
    (out / 'receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))
    print('PASS actual Cursor source/display/cache routes; Windows/game execution not claimed.')


if __name__ == '__main__':
    main()
