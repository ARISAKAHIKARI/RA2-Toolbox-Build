"""Real WebView2 NavigateToString smoke in an isolated hidden own-process HWND."""
import argparse,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zig',required=True);p.add_argument('--sdk',required=True);p.add_argument('--out',required=True);a=p.parse_args();here=Path(__file__).resolve().parent;source=here.parents[1];sdk=Path(a.sdk).resolve();out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True);generated=out/'generated';generated.mkdir(exist_ok=True)
subprocess.run([sys.executable,str(source/'tools/bundle_console.py'),'--header',str(generated/'web_console_assets.h')],check=True)
shutil.copy2(sdk/'bin/WebView2Loader.dll',out/'WebView2Loader.dll');profile=out/'localappdata';profile.mkdir(exist_ok=True)
exe=out/'navigation.exe';cmd=[str(Path(a.zig).resolve()),'c++','-target','x86_64-windows-gnu','-std=c++17','-O1','-UNDEBUG','-municode','-fms-extensions','-I'+str(sdk/'include'),'-I'+str(generated),str(here/'web_console_navigation_windows.cpp'),'-o',str(exe),'-luser32','-lole32','-lkernel32']
r=subprocess.run(cmd,capture_output=True,text=True);(out/'build.log').write_text(r.stdout+r.stderr,encoding='utf-8')
if r.returncode:raise SystemExit(r.stdout+r.stderr)
env=dict(os.environ,LOCALAPPDATA=str(profile));r=subprocess.run([str(exe),str(out/'capture.png')],capture_output=True,encoding='utf-8',errors='replace',env=env,timeout=45);(out/'smoke.log').write_text(r.stdout+r.stderr,encoding='utf-8');print(r.stdout+r.stderr)
report={'schema':'ra2.webview-navigation-windows.v1','exit_code':r.returncode,'localappdata':str(profile),'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'source_sha256':{str(p.relative_to(source)):hashlib.sha256(p.read_bytes()).hexdigest()for p in [source/'suite/source/web_console.cpp',source/'suite/source/web_console.h',here/'web_console_navigation_windows.cpp',Path(__file__)]},'result':json.loads(r.stdout.strip().splitlines()[-1]),'game_process_accessed':False};(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');raise SystemExit(r.returncode)
