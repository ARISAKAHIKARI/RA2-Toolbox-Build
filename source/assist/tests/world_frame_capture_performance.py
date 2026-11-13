"""Compare production capture with the pinned 1.0.50 source; optionally use real own-process Windows RPM."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True, type=Path)
parser.add_argument('--baseline', default='1d970c9c9b2929b2c215213e1834a70bf82178b0')
parser.add_argument('--zig', type=Path)
parser.add_argument('--windows-rpm', action='store_true')
args = parser.parse_args()
if args.windows_rpm and not args.zig:
    parser.error('--windows-rpm requires --zig')
repo = Path(__file__).resolve().parents[3]
out = args.out.resolve()
baseline = out / 'baseline'
baseline.mkdir(parents=True, exist_ok=True)
bindings = []
for name in ('world_frame.h', 'world_frame_capture.h'):
    relative = 'source/shared/source/' + name
    raw = subprocess.check_output(['git', 'show', args.baseline + ':' + relative], cwd=repo)
    text = raw.decode()
    assert text.count('namespace worldframe {') == 1
    (baseline / name).write_text(text.replace('namespace worldframe {', 'namespace baseline_worldframe {', 1))
    bindings.append({'path': relative, 'baseline_sha256': hashlib.sha256(raw).hexdigest(),
                     'current_sha256': hashlib.sha256((repo / relative).read_bytes()).hexdigest()})
binary = out / ('capture-correctness.exe' if args.windows_rpm else 'capture-correctness')
compiler = [str(args.zig.resolve()), 'c++', '-target', 'x86-windows-gnu', '-DREAL_WINDOWS_RPM'] if args.windows_rpm else ['g++']
sanitizer = [] if args.windows_rpm else ['-fsanitize=undefined', '-fno-sanitize-recover=all']
command = compiler + ['-std=c++17', '-O2', '-UNDEBUG', '-fno-exceptions'] + sanitizer + [
           '-DBASELINE_CAPTURE_HEADER="' + str(baseline / 'world_frame_capture.h') + '"',
           str(Path(__file__).with_suffix('.cpp')), '-o', str(binary)]
if args.windows_rpm:
    command.append('-lkernel32')
build = subprocess.run(command, capture_output=True, text=True)
(out / 'build.log').write_text(build.stdout + build.stderr)
if build.returncode:
    raise SystemExit(build.returncode)
run = subprocess.run([str(binary)], capture_output=True, text=True)
(out / 'run.log').write_text(run.stdout + run.stderr)
for relative in ('source/assist/source/world_frame_runtime.h', 'source/assist/tests/world_frame_core.cpp',
                 'source/assist/tests/world_frame_capture_performance.cpp', 'source/assist/tests/world_frame_capture_performance.py'):
    bindings.append({'path': relative, 'current_sha256': hashlib.sha256((repo / relative).read_bytes()).hexdigest()})
receipt = {'baseline_commit': args.baseline, 'passed': run.returncode == 0,
           'build_command': command, 'build_exit_code': build.returncode, 'run_exit_code': run.returncode,
           'source_bindings': bindings, 'windows_executed': args.windows_rpm, 'own_process_windows_rpm': args.windows_rpm, 'game_tested': False,
           'limits': ['Actual production C++ capture with synthetic bytes; Windows mode calls RPM only on this fixture process. No game FPS measurement.',
                      'Frozen baseline headers change only namespace to permit side-by-side execution.']}
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(run.stdout, end='')
if run.returncode:
    print(run.stderr, end='')
raise SystemExit(run.returncode)
