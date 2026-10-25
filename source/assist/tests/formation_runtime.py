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
 m.units=[m.unit]+[m.obj(0x7f5c70,m.t,m.house,20+j) for j in range(count-1)];m.vector(0xa8ec78,m.units);m.vector(0xa8ecb8,m.units);s=state(m);s.formationPolicy.enabled=1;s.formationPolicy.mouse=2;s.formationPolicy.hold=18;s.formationPolicy.keys[:]=[90,90,67,90,90,90];save(m,s);tick(m);m.codeWrites.clear();return m

def message(m,kind,key=0,repeat=False,x=100,y=100):
 lp=(1<<30) if repeat else 0
 if kind in (0x201,0x202,0x204,0x205,0x200):lp=x|(y<<16)
 m.queryPoint=(x,y);m.queryCell=(20+(x-100)//30,30+(y-100)//30)
 m.u.mem_write(m.msg,struct.pack('<7I',1,kind,key,lp,m.clock,0,0));m.run(m.entry,(0,1,m.msg));return m.r(m.msg+4),state(m)

def geometry(preview):
 return frozenset((slot.x,slot.y,slot.z) for slot in preview.slots[:preview.count] if slot.valid)

# Shape changes solve immediately while the pointer and clock stand still.
# Distinct keys and a shared-key cycle must change positions, not only labels.
for mode in (0,3):
 for shared_key in (False,True):
  m=setup(mode,25);s=state(m);keys=[90]*6 if shared_key else [90,88,67,86,66,78];s.formationPolicy.keys[:]=keys;save(m,s)
  m.held_keys.add(18);message(m,0x104,18);initial=state(m).formationPreview;shapes={0:geometry(initial)};clock=m.clock
  assert len(shapes[0])==25
  for shape in (1,2,3,4,5,0):
   previous=state(m).formationPreview;kind,s=message(m,0x104,keys[shape]);preview=s.formationPreview;points=geometry(preview)
   assert kind==0 and preview.active and not preview.dragging and preview.shape==shape
   assert m.clock==clock and preview.tick==initial.tick and preview.revision>previous.revision
   assert (preview.ax,preview.ay,preview.facing)==(initial.ax,initial.ay,initial.facing)
   assert len(points)==25 and points!=geometry(previous), ('stationary-shape-geometry',mode,shared_key,shape)
   if shape==0:assert points==shapes[0], 'cycling back restores the initial geometry'
   else:
    assert points not in shapes.values(), ('distinct-shape-geometry',mode,shared_key,shape)
    shapes[shape]=points
   before=bytes(preview.slots);kind,s=message(m,0x104,keys[shape],True)
   assert kind==0 and s.formationPreview.shape==shape and bytes(s.formationPreview.slots)==before
   assert message(m,0x105,keys[shape])[0]==0
  assert len(shapes)==6 and count(m)==0 and not m.codeWrites
  message(m,0x100,27)
  rows.append({'case':f'{mode}_{shared_key}_stationary_six_shape_geometry_cycle','status':'PASS'});print('PASS',mode,shared_key,'stationary six-shape geometry and repeat suppression',flush=True)

for mode in (0,3):
 for case in ('click','cycle','escape','chat','focus','disabled','duplicate_up','double_alt','custom_double_alt','policy_in_flight','advanced_frame'):
  m=setup(mode);m.held_keys.add(18);kind,s=message(m,0x104,18);assert kind==0 and s.formationPreview.active,(case,kind,s.data.status,s.formationPreview.active)
  if case=='double_alt':
   m.held_keys.clear();message(m,0x105,18);m.clock+=100;m.w(m.SH+m.Shared.heartbeat.offset,m.clock);m.held_keys.add(18);message(m,0x104,18);m.held_keys.clear();_,s=message(m,0x105,18);assert s.formationPreview.active
  if case=='cycle':
   for expected in (1,3,4,5,0):
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
 m=setup(mode);s=state(m);s.formationPolicy.hold=0x8000|32;s.formationPolicy.keys[:]=[90|0x8000,90|0x8000,67,86,0,0];save(m,s)
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
  message(m,0x104,67);assert before!=bytes(state(m).formationPreview.slots), 'shape selection updates the zero-drag preview';message(m,0x105,67)
  message(m,0x104,90);message(m,0x105,90);message(m,0x204);m.clock+=20;message(m,0x200,x=420,y=260);_,s=message(m,0x205,x=420,y=260)
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
# Configurable left/right drag, one native voice, and persistent per-member offsets.
for mode in (0,3):
 for button in (1,2):
  for scenario in ('move','reordered_selection','deselect','replace_member','box_drag','native_pending','scattered_destinations','rapid_retarget','right_click_same_selection','right_pan','right_deselect','object_click'):
   m=setup(mode,4);s=state(m);s.formationPolicy.mouse=button;save(m,s)
   voices=[];m.vector(m.t+0x468,[71]);m.w(m.unit+0x4f0,0xffffffff);m.b(0x822cf2,1)
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,0),begin=0x50b6f0,end=0x50b6f0)
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(0,0),begin=0x65c780,end=0x65c780)
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:voices.append(at),begin=0x708d90,end=0x708d90)
   m.held_keys.add(18);message(m,0x104,18);down,up=(0x201,0x202) if button==1 else (0x204,0x205)
   assert message(m,down)[0]==0;assert message(m,up)[0]==0
   s=tick(m);assert count(m)==4 and len(voices)==1
   initial={struct.unpack_from('<I',packets(m),j*111+7)[0]:struct.unpack_from('<I',packets(m),j*111+19)[0] for j in range(4)}
   m.held_keys.clear();message(m,0x105,18)
   if scenario=='reordered_selection':m.vector(0xa8ecb8,m.units[::-1])
   if scenario=='deselect':m.vector(0xa8ecb8,[])
   if scenario=='replace_member':m.vector(0xa8ecb8,m.units[:-1]+[m.enemyUnit])
   if scenario.startswith('right_'):
    assert message(m,0x204)[0]==0x204
    if scenario=='right_pan':assert message(m,0x200,x=220,y=190)[0]==0x200
    assert message(m,0x205,x=220 if scenario=='right_pan' else 100,y=190 if scenario=='right_pan' else 100)[0]==0x205
    # Only an actual native deselection breaks membership, not the button.
    if scenario=='right_deselect':m.vector(0xa8ecb8,[])
   # Original click/voice stays native; continuation does not wait for delivery.
   assert message(m,0x201,x=190,y=160)[0]==0x201
   if scenario=='box_drag':message(m,0x200,x=220,y=190)
   if scenario=='object_click':m.queryObject=m.enemyUnit
   assert message(m,0x202,x=190,y=160)[0]==0x202
   if scenario=='rapid_retarget':
    message(m,0x201,x=220,y=190);message(m,0x202,x=220,y=190)
   assert count(m)==4,'continuation waits until native mouse-up has run'
   cell=32023;mapCell=m.r(m.cells+(32*512+23)*4)
   if scenario!='native_pending':
    for j,p in enumerate(m.units):
     m.w(p+0xac,2);m.w(p+0x5a4,m.r(m.cells+(32*512+23+j)*4) if scenario=='scattered_destinations' else mapCell)
   s=tick(m);expected=scenario in ('move','reordered_selection','native_pending','scattered_destinations','rapid_retarget','right_click_same_selection','right_pan');assert count(m)==(8 if expected else 4),(mode,button,scenario,count(m))
   if expected:
    assert s.formationAccepted==4 and s.formationRejected==0
    final={struct.unpack_from('<I',packets(m),j*111+7)[0]:struct.unpack_from('<I',packets(m),j*111+19)[0] for j in range(4,8)}
    assert all(final[id]-initial[id]==(3004 if scenario=='rapid_retarget' else 2003) for id in initial)
   assert len(voices)==1,'automatic continuation must not replay voice'
   assert not m.codeWrites;rows.append({'case':f'{mode}_{button}_{scenario}_persistent_formation_voice','status':'PASS'})
