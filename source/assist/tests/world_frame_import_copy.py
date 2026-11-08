"""Run actual baseline/current import paths with a namespace-only baseline copy."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--out', required=True, type=Path)
parser.add_argument('--baseline', default='e6a157fe4b51e330915eaaa58f8192f5cdbce168')
parser.add_argument('--zig', type=Path, help='Optionally verify Windows x64 optimized import initialization operations.')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
relative = 'source/shared/source/world_frame.h'
raw = subprocess.check_output(['git', 'show', args.baseline + ':' + relative], cwd=repo)
text = raw.decode()
assert text.count('namespace worldframe {') == 1
baseline = out / 'baseline-world-frame.h'
baseline.write_text(text.replace('namespace worldframe {', 'namespace baseline_worldframe {', 1))
command = ['g++', '-std=c++17', '-O2', '-fno-exceptions', '-fsanitize=undefined', '-fno-sanitize-recover=all',
           '-DBASELINE_WORLD_FRAME_HEADER="' + str(baseline) + '"',
           str(Path(__file__).with_suffix('.cpp')), '-o', str(out / 'import-copy')]
build = subprocess.run(command, capture_output=True, text=True)
(out / 'build.log').write_text(build.stdout + build.stderr)
if build.returncode:
    raise SystemExit(build.returncode)
run = subprocess.run([str(out / 'import-copy')], capture_output=True, text=True)
(out / 'run.log').write_text(run.stdout + run.stderr)
codegen = {}
if args.zig:
    original = out / 'baseline-production-world-frame.h'
    original.write_bytes(raw)
    probe = out / 'import-codegen.cpp'
    probe.write_text('#include WORLD_FRAME_HEADER\nextern "C" bool importWorld(worldframe::Snapshot*s,const worldframe::Key*k,const worldframe::Counters*c,const worldframe::Page*p,size_t n){return s->import(*k,*c,p,n);}\n')
    for label, header in [('baseline', original), ('current', repo / relative)]:
        ir = out / (label + '-import.ll')
        cmd = [str(args.zig.resolve()), 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-O2',
               '-S', '-emit-llvm', '-DWORLD_FRAME_HEADER="' + str(header) + '"', str(probe), '-o', str(ir)]
        result = subprocess.run(cmd, capture_output=True, text=True)
        (out / (label + '-codegen.log')).write_text(result.stdout + result.stderr)
        result.check_returncode()
        body = ir.read_text()
        codegen[label] = {'command': cmd, 'ir_sha256': hashlib.sha256(ir.read_bytes()).hexdigest(),
                          'whole_page_zero_sites': len(re.findall(r'call void @llvm\.memset[^\n]*\bi64 5128\b', body)),
                          'whole_page_copy_sites': len(re.findall(r'call void @llvm\.memcpy[^\n]*\bi64 5128\b', body))}
    assert codegen['baseline']['whole_page_zero_sites'] == 1
    assert codegen['current']['whole_page_zero_sites'] == 0
    assert codegen['current']['whole_page_copy_sites'] > 0
paths = [relative, 'source/shared/source/world_frame_transport.h',
         'source/assist/tests/world_frame_import_copy.cpp', 'source/assist/tests/world_frame_import_copy.py']
receipt = {'passed': run.returncode == 0, 'baseline_commit': args.baseline,
           'baseline_header_sha256': hashlib.sha256(raw).hexdigest(),
           'build_command': command, 'run_exit_code': run.returncode,
           'windows_x64_codegen': codegen,
           'source_bindings': [{'path': p, 'sha256': hashlib.sha256((repo / p).read_bytes()).hexdigest()} for p in paths],
           'windows_executed': False, 'game_tested': False,
           'limits': ['Actual import functions with synthetic wire pages; baseline changes namespace only.',
                      'No native Windows shared-memory race execution or game FPS measurement.']}
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
print(run.stdout + run.stderr, end='')
raise SystemExit(run.returncode)
