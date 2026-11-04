"""Compile isolated read-only rendering fixtures; never opens the game.

--baseline-root is a materialized 2bb8215 repository root (only the three
referenced headers are needed). Exact SHA bindings and commands go in result.
The large-map comparison explicitly lifts only the baseline Live's cell-count
admission, because original Live rejects maps beyond 65536 cells outright.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--zig', type=Path, required=True)
    parser.add_argument('--baseline-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--resume', action='store_true')
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    baseline = args.baseline_root.resolve() / 'source/external_reader/source'
    originals = {name: (baseline / name).read_text(encoding='utf-8-sig')
                 for name in ('radar_scene.h', 'radar_live_shroud.h', 'gpu_scene_record.h')}
    scene = originals['radar_scene.h']
    scene = 'namespace external { namespace radar {\n' + scene[scene.index('template<class Assets>class SceneRenderer'):]
    scene = scene.replace('class SceneRenderer', 'class BeforeSceneRenderer').replace('explicit SceneRenderer(', 'explicit BeforeSceneRenderer(')
    (out / 'scene_reference.h').write_text(scene, encoding='utf-8')
    record = originals['gpu_scene_record.h']
    record = 'namespace external { namespace camera {\n' + record[record.index('class GpuSceneRecord'):]
    (out / 'record_reference.h').write_text(record.replace('GpuSceneRecord', 'BeforeGpuSceneRecord'), encoding='utf-8')
    live = originals['radar_live_shroud.h']
    live = live[live.index('class Live'):]
    prefix = 'namespace external { namespace radar { namespace shroudmask {\n'
    (out / 'live_reference.h').write_text(prefix + live.replace('class Live', 'class BeforeLive') + '\n' +
        prefix + live.replace('class Live', 'class BeforeLiftedLive').replace('candidate.cells.size()>=65536', 'candidate.cells.size()>=512*512'), encoding='utf-8')
    changed = ['radar_scene.h', 'radar_live_shroud.h', 'radar_map.h', 'gpu_scene_record.h',
               'gpu_motion.h', 'gpu_shroud.h', 'vulkan_scene_types.h', 'vulkan_scene.h',
               'vulkan_shroud_backend.h', 'viewport_canvas.h']
    files = [source / 'external_reader/source' / name for name in changed]
    files += [source / 'assist/source/minimap_overlay.h', source / 'third_party/xbrz/xbrz.cpp']
    tests = ['radar_sampler_compact', 'radar_live_exploration', 'cpu_terrain_prefix', 'radar_large_map',
             'radar_visibility_publication', 'radar_boundary', 'radar_shroud_mask', 'radar_output_scene', 'radar_cpu_reuse']
    files += [source / 'assist/tests' / (name + '.cpp') for name in tests]
    files += [Path(__file__).resolve()]
    hashes = {str(path.relative_to(source)): digest(path) for path in files}
    prior = None
    if args.resume and (out / 'result.json').is_file():
        prior = json.loads((out / 'result.json').read_text(encoding='utf-8'))
        failed_tests = {str(Path('assist/tests') / (case['name'] + '.cpp')) for case in prior['results']
                        if case['build_exit_code'] or not case.get('runs') or any(run['exit_code'] for run in case['runs'])}
        for name, value in hashes.items():
            if name != str(Path(__file__).resolve().relative_to(source)) and name not in failed_tests:
                assert prior['source_sha256'].get(name) == value, 'Cannot reuse changed source: ' + name
        assert prior['baseline_sha256'] == {name: digest(baseline / name) for name in originals}, 'Baseline changed'
        (out / 'result-before-resume.json').write_text(json.dumps(prior, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    cases = [(name, arch) for name in tests for arch in ('x64', 'x86')
             if arch == 'x64' or name in tests[:4]]
    env = dict(os.environ)

    def execute(case):
        name, arch = case
        exe = out / (name + '-' + arch + '.exe')
        command = [str(args.zig.resolve()), 'c++', '-target', 'x86_64-windows-gnu' if arch == 'x64' else 'x86-windows-gnu',
                   '-std=c++17', '-O2', '-UNDEBUG', '-w', str(source / 'assist/tests' / (name + '.cpp'))]
        for macro, header in [('RA2_REFERENCE_SCENE', 'scene_reference.h'), ('RA2_REFERENCE_RECORD', 'record_reference.h'), ('RA2_REFERENCE_LIVE', 'live_reference.h')]:
            command.append('-D' + macro + '="' + str((out / header).as_posix()) + '"')
        if name not in ('radar_sampler_compact', 'radar_visibility_publication', 'radar_shroud_mask'):
            command += [str(source / 'third_party/xbrz/xbrz.cpp'), '-lgdi32', '-luser32']
        command += ['-o', str(exe)]
        if prior:
            old = next((case for case in prior['results'] if case['name'] == name and case['arch'] == arch), None)
            test_key = str(Path('assist/tests') / (name + '.cpp'))
            if old and prior['source_sha256'].get(test_key) == hashes[test_key] and old['command'] == command and old['build_exit_code'] == 0 and old.get('runs') and all(run['exit_code'] == 0 for run in old['runs']) and exe.is_file() and digest(exe) == old['exe_sha256']:
                print(name + ' ' + arch + ' PASS (unchanged command/source/binary)', flush=True)
                return old
        started = time.monotonic()
        result = subprocess.run(command, env=env, cwd=source, capture_output=True, text=True, timeout=600)
        (out / (name + '-' + arch + '-build.log')).write_text(result.stdout + result.stderr, encoding='utf-8')
        if result.returncode:
            return {'name': name, 'arch': arch, 'command': command, 'build_exit_code': result.returncode,
                    'build_seconds': time.monotonic() - started}
        runs = []
        for n in range(3 if name == 'cpu_terrain_prefix' else 1):
            run = subprocess.run([str(exe)], env=env, capture_output=True, text=True, timeout=300)
            runs.append({'exit_code': run.returncode, 'output': run.stdout + run.stderr})
            if run.returncode:
                break
        print(name + ' ' + arch + ' ' + ('PASS' if all(r['exit_code'] == 0 for r in runs) else 'FAIL'), flush=True)
        return {'name': name, 'arch': arch, 'command': command, 'build_exit_code': result.returncode,
                'build_seconds': time.monotonic() - started, 'runs': runs, 'exe_sha256': digest(exe)}

    with ThreadPoolExecutor(max_workers=max(1, min(3, args.jobs))) as pool:
        results = list(pool.map(execute, cases))
    assert hashes == {str(path.relative_to(source)): digest(path) for path in files}, 'Source changed during fixture build'
    passed = all(case['build_exit_code'] == 0 and case.get('runs') and all(run['exit_code'] == 0 for run in case['runs']) for case in results)
    report = {'passed': passed, 'baseline_commit': '2bb821569c19e79a5869ec043b2ee47fcade1d7a',
              'source_sha256': hashes, 'baseline_sha256': {name: digest(baseline / name) for name in originals},
              'results': results, 'live_game_tested': False, 'hardware_gpu_tested': False,
              'notes': ['Large visibility performance comparison lifts only the original 65536-cell admission; original admission is separately rejected.',
                        'GPU 16-bit draw-reference admission remains 65536; 262144 is the bounded CPU recording/replay ceiling.',
                        'Page and prefix byte bounds are held/charged payloads, not process resident memory.']}
    (out / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
