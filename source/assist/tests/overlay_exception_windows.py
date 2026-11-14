"""Run actual compositor HWND/thread failures and frozen pre.7 termination control."""
from pathlib import Path
import argparse,json,subprocess,hashlib
p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[3];old=a.out/'pre7-compositor.h';old.write_bytes(subprocess.check_output(['git','show','c46aa4be4c0b562ca163a6df6f510459a4872ca6:source/shared/source/overlay_compositor.h'],cwd=root));fixture=Path(__file__).with_suffix('.cpp');rows=[]
for name,header,expected in [('current',root/'source/shared/source/overlay_compositor.h',0),('pre7',old,86)]:
 exe=a.out/(name+'.exe');cmd=[str(a.zig),'c++','-target','x86_64-windows-gnu','-std=c++17','-O1','-UNDEBUG','-DOVERLAY_COMPOSITOR_HEADER="'+str(header)+'"','-I'+str(root/'source/shared/source'),str(fixture),'-o',str(exe),'-luser32','-lgdi32','-ld3d11','-ldxgi','-ldwmapi','-lole32','-luuid']
 r=subprocess.run(cmd,capture_output=True,text=True);(a.out/(name+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:print(r.stderr);raise SystemExit(r.returncode)
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30);(a.out/(name+'-run.log')).write_text(r.stdout+r.stderr);print(name+': '+str(r.returncode)+' '+r.stdout+r.stderr);rows.append(dict(name=name,exit=r.returncode,expected=expected,passed=r.returncode==expected,source_sha256=hashlib.sha256(header.read_bytes()).hexdigest()))
(a.out/'result.json').write_text(json.dumps(dict(passed=all(x['passed'] for x in rows),cases=rows,windows_executed=True,game_tested=False),indent=2)+'\n');assert all(x['passed'] for x in rows),rows
