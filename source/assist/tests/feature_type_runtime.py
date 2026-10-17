"""Actual x86 DLL filter ingress and queued-task revalidation; world/Win32 are modeled."""
import traceback
from pathlib import Path
# Reuse the input fixture only; no tests from the base suite are executed here.
_base=Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0]
exec(compile(_base,str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def config(m,feature,setting,identity=b'HTNK',flags=0):
 s=state(m);f=s.featureConfig.filters[feature];f.custom=int(setting!='default');f.count=int(setting not in ('default','empty'))
 f.ids[0].value=identity if setting=='include' else b'OTHER';s.featureConfigSeq+=2;s.flags=flags;save(m,s)
def command(m,n):
 s=state(m);s.command=n;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock;save(m,s);return m.hook()
def weapons(m,mag=False):
 projectile=m.alloc();warhead=m.alloc();m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,warhead);m.w(m.weapon+0xb4,30000);m.b(projectile+0x2a5,1)
 for n in range(11):m.u.mem_write(warhead+0xa0+n*8,struct.pack('<d',1.))
 if mag:m.b(m.weapon+0x15c,1);m.b(warhead+0x15b,1)
 m.w(m.enemy+0x34,m.alloc());m.w(m.unit+0x18c,0);return warhead
for mode in (0,3):
 for feature in range(14):
  for setting in ('default','include','exclude','empty'):
   def scenario(mode=mode,feature=feature,setting=setting):
    m=fixture(mode,0);expected=setting in ('default','include');flags=0;identity=b'HTNK'
    if feature==0:flags=2097152;m.b(m.unit+0x134,1);weapons(m);m.queryObject=m.enemyUnit
    elif feature==1:flags=4194304;m.u.mem_write(m.t+0x24,b'DISK\0');identity=b'DISK';m.queryObject=m.building;m.w(m.building+0x9c,20*256+128);m.w(m.building+0xa0,30*256+128)
    elif feature==2:flags=1024|262144;weapons(m,True)
    elif feature==3:flags=1024|8192
    elif feature in (4,5):
     m.u.mem_write(m.engT+0x380,struct.pack('<d',1.));m.u.mem_write(m.t+0x388,struct.pack('<d',3.));m.vector(0xa8ecb8,[m.eng]);identity=b'ENGINEER' if feature==4 else b'HTNK'
    elif feature in (6,7):
     flags=1048576;m.u.mem_write(m.engT+0x380,struct.pack('<d',1.));m.u.mem_write(m.t+0x388,struct.pack('<d',3.));m.queryObject=m.unit;m.b(m.eng+0x83,1);identity=b'ENGINEER' if feature==6 else b'HTNK'
    elif feature in (8,9,10,11):
     flags=1024|(16384 if feature in (8,9) else 2048);identity=b'ENGINEER' if feature in (9,11) else b'HTNK'
     m.vector(0xa8ecb8,[m.unit]);m.vector(0xa83de8,[m.eng]);m.w(m.unit+0x114,1);m.w(m.unit+0x118,m.eng);m.w(m.eng+0x11c,m.unit);m.b(m.eng+0x81,1);m.b(m.eng+0x90,0)
    elif feature==12:
     flags=16777216;m.queryObject=m.unit;m.w(m.unit+0x9c,20*256+128);m.w(m.unit+0xa0,30*256+128);m.w(m.crateCell+0xe4,m.building)
    elif feature==13:flags=64;identity=b'NAPOWR'
    config(m,feature,setting,identity,flags)
    if feature==0:
     s=pointer(m,1,16);assert bool(s.cooperativeRequest.count)==expected,(s.cooperativeRequest.count,s.pointer.result)
    elif feature==1:s=pointer(m,1,64);assert count(m)==int(expected)
    elif feature in (2,3):s=pointer(m,1,8192|(2 if feature==2 else 1));assert count(m)==int(expected) and s.data.advancedUnits==int(expected),(count(m),s.data.advancedUnits,s.pointer.result)
    elif feature in (4,5):s=command(m,15);assert count(m)==int(expected),(s.result,count(m))
    elif feature in (6,7):
     s=pointer(m,1,8,lambda s,i:setattr(i.sources[0],'id',11) or setattr(i.sources[0],'pointer',m.eng));assert bool(s.cooperativeRequest.count)==expected,(s.cooperativeRequest.count,s.pointer.result)
    elif feature in (8,9):
     s=command(m,17);s=tick(m);assert count(m)==int(expected if feature==8 else True),(feature,count(m),s.result)
     assert bool(s.data.unloadPending)==expected,(s.data.unloadPending,feature)
    elif feature in (10,11):s=tick(m);assert s.data.ejectWatched==int(expected)
    elif feature==12:s=pointer(m,3,64);assert count(m)==int(expected)
    elif feature==13:s=m.hook();assert count(m)==int(expected)
    assert not m.codeWrites
   check(f'filter_{mode}_{feature}_{setting}',scenario)
 for case in ('plane_allowed','plane_revoke','magnet_allowed','magnet_revoke','advanced_revoke','half_written','invalid_count','repair_revoke','host_restart_same_seq'):
  def pending(mode=mode,case=case):
   m=fixture(mode,0)
   if case=='host_restart_same_seq':
    config(m,3,'include',flags=1024|8192);s=pointer(m,1,8192|1);assert count(m)==1
    prior_seq=s.featureConfigSeq;s.stop=1;save(m,s);m.hook()
    s=state(m);s.stop=0;s.pointer.request=s.pointer.ack=0;s.featureConfig.filters[3].ids[0].value=b'OTHER';assert s.featureConfigSeq==prior_seq;save(m,s);s=m.hook()
    s=pointer(m,1,8192|1);assert count(m)==1 and s.data.advancedUnits==0,(count(m),s.data.advancedUnits,s.pointer.result)
   elif case.startswith('plane'):

    m.b(m.unit+0x134,1);weapons(m);m.queryObject=m.enemyUnit;config(m,0,'include',flags=2097152);s=pointer(m,1,16)
    assert s.cooperativeRequest.count==1
    s.cooperativePlan+=1;s.cooperativeSerial=s.cooperativeRequest.serial;s.cooperativeMode=2;s.cooperativeRequest.mode=2;s.cooperativeCount=1;s.cooperativeTick=m.clock;s.cooperativeFrame=s.data.frame;s.cooperativeMatch=s.data.match;s.cooperativeOrders[0]=m.CooperativeAssignment(10,s.cooperativeRequest.sources[0].token,13,m.t);save(m,s)
    if case.endswith('revoke'):config(m,0,'exclude',flags=2097152)
    s=tick(m);assert s.cooperativeAck==s.cooperativePlan and s.cooperativeAccepted==int(case.endswith('allowed')),(s.cooperativeAccepted,s.cooperativeRejected,s.cooperativeResult)
   elif case.startswith(('magnet','advanced')):
    mag=case.startswith('magnet');weapons(m,mag);feature=2 if mag else 3;flags=1024|(262144 if mag else 8192);config(m,feature,'include',flags=flags);s=pointer(m,1,8192|(2 if mag else 1));assert s.data.advancedUnits==1
    s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame;s.plan[0]=m.Order(10,1,13,0,1,0,3 if mag else 1,0);save(m,s)
    if case.endswith('revoke'):config(m,feature,'exclude',flags=flags)
    s=tick(m);assert s.planAck==s.planRequest and s.planAccepted==int(case.endswith('allowed')),(s.planAccepted,s.planResult)
   elif case=='repair_revoke':
    m.w(0xa802c8,112) if mode==3 else m.w(0x8b41f8,16384)
    config(m,13,'include',b'NAPOWR',64);s=m.hook();assert s.data.repairTotal==0
    config(m,13,'exclude',b'NAPOWR',64);m.w(0xa802c8,0);m.w(0x8b41f8,0);s=tick(m);assert count(m)==0
   else:
    config(m,3,'include',flags=1024|8192);s=state(m)
    if case=='half_written':s.featureConfigSeq+=1
    else:s.featureConfig.filters[3].count=513
    save(m,s);s=pointer(m,1,8192|1);assert count(m)==0 and s.data.advancedUnits==0
   assert not m.codeWrites
  check(f'pending_filter_{mode}_{case}',pending)
Path(a.output).write_text(json.dumps({'inputs':{'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()},'scope':__doc__,'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))
