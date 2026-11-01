import argparse
import json
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--zig', required=True)
p.add_argument('--baseline-header', required=True)
p.add_argument('--out', required=True)
p.add_argument('--baseline-only', action='store_true')
a = p.parse_args()
source = Path(__file__).resolve().parent
external = source.parent.parent / 'external_reader/source'
out = Path(a.out).resolve()
out.mkdir(parents=True, exist_ok=True)
results = {}
for name in ('before',) if a.baseline_only else ('before', 'after'):
    exe = out / (name + '.exe')
    cmd = [str(Path(a.zig).resolve()), 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-O2', '-UNDEBUG',
           str(source / 'palette_read_cost.cpp'), '-I' + str(external), '-o', str(exe), '-lgdi32', '-luser32']
    if name == 'before':
        cmd += ['-DPALETTE_BASELINE', '-DCAMERA_ASSETS_HEADER="' + Path(a.baseline_header).resolve().as_posix() + '"']
    built = subprocess.run(cmd, capture_output=True, text=True)
    (out / (name + '-build.log')).write_text(built.stdout + built.stderr, encoding='utf-8')
    if built.returncode:
        raise SystemExit(built.stderr)
    run = subprocess.run([str(exe)], capture_output=True, text=True)
    (out / (name + '.log')).write_text(run.stdout + run.stderr, encoding='utf-8')
    if run.returncode:
        raise SystemExit(run.stdout + run.stderr)
    results[name] = json.loads(run.stdout.strip().splitlines()[-1])
if 'after' in results:
    assert results['before']['digest'] == results['after']['digest']
    assert results['before']['paletteReadsPerFrame'] == results['after']['paletteReadsPerFrame']
    assert results['after']['readsPerFrame'] < results['before']['readsPerFrame']
    results['ratio'] = results['after']['medianMs'] / results['before']['medianMs']
(out / 'result.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
print(json.dumps(results, indent=2))
