"""Actual x86 Runtime and ScrollMap/SetTacticalPosition; mocked mouse, edge/drag source and renderer. Not Windows, visual FPS or netplay validation."""
from pathlib import Path
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[];site=0x4a9840

def setup(mode):
 m=fixture(mode);m.precise_us=m.clock*1000;m.w(0xb73550,1);m.scrollDraws=0;m.edgeCalls=0;m.dragCalls=0;m.mousePoint=(799,300);m.dragMode=0
 for at,value in zip((0x87f8dc,0x87f8e4,0x87f8e8,0x87f8ec,0x87f8f0,0x886fa8,0x886fac,0x886fb8,0x886fbc),(80,2,4,70,60,640,600,800,600)):m.w(at,value)
 m.tactical=m.r(0x887324)
 for off,value in ((0xd64,0),(0xd68,2000),(0xd74,0),(0xd78,2000)):m.w(m.tactical+off,value)
 m.b(0x87f7e8+0x5559,1);m.b(0xb0cde0,1)
 for i,(x,y) in enumerate(((0,-1),(1,-1),(1,0),(1,1),(0,1),(-1,1),(-1,0),(-1,-1))):m.u.mem_write(0xb0ce38+i*8,struct.pack('<ii',x,y))
 mouse=m.alloc();table=m.alloc();getter=m.alloc(16);m.w(mouse,table);m.w(table+0x34,getter);m.w(0x887640,mouse);m.w(0x88731c,m.alloc());m.w(0x887308,m.alloc());m.w(0x87f770,m.alloc())
 def add(at,fn):m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_:fn(),begin=at,end=at)
 def arg(i=0):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 def point():out=arg();m.u.mem_write(out,struct.pack('<ii',300,300));m.ret(out,1)
 add(getter,point);add(0x54f5c0,lambda:m.ret(1,1));add(0x5fbf70,lambda:m.ret(m.dragMode,0));add(0x6da230,lambda:m.ret(0,1))
 add(0x6d8b30,lambda:m.ret(0,0)) # map projection cache, no real surfaces
 def draw():m.scrollDraws+=1;m.ret(0,0)
 add(0x4f4480,draw)
 distance=m.alloc(4);m.w(distance,10)
 stub=m.alloc(32);code=b'\x6a\x01\x68'+struct.pack('<I',distance)+b'\x6a\x02\xb8'+struct.pack('<I',site)+b'\xff\xd0\xc2\x04\x00';m.u.mem_write(stub,code)
 def scroll(which):
  if which=='edge':m.edgeCalls+=1
  else:m.dragCalls+=1
  m.u.reg_write(UC_X86_REG_EIP,stub)
 add(0x692b60,lambda:scroll('edge'));add(0x693440,lambda:scroll('drag'))
 return m

def pulse(m):m.precise_us+=8334;m.clock-=102;return m.hook() # 8.334 ms QPC; 8 ms coarse clock; same simulation frame

def record(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,3):
 for case in ('edge','drag','drag_displacement','idle','disabled','unfocused','chat','menu','foreign_entry','foreign_body','foreign_render','full_idle','full_outside','cursor_only','invalid_strategy'):
  m=setup(mode);original=bytes(m.u.mem_read(site,8));s=state(m);s.scrollRefreshMode=1;s.scrollRefreshHz=120
  if case.startswith('drag'):m.b(0x87f7e8+0x555a,1);m.b(0x87f7e8+0x5558,1);m.b(0x87f7e8+0x554c,1);m.mousePoint=(300,300)
  if case=='drag_displacement':m.dragMode=1
  if case in ('idle','full_idle'):m.mousePoint=(300,300)
  if case.startswith('full_'):s.scrollRefreshMode=2
  if case=='full_outside':m.mousePoint=(-10,-10)
  if case=='cursor_only':s.scrollRefreshMode=0
  if case=='invalid_strategy':s.scrollRefreshMode=3
  if case=='disabled':s.scrollRefreshHz=0
  if case=='unfocused':s.foreground=0
  if case=='chat':m.b(0xa8bc60+25,1)
  if case=='menu':m.w(0xa8eda0,1)
  if case=='foreign_entry':m.u.mem_write(site,b'\x90')
  if case=='foreign_body':m.u.mem_write(site+35,b'\x90')
  if case=='foreign_render':m.u.mem_write(0x4f4480,b'\x90')
  save(m,s);before=bytes(m.u.mem_read(m.unit,0x800));frame=m.r(0xa8ed84)
  for _ in range(6):s=pulse(m)
  active=case in ('edge','drag','drag_displacement');rendered=active or case.startswith('full_');assert m.scrollDraws==(6 if rendered else 0),(mode,case,m.scrollDraws,s.scrollRefreshStatus)
  assert m.r(0xa8ed84)==frame and count(m)==0 and before==bytes(m.u.mem_read(m.unit,0x800))
  if active:
   x=C.c_int32(m.r(m.tactical+0xd64)).value;assert x==(60 if case=='drag_displacement' else 30),(case,x)
   assert s.scrollRefreshDraws==6
  if case.startswith('full_'):assert m.edgeCalls==m.dragCalls==0 and s.scrollRefreshDraws==6
  if case.startswith('foreign'):assert s.scrollRefreshStatus==-2 and not m.codeWrites
  s=state(m);s.scrollRefreshHz=0;save(m,s);pulse(m)
  if case!='foreign_entry':assert bytes(m.u.mem_read(site,8))==original
  record(f'{mode}_{case}')
 # Live mode changes stop continuous drawing, resume it, and restore native code.
 m=setup(mode);m.mousePoint=(300,300);s=state(m);s.scrollRefreshMode=2;s.scrollRefreshHz=120;save(m,s);pulse(m);assert m.scrollDraws==1
 s=state(m);s.scrollRefreshMode=1;save(m,s);pulse(m);assert m.scrollDraws==1
 s=state(m);s.scrollRefreshMode=2;save(m,s);pulse(m);assert m.scrollDraws==2
 for boundary in ('foreground','chat','menu'):
  if boundary=='foreground':s=state(m);s.foreground=0;save(m,s)
  elif boundary=='chat':m.b(0xa8bc60+25,1)
  else:m.w(0xa8eda0,1)
  pulse(m);assert m.scrollDraws==2 and bytes(m.u.mem_read(site,8))==m.game.get_data(site-0x400000,8)
  if boundary=='foreground':s=state(m);s.foreground=1;save(m,s)
  elif boundary=='chat':m.b(0xa8bc60+25,0)
  else:m.w(0xa8eda0,0)
 s=state(m);s.scrollRefreshMode=0;save(m,s);pulse(m);assert m.scrollDraws==2
 record(f'{mode}_strategy_switch_and_boundaries')
 # Native behavior is retained when callback Capture belongs to another thread.
 m=setup(mode);s=state(m);s.scrollRefreshMode=1;s.scrollRefreshHz=120;save(m,s);pulse(m);m.current_thread=778
 d=m.alloc(4);m.w(d,10);before=m.r(m.tactical+0xd74);m.run(site,(2,d,1));assert m.r(m.tactical+0xd74)==before+10
 m.current_thread=777;s=state(m);s.stop=1;save(m,s);m.hook();assert bytes(m.u.mem_read(site,8))==m.game.get_data(site-0x400000,8)
 record(f'{mode}_wrong_thread_native_fallback_and_stop_restore')
