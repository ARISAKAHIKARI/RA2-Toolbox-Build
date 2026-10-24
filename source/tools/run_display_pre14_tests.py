"""Focused host display regression; optional original SHA/palette stay private."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--sha', type=Path)
parser.add_argument('--palette', type=Path)
parser.add_argument('--output', type=Path, default=ROOT/'verification/review-137-pre14')
args = parser.parse_args()
assert bool(args.sha) == bool(args.palette)
args.output.mkdir(parents=True, exist_ok=True)
native = [
    'assist/cursor_visual_anchor', 'assist/unit_status', 'assist/status_overlay',
    'assist/label_motion', 'assist/enemy_markers', 'assist/watch_host',
    'assist/watch_view', 'assist/watch_slots', 'assist/formation_host',
    'assist/scene_lighting', 'assist/visual_pose', 'assist/terrain_preview',
    'assist/voxel_preview', 'assist/pacing', 'unit_monitor/overlay_recovery',
    'unit_monitor/round_transition',
]
python = ['watch_windows_test', 'enemy_overlay_test', 'cursor_settings_ui_test',
          'ui_geometry_test', 'persistent_ui_source_test', 'suite_display_test']
with tempfile.TemporaryDirectory(prefix='ra2-display-') as temp:
    def run(item):
        name, command = item
        results = []
        for cmd in command:
            proc = subprocess.run(cmd, cwd=ROOT, text=True, capture_output=True)
            results.append(proc.stdout+proc.stderr)
            if proc.returncode:
                break
        (args.output/(name+'.log')).write_text('\n'.join(results))
        return {'test': name, 'status': 'PASS' if proc.returncode == 0 else 'FAIL',
                'exit_code': proc.returncode}
    jobs = []
    for name in native:
        module, test = name.split('/')
        exe = str(Path(temp)/(module+'-'+test))
        invocation = [exe]
        if test == 'cursor_visual_anchor' and args.sha:
            invocation += [str(args.sha.resolve()), str(args.palette.resolve())]
        jobs.append((module+'-'+test, [
            ['g++', '-O2', '-std=c++17', str(ROOT/module/'tests'/(test+'.cpp')), '-o', exe],
            invocation,
        ]))
    jobs += [(test, [[sys.executable, str(ROOT/'assist/tests'/(test+'.py'))]]) for test in python]
    rows = list(ThreadPoolExecutor(max_workers=4).map(run, jobs))
summary = {'scope': __doc__, 'suites': len(rows), 'results': rows,
           'windows_visual_tested': False, 'physical_refresh_measured': False,
           'multiplayer_tested': False}
if args.sha:
    summary['private_resource_sha256'] = {
        'cursor': hashlib.sha256(args.sha.read_bytes()).hexdigest(),
        'palette': hashlib.sha256(args.palette.read_bytes()).hexdigest(),
    }
(args.output/'host-tests.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps(summary, indent=2))
sys.exit(any(row['exit_code'] for row in rows))
