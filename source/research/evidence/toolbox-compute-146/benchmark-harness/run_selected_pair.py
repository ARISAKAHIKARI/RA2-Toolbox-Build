"""One paired replay of the frozen compiled binaries; preserves earlier runs."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
root = Path(__file__).resolve().parent
out = root / 'selected-benchmark'
os.sched_setaffinity(0, {max(os.sched_getaffinity(0))})
results = {}
for name, binary in [('baseline', root / 'benchmark/baseline-old-crt'), ('selected', out / 'capture-selected')]:
    run = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
    (out / (name + '-paired.jsonl')).write_text(run.stdout)
    results[name] = [json.loads(line) for line in run.stdout.splitlines()]
    print(name, run.stdout, end='', flush=True)
receipt = json.loads((out / 'receipt.json').read_text())
receipt.setdefault('contended_results', receipt['results'])
receipt.setdefault('previous_baseline_results', receipt['baseline_results'])
receipt['results'] = results['selected']
receipt['baseline_results'] = results['baseline']
receipt['timing_replay_reason'] = 'First final benchmark overlapped core compilation/execution and showed large scheduling variation; replayed frozen baseline/selected binaries once sequentially after core completed, same CPU 8.'
for before, after in zip(results['baseline'], results['selected']):
    for field in ['case', 'actors', 'demand', 'requested_extra_pages', 'pages', 'read_calls_per_frame', 'read_bytes_per_frame', 'rosters', 'payload_fnv64']:
        assert before[field] == after[field]
receipt['binary_bindings'] = [{'path': str(p), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()} for p in [root / 'benchmark/baseline-old-crt', out / 'capture-selected']]
(out / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
