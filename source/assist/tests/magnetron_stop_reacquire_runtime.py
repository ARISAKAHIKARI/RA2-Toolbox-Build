"""First native attack -> Stop -> global enrolment -> plain LEFT attack.
Production host routing + compiled Runtime, native event constructors, modeled
native delivery/AI fields. Does not execute real movement or network peers.
"""
from pathlib import Path
import subprocess,tempfile
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'route.cpp';exe=Path(d)/'route'
 cpp.write_text('#include "'+str(root/'assist/source/pointer_intent.h')+'"\n#include <cstdio>\nint main(){using namespace assist::pointerinput;unsigned buttons[MouseCount]{};unsigned char keys[256]{};MouseRoute routes[MouseCount];for(bool all:{false,true}){int n=resolveMouseMatches(keys,6,buttons,{},0,all,routes);printf("%d %d %u\\n",n,n?routes[0].operation:0,n?routes[0].bindings:0);}}'.replace('#include <cstdio>','#include <cstdio>\n#include <initializer_list>'))
 subprocess.run(['g++','-std=c++17',str(cpp),'-o',str(exe)],check=True)
 routes=[tuple(map(int,line.split())) for line in subprocess.check_output([str(exe)],text=True).splitlines()]
assert all(count_==1 and operation==4 and bindings&65536 for count_,operation,bindings in routes)
rows=[]
for mode in (0,3):
 for inside in (False,True):
  for target_kind in ('vehicle','building'):
   for all_matches,(_,operation,bindings) in enumerate(routes):
    m=setup(mode,inside);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.enemyUnit+0xa4,0)
    # First ordinary Attack is already active, so global mode leaves it alone.
    m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,m.enemyUnit);m.w(m.unit+0x5a4,0)
    m.w(m.enemyUnit+0x9c,m.r(m.unit+0x9c)+4352);s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;s.magnetronDrop=0;save(m,s)
    s=tick(m);assert s.data.advancedUnits==0 and count(m)==0
    m.w(m.unit+0x9c,m.r(m.unit+0x9c)+256);tick(m)
    # Player Stop, followed by native idle and the next automatic acquisition.
    pointer(m,5);m.w(m.unit+0xac,5);m.w(m.unit+0xb4,0xffffffff);m.w(m.unit+0x2b4,0)
    m.w(0xa8ed84,m.r(0xa8ed84)+91);s=tick(m);assert s.data.advancedUnits==1
    old=count(m);old_token=s.world.intents[0].token
    m.w(m.building+0x21c,m.enemy);m.queryObject=m.building if target_kind=='building' else m.enemyUnit
    s=pointer(m,operation,bindings);assert s.pointer.result==1 and s.pointer.ack==s.pointer.request
    for _ in range(8):
     s=tick(m);assert s.data.advancedUnits==0 and count(m)==old,('retook second native attack',mode,inside,target_kind,all_matches)
    # A late external old Attack must not revive the discarded controller task.
    if not inside:
     s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(10,old_token,m.r(m.enemyUnit+0x10),0,1,0,3,0);save(m,s)
     s=tick(m);assert s.planRejectedMask==1 and count(m)==old
    m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,m.queryObject);m.w(0xa8ed84,m.r(0xa8ed84)+100)
    s=tick(m);assert s.data.advancedUnits==0 and count(m)==old
    # Another Stop/idle cycle and manual ground movement also remain native.
    pointer(m,5);m.w(m.unit+0xac,5);m.w(m.unit+0x2b4,0);m.w(0xa8ed84,m.r(0xa8ed84)+91);s=tick(m);assert s.data.advancedUnits==1
    old=count(m);m.queryObject=0;s=pointer(m,operation,bindings)
    assert s.pointer.ack==s.pointer.request and s.pointer.result==1
    for _ in range(8):s=tick(m);assert s.data.advancedUnits==0 and count(m)==old
    assert not m.codeWrites
    rows.append({'case':f'first_stop_reacquire_left_{mode}_{inside}_{target_kind}_{all_matches}','status':'PASS'})
# Selecting a friendly object is not an order to the previously selected magnet.
for action in (7,8,31,3):
 m=setup(0,True);m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.enemyUnit+0xa4,0);m.w(m.unit+0x2b4,0);m.w(m.unit+0xac,5)
 s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;save(m,s);s=tick(m);assert s.data.advancedUnits==1
 m.queryObject=m.building;m.action_overrides[(m.unit,m.building)]=action
 s=pointer(m,routes[0][1],routes[0][2]);s=tick(m)
 assert s.data.advancedUnits==int(action!=3),(action,s.data.advancedUnits)
 rows.append({'case':f'friendly_click_native_action_{action}','status':'PASS'})
print('PASS',len(rows),'first attack / stop / global reacquisition / ordinary LEFT ownership cases')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')
