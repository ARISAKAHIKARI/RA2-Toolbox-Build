"""Run the production control callback branch with a bounded synthetic memory model."""
import argparse, hashlib, json, subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--cxx',default='g++');p.add_argument('--out',type=Path,required=True);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
here=Path(__file__).resolve().parent;source=here.parent/'source';text=(source/'tech_ui_breakpoint_bridge.h').read_text();start=text.index('if(tag==4){');at=text.index('{',start);end=at+1;depth=1
while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
branch=a.out/'control-callback.inc';branch.write_text(text[start:end]+'\n');exe=a.out/'control-parameters'
start=text.index('if(tag==0){');at=text.index('{',start);end=at+1;depth=1
while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
full=a.out/'full-callback.inc';full.write_text(text[start:end].replace('reinterpret_cast<uint32_t*>(uintptr_t(context.Esp))','stack')+'\n')
paths=[source.parents[1]/'shared/source/hardware_slot_budget.h',source/'control_parameter_policy.h',source/'tech_ui_breakpoint_bridge.h',here/'control_parameters.cpp',Path(__file__)];hashes={str(n):hashlib.sha256(n.read_bytes()).hexdigest() for n in paths}
subprocess.run([a.cxx,'-std=c++17','-O2','-UNDEBUG','-DCONTROL_CALLBACK_BRANCH="'+branch.resolve().as_posix()+'"','-DFULL_CALLBACK_BRANCH="'+full.resolve().as_posix()+'"',str(here/'control_parameters.cpp'),'-o',str(exe)],check=True)
r=subprocess.run([str(exe.resolve())],capture_output=True,text=True,check=True);print(r.stdout,end='');assert hashes=={str(n):hashlib.sha256(n.read_bytes()).hexdigest() for n in paths}
(a.out/'result.json').write_text(json.dumps({'pass':True,'scope':__doc__,'source_sha256':hashes,'stdout':r.stdout,'game_process_tested':False,'multiplayer_tested':False},indent=2)+'\n')
