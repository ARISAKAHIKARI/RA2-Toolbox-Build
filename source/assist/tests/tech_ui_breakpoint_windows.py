"""Build/run a standalone Windows x86 own-process DR/VEH verification.

The shared production Service core is used with a small test caller/classifier.
No game process is opened. QPC measurements are exception/call overhead in this
fixture, not game FPS or the cost of the complete production classifier.
"""
import argparse,hashlib,json,re,statistics,subprocess
from pathlib import Path
from breakpoint_windows_build import build_guard
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',required=True,type=Path);p.add_argument('--build-dir',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--build-only',action='store_true');a=p.parse_args()
here=Path(__file__).resolve().parent;src=here/'tech_ui_breakpoint_windows.cpp';core=here.parent/'source/tech_ui_breakpoint_core.h';a.build_dir.mkdir(parents=True,exist_ok=True);exe=a.build_dir/'tech-ui-breakpoint-windows.exe'
flags=['-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-fms-extensions','-fasm-blocks']
obj=build_guard(a.zig,a.build_dir)
command=[str(a.zig.resolve()),'c++',*flags,str(src),str(obj),'-o',str(exe.resolve()),'-lkernel32'];subprocess.run(command,check=True)
if a.build_only:
 paths=[core,core.parent/'tech_ui_breakpoint_hot.h',core.parent/'tech_ui_breakpoint_rendezvous_guard.h',core.parent/'guard.cpp',src,Path(__file__).resolve(),here/'breakpoint_windows_build.py']
 report={'build_pass':True,'windows_executed':False,'game_executed':False,'target':'Windows x86 PE32','command':command,'source_sha256':{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},'exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS: Windows x86 production Service + MSVC SEH compiled and linked; execution NOT performed');raise SystemExit(0)
runs=[];outputs=[]
for i in range(3):
 r=subprocess.run([str(exe.resolve())],check=True,capture_output=True,text=True);outputs.append(r.stdout);print(r.stdout,end='')
 match=re.search(r'QPC calls_per_caller=(\d+) total_calls=(\d+) baseline_us=([\d.]+) active_us=([\d.]+) additional_us_per_exception=([\d.]+) hits=(\d+) changes=(\d+)',r.stdout);assert match,r.stdout
 count,total,baseline,active,additional,hits,changes=match.groups();assert int(count)==10000 and int(total)==int(hits)==int(changes)==20000
 runs.append({'calls_per_caller':int(count),'total_calls':int(total),'baseline_us':float(baseline),'active_us':float(active),'additional_us_per_exception':float(additional),'hits':int(hits),'changed_arguments':int(changes)})
cases=['true Windows own-thread hardware execution breakpoints at two original CALLs','native includeProduction preserved for both callers','only local House and selected Type classified','20000 exact hardware exceptions and stack argument changes per run','foreign single-step passes to another registered VEH','mixed own+foreign Dr6/TF CONTEXT remains byte-identical on CONTINUE_SEARCH','code bytes unchanged before/after DR installation and removal','enabled foreign DR slot retained while two other slots leased','four occupied foreign addresses cause refusal without context overwrite','foreign replacement survives restore; only still-owned slot removed','failed restore readback retains lease and VEH until later confirmation','ignored continuation with still-owned readback retains lease and disables callback until retirement','fully confirmed retirement restores original debug-register slots']
report={'scope':__doc__,'assertions_enabled':True,'compiler_flags':flags,'target':'Windows x86 standalone fixture','production_core_sha256':hashlib.sha256(core.read_bytes()).hexdigest(),'production_hot_sha256':hashlib.sha256((core.parent/'tech_ui_breakpoint_hot.h').read_bytes()).hexdigest(),'test_source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'fixture_exe_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'game_process_opened':False,'game_executable_required':False,'platform_detection_tested':False,'multiplayer_tested':False,'results':[{'case':c,'status':'PASS'} for c in cases],'qpc_runs':runs,'median_additional_us_per_exception':statistics.median(r['additional_us_per_exception'] for r in runs),'stdout':outputs}
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

