"""Execute production Win32 publication worker inside an actual loadable DLL."""
from pathlib import Path
import argparse,subprocess,json,hashlib,os
p=argparse.ArgumentParser();p.add_argument('--zig',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True);here=Path(__file__).resolve().parent;rows=[]
for arch in ['x86','x86_64']:
 out=a.out/arch;out.mkdir(exist_ok=True);dll=out/'async-publication.dll';exe=out/'async-host.exe'
 for name,source,extra in [('dll','world_frame_async_dll.cpp',['-shared']),('host','world_frame_async_host.cpp',[])]:
  dest=dll if name=='dll' else exe;cmd=[str(a.zig),'c++','-target',arch+'-windows-gnu','-std=c++17','-O2','-UNDEBUG',*extra,str(here/source),'-o',str(dest)]
  r=subprocess.run(cmd,capture_output=True,text=True);(out/(name+'-build.log')).write_text(r.stdout+r.stderr)
  if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
 for mode in ('publisher-sync','runtime-async-default','runtime-async-candidate'):
  for attempt in range(3):
   # Existing publisher fixture verifies its synchronous publication seam.
   # The new default-on fixture exercises production captureWorldFrame through
   # a real x86/x64 Win32 command worker with simulated game memory.
   env=os.environ.copy()
   env.pop("RA2TOOLBOX_ASYNC_COMMAND_CAPTURE",None)
   if mode=='publisher-sync':env["RA2TOOLBOX_ASYNC_COMMAND_CAPTURE"]="0"
   argv=[str(exe),str(dll)]+(['async'] if mode=='runtime-async-default' else ['async-candidate'] if mode=='runtime-async-candidate' else [])
   r=subprocess.run(argv,env=env,capture_output=True,text=True,timeout=30)
   (out/f'run-{mode}-{attempt}.log').write_text(r.stdout+r.stderr)
   print(arch,mode,attempt,r.returncode,r.stdout+r.stderr)
   rows.append(dict(arch=arch,mode=mode,attempt=attempt,exit=r.returncode,passed=r.returncode==0))
   if r.returncode:break
sources={str(x):hashlib.sha256(x.read_bytes()).hexdigest() for x in [here/'world_frame_async_dll.cpp',here/'world_frame_async_host.cpp',here.parent/'source/world_frame_runtime.h',here.parent/'source/world_lifecycle.h',here/'world_frame_core.cpp',here.parents[1]/'shared/source/world_frame_capture.h',here.parents[1]/'shared/source/world_frame_async_win.h',here.parents[1]/'shared/source/world_frame_publication.h',here.parents[1]/'shared/source/world_frame_transport.h']}
passed=len(rows)==18 and all(r['passed']for r in rows);(a.out/'result.json').write_text(json.dumps(dict(passed=passed,windows_dll_executed=True,game_tested=False,runs=rows,sources=sources),indent=2)+'\n');assert passed
