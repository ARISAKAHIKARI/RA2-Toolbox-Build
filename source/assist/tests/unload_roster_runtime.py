"""Focused comparison of actual pre/post production roster observation code."""
import argparse,hashlib,json,subprocess
from pathlib import Path

def block(text,marker):
 start=text.index(marker);opening=text.index('{',start);depth=1;end=opening+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--before',type=Path,required=True);a=p.parse_args()
 root=Path(__file__).resolve().parents[3];here=Path(__file__).resolve().parent;out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 source=root/'source/assist/source/native_orders_bridge.h';current=block(source.read_text(),'static void observeUnloadRoster(');before=block(a.before.read_text(),'static void observeUnloadRoster(').replace('observeUnloadRoster(','observeUnloadRosterBefore(',1)
 paths=[source,root/'source/assist/source/unload_policy.h',here/'unload_roster_runtime.cpp',Path(__file__).resolve()];hashes={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths}
 commands=[]
 for name,content in [('before',before),('current',current)]: (out/(name+'.inc')).write_text(content+'\n')
 exe=out/'fixture'
 for cmd in [['g++','-std=c++17','-O2','-UNDEBUG','-DROSTER_BEFORE="'+str(out/'before.inc')+'"','-DROSTER_CURRENT="'+str(out/'current.inc')+'"',str(here/'unload_roster_runtime.cpp'),'-o',str(exe)],[str(exe)]]:
  r=subprocess.run(cmd,capture_output=True,text=True,timeout=60);commands.append(dict(command=cmd,exit_code=r.returncode,stdout=r.stdout,stderr=r.stderr));print(r.stdout,r.stderr,flush=True)
  if r.returncode:break
 stable=hashes=={str(x.relative_to(root)):hashlib.sha256(x.read_bytes()).hexdigest() for x in paths};result=dict(passed=all(r['exit_code']==0 for r in commands) and len(commands)==2 and stable,source_hashes=hashes,source_hash_stable=stable,baseline_sha256=hashlib.sha256(a.before.read_bytes()).hexdigest(),commands=commands,game_process_executed=False,windows_runtime_executed=False)
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');raise SystemExit(not result['passed'])
if __name__=='__main__':main()
