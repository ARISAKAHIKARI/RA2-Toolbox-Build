"""Own-process Windows x86 DR/VEH control routing and joint-slot verification; no game or peers."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path
from breakpoint_windows_build import build_guard
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.build_dir.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;source=here.parent/'source';paths=[source/'tech_ui_breakpoint_core.h',source/'tech_ui_breakpoint_hot.h',source/'control_parameter_policy.h',here/'control_parameters_windows.cpp',here/'tech_ui_breakpoint_windows.cpp',source/'guard.cpp',source/'tech_ui_breakpoint_rendezvous_guard.h',source.parents[1]/'shared/source/runtime_seh_scope.h',here/'breakpoint_windows_build.py',Path(__file__)];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={str(p):sha(p)for p in paths}
obj=build_guard(a.zig,a.build_dir)
exe=a.build_dir/'control-parameters-windows.exe';flags=['-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-fms-extensions','-fasm-blocks'];subprocess.run([str(a.zig.resolve()),'c++',*flags,str(here/'control_parameters_windows.cpp'),str(obj),'-o',str(exe.resolve()),'-lkernel32'],check=True)
# Exercise the current production service. The historical text-spliced VEH
# benchmark predates owner-thread rendezvous and cannot be compared safely.
def run_fixture(path):
 r=subprocess.run([str(path.resolve())],capture_output=True,text=True,timeout=45);print(r.stdout,end='');print(r.stderr,end='');assert r.returncode==0,r.returncode
 m=re.search(r'CONTROL_QPC calls=30000 slots=1 baseline_us=([\d.]+) active_us=([\d.]+) additional_us_per_exception=([\d.]+)',r.stdout);assert m
 return {'stdout':r.stdout,'baseline_us':float(m[1]),'active_us':float(m[2]),'additional_us_per_exception':float(m[3]),'exceptions':30000}
runs=[run_fixture(exe) for _ in range(3)]
assert before=={str(p):sha(p) for p in paths}
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(json.dumps({'pass':True,'scope':__doc__,'assertions_enabled':True,
 'compiler_flags':flags,'source_sha256':before,'exe_sha256':sha(exe),
 'game_process_tested':False,'multiplayer_tested':False,'runs':runs,
 'historical_dispatch_comparison_performed':False},indent=2)+'\n',encoding='utf-8')
