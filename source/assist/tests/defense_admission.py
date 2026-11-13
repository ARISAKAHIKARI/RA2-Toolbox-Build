"""Run complete production defense admission, including a released-source counterexample."""
import argparse, hashlib, json, os, subprocess
from pathlib import Path
from tactical_frame_sampling import production, function

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--out',required=True,type=Path)
p.add_argument('--baseline',default='1d970c9c9b2929b2c215213e1834a70bf82178b0')
p.add_argument('--sanitize',action='store_true')
a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;repo=here.parents[2];src=here.parent/'source'
prefix=(here/'tactical_frame_sampling.cpp').read_text().split('#include "tactical_frame_production.h"')[0]
prefix=prefix.replace('static bool antiTheftBuilding(uint32_t){return false;}static bool antiTheftThreat(uint32_t,uint32_t){return false;}',
 'static bool antiTheftBuilding(uint32_t);static bool antiTheftThreat(uint32_t,uint32_t);static featuretypes::Config featureConfig{};')
routes,scope,names=production();(out/'sample_scope_production.h').write_text(scope)
current=(src/'external_orders_bridge.h').read_text()
base=subprocess.check_output(['git','show',a.baseline+':source/assist/source/external_orders_bridge.h'],cwd=repo,text=True)
helper=function(current,'static bool antiTheftCurrentTargetUsable(')
bridge=(src/'bridge.cpp').read_text()
combat_demand=next(line.strip() for line in bridge.splitlines() if line.strip().endswith('worldDemand|=worldframe::Combat;') and 'AircraftSpread' in line)
runtime_demand='static bool runtimeCoopPending=false;static uint32_t productionCombatDemand(){uint32_t worldDemand=worldframe::Base;'+combat_demand+'return worldDemand;}\n'
results={}
for phase,text in [('baseline',base),('current',current)]:
 code=prefix+'\n'+routes+'\n#include "anti_theft_bridge.h"\n'+runtime_demand+helper+'\n'+function(text,'static bool externalOrderValid(')+'\n#include "defense_admission.cpp"\n'
 cpp=out/(phase+'.cpp');cpp.write_text(code);exe=out/phase
 cmd=['g++','-std=c++17','-O1','-UNDEBUG','-I'+str(out),'-I'+str(src),'-I'+str(here),str(cpp),'-o',str(exe)]
 if a.sanitize:cmd[2:2]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
 r=subprocess.run(cmd,capture_output=True,text=True);(out/(phase+'-build.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.stdout+r.stderr)
 env=os.environ.copy()
 if a.sanitize:env['ASAN_OPTIONS']=env.get('ASAN_OPTIONS','')+':detect_leaks=0'
 r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=30,env=env);(out/(phase+'.log')).write_text(r.stdout+r.stderr)
 print(phase+': '+r.stdout+r.stderr,end='')
 if phase=='baseline':assert r.returncode!=0 and 'out-of-range old threat must allow nearby replacement' in r.stderr
 else:assert r.returncode==0
 results[phase]={'exit_code':r.returncode,'code_sha256':hashlib.sha256(code.encode()).hexdigest()}
report={'pass':True,'baseline':a.baseline,'results':results,'sanitized':a.sanitize,'real_game_tested':False,'windows_tested':False}
(out/'result.json').write_text(json.dumps(report,indent=2)+'\n')
