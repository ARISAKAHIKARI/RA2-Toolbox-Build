"""Pair actual SceneRenderer versions with real RPM into fixture-owned pages."""
import argparse,hashlib,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--baseline',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--runs',type=int,default=3);a=p.parse_args()
source=Path(__file__).resolve().parents[1];headers=source/'external_reader/source';out=a.build_dir.resolve();out.mkdir(parents=True,exist_ok=True)
old=a.baseline.read_text(encoding='utf-8-sig');begin=old.index('namespace external { namespace radar {');kind=old.index('template<class Assets>class SceneRenderer',begin)
old=old[:begin]+'namespace external { namespace radar {\n'+old[kind:];old=old.replace('class SceneRenderer','class BeforeSceneRenderer').replace('explicit SceneRenderer(','explicit BeforeSceneRenderer(')
old=re.sub(r'#include "([^"]+)"',lambda m:'#include "'+(headers/m.group(1)).resolve().as_posix()+'"',old)
reference=out/'scene_reference.h';reference.write_text(old,encoding='utf-8');exe=out/'terrain_cell_prefetch.exe'
cmd=[str(a.zig.resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O2','-UNDEBUG',str(source/'assist/tests/terrain_cell_prefetch.cpp'),'-DRA2_REFERENCE_SCENE="'+reference.as_posix()+'"',str(source/'third_party/xbrz/xbrz.cpp'),'-o',str(exe),'-lgdi32','-luser32']
result=[]
for name,args in [('build',cmd)]+[('test-'+str(n+1),[str(exe)]) for n in range(max(1,a.runs))]:
 r=subprocess.run(args,capture_output=True,text=True);(out/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8');result.append(dict(name=name,exit_code=r.returncode,command=args,output=r.stdout+r.stderr));print(r.stdout+r.stderr,end='')
 (out/'result.json').write_text(json.dumps(dict(baseline_sha256=hashlib.sha256(a.baseline.read_bytes()).hexdigest(),source_sha256=hashlib.sha256((headers/'radar_scene.h').read_bytes()).hexdigest(),results=result),indent=2)+'\n',encoding='utf-8');r.check_returncode()
