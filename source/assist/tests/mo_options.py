"""Source-bound MO geometry/capture/options tests; no game process."""
from pathlib import Path
import argparse,hashlib,json,subprocess,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import smartarea_capacity as capacity
import mo_orders_bridge_test as mo
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1];SOURCE=HERE.parent/'source'
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--zig',required=True,type=Path);p.add_argument('--out',required=True,type=Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
 fixture=HERE/'fixtures/mo_smartarea_baseline';manifest=json.loads((fixture/'manifest.json').read_text())
 for name,digest in manifest['files'].items():assert hashlib.sha256((fixture/name).read_bytes()).hexdigest()==digest
 paths=['assist/source/mo_orders.h','assist/source/mo_gesture_bridge.h','assist/source/mo_orders_bridge.h','assist/source/bridge.cpp','assist/source/mo_patrol_graph.h','assist/source/mo_patrol_planning_bridge.h','assist/tests/mo_options.py','assist/tests/mo_options.cpp','assist/tests/smartarea_capacity.py','assist/tests/smartarea_capacity.cpp','assist/tests/mo_orders_bridge_test.py','assist/tests/formation_quick_bridge_test.py','assist/tests/fixtures/mo_smartarea_baseline/mo_orders_geometry.h']
 snapshot=lambda:{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in paths};before=snapshot()
 old=(fixture/'mo_orders_geometry.h').read_text(encoding='utf-8-sig').replace('#include "../../shared/source/key_gesture.h"','#include '+json.dumps((ROOT/'shared/source/key_gesture.h').as_posix()))
 oldpath=a.out/'old-geometry.inc';oldpath.write_text(old,encoding='utf-8')
 body=(HERE/'smartarea_capacity.cpp').read_text().split('int main(){')[0]
 body+='\nnamespace historical{\n#include '+json.dumps(oldpath.resolve().as_posix())+'\n}\n'
 body+=capacity.block(mo.source(),'static void graph(')+'\n'+(HERE/'mo_options.cpp').read_text()
 text=capacity.source((SOURCE/'bridge.cpp').read_text(encoding='utf-8-sig'),SOURCE/'mo_orders_bridge.h',body)
 text=text.replace('static void MemoryBarrier(){}','static std::function<void()> barrierHook;static void MemoryBarrier(){if(barrierHook){auto run=std::move(barrierHook);barrierHook={};run();}}')
 cpp=a.out/'fixture.cpp';cpp.write_text(text,encoding='utf-8');results={}
 for arch in ('x86_64','x86'):
  exe=a.out/(arch+'.exe');run=subprocess.run([str(a.zig.resolve()),'c++','-target',arch+'-windows-gnu','-std=c++17','-O1','-UNDEBUG','-I'+str(SOURCE),str(cpp),'-o',str(exe)],capture_output=True,text=True);(a.out/(arch+'-build.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
  if run.returncode:raise SystemExit(run.stdout+run.stderr)
  run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=90);(a.out/(arch+'.log')).write_text(run.stdout+run.stderr,encoding='utf-8')
  if run.returncode:raise SystemExit(run.stdout+run.stderr)
  results[arch]=json.loads(run.stdout.strip().splitlines()[-1])
 changed=[n for n,d in snapshot().items() if d!=before[n]];report={'results':results,'sources':before,'changed_during_run':changed,'baseline':manifest,'scope':'Actual MO pure geometry, native gesture snapshot/capture and patrol state machine with controlled world/queue/Win32 boundaries. No game process or multiplayer acceptance.'}
 (a.out/'result.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert not changed,changed