# A directed first layout still aligns arrival; later clicks go straight to
# translated slots. Deliver every continuation and verify no extra approach.
for mode in (0,3):
 m=setup(mode,4);m.held_keys.add(18);message(m,0x104,18);message(m,0x204);m.clock+=20;message(m,0x200,x=190,y=145);message(m,0x205,x=190,y=145);tick(m)
 assert count(m)==4
 m.held_keys.clear();message(m,0x105,18)
 for step in range(2):
  old=count(m);message(m,0x201,x=160+step*30,y=160);message(m,0x202,x=160+step*30,y=160);tick(m)
  assert count(m)==old+4
  for n in range(old,old+4):
   packet=packets(m)[n*111:(n+1)*111];uid,cell=struct.unpack_from('<I',packet,7)[0],struct.unpack_from('<I',packet,19)[0]
   unit=next(p for p in m.units if m.r(p+0x10)==uid);m.w(unit+0xac,2);m.w(unit+0x5a4,m.r(m.cells+(cell//1000*512+cell%1000)*4));m.w(unit+0x9c,cell%1000*256+128);m.w(unit+0xa0,cell//1000*256+128)
  s=tick(m);assert count(m)==old+4 and s.formationJobs==0,('continuation-facing-detour',mode,step,count(m),s.formationJobs)
 rows.append({'case':f'{mode}_directed_layout_then_two_direct_continuations','status':'PASS'})
# Click commits the newly selected shape verbatim, including per-member assignment.
for mode in (0,3):
 for button in (1,2):
  for shape in range(6):
   m=setup(mode,25);s=state(m);s.formationPolicy.mouse=button;keys=[90,88,67,86,66,78];s.formationPolicy.keys[:]=keys;save(m,s)
   m.held_keys.add(18);message(m,0x104,18)
   message(m,0x104,keys[(shape+5)%6]);message(m,0x105,keys[(shape+5)%6]);previous=state(m).formationPreview
   message(m,0x104,keys[shape]);message(m,0x105,keys[shape])
   preview=state(m).formationPreview;before=bytes(preview.slots)
   assert preview.shape==shape and not preview.dragging and geometry(preview)!=geometry(previous)
   assert preview.tick==previous.tick and len(geometry(preview))==25
   destinations={preview.units[n].id:preview.slots[n].y//256*1000+preview.slots[n].x//256 for n in range(preview.count)}
   # Units move after sampling: a click must not run assignment again.
   for n,unit in enumerate(m.units):m.w(unit+0x9c,(25-n)*256+128)
   down,up=(0x201,0x202) if button==1 else (0x204,0x205)
   _,s=message(m,down);assert bytes(s.formationPreview.slots)==before and not s.formationPreview.dragging
   m.clock+=20;message(m,0x200,x=103,y=102);_,s=message(m,up,x=101,y=100)
   assert bytes(s.formationPreview.slots)==before and not s.formationPreview.dragging
   tick(m);assert count(m)==25
   actual={struct.unpack_from('<I',packets(m),n*111+7)[0]:struct.unpack_from('<I',packets(m),n*111+19)[0] for n in range(25)}
   assert actual==destinations,(mode,button,shape,actual,destinations)
   rows.append({'case':f'{mode}_{button}_{shape}_zero_drag_preview_exact_commit','status':'PASS'})
  m=setup(mode,9);s=state(m);s.formationPolicy.mouse=button;save(m,s);m.held_keys.add(18);message(m,0x104,18)
  down,up=(0x201,0x202) if button==1 else (0x204,0x205);message(m,down);m.clock+=20;_,s=message(m,0x200,x=150,y=130);assert s.formationPreview.dragging
  before=bytes(s.formationPreview.slots);message(m,up,x=155,y=133);assert bytes(state(m).formationPreview.slots)==before
  rows.append({'case':f'{mode}_{button}_release_does_not_resolve_layout','status':'PASS'})
  m=setup(mode,4);s=state(m);s.formationPolicy.mouse=button;save(m,s);m.held_keys.add(18);message(m,0x104,18);message(m,down)
  m.vector(0xa8ecb8,m.units[:-1]);message(m,up);tick(m);assert count(m)==0
  rows.append({'case':f'{mode}_{button}_changed_members_cancel_frozen_preview','status':'PASS'})
Path(a.output).write_text(json.dumps({'scope':__doc__,'results':rows},indent=2)+'\n')
