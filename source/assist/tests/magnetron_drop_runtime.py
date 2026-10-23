"""Execute compiled Runtime sampling/admission and native Idle constructors.
Synthetic Win32/world/terrain; release and falling physics are not simulated.
No claim of real multiplayer or guaranteed kill after delivery latency.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
def setup(mode,inside):
 m=fixture(mode);projectile=m.alloc();warhead=m.alloc();m.w(m.weapon+0xb4,4096);m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,warhead);m.b(projectile+0x2a5,1);m.b(m.weapon+0x15c,1);m.b(warhead+0x15b,1)
 m.w(m.enemy+0x34,m.alloc());m.w(m.unit+0x18c,0)
 for n in range(11):m.u.mem_write(warhead+0xa0+n*8,struct.pack('<d',1.))
 m.vector(0xa8ec78,[m.unit,m.enemyUnit,m.building]);m.w(m.unit+0x2ac,m.enemyUnit);m.w(m.enemyUnit+0x2b0,m.unit)
 for obj in (m.unit,m.enemyUnit):m.w(obj+0x9c,20*256+128);m.w(obj+0xa0,30*256+128)
 m.w(m.enemyUnit+0xa4,512);m.w(m.crateCell+0xe4,m.building);m.floor_height=0
 m.clock+=16;m.b(0xa8e9a0,0);m.hook();s=state(m);s.computeRequested=192|(2 if inside else 0);save(m,s)
 m.clock+=16;m.b(0xa8e9a0,1);m.w(0xa8ed84,1);s=m.hook();assert s.computeApplied==192|(2 if inside else 0)
 return m

def submit(m):
 s=state(m);s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(10,0,0,0,13,0,6,m.r(m.enemyUnit+0x10));save(m,s);return tick(m)
for mode in (0,3):
 for inside in (False,True):
  m=setup(mode,inside);s=state(m);s.magnetronDrop=1;save(m,s);s=tick(m)
  assert s.world.count>=1 and any(x.id==10 and x.flags&536870912 for x in s.world.actors[:s.world.count]),('no hazard',mode,inside,[(x.id,x.flags,x.liftTarget) for x in s.world.actors[:s.world.count]],hex(m.r(m.unit+0x2ac)),hex(m.r(m.enemyUnit+0x2b0)),m.r(m.enemyUnit+0xa4),hex(m.r(0x87f7e8+316)),m.r(0x87f7e8+320),m.calls[-10:])
  if not inside:s=submit(m)
  assert count(m)==1,(mode,inside,s.planAccepted,s.planRejectedMask,s.data.lastSubmitError,count(m))
  event=packets(m);assert event[0]==6 and event[7:12]==struct.pack('<IB',10,0x34),event[:20].hex()
  for n in range(4):tick(m)
  assert count(m)==1 and not m.codeWrites
  m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);tick(m);assert count(m)==1
  rows.append({'case':f'native_stop_{mode}_{inside}','status':'PASS'})
 for race in ('disabled','bridge','ground','broken_link','filter'):
  m=setup(mode,False);s=state(m);s.magnetronDrop=1;save(m,s);tick(m)
  if race=='disabled':s=state(m);s.magnetronDrop=0;save(m,s)
  if race=='bridge':m.w(m.crateCell+0x140,0x100)
  if race=='ground':m.w(m.enemyUnit+0xa4,0)
  if race=='broken_link':m.w(m.enemyUnit+0x2b0,0)
  if race=='filter':
   s=state(m);s.featureConfig.filters[19].custom=1;s.featureConfig.filters[19].count=0;s.featureConfigSeq+=2;save(m,s)
  s=submit(m);assert count(m)==0 and s.planRejectedMask==1,(race,s.planAccepted,s.planRejectedMask,count(m));assert not m.codeWrites
  rows.append({'case':f'race_{mode}_{race}','status':'PASS'})
print('PASS',len(rows),'native Idle/outgoing cases: independent toggle, both compute locations, dedup, release, live races and independent type filter')
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'inputs':{Path(x).name:hashlib.sha256(Path(x).read_bytes()).hexdigest() for x in (a.game,a.dll)},'results':rows},indent=2)+'\n')
