import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--out',required=True);a=p.parse_args();here=Path(__file__).resolve().parent;out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True)
root=here.parents[1];paths=['external_reader/source/radar_scene.h','external_reader/source/main_viewport.h','assist/source/native_viewport_host.h','shared/source/native_viewport_projection.h','assist/tests/radar_trigger_projection.cpp','assist/tests/mo_viewport_scene.cpp','assist/tests/mo_viewport_scene.py']
snapshot=lambda:{n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in paths}
before=snapshot()
source=(here/'radar_trigger_projection.cpp').read_text(encoding='utf-8-sig');setup=source[source.index('namespace {'):source.index('int main(')];inc=out/'synthetic-scene-setup.inc';inc.write_text(setup,encoding='utf-8');results={}
for arch in ('x86_64','x86'):
 exe=out/(arch+'.exe');cmd=[str(Path(a.zig).resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O1','-UNDEBUG',str(here/'mo_viewport_scene.cpp'),str(here.parents[1]/'third_party/xbrz/xbrz.cpp'),'-DMO_SCENE_SETUP="'+inc.as_posix()+'"','-o',str(exe),'-luser32','-lgdi32','-lole32']
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(arch+'-build.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise SystemExit(r.stdout+r.stderr)
 r=subprocess.run([str(exe)],capture_output=True,text=True);(out/(arch+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
 if r.returncode:raise SystemExit(r.stdout+r.stderr)
 results[arch]=json.loads(r.stdout.strip().splitlines()[-1])
changed=[n for n,d in snapshot().items() if d!=before[n]]
report={'results':results,'sources':before,'changed_during_run':changed,'scope':'Actual CPU SceneRenderer and synthetic terrain/assets; no native Render, DDraw, GPU or game process acceptance.'};(out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert not changed,changed
