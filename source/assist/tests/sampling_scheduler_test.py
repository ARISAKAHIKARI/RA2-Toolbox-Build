"""Run actual current/pre.7 headers with controlled Win32 I/O and wake counters."""
from pathlib import Path
import argparse
import ast
import hashlib
import json
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--out', type=Path, required=True)
p.add_argument('--baseline', required=True)
a = p.parse_args()
repo = Path(__file__).resolve().parents[3]
source = repo / 'source'
out = a.out.resolve()
out.mkdir(parents=True, exist_ok=True)
seeds = [Path(__file__).with_name(n) for n in ('sampling_polling.cpp', 'recurring_renewals.cpp')]
closure = {}
pending = list(seeds)
while pending:
    path = pending.pop().resolve()
    if path in closure:
        continue
    closure[path] = path.read_bytes()
    for name in re.findall(r'^\s*#include\s*"([^"]+)"', closure[path].decode(), re.M):
        child = (path.parent / name).resolve()
        if child.is_file() and child.is_relative_to(source):
            pending.append(child)
support = {path: path.read_bytes() for path in (source/'tests/winmock/windows.h', Path(__file__).with_name('world_frame_core_test.py'), Path(__file__))}
module = ast.parse(support[Path(__file__).with_name('world_frame_core_test.py')].decode())
windows = support[source/'tests/winmock/windows.h'].decode()
windows += next(ast.literal_eval(n.value) for n in module.body if isinstance(n, ast.AugAssign) and isinstance(n.target, ast.Name) and n.target.id == 'windows')
(out/'windows.h').write_text(windows)
results = {}
baseline_hashes = {}
for label in ('baseline', 'current'):
    work = out / label
    for path, raw in closure.items():
        relative = path.relative_to(repo)
        if label == 'baseline' and '/tests/' not in str(relative):
            raw = subprocess.check_output(['git', 'show', a.baseline+':'+str(relative)], cwd=repo)
            baseline_hashes[str(relative)] = hashlib.sha256(raw).hexdigest()
        target = work / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
    header = work/'source/shared/source/world_frame_transport.h'
    text = header.read_text()
    text = text.replace('namespace worldframe {', 'namespace worldframe { inline uint64_t demandProbeSweeps=0;', 1)
    assert text.count('uint32_t combined=0') == 1
    text = text.replace('uint32_t combined=0', '++demandProbeSweeps;uint32_t combined=0', 1)
    header.write_text(text)
    header = work/'source/shared/source/deadline_wake.h'
    text = header.read_text().replace('#include <chrono>', '#include <chrono>\n#include <atomic>', 1)
    text = text.replace('namespace asyncwork {', 'namespace asyncwork { inline std::atomic<uint64_t> wakeProbeRequests{0};', 1)
    assert text.count('void notify_one(){') == 1
    text = text.replace('void notify_one(){', 'void notify_one(){++wakeProbeRequests;', 1)
    header.write_text(text)
    results[label] = {}
    for seed in seeds:
        binary = work/seed.stem
        command = ['g++', '-std=c++17', '-O2', '-pthread', '-fsanitize=undefined', '-fno-sanitize-recover=all', '-I'+str(out)]
        if label == 'current':
            command += ['-DEXPECT_POLL_FAST', '-DEXPECT_RENEWAL_FAST']
        command += [str(work/seed.relative_to(repo)), '-o', str(binary)]
        build = subprocess.run(command, capture_output=True, text=True)
        (work/(seed.stem+'-build.log')).write_text(build.stdout+build.stderr)
        build.check_returncode()
        run = subprocess.run([str(binary)], capture_output=True, text=True, timeout=60)
        (work/(seed.stem+'-run.log')).write_text(run.stdout+run.stderr)
        print(label, seed.stem, run.stdout+run.stderr, end='')
        run.check_returncode()
        results[label][seed.stem] = {'command':command, 'compile_exit_code':build.returncode, 'exit_code':run.returncode,
            'metrics':[json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]}
bindings = {str(path.relative_to(repo)):hashlib.sha256(raw).hexdigest() for path, raw in (closure|support).items()}
assert all(hashlib.sha256((repo/path).read_bytes()).hexdigest()==sha for path,sha in bindings.items())
(out/'receipt.json').write_text(json.dumps({'passed':True, 'baseline_commit':a.baseline, 'baseline_hashes':baseline_hashes,
    'source_bindings':bindings, 'source_unchanged':True, 'results':results, 'windows_executed':False, 'game_tested':False,
    'limits':['Actual production C++ Client/Server/ProcessReader and recurring worker.',
              'Controlled virtual clock/mappings for sampling; actual std::thread for scheduling.',
              'Only counter insertions in copied demand/wake functions; no Windows timer resolution or game FPS claim.']},indent=2)+'\n')
