"""Compiled Runtime must emit interruptible Guard and reject old Sticky plans.
Native event constructors/outgoing queues are real code; world/Win32 and event
delivery are modeled. Native mission readiness is tested separately.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for inside in (False,True):
  for requested in ((5,6) if not inside else (5,)):
   m=setup(mode,inside)
   m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.enemyUnit+0xa4,0)
   m.w(m.unit+0x2b4,0);m.w(m.unit+0x5a4,0);m.w(m.unit+0xac,5);m.w(m.unit+0xb4,0xffffffff)
   m.w(m.enemyUnit+0x9c,100000)
   s=state(m);s.flags=1024|262144;s.magnetronGlobal=1;s.magnetronDrop=0;save(m,s)
   s=tick(m);assert s.data.advancedUnits==1
   if not inside:
    s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame
    s.plan[0]=m.Order(10,s.world.intents[0].token,0,0,requested,0,3,0);save(m,s);s=tick(m)
   if requested==6:
    assert count(m)==0 and s.planRejectedMask==1
   else:
    assert count(m)==1,(mode,inside,s.planAccepted,s.planRejectedMask,count(m))
    event=packets(m);assert event[0]==4 and event[12]==5 and event[14:24]==bytes(10),event[:30].hex()
    # Native delivery is deliberately not simulated as an unconditional write:
    # the separate native probe proves Guard permits the next queued mission.
    for _ in range(5):tick(m)
    assert count(m)==1
   before=count(m);s=state(m);s.flags=0;s.magnetronGlobal=0;save(m,s)
   for _ in range(4):s=tick(m)
   assert count(m)==before and s.data.advancedUnits==0 and not m.codeWrites
   rows.append({'case':f'guard_{mode}_{inside}_{requested}','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')
print('PASS',len(rows),'Runtime guard/old-sticky-rejection/disable cases')
