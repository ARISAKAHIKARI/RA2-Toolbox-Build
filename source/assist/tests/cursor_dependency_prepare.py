#!/usr/bin/env python3
"""Exercise actual Cursor bootstrap UI routes with controlled Win32 boundaries."""
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
    main_cpp = source / 'cursor/source/main.cpp'
    fixture = here / 'cursor_dependency_prepare.cpp'
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    body = main_cpp.read_text(encoding='utf-8')
    routes = out / 'production-preparation.inc'
    functions = (
        'static void readPreparedPairs(){',
        'static std::wstring prepareArgument(const std::wstring& value){',
        'static std::wstring preparePowerShell(){',
        'static bool cancelPrepare(){',
        'static void closePrepare(){',
        'static void materializeWorkProfile(){',
        'static std::wstring prepareTaskDirectory(){',
        'static void launchPrepare(cursorondemand::Mode mode,',
        'static void serviceAutomaticPrepare(){',
        'static void beginPrepare(){',
        'static void pollPrepare(){',
        'static void previewCurrent(){',
    )
    extracted = '\n\n'.join(function(body, name) for name in functions) + '\n'
    filesystem_call = 'std::filesystem::create_directories(std::filesystem::path(workProfile));'
    assert extracted.count(filesystem_call) == 1
    extracted = extracted.replace(filesystem_call, 'fixtureCreateDirectories(workProfile);', 1)
    stream_call = 'std::ifstream f(std::filesystem::path(workProfile)/L"pairs.txt");'
    assert extracted.count(stream_call) == 1
    extracted = extracted.replace(stream_call, 'auto f=fixturePreparedPairStream();', 1)
    routes.write_text(extracted, encoding='utf-8')
    inputs = [main_cpp, source / 'cursor/source/ondemand.h', fixture, Path(__file__), here / 'runtime_broker_retirement.py']
    hashes = {str(path.relative_to(source.parent)): digest(path) for path in inputs}
    (out / 'inputs.json').write_text(json.dumps({
        'source_sha256': hashes, 'production_routes_sha256': digest(routes),
        'executed_functions': functions,
        'filesystem_adaptations': [filesystem_call, stream_call],
    }, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    # Startup may poll the guarded automatic queue but cannot eagerly launch
    # preprocessing. Actual animation demand and explicit actions own work.
    startup = function(body, 'int WINAPI wWinMain(')
    assert 'beginPrepare(' not in startup
    assert 'launchPrepare(' not in startup
    assert 'beginPrepare();' in function(body, 'static void importStyle(){')
    assert 'case 131:beginPrepare();' in function(body, 'static LRESULT CALLBACK wndproc(')
    assert 'closePrepare();' in function(body, 'static LRESULT CALLBACK wndproc(')
    exe = out / 'cursor-dependency-prepare'
    command = [args.cxx, '-std=c++17', '-O1', '-g', '-fsanitize=undefined', '-fno-sanitize-recover=all', '-D_WIN64',
               '-DPREPARATION_PRODUCTION="' + str(routes) + '"', str(fixture), '-o', str(exe)]
    build = subprocess.run(command, text=True, capture_output=True)
    (out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], text=True, capture_output=True)
    (out / 'run.log').write_text(run.stdout + run.stderr)
    if run.returncode:
        raise SystemExit(run.stdout + run.stderr)
    result = json.loads(run.stdout.strip().splitlines()[-1])
    if any(digest(path) != hashes[str(path.relative_to(source.parent))] for path in inputs):
        raise SystemExit('Test inputs changed while checking')
    receipt = {
        'schema': 'ra2.cursor-dependency-prepare-ui.v2',
        'pass': result['failures'] == 0,
        'result': result,
        'build_command': command,
        'build_exit_code': build.returncode,
        'run_exit_code': run.returncode,
        'source_sha256': hashes,
        'production_routes_sha256': digest(routes),
        'windows_executed': False,
        'network_downloads_executed': False,
        'game_tested': False,
        'scope': [
            'Actual production materialize/launch/serviceAutomatic/begin/poll/cancel/close/preview and command-line builders execute against controlled Win32 process, file, INI and label boundaries; ondemand.h is included unchanged.',
            'Directory creation and the generated-pairs ifstream are deterministic filesystem boundaries; the complete materialization comparison, temporary write/MoveFileExW publication and generated-pairs parser execute.',
            'Missing-runtime launch, literal arguments, phase progress, failures, retries, cancellation, profile switch and zero-timeout waits are exercised.',
            'Every retry and quick close/reopen uses a unique task directory; old cancellation flags and status/error files are retained, and filename collisions never overwrite prior tasks.',
            'Source entry checks prevent eager startup launches; empty/blocked/unconfirmed animation demand and explicit preview/full-preparation routes execute separately.',
            'Automatic batches contain at most four observed pairs; failures/cancellation remain blocked, and successful same-source work only schedules playback loading without stopping the hook.',
            'Source/epoch changes reject late success/failure effects; previews open only after an explicit action, and malformed generated pair indexes cannot partially replace a valid index.',
        ],
        'limits': [
            'PowerShell execution, HTTP downloads, dependency installation and native Windows UI are not executed by this fixture.',
            'The quote consumer checks Microsoft command-line rules in a portable fixture; it is not a Windows shell execution.',
            'ExecutionPolicy Bypass applies only to the app-owned PowerShell process; no system/user policy is modified, and Group Policy remains authoritative.',
        ],
    }
    (out / 'receipt.json').write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))
    print('PASS actual Cursor preparation routes; Windows/game/network execution not claimed.')


if __name__ == '__main__':
    main()
