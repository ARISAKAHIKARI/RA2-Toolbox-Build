"""Compare actual pre-change/current Context material preparation at -O2."""
import argparse, hashlib, json, subprocess
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--zig',required=True)
p.add_argument('--baseline-assets',required=True)
p.add_argument('--build-dir',required=True)
a=p.parse_args()
source=Path(__file__).resolve().parents[2]
build=Path(a.build_dir).resolve();build.mkdir(parents=True,exist_ok=True)
baseline=Path(a.baseline_assets).resolve()
commands=[]
for name,header in [('before',baseline),('after',source/'external_reader/source/camera_assets.h')]:
    wrapper=build/(name+'.cpp')
    wrapper.write_text(('#define MATERIAL_BASELINE 1\n' if name=='before' else '')+'#define CAMERA_ASSETS_HEADER "'+header.as_posix()+'"\n#include "'+(source/'assist/tests/actor_material_reuse.cpp').as_posix()+'"\n',encoding='utf-8')
    exe=build/(name+'.exe')
    cmd=[str(Path(a.zig).resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(wrapper),str(source/'third_party/xbrz/xbrz.cpp'),'-I',str(source/'external_reader/source'),'-o',str(exe),'-lgdi32','-luser32','-lshlwapi','-lversion','-lshell32','-lole32','-lwinmm']
    for step,command in [('build',cmd),('test',[str(exe)])]:
        r=subprocess.run(command,capture_output=True,text=True,timeout=240)
        (build/(name+'-'+step+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
        commands.append(dict(name=name+'-'+step,args=command,exit_code=r.returncode))
        (build/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
        print(name,step,r.stdout+r.stderr,flush=True)
        if r.returncode:raise SystemExit(r.returncode)
(build/'sources.json').write_text(json.dumps({str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [baseline,source/'external_reader/source/camera_assets.h',source/'assist/tests/actor_material_reuse.cpp']},indent=2),encoding='utf-8')
