"""Compare real baseline/current subscription behavior and full-table visits."""
from pathlib import Path
import argparse
import ast
import filecmp
import hashlib
import json
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
parser.add_argument('--baseline', default='e5e9abd69cdf2963f7bef3dd8fbc3e12c4537a63')
args = parser.parse_args()
repo = Path(__file__).resolve().parents[3]
source = repo / 'source'
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
seed = Path(__file__).with_suffix('.cpp').resolve()
closure = {}
pending = [seed]
while pending:
    path = pending.pop()
    if path in closure:
        continue
    raw = path.read_bytes()
    closure[path] = raw
    for name in re.findall(r'^\s*#include\s*"([^"]+)"', raw.decode(), re.M):
        child = (path.parent / name).resolve()
        if child.is_file() and child.is_relative_to(source):
            pending.append(child)
support = {path: path.read_bytes() for path in [source / 'tests/winmock/windows.h', source / 'assist/tests/world_frame_core_test.py', Path(__file__).resolve()]}
windows = support[source / 'tests/winmock/windows.h'].decode()
module = ast.parse(support[source / 'assist/tests/world_frame_core_test.py'].decode())
windows += next(ast.literal_eval(n.value) for n in module.body if isinstance(n, ast.AugAssign) and isinstance(n.target, ast.Name) and n.target.id == 'windows')
(out / 'windows.h').write_text(windows)
transport = source / 'shared/source/world_frame_transport.h'
baseline = subprocess.check_output(['git', 'show', args.baseline + ':' + str(transport.relative_to(repo))], cwd=repo)
results = {}
for label in ['baseline', 'current']:
    work = out / label
    for path, raw in closure.items():
        target = work / path.relative_to(repo)
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(baseline if path == transport and label == 'baseline' else raw)
    header = work / transport.relative_to(repo)
    text = header.read_text()
    text = text.replace('namespace worldframe {', 'namespace worldframe { inline uint64_t subscriptionProbeVisits=0,subscriptionProbeSweeps=0;', 1)
    loop = 'for(auto it=wanted_.begin();it!=wanted_.end();)'
    assert text.count(loop) == 1
    text = text.replace(loop, '++subscriptionProbeSweeps;' + loop, 1)
    old = 'if(uint32_t(now-it->second)>250)'
    new = 'const uint32_t age=uint32_t(now-it->second);'
    if old in text:
        text = text.replace(old, 'if((++subscriptionProbeVisits,uint32_t(now-it->second)>250))', 1)
    else:
        assert text.count(new) == 1
        text = text.replace(new, 'const uint32_t age=(++subscriptionProbeVisits,uint32_t(now-it->second));', 1)
    header.write_text(text)
    binary = work / 'deadlines'
    command = ['g++', '-std=c++17', '-O2', '-pthread', '-fsanitize=undefined', '-fno-sanitize-recover=all',
               '-DWORLD_FRAME_SCAN_PROBE', '-I' + str(out)]
    if label == 'current':
        command.append('-DEXPECT_DEADLINE')
    command += [str(work / seed.relative_to(repo)), '-o', str(binary)]
    build = subprocess.run(command, capture_output=True, text=True)
    (work / 'build.log').write_text(build.stdout + build.stderr)
    build.check_returncode()
    run = subprocess.run([str(binary), str(work / 'trace.bin')], capture_output=True, text=True)
    (work / 'run.log').write_text(run.stdout + run.stderr)
    results[label] = {'exit_code': run.returncode, 'command': command,
                      'probe_header_sha256': hashlib.sha256(header.read_bytes()).hexdigest()}
    if run.returncode:
        print(run.stdout + run.stderr)
        raise SystemExit(run.returncode)
    results[label]['metrics'] = [json.loads(line) for line in run.stdout.splitlines()]
    results[label]['trace_sha256'] = hashlib.sha256((work / 'trace.bin').read_bytes()).hexdigest()
    print(label + ': ' + run.stdout, end='')
equal = filecmp.cmp(out / 'baseline/trace.bin', out / 'current/trace.bin', shallow=False)
assert equal, 'Actual per-operation published subscription bytes changed'
bindings = [{'path': str(path.relative_to(repo)), 'sha256': hashlib.sha256(raw).hexdigest()} for path, raw in (closure | support).items()]
unchanged = all(hashlib.sha256((repo / b['path']).read_bytes()).hexdigest() == b['sha256'] for b in bindings)
assert unchanged, 'Source changed while executing'
receipt = {'passed': True, 'baseline_commit': args.baseline, 'baseline_transport_sha256': hashlib.sha256(baseline).hexdigest(),
           'source_bindings': bindings, 'source_unchanged': unchanged, 'results': results, 'exact_trace_bytes_equal': equal,
           'trace_bytes': (out / 'current/trace.bin').stat().st_size,
           'windows_executed': False, 'game_tested': False,
           'limits': ['Actual Client/Server and existing OS fixture; probe copies add only sweep/entry counters at the original loop.',
                      'Per-operation wire state and complete page lists are compared byte-for-byte; unchanged lists use lossless repeat markers.',
                      'Thread test uses real C++ threads and Client mutex; no native Windows memory mappings or game FPS measured.']}
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
