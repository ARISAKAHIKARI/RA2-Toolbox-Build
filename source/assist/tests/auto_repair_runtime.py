"""Run extracted production repair scheduling and cancellation in synthetic memory."""
import argparse,hashlib,json,subprocess
from pathlib import Path

def block(text,marker):
 start=text.index(marker);opening=text.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--before',type=Path);a=p.parse_args()
 r=Path(__file__).resolve().parents[3];here=Path(__file__).resolve().parent;out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 paths=[r/'source/assist/source'/name for name in ['bridge.cpp','repair_delivery.h','compute_tasks.h','compute_frame.h','compute_policy.h','plan_freshness.h','tactical_decisions.h','world_frame_runtime.h','tactical_frame_read.h']]+[r/'source/shared/source/world_frame.h',here/'auto_repair_runtime.cpp',Path(__file__).resolve()]
 digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();hashes={str(p.relative_to(r)):digest(p) for p in paths}
 def run(source,dest,before=False):
  dest.mkdir(parents=True,exist_ok=True);text=source.read_text();reset=block(text,'static void reset(bool retireHooks=true)')
  manual=block(text,'else if(c==Repair)');manual=manual[manual.index('{')+1:-1]
  resetStart=reset.index('repairCount=repairAt=repairDone=0;');resetEnd=reset.index('for(auto&v:lastIdle)',resetStart)
  autoStart=reset.index('autoWas=false;');autoEnd=reset.index('if(retireHooks)',autoStart)
  sections={
   'SAMPLE_SCOPE':(r/'source/assist/source/world_frame_runtime.h').read_text().split('static void requestWorldRange(',1)[1].split('static void retireWorldFrame(',1)[0],
   'REPAIR_EVENT':block(text,'static bool repairEvent('),
   'REPAIR_CALCULATION':text[text.index('static DWORD clampedRepairInterval('):text.index('static int superSelect(')],
   'REPAIR_TICK':block(text,'static void repairTick('),
   'REPAIR_MANUAL':manual,
   'REPAIR_RESET':reset[resetStart:resetEnd]+reset[autoStart:autoEnd]+'pendingRepairs.clear();',
  }
  sections['SAMPLE_SCOPE']='static worldframe::DemandLeases runtimeDemandLeases;\nstatic void requestWorldRange('+sections['SAMPLE_SCOPE']
  defines=[]
  for name,value in sections.items():
   target=dest/(name.lower()+'.inc');target.write_text(value+'\n');defines.append('-D'+name+'="'+str(target)+'"')
  exe=dest/'fixture';commands=[['g++','-std=c++17','-O2','-UNDEBUG',*defines,str(here/'auto_repair_runtime.cpp'),'-o',str(exe)],[str(exe)]+(['--regression-only'] if before else [])];records=[]
  for command in commands:
   proc=subprocess.run(command,capture_output=True,text=True,timeout=60);records.append(dict(command=command,exit_code=proc.returncode,stdout=proc.stdout,stderr=proc.stderr));print(proc.stdout,proc.stderr,flush=True)
   if proc.returncode and len(records)==1:break
  cases=[];summary=None
  if len(records)==2:
   for line in records[-1]['stdout'].splitlines():
    parts=line.split('\t')
    if parts[0]=='CASE':cases.append(dict(name=parts[1],passed=parts[2]=='PASS',checks=int(parts[3]),failure=parts[4] if len(parts)>4 else None))
    elif parts[0]=='SUMMARY':summary=dict(cases=int(parts[1]),checks=int(parts[2]),failed=int(parts[3]))
  passed=len(records)==2 and records[0]['exit_code']==0 and ((summary and summary['cases']==3 and summary['failed']==3 and records[1]['exit_code']==1) if before else records[1]['exit_code']==0)
  return dict(passed=bool(passed),source_sha256=digest(source),negative_control=before,summary=summary,cases=cases,commands=records)
 baseline=run(a.before.resolve(),out/'before',True) if a.before else None
 current=run(r/'source/assist/source/bridge.cpp',out/'after')
 stable=hashes=={str(p.relative_to(r)):digest(p) for p in paths}
 result=dict(passed=current['passed'] and stable and (baseline is None or baseline['passed']),scope=__doc__,source_sha256=hashes,source_hash_stable=stable,game_process_executed=False,windows_runtime_executed=False,before=baseline,after=current)
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');raise SystemExit(not result['passed'])
if __name__=='__main__':main()
