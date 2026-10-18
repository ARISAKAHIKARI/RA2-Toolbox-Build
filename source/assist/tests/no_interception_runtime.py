"""Actual x86 DLL and original event constructors; mocked Win32/world/query.
The Sell receiver branch is executed from the supplied EXE with resolved-object,
map lookup and final virtual sale mocked. This is not real network validation.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',default='no-interception-runtime.json');p.add_argument('--filter',default='');a=p.parse_args();results=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode=5,flags=0):
 spec=importlib.util.spec_from_file_location('f',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);m.queryObject=0;m.queryCalls=0;m.codeWrites=[]
 for section in m.game.sections:
  if section.Name.rstrip(b'\0')==b'.text':
   m.textAt=m.game.OPTIONAL_HEADER.ImageBase+section.VirtualAddress;m.textSize=section.Misc_VirtualSize;m.textBefore=bytes(m.u.mem_read(m.textAt,m.textSize))
 m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,size,value,_:m.codeWrites.append((at,size)),begin=m.textAt,end=m.textAt+m.textSize-1)
 def query(uc,at,size,_):
  sp=m.u.reg_read(UC_X86_REG_ESP);arg=lambda i:m.r(sp+4+i*4)
  assert m.u.reg_read(UC_X86_REG_ECX)==0x87f7e8
  assert struct.unpack('<ii',m.u.mem_read(arg(0),8))==(100,100)
  m.u.mem_write(arg(1),struct.pack('<hh',20,30));m.u.mem_write(arg(2),struct.pack('<iii',20*256+128,30*256+128,0));m.w(arg(3),m.queryObject);m.b(arg(4),0);m.b(arg(5),0);m.queryCalls+=1;m.ret(0,6)
 m.u.hook_add(UC_HOOK_CODE,query,begin=0x692300,end=0x692300)
 m.u.mem_write(m.crateCell+0x24,struct.pack('<hh',20,30));weapon=m.alloc();m.w(m.t+0x898,weapon);m.weapon=weapon
 s=state(m);s.flags=flags;s.externalDisplay=1;save(m,s);m.hook()
 return m

def pointer(m,operation=2,bindings=0,mutate=None):
 s=state(m);b=s.pointer;b.request+=1;b.cancelSerial+=1;i=b.intent;i.tick=m.clock;i.match=s.data.match;i.house=m.house;i.frame=s.data.frame;i.operation=operation;i.x=i.y=100;i.width=800;i.height=600;i.cameraX=i.cameraY=0;i.bindings=bindings;i.inputSerial=b.cancelSerial;i.count=1;i.sources[0]=m.PointerSource(10,m.unit)
 if mutate:mutate(s,i)
 save(m,s);return m.hook()
def tick(m):m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def count(m):return m.r(0x8b41f8 if m.r(0xa8b238) in (0,5) else 0xa802c8)
def packets(m):return bytes(m.u.mem_read(0x8b4204 if m.r(0xa8b238) in (0,5) else 0xa802d4,count(m)*111))
def check(name,fn):
 if a.filter and a.filter not in name:return
 fn();results.append({'name':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,5,3,4):
 def no_patch(mode=mode):
  m=fixture(mode,0x7fffffff & ~65536);s=state(m);s.driverMode=1;s.driverTarget=6;save(m,s)
  for n in range(4):s=tick(m)
  assert not m.codeWrites and bytes(m.u.mem_read(m.textAt,m.textSize))==m.textBefore
  assert s.data.inputPatchMask==0 and s.data.hookInstalled==0 and s.driverMode==0
  assert s.data.placementStatus==0
  s.command=10;s.arg=1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook()
  assert C.c_int32(s.result).value==-20 and not s.data.techOpen and not m.codeWrites
 check(f'normal_flags_no_engine_patch_session_{mode}',no_patch)
 def order(mode=mode):
  m=fixture(mode);before=bytes(m.u.mem_read(m.unit,0x2000));s=pointer(m)
  assert s.pointer.ack==s.pointer.request and s.pointer.accepted==1,(s.pointer.result,s.pointer.accepted,s.data.lastSubmitError)
  assert count(m)==1 and not m.codeWrites
  b=packets(m);assert b[0]==4 and b[12]==29 and b[7:12]==struct.pack('<IB',10,0x34) and b[19:24]==struct.pack('<IB',30020,11)
  assert before==bytes(m.u.mem_read(m.unit,0x2000))
  for _ in range(3):tick(m)
  assert count(m)==1
 check(f'explicit_attack_move_once_session_{mode}',order)
 def follow(mode=mode):
  m=fixture(mode,131072);s=pointer(m,4,4)
  assert s.pointer.ack!=s.pointer.request and count(m)==0
  m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell);s=tick(m)
  assert s.pointer.accepted==1 and s.pointer.ack==s.pointer.request and count(m)==1,(s.pointer.result,s.pointer.accepted,s.data.lastSubmitError)
  assert packets(m)[12]==29
  for _ in range(3):tick(m)
  assert count(m)==1 and not m.codeWrites
 check(f'native_move_then_append_once_session_{mode}',follow)
# 1.0: actual original display timer, no gameplay writes or extra event.
for mode in (0,5,3,4):
 def lines(mode=mode):
  m=fixture(mode);m.b(m.unit+0x83,1);writes=[]
  m.u.hook_add(UC_HOOK_MEM_WRITE,lambda uc,access,at,size,value,_:writes.append((at,size,value)),begin=0xb0ea80,end=0xb0ea8b)
  frame=m.r(0xa8ed84);before=bytes(m.u.mem_read(m.unit,0x2000));s=pointer(m)
  assert s.pointer.accepted==1 and count(m)==1
  assert m.r(0xb0ea80)==frame and m.r(0xb0ea88)==25
  assert {w[0] for w in writes}=={0xb0ea80,0xb0ea84,0xb0ea88}
  assert bytes(m.u.mem_read(m.unit,0x2000))==before and not m.codeWrites
  writes.clear();tick(m);assert not writes # no repeated refresh without a new order
  m.b(m.unit+0x83,0);pointer(m);assert not writes # automatic unselected units do not flash all lines
  tick(m);m.b(m.unit+0x83,1);pointer(m,mutate=lambda s,i:setattr(i,'match',i.match+1));assert not writes
  # Unsupported visual function must fail closed without rejecting the legal order.
  tick(m);m.u.mem_write(0x70d150,b'\x90');s=pointer(m);assert s.pointer.accepted==1 and not writes
 check(f'accepted_selected_order_display_timer_session_{mode}',lines)
 def reveal(mode=mode):
  m=fixture(mode);m.w(0x87f7e8+316,m.alloc());m.w(0x87f7e8+320,512*512)
  def request():
   s=state(m);s.command=9;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);return m.hook()
  m.calls.clear();s=request();assert C.c_int32(s.result).value==-21 and not [x for x in m.calls if x[0]==0x577d90]
  s.flags|=67108864;save(m,s);s=request();calls=[x for x in m.calls if x[0]==0x577d90]
  assert s.ack==s.request and len(calls)==1 and calls[0][1]==0x87f7e8 and calls[0][2][0]==m.house
  assert count(m)==0 and not m.codeWrites
  m.hook();assert len([x for x in m.calls if x[0]==0x577d90])==1
  s.flags&=~67108864;save(m,s);s=request();assert C.c_int32(s.result).value==-21
  assert len([x for x in m.calls if x[0]==0x577d90])==1
  s.flags|=67108864;save(m,s)
  m.w(0x87f7e8+320,0);s=request();assert C.c_int32(s.result).value==-14
  assert len([x for x in m.calls if x[0]==0x577d90])==1
  m.w(0x87f7e8+320,512*512);m.w(0xa8eda0,1);request()
  assert len([x for x in m.calls if x[0]==0x577d90])==1
 check(f'reveal_local_house_once_and_ready_gate_session_{mode}',reveal)
# Configured right clicks retain the original action and append through Runtime.
for mode in (0,5,3,4):
 def right_ground(mode=mode):
  m=fixture(mode,131072);s=pointer(m,1,4|1024)
  assert count(m)==0 and s.pointer.ack!=s.pointer.request
  s=tick(m);assert count(m)==0 # no native Move observed yet
  m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell);s=tick(m)
  assert s.pointer.accepted==1 and count(m)==1 and packets(m)[12]==29
  tick(m);assert count(m)==1
 check(f'configured_right_ground_waits_native_move_{mode}',right_ground)
 def right_sell(mode=mode):
  m=fixture(mode,16777216);m.queryObject=m.unit
  m.u.mem_write(m.unit+0x9c,struct.pack('<iii',20*256+128,30*256+128,0));m.w(m.crateCell+0xe4,m.building)
  s=pointer(m,3,64|1024);assert count(m)==0 and s.pointer.ack!=s.pointer.request
  s=tick(m);assert count(m)==1 and s.pointer.accepted==1
  tick(m);assert count(m)==1
 check(f'configured_right_sell_later_once_{mode}',right_sell)
 for case in ('unselected','deselect','selected','switch_off','normal_exit','manual_stop','deployer'):
  def escape(mode=mode,case=case):
   m=fixture(mode,1024|2048);m.vector(0xa83de8,[m.eng]);m.b(m.unit+0x83,case!='unselected')
   m.w(m.unit+0x114,1);m.w(m.unit+0x118,m.eng);m.w(m.eng+0x11c,m.unit)
   m.b(m.eng+0x81,1);m.b(m.eng+0x90,0);m.w(m.eng+0xac,7);m.w(m.eng+0x5a4,m.unit)
   m.w(m.enemy+0x34,m.alloc());m.w(m.building+0x21c,m.enemy);m.action_overrides[(m.eng,m.building)]=9
   s=tick(m);assert s.data.ejectWatched==int(case!='unselected') and count(m)==0,(case,s.data.ejectWatched)
   if case=='unselected':
    for _ in range(3):tick(m)
    assert count(m)==0 and state(m).data.unloadPending==0;return
   if case=='deselect':m.b(m.unit+0x83,0)
   s=tick(m);assert s.data.ejectWatched==int(case!='deselect') and count(m)==0
   if case=='switch_off':s.flags=0;save(m,s)
   if case!='normal_exit':m.w(m.unit+0x6c,0);m.b(m.unit+0x74,0)
   m.w(m.eng+0x11c,0);m.b(m.eng+0x81,0);m.b(m.eng+0x90,1);m.w(m.eng+0xac,13 if case=='manual_stop' else 5)
   if case=='deployer':
    m.b(m.engT+0xec8,1)
    for offset,value in ((0x314,1),(0x554,0)):
     address=m.r(0x7eb058+offset)
     m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_,value=value:m.ret(value,0),begin=address,end=address)
   s=tick(m)
   expected=case in ('selected','deployer')
   assert count(m)==int(expected),(case,count(m),s.data.unloadPending,s.data.lastSubmitError)
   if expected:assert packets(m)[12]==(16 if case=='deployer' else 8)
   for _ in range(4):tick(m)
   assert count(m)==int(expected) and not m.codeWrites
  check(f'ejection_{case}_{mode}',escape)
# Five survivors share an exit cell; four need native scatter before Deploy.
for mode in (0,5,3,4):
 for scenario in ('five_scatter','same_cell_pending','same_cell_mixed','moving_ready','blocked','manual_cancel','scatter_timeout'):
  def dispersal(mode=mode,scenario=scenario):
   m=fixture(mode,1024|2048);m.b(m.engT+0xec8,1)
   people=[m.eng]+[m.obj(0x7eb058,m.engT,m.house,20+n) for n in range(4)]
   m.vector(0xa83de8,people);m.vector(0xa8ec78,[m.unit,m.building,m.enemyUnit]+people)
   m.b(m.unit+0x83,1);m.w(m.unit+0x114,5)
   for p in people:
    m.w(p+0x9c,25600);m.w(p+0xa0,25600)
    m.w(p+0x11c,m.unit);m.b(p+0x81,1);m.b(p+0x90,0);m.w(p+0xac,7);m.w(p+0x5a4,m.unit)
   ready=set();deployed=set()
   for offset,values in ((0x314,ready),(0x554,deployed)):
    address=m.r(0x7eb058+offset)
    m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_,values=values:m.ret(int(m.u.reg_read(UC_X86_REG_ECX) in values),0),begin=address,end=address)
   s=tick(m);assert s.data.ejectWatched==5 and count(m)==0
   m.w(m.unit+0x6c,0);m.b(m.unit+0x74,0)
   for p in people:m.w(p+0x11c,0);m.b(p+0x81,0);m.b(p+0x90,1);m.w(p+0xac,2)
   seen=0;ids=[]
   for age in range(1,191 if scenario=='scatter_timeout' else 40):
    for n,p in enumerate(people):
     moving=age<1+n*6 or scenario=='scatter_timeout'
     if scenario in ('same_cell_pending','same_cell_mixed'):moving=False
     if scenario=='moving_ready':moving=True
     m.w(p+0xac,2 if moving else 5)
     if moving:m.w(p+0x9c,m.r(p+0x9c)+16)
     elif scenario!='blocked':
      ready.add(p)
      if scenario not in ('same_cell_pending','same_cell_mixed'):m.w(p+0x9c,25600+n*256)
     if scenario=='same_cell_mixed' and age<1+n*6:ready.discard(p)
     if scenario=='moving_ready':ready.add(p)
    if scenario=='manual_cancel' and age==1:
     pointer(m,5,mutate=lambda s,i:(setattr(i,'count',5),[setattr(i.sources[n],'pointer',p) or setattr(i.sources[n],'id',m.r(p+0x10)) for n,p in enumerate(people)]))
    s=tick(m);data=packets(m)
    for n in range(seen,count(m)):
     packet=data[n*111:(n+1)*111];ident=struct.unpack_from('<I',packet,7)[0]
     assert packet[12]==16,(scenario,age,packet[12]) # never Sleep/Move to block native scatter
     p=next(p for p in people if m.r(p+0x10)==ident)
     assert p in ready and ident not in ids and (m.r(p+0xac)==5 or scenario=='moving_ready')
     ids.append(ident)
     if scenario not in ('same_cell_pending','same_cell_mixed','moving_ready'):deployed.add(p);m.w(p+0x6c4,28)
    if scenario in ('same_cell_pending','moving_ready'):assert count(m)==5,(scenario,age,count(m))
    if scenario=='same_cell_mixed':assert count(m)==sum(age>=1+n*6 for n in range(5)),(scenario,age,count(m))
    seen=count(m)
   assert len(ids)==(5 if scenario in ('five_scatter','same_cell_pending','same_cell_mixed','moving_ready') else 0),(scenario,ids)
   if scenario in ('manual_cancel','scatter_timeout'):assert s.data.unloadPending==0
   assert not m.codeWrites
  check(f'ejection_{scenario}_{mode}',dispersal)
# Regression: IsDeployed returns true during sequence 27, then native scatter
# cancels it. Two survivors settle immediately; three must independently retry.
for mode in (0,5,3,4):
 for scenario in ('interrupted','queued_toggle','manual','off','full_queue'):
  def interrupted(mode=mode,scenario=scenario):
   m=fixture(mode,1024|2048);m.b(m.engT+0xec8,1)
   people=[m.eng]+[m.obj(0x7eb058,m.engT,m.house,20+n) for n in range(4)]
   m.vector(0xa83de8,people);m.vector(0xa8ec78,[m.unit,m.building,m.enemyUnit]+people)
   m.b(m.unit+0x83,1);m.w(m.unit+0x114,5)
   for p in people:m.w(p+0x11c,m.unit);m.b(p+0x81,1);m.b(p+0x90,0);m.w(p+0xac,7);m.w(p+0x5a4,m.unit)
   ready=m.r(0x7eb058+0x314);deployed=m.r(0x7eb058+0x554)
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,0),begin=ready,end=ready)
   # Match the actual game's inclusive 27..30 behavior, NOT a completed flag.
   m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(int(27<=m.r(m.u.reg_read(UC_X86_REG_ECX)+0x6c4)<=30),0),begin=deployed,end=deployed)
   tick(m);m.w(m.unit+0x6c,0);m.b(m.unit+0x74,0)
   for p in people:m.w(p+0x11c,0);m.b(p+0x81,0);m.b(p+0x90,1);m.w(p+0xac,5)
   s=tick(m);assert count(m)==5 and s.data.unloadPending==5
   for n,p in enumerate(people):m.w(p+0x6c4,28 if n<2 else 27);m.w(p+0xac,16)
   if scenario=='full_queue':m.w(0x8b41f8 if mode in (0,5) else 0xa802c8,16384 if mode in (0,5) else 112)
   for _ in range(4):s=tick(m)
   if scenario=='full_queue':m.w(0x8b41f8 if mode in (0,5) else 0xa802c8,5)
   assert count(m)==5 and s.data.unloadPending==3,(scenario,s.data.unloadPending)
   for p in people[2:]:m.w(p+0x6c4,3);m.w(p+0xac,2)
   if scenario=='manual':pointer(m,5,mutate=lambda s,i:(setattr(i,'count',3),[setattr(i.sources[n],'pointer',p) or setattr(i.sources[n],'id',m.r(p+0x10)) for n,p in enumerate(people[2:])]))
   if scenario=='off':s=state(m);s.flags=0;save(m,s)
   seen=count(m);retries=[]
   for age in range(36):
    for n,p in enumerate(people[2:]):
     if age<8+n*5:m.w(p+0x9c,m.r(p+0x9c)+16)
     else:m.w(p+0xac,5);m.w(p+0x6c4,0)
     if scenario=='queued_toggle':m.w(p+0xb4,16 if age<28 else 0xffffffff)
    for p in people:
     if m.r(p+0x10) in retries:m.w(p+0x6c4,28);m.w(p+0xac,16)
    s=tick(m)
    for at in range(seen,count(m)):
     event=packets(m)[at*111:(at+1)*111];ident=struct.unpack_from('<I',event,7)[0]
     assert event[12]==16 and ident not in retries and ident in [m.r(p+0x10) for p in people[2:]],(scenario,event[12],ident)
     retries.append(ident);p=next(p for p in people if m.r(p+0x10)==ident)
     m.w(p+0x6c4,28);m.w(p+0xac,16)
    seen=count(m)
    # Keep successful retry outcomes alive on subsequent samples.
    for p in people:
     if m.r(p+0x10) in retries:m.w(p+0x6c4,28);m.w(p+0xac,16)
    if scenario=='queued_toggle' and age<28:assert count(m)==5
   assert len(retries)==(3 if scenario in ('interrupted','queued_toggle','full_queue') else 0),(scenario,retries)
   assert s.data.unloadPending==0 and not m.codeWrites,(scenario,s.data.unloadPending)
  check(f'ejection_animation_{scenario}_{mode}',interrupted)
# Independent physical bindings must not authorize other features at Runtime.
for mode in (0,5,3,4):
 for scenario in ('enter_ground','disk_ground_auto_cs','vehicle_allow','vehicle_wrong_button','building_allow','building_wrong_button'):
  def scoped(mode=mode,scenario=scenario):
   m=fixture(mode,512|1048576|4194304|16777216|8388608)
   if scenario in ('enter_ground','disk_ground_auto_cs'):
    s=state(m);s.csSeq=2;s.csMatch=s.data.match;s.csAllowed[1]=1;save(m,s)
    s=pointer(m,1,8192|(8 if scenario=='enter_ground' else 64))
    assert count(m)==0 and s.pointer.accepted==0
   else:
    building=scenario.startswith('building');m.queryObject=m.building if building else m.unit
    m.w(m.building+0x1d0,m.enemyUnit);m.w(m.unit+0x9c,20*256+128);m.w(m.unit+0xa0,30*256+128);m.w(m.crateCell+0xe4,m.building)
    expected=scenario.endswith('_allow');bit=(2048 if building else 4096) if expected else (4096 if building else 2048)
    s=pointer(m,3,8192|64|bit)
    assert count(m)==int(expected) and s.pointer.accepted==int(expected),(scenario,count(m),s.pointer.result)
   assert s.pointer.ack==s.pointer.request and not m.codeWrites
  check(f'per_feature_mouse_{scenario}_{mode}',scoped)
for what in ('chat','menu','dialog','inactive_tactical','foreign_actor','pointer_reused','stale_ms','stale_frame','wrong_match','camera_changed','sidebar','shift','cancelled_serial','oversized_selection'):
 def reject(what=what):
  m=fixture()
  def mutate(s,i):
   if what=='chat':m.b(0xa8bc60+25,1)
   elif what=='menu':m.w(0xa8eda0,1)
   elif what=='dialog':m.w(0xa8ed8c,1)
   elif what=='inactive_tactical':m.b(0xa8e378,0)
   elif what=='foreign_actor':m.w(m.unit+0x21c,m.enemy)
   elif what=='pointer_reused':i.sources[0].pointer=m.eng
   elif what=='stale_ms':i.tick-=1000
   elif what=='stale_frame':i.frame-=20
   elif what=='wrong_match':i.match+=1
   elif what=='camera_changed':i.cameraX=100
   elif what=='sidebar':i.x=700
   elif what=='shift':i.bindings=128
   elif what=='cancelled_serial':i.inputSerial-=1
   elif what=='oversized_selection':i.count=513
  s=pointer(m,mutate=mutate);assert s.pointer.ack==s.pointer.request and s.pointer.accepted==0 and count(m)==0,(what,s.pointer.result,s.pointer.accepted)
 check('reject_'+what,reject)
for what in ('new_input','expired','feature_disabled','wrong_destination','target_attack'):
 def pending(what=what):
  m=fixture(3,131072);s=pointer(m,4,4);assert count(m)==0
  if what=='new_input':s.pointer.cancelSerial+=1;save(m,s)
  elif what=='expired':m.clock+=1600
  elif what=='feature_disabled':s.flags=0;save(m,s)
  elif what=='wrong_destination':m.w(m.unit+0x5a4,m.building)
  elif what=='target_attack':m.w(m.unit+0x2b4,m.enemyUnit)
  m.w(m.unit+0xac,2)
  if what not in ('wrong_destination',):m.w(m.unit+0x5a4,m.crateCell)
  s=tick(m);assert count(m)==0
  if s.pointer.ack!=s.pointer.request:m.clock+=1600;s=tick(m)
  assert s.pointer.ack==s.pointer.request and s.pointer.accepted==0
 check('pending_follow_'+what,pending)
def pressure():
 m=fixture(3);m.w(0xa802c8,112);s=pointer(m);assert s.pointer.ack!=s.pointer.request and count(m)==112
 m.w(0xa802c8,0);s=tick(m);assert s.pointer.accepted==1 and count(m)==1
 for _ in range(3):tick(m)
 assert count(m)==1
check('queue_full_retry_without_replay',pressure)
def denied_patch():
 m=fixture();req=m.request(0);result,req=m.manager_call(req);assert result==1
 req.operation=2;req.address=0x646e90;req.length=5
 for j,v in enumerate(m.u.mem_read(req.address,5)):req.before[j]=v
 req.after[:]=[0xe9]+[0]*15
 result,_=m.manager_call(req);assert result<0 and not m.codeWrites
 req.operation=1;m.manager_call(req)
check('manager_rejects_engine_patch_even_with_lease',denied_patch)
for mode in (0,3):
 for name in ('own_vehicle_with_building','own_vehicle_empty_cell','enemy_vehicle','drained_building'):
  def sell(mode=mode,name=name):
   m=fixture(mode,16777216|8388608);m.queryObject=m.building if name=='drained_building' else m.unit
   if name=='drained_building':m.w(m.building+0x1d0,m.enemyUnit)
   elif name=='enemy_vehicle':m.w(m.unit+0x21c,m.enemy)
   m.w(m.unit+0x9c,20*256+128);m.w(m.unit+0xa0,30*256+128)
   if name!='own_vehicle_empty_cell':m.w(m.crateCell+0xe4,m.building)
   s=pointer(m,3,64);expected=name in ('own_vehicle_with_building','drained_building')
   assert bool(s.pointer.accepted)==expected and count(m)==int(expected),(name,s.pointer.result,s.pointer.accepted)
   if expected:assert packets(m)[0]==0x16 and packets(m)[2]==0 and packets(m)[7:12]==struct.pack('<IB',12 if name=='drained_building' else 10,0x34)
   assert not m.codeWrites
  check(f'sell_event_{mode}_{name}',sell)
# Run the EXE's real Sell receiver branch through ownership/building checks.
for own in (True,False):
 for building in (True,False):
  def receiver(own=own,building=building):
   m=fixture();packet=m.alloc();m.b(packet,0x16);m.b(packet+2,0 if own else 1);m.w(packet+7,10);m.b(packet+11,0x34)
   vt=m.alloc(0x400);m.w(m.unit,vt);gettype=m.STUB+0xe100;coords=m.STUB+0xe110;sell=m.STUB+0xe120;m.w(vt+0x2c,gettype);m.w(vt+0x48,coords);m.w(vt+0x1a0,sell);m.sales=0
   def stub(uc,at,size,_):
    sp=m.u.reg_read(UC_X86_REG_ESP)
    if at==0x6e6f20:m.ret(m.unit,0)
    elif at==0x6386e0:m.ret(0,0)
    elif at==gettype:m.ret(2,0)
    elif at==coords:m.ret(m.r(sp+4),1)
    elif at==0x565730:m.ret(m.crateCell,1)
    elif at==0x47c520:m.ret(m.building if building else 0,0)
    elif at==sell:m.sales+=1;m.ret(1,1)
   for at in (0x6e6f20,0x6386e0,gettype,coords,0x565730,0x47c520,sell):m.u.hook_add(UC_HOOK_CODE,stub,begin=at,end=at)
   m.run(0x4c6cb0,(),packet);assert m.sales==int(own and building),(own,building,m.sales)
  check(f'original_sell_receiver_owner_{own}_building_{building}',receiver)
for allowed in (False,True):
 def auto_cs(allowed=allowed):
  m=fixture(3,512);s=state(m);s.csSeq=2;s.csMatch=s.data.match;s.csAllowed[1]=int(allowed);save(m,s)
  m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell);s=pointer(m,4)
  assert s.pointer.accepted==int(allowed) and count(m)==int(allowed)
 check('auto_cs_type_whitelist_'+str(allowed),auto_cs)
for mag in (False,True):
 def advanced(mag=mag):
  m=fixture(3,1024|(262144 if mag else 8192));wh=m.alloc();m.w(m.weapon+0xac,wh);m.b(m.weapon+0x15c,mag);m.b(wh+0x15b,mag)
  m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell);s=pointer(m,4,2 if mag else 1)
  assert s.pointer.accepted==1 and s.world.intentCount==1,(s.pointer.result,s.pointer.accepted,s.world.intentCount)
  assert count(m)==(0 if mag else 1) and s.data.advancedUnits==1
  assert not m.codeWrites
 check('native_move_follow_'+('magnetron' if mag else 'advanced'),advanced)
def enter():
 m=fixture(3,1048576);m.queryObject=m.unit;m.u.mem_write(m.t+0x388,struct.pack('<d',4.0));m.u.mem_write(m.engT+0x380,struct.pack('<d',1.0));m.b(m.eng+0x83,1)
 def choose(s,i):i.sources[0]=m.PointerSource(11,m.eng)
 s=pointer(m,1,8,choose);assert s.pointer.accepted==1 and s.cooperativeRequest.count==1 and s.cooperativeRequest.target==10
 assert s.cooperativeRequest.sources[0].id==11 and count(m)==0 and not m.codeWrites
check('explicit_middle_enter_captures_existing_external_plan',enter)
def disk():
 m=fixture(3,4194304);m.queryObject=m.building;m.u.mem_write(m.t+0x24,b'DISK\0');m.w(m.building+0x9c,20*256+128);m.w(m.building+0xa0,30*256+128)
 s=pointer(m,1,64);assert s.pointer.accepted==1 and count(m)==1
 b=packets(m);assert b[12]==1 and b[14:19]==struct.pack('<IB',30020,11) and not m.codeWrites
check('disk_primary_uses_original_force_ground_event',disk)
def batch():
 m=fixture(3);units=[m.unit]+[m.obj(0x7f5c70,m.t,m.house,20+n,selected=True) for n in range(39)]
 m.vector(0xa8ec78,units+[m.building]);m.vector(0xa8ecb8,units)
 def choose(s,i):
  i.count=len(units)
  for n,u in enumerate(units):i.sources[n]=m.PointerSource(m.r(u+0x10),u)
 s=pointer(m,2,0,choose);assert s.pointer.accepted==24 and s.pointer.ack!=s.pointer.request
 s=tick(m);assert s.pointer.accepted==40 and s.pointer.ack==s.pointer.request and count(m)==40
 ids=[struct.unpack_from('<I',packets(m),j*111+7)[0] for j in range(40)];assert len(set(ids))==40
check('multi_frame_batch_accepted_prefix_never_replayed',batch)
def reconnect():
 m=fixture(3,131072);s=pointer(m,4,4);assert s.pointer.request==1 and s.pointer.ack==0
 s.stop=1;save(m,s);m.hook()
 s=state(m);s.stop=0;s.heartbeat=m.clock;s.pointer=m.PointerMailbox();save(m,s);m.hook()
 m.w(0xa802c8,0);s=pointer(m,2);assert s.pointer.request==1 and s.pointer.ack==1 and s.pointer.accepted==1 and count(m)==1
 assert packets(m)[12]==29 and not m.codeWrites
check('reconnect_reused_request_serial_clears_pending_follow',reconnect)
Path(a.output).write_text(json.dumps({'inputs':{'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()},'filter':a.filter,'scope':'actual DLL/original constructors; synthetic world and Win32, no real network','results':results},indent=2)+'\n')
print('TOTAL',len(results))
