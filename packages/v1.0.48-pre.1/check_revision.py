#!/usr/bin/env python3
"""Run focused checks and bind their logs to the frozen source checkout."""
from pathlib import Path
import argparse,concurrent.futures,hashlib,json,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--repo',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--zig',type=Path,required=True);a=p.parse_args();r=a.repo.resolve();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
def git(*args):return subprocess.check_output(['git','-C',str(r),*args]).decode().strip()
def snapshot():
 records={}
 for name in git('ls-tree','-r','--name-only','HEAD','--','source').splitlines():
  records[name]=hashlib.sha256((r/name).read_bytes()).hexdigest()
 return records
before=snapshot();commit=git('rev-parse','HEAD');tree=git('rev-parse','HEAD:source');py=[sys.executable,'-B'];test='source/assist/tests/'
cases={
 'native-air-bomb':[py+[test+'air_events_bomb_board_test.py','--out',str(out/'native-air-bomb')]],
 'new-settings':[py+[test+'input_bomb_settings_test.py','--out',str(out/'new-settings')]],
 'mo-gestures':[py+[test+'mo_orders_bridge_test.py']],
 'formation':[py+[test+'formation_quick_bridge_test.py']],
 'air-compatibility':[py+[test+'air_waypoint_host_test.py']],
 'air-admission':[py+[test+'air_path_command_admission.py','--build-dir',str(out/'air-admission')]],
 'automatic-controls':[py+[test+'automatic_controls_test.py','--out',str(out/'automatic-controls')]],
 'hotkeys':[py+[test+'hotkey_command_context.py','--out',str(out/'hotkeys')]],
 'readonly-lifecycle':[py+[test+'read_only_lifecycle_integration.py','--out',str(out/'readonly-lifecycle')]],
 'passenger-settings':[py+[test+'passenger_settings_test.py',str(out/'passenger-settings')]],
 'display-settings':[py+['source/suite/tests/console_display_entry_test.py','--out',str(out/'display-settings')]],
 'crossbit-abi':[py+['source/tools/verify_crossbit_abi.py','--zig',str(a.zig.resolve()),'--output',str(out/'crossbit-abi.json')]],
}
for name,file in [('input-lock','input_lock_test.cpp'),('runtime-demand','runtime_feature_demand.cpp'),('planning-key-release','mo_patrol_planning.cpp')]:
 exe=out/(name+'-test');cases[name]=[['g++','-std=c++17','-O2','-UNDEBUG','-Isource/assist/source',test+file,'-o',str(exe)],[str(exe)]]
def run(item):
 name,commands=item;log=out/(name+'.log');codes=[]
 with log.open('w') as f:
  for cmd in commands:
   code=subprocess.run(cmd,cwd=r,stdout=f,stderr=subprocess.STDOUT).returncode;codes.append(code)
   if code:break
 return name,{'passed':len(codes)==len(commands) and not any(codes),'commands':commands,'returncodes':codes,'log':log.name,'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest()}
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
 results=dict(pool.map(run,cases.items()))
unchanged=before==snapshot() and commit==git('rev-parse','HEAD')
report={'schema':1,'source_commit':commit,'source_tree_sha':tree,'source_sha256':before,'source_unchanged':unchanged,'passed':unchanged and all(v['passed'] for v in results.values()),'checks':results,'scope':'15 executed focused groups: controlled production logic plus Windows x86/x64 static ABI. Not Windows runtime, native aircraft arrival, explosion, game or multiplayer acceptance.'}
(out/'checks.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'passed':report['passed'],'groups':len(results),'failed':[k for k,v in results.items() if not v['passed']],'source_unchanged':unchanged}))
raise SystemExit(0 if report['passed'] else 1)
