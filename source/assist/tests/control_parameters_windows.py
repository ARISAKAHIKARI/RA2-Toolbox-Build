"""Own-process Windows x86 DR/VEH control routing and joint-slot verification; no game or peers."""
import argparse, hashlib, json, re, statistics, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--zig',type=Path,required=True);p.add_argument('--build-dir',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.build_dir.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;source=here.parent/'source';paths=[source/'tech_ui_breakpoint_core.h',source/'tech_ui_breakpoint_hot.h',source/'control_parameter_policy.h',here/'control_parameters_windows.cpp',here/'tech_ui_breakpoint_windows.cpp',Path(__file__)];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();before={str(p):sha(p)for p in paths}
exe=a.build_dir/'control-parameters-windows.exe';flags=['-target','x86-windows-gnu','-std=c++17','-O2','-UNDEBUG','-fms-extensions','-fasm-blocks'];subprocess.run([str(a.zig.resolve()),'c++',*flags,str(here/'control_parameters_windows.cpp'),'-o',str(exe.resolve()),'-lkernel32'],check=True)
# Build an isolated comparison from the SAME fixture and dependencies. Only the
# pre-optimization VEH dispatch/counter block is restored below; the classifier,
# slot lifecycle, compiler and validation remain identical to the current run.
repo=here.parents[2];legacy_root=a.build_dir.resolve()/'legacy-core';todo=[here/'control_parameters_windows.cpp'];copied=set()
while todo:
 path=todo.pop().resolve()
 if path in copied:continue
 copied.add(path);text=path.read_text();target=legacy_root/path.relative_to(repo);target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text)
 for include in re.findall(r'#include\s+"([^"]+)"',text):todo.append((path.parent/include).resolve())
legacy_core=legacy_root/(source/'tech_ui_breakpoint_core.h').relative_to(repo);text=legacy_core.read_text()
start=text.index('  auto&c=*p->ContextRecord;const Registers r=registers(c);const Hit hit=ownedHit(')
end=text.index('  return EXCEPTION_CONTINUE_EXECUTION;',start)
text=text[:start]+'''  auto&c=*p->ContextRecord;const Registers r=registers(c);uint32_t ours=0,tag=0;
  for(unsigned n=0;n<4;++n)if((r.status&(1u<<n))&&s->lease.exact(r,n)&&c.Eip==s->lease.sites[s->lease.actualTag(r,n)]){ours|=1u<<n;tag=s->lease.actualTag(r,n);}
  if(!ours)return EXCEPTION_CONTINUE_SEARCH;
  if(r.status&(0xE00Fu&~ours))return EXCEPTION_CONTINUE_SEARCH;
  c.Dr6&=~ours;c.EFlags|=0x10000u;InterlockedIncrement(&s->exceptions);
  if(s->callback&&s->callback(&c,tag))InterlockedIncrement(&s->changed);
'''+text[end:];legacy_core.write_text(text)
legacy_exe=a.build_dir/'control-parameters-legacy-dispatch.exe';legacy_source=legacy_root/(here/'control_parameters_windows.cpp').relative_to(repo)
subprocess.run([str(a.zig.resolve()),'c++',*flags,str(legacy_source),'-o',str(legacy_exe.resolve()),'-lkernel32'],check=True)
def run_fixture(path):
 r=subprocess.run([str(path.resolve())],capture_output=True,text=True,timeout=45);print(r.stdout,end='');print(r.stderr,end='');assert r.returncode==0,r.returncode
 m=re.search(r'CONTROL_QPC calls=30000 slots=1 baseline_us=([\d.]+) active_us=([\d.]+) additional_us_per_exception=([\d.]+)',r.stdout);assert m
 return {'stdout':r.stdout,'baseline_us':float(m[1]),'active_us':float(m[2]),'additional_us_per_exception':float(m[3]),'exceptions':30000}
runs=[];legacy_runs=[]
for n in range(3):
 if n&1:runs.append(run_fixture(exe));legacy_runs.append(run_fixture(legacy_exe))
 else:legacy_runs.append(run_fixture(legacy_exe));runs.append(run_fixture(exe))
assert before=={str(p):sha(p)for p in paths};a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'pass':True,'scope':__doc__,'assertions_enabled':True,'compiler_flags':flags,'source_sha256':before,'exe_sha256':sha(exe),'game_process_tested':False,'multiplayer_tested':False,'runs':runs,'dispatch_comparison':{'scope':'same synthetic own-process classifier and real DR/VEH; only prior dispatch and locked counters reconstructed; excludes production world/signature classifier and game FPS','legacy_core_sha256':sha(legacy_core),'legacy_exe_sha256':sha(legacy_exe),'legacy_runs':legacy_runs,'legacy_median_additional_us':statistics.median(r['additional_us_per_exception']for r in legacy_runs),'current_median_additional_us':statistics.median(r['additional_us_per_exception']for r in runs)}},indent=2)+'\n')

