"""Explicit global drop also releases hazardous beams during manual left attacks.
Runs actual Runtime/event constructors with modeled delayed native delivery;
does not simulate Windows input, projectile/lift physics or network peers.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
def current_drop(m):
 s=state(m);source=next(x for x in s.world.actors[:s.world.count] if x.id==10)
 s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(10,source.manualVersion,0,0,13,0,6,m.r(m.enemyUnit+0x10));save(m,s);return tick(m)
for mode in (0,3):
 for inside in (False,True):
  for target_kind in ('vehicle','building'):
   m=setup(mode,inside);m.w(m.building+0x21c,m.enemy)
   target=m.building if target_kind=='building' else m.obj(0x7f5c70,m.t,m.enemy,101)
   roster=[m.unit,m.enemyUnit,m.building]+([target] if target_kind=='vehicle' else [])
   m.vector(0xa8ec78,roster);m.queryObject=target;s=state(m);s.flags=1024|262144;s.magnetronGlobal=0;s.magnetronDrop=1;save(m,s)
   before=count(m);s=pointer(m,4,65536);assert s.pointer.result==1
   # Explicit global drop now has precedence even for manually controlled beams.
   s=tick(m)
   assert any(x.id==10 and x.flags&536870912 for x in s.world.actors[:s.world.count])
   if not inside:s=current_drop(m)
   assert count(m)==before+1 and packets(m)[before*111]==6,(mode,inside,target_kind,count(m))
   for _ in range(8):tick(m)
   assert count(m)==before+1,'the same beam must not spam Idle'
   # A different manual target does not disqualify an active hazardous beam.
   m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,target);s=tick(m)
   assert any(x.id==10 and x.flags&536870912 for x in s.world.actors[:s.world.count])
   before=count(m)
   # A subsequent lift remains eligible with global/enhanced orders disabled.
   next_victim=m.obj(0x7f5c70,m.t,m.enemy,102);roster.append(next_victim);m.vector(0xa8ec78,roster)
   for offset in (0x9c,0xa0,0xa4):m.w(next_victim+offset,m.r(m.enemyUnit+offset))
   m.w(m.enemyUnit+0x2b0,0);m.enemyUnit=next_victim;m.w(next_victim+0x2b0,m.unit);m.w(m.unit+0x2ac,next_victim);m.w(m.unit+0x2b4,next_victim)
   s=state(m);s.flags=0;save(m,s);s=tick(m)
   if not inside:s=current_drop(m)
   assert count(m)==before+1 and packets(m)[before*111]==6,('new beam remains eligible',mode,inside,count(m),before,s.planRejectedMask,[(x.id,x.flags,x.manualVersion,x.liftTarget) for x in s.world.actors[:s.world.count]])
   assert not m.codeWrites
   rows.append({'case':f'left_attack_old_beam_{mode}_{inside}_{target_kind}','status':'PASS'})
print('PASS',len(rows),'manual attacks / independent global drop cases')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')