# QPC advances while GetTickCount is quantized at 16 ms. Verify real Runtime
# draw counts, gaps and phase for 30-500 FPS targets, not just the pure helper.
for hz in (30,60,120,144,165,240,360,500):
 m=setup(0);m.mousePoint=(300,300);s=state(m);s.scrollRefreshMode=2;s.scrollRefreshHz=hz;save(m,s)
 origin=m.precise_us;base=m.clock;drawTimes=[]
 for elapsed in range(0,1001000,1000):
  m.precise_us=origin+elapsed;m.clock=base+(elapsed//16000)*16-110
  before=m.scrollDraws;s=m.hook()
  if m.scrollDraws!=before:drawTimes.append(elapsed)
 assert len(drawTimes)==hz+1,(hz,len(drawTimes))
 gaps=[b-a for a,b in zip(drawTimes,drawTimes[1:])]
 assert max(gaps)<=((1000000+hz-1)//hz)+1000 and min(gaps)>=1000000//hz-1000,(hz,min(gaps),max(gaps))
 assert s.scrollRefreshRateMilli==hz*1000 and s.scrollRefreshMaxGapUs==max(gaps)
 assert count(m)==0
 record(f'coarse_16ms_clock_precise_{hz}hz_phase_and_diagnostics')
# At 500 Hz, timed scrolling must retain native speed; live rate changes and
# missed slots must not restart a burst or advance simulation.
for strategy in (1,2):
 for kind in ('edge','drag'):
  m=setup(0);s=state(m);s.scrollRefreshMode=strategy;s.scrollRefreshHz=500;save(m,s)
  if kind=='drag':
   m.b(0x87f7e8+0x555a,1);m.b(0x87f7e8+0x5558,1);m.b(0x87f7e8+0x554c,1);m.mousePoint=(300,300)
  origin=m.precise_us;base=m.clock;frame=m.r(0xa8ed84)
  def wake(elapsed):
   m.precise_us=origin+elapsed;m.clock=base+(elapsed//16000)*16-110;return m.hook()
  for elapsed in range(0,1001000,1000):s=wake(elapsed)
  assert m.scrollDraws==501 and C.c_int32(m.r(m.tactical+0xd64)).value==604,(strategy,kind,m.scrollDraws,m.r(m.tactical+0xd64))
  # Changing rates resets phase once; repeated same-time wakes never catch up.
  s=state(m);s.scrollRefreshHz=120;save(m,s);wake(1001000);before=m.scrollDraws
  for _ in range(3):wake(1001000)
  assert m.scrollDraws==before
  s=state(m);s.scrollRefreshHz=500;save(m,s);wake(1002000);before=m.scrollDraws
  wake(1039000);assert m.scrollDraws==before+1
  before=m.scrollDraws
  for _ in range(3):wake(1039000)
  assert m.scrollDraws==before and m.r(0xa8ed84)==frame and count(m)==0
  record(f'scope_{strategy}_{kind}_500hz_native_speed_live_rates_and_missed_slots')
Path(a.output).write_text(json.dumps({'scope':'Actual x86 Runtime + native ScrollMap/SetTacticalPosition; mocked Win32, input producers and renderer; QPC/coarse-clock cadence simulation, not visual FPS or netplay','dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
