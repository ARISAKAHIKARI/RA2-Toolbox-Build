"""Paired actual Context HVA/body/shadow preparation, synthetic data only."""
import argparse,hashlib,json,statistics,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--baseline-assets',required=True);p.add_argument('--build-dir',required=True);p.add_argument('--x86',action='store_true');a=p.parse_args()
src=Path(__file__).resolve().parents[2];out=Path(a.build_dir).resolve();out.mkdir(parents=True,exist_ok=True);results={};commands=[]
for target in (['x86_64-windows-gnu','x86-windows-gnu'] if a.x86 else ['x86_64-windows-gnu']):
 exes={}
 for name,header in [('before',Path(a.baseline_assets).resolve()),('after',src/'external_reader/source/camera_assets.h')]:
  wrapper=out/(target+'-'+name+'.cpp');wrapper.write_text(('#define GEOMETRY_BASELINE 1\n' if name=='before' else '')+'#define CAMERA_ASSETS_HEADER "'+header.as_posix()+'"\n#include "'+(src/'assist/tests/voxel_geometry_reuse.cpp').as_posix()+'"\n',encoding='utf-8');exe=out/(target+'-'+name+'.exe');exes[name]=exe
  cmd=[str(Path(a.zig).resolve()),'c++','-target',target,'-std=c++17','-O2','-UNDEBUG',str(wrapper),str(src/'third_party/xbrz/xbrz.cpp'),'-I',str(src/'external_reader/source'),'-o',str(exe),'-lgdi32','-luser32','-lshlwapi','-lversion','-lshell32','-lole32','-lwinmm']
  c=subprocess.run(cmd,text=True,capture_output=True);(out/(target+'-'+name+'-build.log')).write_text(c.stdout+c.stderr,encoding='utf-8');commands.append(dict(args=cmd,exit_code=c.returncode));(out/'commands.json').write_text(json.dumps(commands,indent=2),encoding='utf-8')
  if c.returncode:print(c.stdout+c.stderr);raise SystemExit(c.returncode)
 runs={'before':[],'after':[]}
 for trial in range(3):
  for name in (['before','after'] if trial%2==0 else ['after','before']):
   r=subprocess.run([str(exes[name])],text=True,capture_output=True);(out/(target+'-'+name+'-'+str(trial)+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8')
   if r.returncode:print(r.stdout+r.stderr);raise SystemExit(r.returncode)
   row=json.loads(r.stdout);assert row['status']=='PASS';runs[name].append(row)
 digests={r['digest'] for group in runs.values() for r in group};assert len(digests)==1,digests
 times={key:{version:statistics.median(row[key] for row in values) for version,values in runs.items()} for key in ['cpu_body_shadow_ms','gpu_packet_prepare_ms']}
 results[target]={'digest':next(iter(digests)),'timings':times,'runs':runs};print(target,json.dumps(times),flush=True)
report={'status':'PASS','results':results,'source_sha256':{str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in [Path(a.baseline_assets),src/'external_reader/source/camera_assets.h',src/'external_reader/source/voxel_geometry_cache.h',Path(__file__).with_suffix('.cpp')]},'boundary':'Actual Context body/shadow CPU raster and GPU packet preparation; no GPU execution/gameplay/FPS measurement.'};(out/'result.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
