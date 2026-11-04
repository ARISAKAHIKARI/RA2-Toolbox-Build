"""Check production hot signature/counter helpers; benchmark signature validation only."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--zig', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    here = Path(__file__).resolve().parent
    header = here.parent / 'source' / 'tech_ui_breakpoint_hot.h'
    source = here / 'tech_ui_breakpoint_hot.cpp'
    paths = [header, source, Path(__file__).resolve()]
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    hashes = {str(path): sha(path) for path in paths}
    exe = args.out.resolve() / ('tech-ui-breakpoint-hot.exe' if args.zig else 'tech-ui-breakpoint-hot')
    flags = ['-std=c++17', '-O2', '-UNDEBUG']
    if args.zig:
        command = [str(args.zig.resolve()), 'c++', '-target', 'x86-windows-gnu', *flags,
                   '-fno-exceptions', '-fno-rtti', '-nostdlib++', str(source), '-o', str(exe), '-lkernel32']
    else:
        command = [args.cxx, *flags, str(source), '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (args.out / 'build.log').write_text(build.stdout + build.stderr)
    if build.returncode:
        raise RuntimeError(build.stdout + build.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=45)
    (args.out / 'run.log').write_text(run.stdout + run.stderr)
    print(run.stdout, end='')
    if run.returncode:
        raise RuntimeError(run.stderr or f'fixture exited {run.returncode}')
    result = json.loads(run.stdout)
    assert result['pass'] and not result['windows_veh_measured'] and not result['game_process_tested']
    assert hashes == {str(path): sha(path) for path in paths}
    report = {
        'pass': True,
        'scope': __doc__,
        'compiler_flags': flags,
        'source_sha256': hashes,
        'exe_sha256': sha(exe),
        'result': result,
    }
    (args.out / 'result.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    main()
