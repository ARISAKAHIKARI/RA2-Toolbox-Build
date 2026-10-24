"""Explicit Sticky: local SP transitions and network entry by synchronized event.
Executes the compiled DLL and original ForceMission/NextMission instructions.
World, Win32 and event delivery are fixtures, not a live game or netplay test.
"""
from pathlib import Path
PROBE_SCOPE=__doc__
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
def command(m,cmd=28):
 s=state(m);s.request+=1;s.command=cmd;s.requestMatch=s.data.match;s.requestTick=m.clock;save(m,s);return tick(m)
def arm(mode=0):
 m=fixture(mode);m.vector(0xa8ecb8,[m.unit]);m.w(m.unit+0xac,5);m.w(m.unit+0xb4,0xffffffff)
 s=command(m);assert s.result==1 and s.count==1,(s.result,s.count)
 assert m.r(m.unit+0xac)==6 and m.r(m.unit+0xb4)==0xffffffff
 return m
for mode in (0,5):
 for action in ('attack','move','repeat_key','manual_left','right_cancel','cancel_tasks','disable','recycled_id'):
  m=arm(mode);before=count(m)
  if action in ('attack','move'):
   requested=1 if action=='attack' else 2;m.w(m.unit+0xb4,requested);tick(m)
   assert m.r(m.unit+0xac)==requested and m.r(m.unit+0xb4)==0xffffffff
  elif action=='repeat_key':command(m);assert m.r(m.unit+0xac)==5
  elif action=='manual_left':pointer(m,4,65536);assert m.r(m.unit+0xac)==5
  elif action=='right_cancel':pointer(m,4,1024);assert m.r(m.unit+0xac)==6
  elif action=='cancel_tasks':command(m,18);assert m.r(m.unit+0xac)==5
  elif action=='disable':
   s=state(m);s.stop=1;save(m,s);m.hook();assert m.r(m.unit+0xac)==5
  elif action=='recycled_id':
   m.w(m.unit+0x10,999);tick(m);assert m.r(m.unit+0xac)==6
  assert not m.codeWrites
  rows.append({'case':f'{mode}_{action}','status':'PASS'})
for mode in (3,4):
 for case in ('entry','repeat_pending','observed_repeat','manual','stop_tool','recycled','foreign','queue_full','unsupported'):
  m=fixture(mode);m.vector(0xa8ecb8,[m.unit]);m.w(m.unit+0xac,5);m.w(m.unit+0xb4,0xffffffff)
  if case=='foreign':m.w(m.unit+0x21c,m.enemy)
  if case=='queue_full':m.w(0xa802c8,112)
  if case=='unsupported':m.w(0xa8b238,2)
  before=bytes(m.u.mem_read(m.unit,0x700));prior=count(m);s=command(m)
  accepted=case not in ('foreign','queue_full','unsupported')
  assert count(m)==prior+int(accepted),(mode,case,s.result,s.count,count(m))
  assert bytes(m.u.mem_read(m.unit,0x700))==before,'network command must not locally execute mission'
  if accepted:
   event=packets(m)[prior*111:(prior+1)*111];assert event[0]==4 and event[12]==6 and event[7:12]==struct.pack('<IB',10,0x34)
   assert event[13:24]==bytes(11),'Sticky has no target or destination'
   if case=='observed_repeat':m.w(m.unit+0xac,6)
   if case in ('repeat_pending','observed_repeat'):command(m)
   if case=='manual':pointer(m,4,65536)
   if case=='stop_tool':s=state(m);s.stop=1;save(m,s);m.hook()
   if case=='recycled':m.w(m.unit+0x10,999);tick(m)
   assert count(m)==prior+1 and m.r(m.unit+0xac)==(6 if case=='observed_repeat' else 5)
  assert not m.codeWrites
  rows.append({'case':f'network_{mode}_{case}_synchronized_no_local_mutation','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'explicit Sticky hold/release/manual/cleanup/session cases')
