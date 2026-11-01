"""Compare actual SceneRenderer with saved pre-change terrain/light leaves."""
import argparse
import hashlib
from pathlib import Path
import re
import subprocess
p = argparse.ArgumentParser()
p.add_argument('--zig', required=True)
p.add_argument('--baseline-scene', required=True)
p.add_argument('--baseline-lighting', required=True)
p.add_argument('--build-dir', required=True)
p.add_argument('--reader-only', action='store_true')
a = p.parse_args()
source = Path(__file__).resolve()
headers = source.parents[2] / 'external_reader' / 'source'
build = Path(a.build_dir).resolve()
build.mkdir(parents=True, exist_ok=True)
old_light = Path(a.baseline_lighting).read_text(encoding='utf-8-sig')
begin = old_light.index('template<class Read>State cell(')
end = old_light.index('\ntemplate<class Read>uint32_t cellAt', begin)
leaf = build / 'lighting_reference.h'
leaf.write_text('namespace external { namespace lighting {\n' + old_light[begin:end].replace('State cell(', 'State cell_before(', 1) + '\n}}\n', encoding='utf-8')
old_scene = Path(a.baseline_scene).read_text(encoding='utf-8-sig')
old_scene = old_scene.replace('struct SceneRegion {double x=0,y=0,width=0,height=0;bool active=false;};', '')
old_scene = old_scene.replace('class SceneRenderer', 'class BeforeSceneRenderer').replace('explicit SceneRenderer(', 'explicit BeforeSceneRenderer(')
old_scene = old_scene.replace('lighting::cell(', 'lighting::cell_before(')
old_scene = re.sub(r'#include "([^"]+)"', lambda m: '#include "' + (headers / m.group(1)).resolve().as_posix() + '"', old_scene)
scene = build / 'scene_reference.h'
scene.write_text(old_scene, encoding='utf-8')
wrapper = build / 'fixture.cpp'
fixture = source.with_name('terrain_light_reads.cpp') if a.reader_only else source.with_suffix('.cpp')
wrapper.write_text('#define RA2_REFERENCE_LIGHTING "' + leaf.as_posix() + '"\n#define RA2_REFERENCE_SCENE "' + scene.as_posix() + '"\n#include "' + fixture.as_posix() + '"\n', encoding='utf-8')
exe = build / ('terrain_light_reads.exe' if a.reader_only else 'terrain_presentation_performance.exe')
cmd = [str(Path(a.zig).resolve()), 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-O2', str(wrapper), str(source.parents[2] / 'third_party' / 'xbrz' / 'xbrz.cpp'), '-I', str(headers), '-o', str(exe), '-lgdi32', '-luser32']
result = subprocess.run(cmd, capture_output=True, text=True)
(build / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8')
if result.returncode:
    print(result.stdout + result.stderr)
    raise SystemExit(result.returncode)
result = subprocess.run([str(exe)], capture_output=True, text=True)
report = 'baseline_scene_sha256=' + hashlib.sha256(Path(a.baseline_scene).read_bytes()).hexdigest() + '\nbaseline_lighting_sha256=' + hashlib.sha256(Path(a.baseline_lighting).read_bytes()).hexdigest() + '\n' + result.stdout + result.stderr
(build / 'test.log').write_text(report, encoding='utf-8')
print(report)
raise SystemExit(result.returncode)
