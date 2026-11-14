"""Execute production Win32 publication worker inside an actual loadable DLL."""
from pathlib import Path
import argparse,subprocess,json,hashlib
p=argparse.ArgumentParser();p.add_argument('--zig',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);here=Path(__file__).resolve().parent;rows=[]
for arch in ['x86','x86_64']:
 out=a.out/arch;out.mkdir(exist_ok=True);dll=out/'async-publication.dll';exe=out/'async-host.exe'
 for name,source,extra in [('dll','world_frame_async_dll.cpp',['-shared']),('host','world_frame_async_host.cpp',[])]:
  dest=dll if name=='dll' else exe;cmd=[str(a.zig),'c++','-target',arch+'-windows-gnu','-std=c++17','-O2','-UNDEBUG',*extra,str(here/source),'-o',str(dest)]
  r=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-build.log')).write_text(r.stdout+r.stderr)
  if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
 for attempt in range(3):
  r=subprocess.run([str(exe),str(dll)],capture_output=True,text=True,timeout=30);(out/f'run-{attempt}.log').write_text(r.stdout+r.stderr);print(arch,attempt,r.returncode,r.stdout+r.stderr);rows.append(dict(arch=arch,attempt=attempt,exit=r.returncode,passed=r.returncode==0))
  if r.returncode:break
sources={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [here/'world_frame_async_dll.cpp',here/'world_frame_async_host.cpp',here.parents[1]/'shared/source/world_frame_async_win.h',here.parents[1]/'shared/source/world_frame_publication.h',here.parents[1]/'shared/source/world_frame_transport.h']}
passed=len(rows)==6 and all(r['passed']for r in rows);(a.out/'result.json').write_text(json.dumps(dict(passed=passed,windows_dll_executed=True,game_tested=False,runs=rows,sources=sources),indent=2)+'\n');assert passed
