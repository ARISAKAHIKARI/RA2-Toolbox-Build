"""Compiled DLL through common WH_GETMESSAGE; synthetic map/Win32/actors.
Tests input isolation, original Move packets and cancellation, not live pathfinding or Windows rendering.
"""
from pathlib import Path
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
rows=[]
def setup(mode=0,count=1):
 m=fixture(mode);m.msg=m.alloc();m.w(0xAC13C8,104)
 for y in range(18,44):
  for x in range(15,36):
   cell=m.alloc(0x200);m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));m.w(m.cells+(y*512+x)*4,cell)
 m.units=[m.unit]+[m.obj(0x7f5c70,m.t,m.house,20+j) for j in range(count-1)];m.vector(0xa8ec78,m.units);m.vector(0xa8ecb8,m.units);s=state(m);s.formationPolicy.enabled=1;s.formationPolicy.hold=18;s.formationPolicy.keys[:]=[90,90,67,90];save(m,s);tick(m);m.codeWrites.clear();return m

def message(m,kind,key=0,repeat=False,x=100,y=100):
 lp=(1<<30) if repeat else 0
 if kind in (0x204,0x205,0x200):lp=x|(y<<16)
 m.queryPoint=(x,y);m.queryCell=(20+(x-100)//30,30+(y-100)//30)
 m.u.mem_write(m.msg,struct.pack('<7I',1,kind,key,lp,m.clock,0,0));m.run(m.entry,(0,1,m.msg));return m.r(m.msg+4),state(m)
for mode in (0,3):
 for case in ('click','cycle','escape','chat','focus','disabled','duplicate_up','double_alt','custom_double_alt','policy_in_flight','advanced_frame'):
  m=setup(mode);m.held_keys.add(18);kind,s=message(m,0x104,18);assert kind==0 and s.formationPreview.active,(case,kind,s.data.status,s.formationPreview.active)
  if case=='double_alt':
   m.held_keys.clear();message(m,0x105,18);m.clock+=100;m.w(m.SH+m.Shared.heartbeat.offset,m.clock);m.held_keys.add(18);message(m,0x104,18);m.held_keys.clear();_,s=message(m,0x105,18);assert s.formationPreview.active
  if case=='cycle':
   for expected in (1,3,0):
    kind,s=message(m,0x104,90);assert kind==0 and s.formationPreview.shape==expected
    _,s=message(m,0x104,90,True);assert s.formationPreview.shape==expected;kind,s=message(m,0x105,90);assert kind==0
  if case=='advanced_frame':m.w(0xA8ED84,m.r(0xA8ED84)+2)
  if case=='policy_in_flight':m.w(m.SH+m.Shared.formationPolicySeq.offset,1)
  if case=='custom_double_alt':
   m.held_keys.clear();message(m,0x105,18);s=state(m);s.formationPolicy.hold=32;save(m,s);m.clock+=100;m.w(m.SH+m.Shared.heartbeat.offset,m.clock);m.held_keys.add(18)
   kind,s=message(m,0x104,18);assert kind==0 and s.formationPreview.active and s.formationPreview.slots[0].valid
   m.held_keys.clear();kind,s=message(m,0x105,18);assert kind==0 and s.formationPreview.active
  kind,s=message(m,0x204);assert kind==0 and s.formationPreview.count==1 and s.formationPreview.slots[0].valid,(kind,s.formationPreview.count,s.formationPreview.slots[0].valid,m.r(m.unit+0x9c),m.r(m.unit+0xa4),m.queryCalls,s.data.status)
  if case=='escape':kind,s=message(m,0x100,27);assert kind==0 and not s.formationPreview.active
  if case=='chat':m.b(0xa8bc60+25,1)
  if case=='focus':s.foreground=0;save(m,s)
  if case=='disabled':s.formationPolicy.enabled=0;save(m,s)
  kind,s=message(m,0x205);assert kind==0 and not s.formationPreview.active
  s=tick(m);expected=case not in ('escape','chat','focus','disabled');assert count(m)==int(expected),(case,count(m),s.formationAccepted,s.formationRejected,s.formationJobs)
  if expected:
   packet=packets(m);assert packet[12]==2 and packet[19:24]==struct.pack('<IB',30020,11)
   if case=='duplicate_up':kind,_=message(m,0x205);assert kind==0x205;tick(m);assert count(m)==1
  assert not m.codeWrites;rows.append({'case':f'{mode}_{case}','status':'PASS'});print('PASS',mode,case,flush=True)
# Configurable double-tap hold and double-tap shape: repeats are not taps.
for mode in (0,3):
 m=setup(mode);s=state(m);s.formationPolicy.hold=0x8000|32;s.formationPolicy.keys[:]=[90|0x8000,90|0x8000,67,86];save(m,s)
 m.held_keys.add(32);kind,s=message(m,0x100,32);assert not s.formationPreview.active
 message(m,0x100,32,True);m.held_keys.clear();message(m,0x101,32);m.clock+=80;m.w(m.SH+m.Shared.heartbeat.offset,m.clock)
 m.held_keys.add(32);kind,s=message(m,0x100,32);assert kind==0 and s.formationPreview.active
 old=s.formationPreview.shape;kind,s=message(m,0x100,90);assert kind==0 and s.formationPreview.shape==old
 message(m,0x100,90,True);message(m,0x101,90);m.clock+=50;m.w(m.SH+m.Shared.heartbeat.offset,m.clock)
 kind,s=message(m,0x100,90);assert kind==0 and s.formationPreview.shape==1;message(m,0x101,90)
 m.held_keys.clear();_,s=message(m,0x101,32);assert not s.formationPreview.active
 assert count(m)==0 and not m.codeWrites
 rows.append({'case':f'{mode}_configured_double_hold_shape','status':'PASS'});print('PASS',mode,'configured double hold and shape',flush=True)
# A 10-pixel drag remains in one queried cell but still has a real facing.
for mode in (0,3):
 m=setup(mode);m.held_keys.add(18);message(m,0x104,18);message(m,0x204);m.clock+=20;_,s=message(m,0x200,x=110,y=100)
 assert abs(s.formationPreview.facing+3.141592653589793/4)<1e-6
 message(m,0x100,27);assert not state(m).formationPreview.active
 rows.append({'case':f'{mode}_subcell_drag_direction','status':'PASS'});print('PASS',mode,'subcell drag direction',flush=True)
for mode in (0,3):
 for manual in (False,True):
  m=setup(mode,16);m.held_keys.add(18);message(m,0x104,18);before=bytes(state(m).formationPreview.slots)
  message(m,0x104,67);assert before!=bytes(state(m).formationPreview.slots), 'stationary shape preview must change';message(m,0x105,67)
  message(m,0x104,90);message(m,0x105,90);message(m,0x204);_,s=message(m,0x205,x=420,y=260)
  assert not s.formationPreview.active and s.formationAccepted==16
  # Input exits immediately, submission drains through the normal gateway.
  s=tick(m);assert count(m)==16
  first=packets(m);destinations=[]
  for j in range(16):
   packet=first[j*111:(j+1)*111];id=struct.unpack_from('<I',packet,7)[0];cell=struct.unpack_from('<I',packet,19)[0];destinations.append(cell);unit=next(p for p in m.units if m.r(p+0x10)==id)
   m.w(unit+0xac,2);m.w(unit+0x5a4,m.r(m.cells+(cell//1000*512+cell%1000)*4));m.w(unit+0x9c,cell%1000*256+128);m.w(unit+0xa0,cell//1000*256+128)
  assert len(set(destinations))==16 and len({d%1000 for d in destinations})==1
  if manual:pointer(m,5)
  s=tick(m);assert count(m)==(31 if manual else 32),(manual,count(m))
  for j in range(16,count(m)):
   packet=packets(m)[j*111:(j+1)*111];id=struct.unpack_from('<I',packet,7)[0];cell=struct.unpack_from('<I',packet,19)[0];unit=next(p for p in m.units if m.r(p+0x10)==id)
   if manual:assert id!=10
   previous=next(destinations[k] for k in range(16) if struct.unpack_from('<I',first,k*111+7)[0]==id);assert cell==previous+1
   m.w(unit+0x9c,cell%1000*256+128);m.w(unit+0xa0,cell//1000*256+128)
  s=tick(m);assert s.formationJobs==0
  rows.append({'case':f'{mode}_drag16_manual_{manual}','status':'PASS'});print('PASS',mode,'16-unit drag and facing approach',manual,flush=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'results':rows},indent=2)+'\n')
