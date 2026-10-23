"""Explicit Sticky owns native mission transitions only in single-player.
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
 m=fixture(mode);before=bytes(m.u.mem_read(m.unit,0x700));s=command(m)
 assert C.c_int32(s.result).value==-40 and not count(m) and bytes(m.u.mem_read(m.unit,0x700))==before
 rows.append({'case':f'network_{mode}_no_local_mutation','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':PROBE_SCOPE,'results':rows},indent=2)+'\n')
print('PASS',len(rows),'explicit Sticky hold/release/manual/cleanup/session cases')
