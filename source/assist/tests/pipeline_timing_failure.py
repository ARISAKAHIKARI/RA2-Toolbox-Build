"""Execute current timing destructors and a frozen pre.7 negative control."""
from pathlib import Path
import argparse,hashlib,json,subprocess
p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[3];fixture=Path(__file__).with_suffix('.cpp');old=a.out/'pre7-timing.h';old.write_bytes(subprocess.check_output(['git','show','c46aa4be4c0b562ca163a6df6f510459a4872ca6:source/shared/source/pipeline_timing.h'],cwd=root))
rows=[]
for name,header,expected in [('current',root/'source/shared/source/pipeline_timing.h',0),('pre7',old,86)]:
 exe=a.out/name;r=subprocess.run(['g++','-std=c++17','-O1','-pthread','-UNDEBUG','-DPIPELINE_TIMING_HEADER="'+str(header)+'"',str(fixture),'-o',str(exe)],capture_output=True,text=True);(a.out/(name+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);(a.out/(name+'-run.log')).write_text(r.stdout+r.stderr);print(name+': '+str(r.returncode)+' '+r.stdout,end='\n');rows.append(dict(name=name,exit=r.returncode,expected=expected,passed=r.returncode==expected,source_sha256=hashlib.sha256(header.read_bytes()).hexdigest()))
assert all(v['passed'] for v in rows),rows
(a.out/'result.json').write_text(json.dumps(dict(passed=True,cases=rows,game_tested=False),indent=2)+'\n')
