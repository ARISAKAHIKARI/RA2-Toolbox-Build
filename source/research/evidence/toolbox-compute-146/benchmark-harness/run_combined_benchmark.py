"""Replay production capture + CRT loops on identical synthetic Linux worlds.
This bounds work counts and C++ CPU cost; no native RPM or game execution.
"""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import sys

repo = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path.cwd()
root = Path(__file__).resolve().parent
out = root / 'benchmark'
out.mkdir(exist_ok=True)
base = out / 'baseline'
base.mkdir(exist_ok=True)
revision = '88812eda0d23d065d61d6f742315b7d0581fd1a8'
bindings = []
def digest(raw):
    return hashlib.sha256(raw).hexdigest()
for name in ['world_frame.h', 'world_frame_capture.h']:
    rel = 'source/shared/source/' + name
    raw = subprocess.check_output(['git', 'show', revision + ':' + rel], cwd=repo)
    (base / name).write_bytes(raw)
    bindings.append({'path': rel, 'baseline_sha256': digest(raw), 'current_sha256': digest((repo / rel).read_bytes())})
rel = 'source/assist/source/bridge.cpp'
old = subprocess.check_output(['git', 'show', revision + ':' + rel], cwd=repo)
new = (repo / rel).read_bytes()
bindings.append({'path': rel, 'baseline_sha256': digest(old), 'current_sha256': digest(new)})
helper = repo / 'source/shared/source/runtime_memory.h'
bindings.append({'path': str(helper.relative_to(repo)), 'current_sha256': digest(helper.read_bytes())})
def extract(text, name):
    start = text.index('extern "C" void* __cdecl ' + name + '(')
    body = text.index('{', start)
    depth = 1
    end = body + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:body].replace('__cdecl ', '') + 'noexcept' + text[body:end]
wrappers = {}
for name, raw in [('old', old), ('new', new)]:
    text = raw.decode()
    contents = '#include <cstring>\n#include <cstdint>\n'
    if name == 'new':
        contents += '#include "' + str(helper) + '"\n'
    contents += '\n'.join(extract(text, function) for function in ['memset', 'memcpy']) + '\n'
    target = out / (name + '-crt.h')
    target.write_text(contents)
    wrappers[name] = target
    bindings.append({'path': str(target), 'sha256': digest(contents.encode()), 'adaptation': 'Exact frozen/live wrapper bodies; __cdecl removed and noexcept added only for Linux C linkage.'})
source = root / 'capture_benchmark.cpp'
bindings.append({'path': str(source), 'sha256': digest(source.read_bytes())})
cpu = max(os.sched_getaffinity(0))
os.sched_setaffinity(0, {cpu})
results = {}
commands = {}
for name, capture, crt, reused in [
    ('baseline-old-crt', base / 'world_frame_capture.h', 'old', False),
    ('subject-old-crt', repo / 'source/shared/source/world_frame_capture.h', 'old', True),
    ('subject-new-crt', repo / 'source/shared/source/world_frame_capture.h', 'new', True),
]:
    binary = out / name
    command = ['g++', '-std=c++17', '-O2', '-fno-exceptions', '-fno-builtin',
               '-DCAPTURE_HEADER="' + str(capture) + '"',
               '-DCRT_HEADER="' + str(wrappers[crt]) + '"']
    if reused:
        command.append('-DREUSE_CAPTURE_SCRATCH')
    command += [str(source), '-o', str(binary)]
    commands[name] = command
    build = subprocess.run(command, capture_output=True, text=True)
    (out / (name + '-build.log')).write_text(build.stdout + build.stderr)
    if build.returncode:
        raise SystemExit(build.returncode)
    run = subprocess.run([str(binary)], capture_output=True, text=True)
    (out / (name + '.jsonl')).write_text(run.stdout)
    (out / (name + '-stderr.log')).write_text(run.stderr)
    if run.returncode:
        raise SystemExit(run.returncode)
    results[name] = [json.loads(line) for line in run.stdout.splitlines()]
    print(name + ': ' + run.stdout, end='', flush=True)
for name, rows in results.items():
    for baseline, row in zip(results['baseline-old-crt'], rows):
        for field in ['case', 'actors', 'demand', 'requested_extra_pages', 'pages', 'read_bytes_per_frame', 'rosters', 'payload_fnv64']:
            assert baseline[field] == row[field], (name, field, baseline, row)
receipt = {'passed': True, 'baseline_commit': revision, 'cpu_affinity': [cpu], 'build_commands': commands,
           'source_bindings': bindings, 'results': results, 'exact_active_page_hashes_equal': True,
           'windows_executed': False, 'game_tested': False,
           'limits': ['Single Linux x86_64 CPU, one warm iteration followed by 24 measured frames per same synthetic world; no Windows kernel RPM latency or game FPS.',
                      'Actual production capture headers and exact production CRT function bodies compiled with -fno-builtin; synthetic reader uses the same CRT memcpy, unlike native RPM.',
                      'Allocation counts include C++ new/delete and injected Snapshot/CaptureScratch PageAllocator; elapsed time excludes fixture setup and payload hashing.',
                      'Sparse failed read fallback and identity mutation correctness are verified separately; these timing fixtures use fully readable memory.']}
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
