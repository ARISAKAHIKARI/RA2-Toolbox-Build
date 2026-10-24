"""Final Runtime: independent explicit/global/drop calculation routing.
Controlled world and Win32; original constructors and outgoing lists execute.
This is not a whole-game, drop-physics or two-peer network test.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('magnetron_drop_runtime.py').read_text().split('for mode in (0,3):',1)[0],str(Path(__file__).with_name('magnetron_drop_runtime.py')),'exec'))
rows=[]
for mode in (0,3):
 for combo in range(8):
  mask=192|(2 if combo&1 else 0)|(8192 if combo&2 else 0)|(16384 if combo&4 else 0)
  for family,bit in (('explicit',1),('global',2),('drop',4)):
   m=setup(mode,False);m.w(0xa8ed84,10);m.hook();m.clock+=16;m.b(0xa8e9a0,0);m.hook();s=state(m);s.computeRequested=mask;save(m,s)
   m.clock+=16;m.b(0xa8e9a0,1);m.w(0xa8ed84,1);s=m.hook();assert s.computeApplied==mask,(mode,combo,family,mask,s.computeApplied,s.data.match,s.data.status)
   if family!='drop':
    m.w(m.unit+0x2ac,0);m.w(m.enemyUnit+0x2b0,0);m.w(m.enemyUnit+0xa4,0);m.w(m.unit+0x2b4,0);m.w(m.unit+0x5a4,0);m.w(m.unit+0xac,5);m.w(m.unit+0xb4,0xffffffff);m.w(m.enemyUnit+0x9c,100000)
    s=state(m);s.flags=1024|262144;s.magnetronGlobal=int(family=='global');save(m,s)
    if family=='explicit':m.queryObject=0;pointer(m,1,2)
   else:s=state(m);s.magnetronDrop=1;save(m,s)
   prior=count(m);s=tick(m);inside=bool(combo&bit);assert count(m)==prior+int(inside),(mode,combo,family,prior,count(m),s.world.intentCount)
   # The same common boundary rejects a host proposal for an in-game feature.
   token=next((i.token for i in s.world.intents[:s.world.intentCount] if i.id==10),0)
   s.planRequest+=1;s.planCount=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=s.data.frame
   s.plan[0]=m.Order(10,token,0,0,13 if family=='drop' else 5,0,6 if family=='drop' else 3,m.r(m.enemyUnit+0x10) if family=='drop' else 0)
   save(m,s);s=tick(m)
   assert count(m)==prior+1 and s.planRejectedMask==int(inside) and s.planAccepted==int(not inside),(mode,combo,family,count(m),s.planAccepted,s.planRejectedMask)
   packet=packets(m)[prior*111:(prior+1)*111];assert packet[0]==(6 if family=='drop' else 4)
   if family!='drop':assert packet[12]==5
   assert not m.codeWrites
   rows.append({'case':f'{mode}_{combo}_{family}','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
print('PASS',len(rows),'independent magnet calculation routes and wrong-backend rejection')
