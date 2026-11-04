"""Real own-process DR1-tag gate and production flag callback on Windows x86."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;src=here.parent/'source';s=(src/'tech_ui_breakpoint_bridge.h').read_text();start=s.index('static bool mapVisibilityParameter(');at=s.index('{',start)+1;depth=1
while depth:depth+=(s[at]=='{')-(s[at]=='}');at+=1
extract=a.out/'map-callback.inc';extract.write_text(s[start:at]+'\n');exe=a.out/'map-visibility.exe'
cmd=[str(a.zig.resolve()),'c++','-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-fms-extensions','-fasm-blocks','-DMAP_CALLBACK_EXTRACT="'+extract.resolve().as_posix()+'"',str(here/'map_visibility_windows.cpp'),'-o',str(exe),'-lkernel32']
build=subprocess.run(cmd,capture_output=True,text=True);(a.out/'build.log').write_text(build.stdout+build.stderr);assert build.returncode==0,build.stdout+build.stderr
run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=40);(a.out/'run.log').write_text(run.stdout+run.stderr);print(run.stdout,end='');assert run.returncode==0,run.stderr
report=dict(scope=__doc__,result=json.loads(run.stdout.strip()),source_sha256={str(x.relative_to(here.parents[1])):hashlib.sha256(x.read_bytes()).hexdigest()for x in [src/'tech_ui_breakpoint_bridge.h',src/'tech_ui_breakpoint_core.h',src/'map_visibility_parameter_policy.h',here/'map_visibility_windows.cpp',Path(__file__)]})
(a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
