#!/usr/bin/env python3
"""Execute the real broker Client and production host pulse against Win32 fixtures."""
import argparse,hashlib,json,subprocess
from pathlib import Path

def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def function(text,name):
 start=text.index(name);at=text.index('{',start);depth=1;end=at+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--baseline-header',type=Path);p.add_argument('--client-header',type=Path);p.add_argument('--cxx',default='c++');a=p.parse_args()
 here=Path(__file__).resolve().parent;source=here.parents[1];client=a.client_header or source/'shared/source/runtime_broker_client.h';host=source/'shared/source/host_game_input.h';out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 shim=out/'include';shim.mkdir(exist_ok=True);(shim/'windows.h').write_text('#pragma once\n#include "runtime_broker_retirement_fixture.h"\n')
 host_hash=digest(host);body=host.read_text();routes=out/'production-host.inc';routes.write_text('\n\n'.join(function(body,name) for name in ('inline bool clear(', 'inline void reapTargets(', 'inline void CALLBACK pulse('))+'\n')
 variants=[('current',client)]
 if a.baseline_header:variants.insert(0,('baseline',a.baseline_header))
 results={}
 for name,path in variants:
  before=digest(path);exe=out/name
  command=[a.cxx,'-std=c++17','-O0','-D_WIN64','-I'+str(shim),'-I'+str(here),'-I'+str(source/'shared/source'),'-DBROKER_CLIENT="'+path.resolve().as_posix()+'"','-DBROKER_HOST="'+routes.as_posix()+'"',str(here/'runtime_broker_retirement.cpp'),'-o',str(exe)]
  build=subprocess.run(command,text=True,capture_output=True);(out/(name+'-build.log')).write_text(build.stdout+build.stderr)
  if build.returncode:raise SystemExit(build.stdout+build.stderr)
  run=subprocess.run([str(exe)],text=True,capture_output=True);(out/(name+'.log')).write_text(run.stdout+run.stderr)
  if run.returncode not in (0,1):raise SystemExit(run.stdout+run.stderr)
  report=json.loads(run.stdout.strip().splitlines()[-1]);print(name+': '+json.dumps(report))
  if name=='current':
   if run.returncode or report['failures']:raise SystemExit(run.stdout+run.stderr)
  else:
   assert run.returncode==1 and report['failures']>0,'Baseline failed to reproduce defects'
   for message in ('a later termination retry recovers from permanent busy','WAIT_FAILED is not treated as confirmed process exit','failed wait cannot falsely complete hook removal'):
    assert 'FAIL: '+message in run.stdout,'Missing baseline counterexample: '+message
  assert digest(path)==before,'Client source changed during verification'
  results[name]={'source':str(path.resolve()),'source_sha256':before,'exit_code':run.returncode,'checks':report['checks'],'failures':report['failures'],'passed_checks':report['checks'] if name=='current' else 0,'client_checks':report['client_checks'],'host_pulse_checks':report['host_pulse_checks'],'actual_pulses_in_quiet_window':report['actual_pulses_in_quiet_window'],'log':str(out/(name+'.log')),'build_command':command}
 assert digest(host)==host_hash,'Host source changed during verification'
 inputs=[client,host,source/'shared/source/runtime_broker_protocol.h',here/'runtime_broker_retirement.cpp',here/'runtime_broker_retirement_fixture.h',Path(__file__)]
 report={'schema':'ra2.runtime-broker-retirement.v1','passed':True,'current_passed_checks':results['current']['passed_checks'],'results':results,'host_routes_sha256':digest(routes),'source_sha256':{str(path.relative_to(source.parent)):digest(path) for path in inputs},'real_windows_tested':False,'game_tested':False,'multiplayer_tested':False,'limits':['Win32 process/handle/IPC calls are deterministic substitutes; the complete production Client and actual host clear/reap/pulse functions execute.','Termination can remain incomplete in the OS. The client retains identity and blocks replacement without positive-time retirement waits; it does not claim to force kernel-level completion.','Normal initial startup/exchange and one graceful shutdown wait retain their existing bounded timeouts. No asynchronous broker architecture is claimed.']}
 (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print('PASS current production checks='+str(report['current_passed_checks'])+'; baseline failures are not counted as passes.')

if __name__=='__main__':main()
