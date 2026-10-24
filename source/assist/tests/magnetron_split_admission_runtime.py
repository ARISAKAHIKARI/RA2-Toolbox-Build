"""Two magnets calculated on different sides share native target reservations.
Actual DLL/event constructors; synthetic actors, Win32 and unconsumed events.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for global_inside in (False,True):
  m=setup(mode,False);m.w(0xa8ed84,10);m.hook();m.b(0xa8e9a0,0);m.hook();s=state(m);s.computeRequested=192|(8192 if global_inside else 2);save(m,s);m.b(0xa8e9a0,1);m.w(0xa8ed84,1);m.hook()
  second=m.obj(0x7f5c70,m.t,m.house,700);spare=m.obj(0x7f5c70,m.t,m.enemy,800)
  for unit in (m.unit,second):
   m.w(unit+0x2ac,0);m.w(unit+0x2b4,0);m.w(unit+0x5a4,0);m.w(unit+0xac,5);m.w(unit+0xb4,0xffffffff);m.w(unit+0x9c,20*256+128);m.w(unit+0xa0,30*256+128)
  for target in (m.enemyUnit,spare):m.w(target+0x2b0,0);m.w(target+0xa4,0);m.w(target+0x9c,21*256+128);m.w(target+0xa0,30*256+128)
  m.vector(0xa8ec78,[m.unit,second,m.enemyUnit,spare]);s=state(m);s.flags=1024|262144;save(m,s);m.queryObject=0
  def select(s,intent):intent.sources[0]=m.PointerSource(700,second)
  s=pointer(m,1,2,select);assert s.pointer.accepted==1
  prior=count(m);s.magnetronGlobal=1;save(m,s);s=tick(m);assert count(m)==prior+1
  native=packets(m)[prior*111:(prior+1)*111];assert native[12]==1
  target=struct.unpack_from('<I',native,14)[0];outside=700 if global_inside else 10;token=next(i.token for i in s.world.intents[:s.world.intentCount] if i.id==outside)
  def host_plan(target):
   s=state(m);s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(outside,token,target,0,1,0,3,0);save(m,s);return tick(m)
  s=host_plan(target);assert s.planRejectedMask==1 and count(m)==prior+1
  alternate=800 if target!=800 else m.r(m.enemyUnit+0x10);s=host_plan(alternate);assert s.planAccepted==1 and count(m)==prior+2,(mode,global_inside,s.planRejectedMask)
  assert not m.codeWrites;rows.append({'case':f'{mode}_global_inside_{global_inside}_shared_target_reservation','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
print('PASS',len(rows),'split planning target reservations and alternative targets')
