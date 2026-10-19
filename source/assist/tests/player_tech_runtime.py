"""Compiled Runtime tests with synthetic worlds; native constructors/branch bytes execute.
Native CanBuild/FindFactory are explicit fixture stubs, not proof of live purchase.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def fixture(mode=5):
 spec=importlib.util.spec_from_file_location('f',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 for h,index,label in [(m.house,0,b'Americans'),(m.enemy,1,b'Russians')]:
  ht=m.alloc();m.w(h+0x34,ht);m.w(h+0x10,index+101);m.u.mem_write(ht+0x24,label+b'\0');m.b(h+0x1EC,h==m.house);m.b(h+0x1ED,h==m.house)
 m.w(0xa8b238,mode);s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=0;s.externalDisplay=1;m.u.mem_write(m.SH,bytes(s));return m,m.hook()
def write(m,s):m.u.mem_write(m.SH,bytes(s))
def issue(m,c,arg=0,arg2=0):
 s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.command=c;s.arg=arg;s.arg2=arg2;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;write(m,s);return m.hook()
def record(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
def addhook(m,at,f):return m.u.hook_add(UC_HOOK_CODE,lambda u,addr,size,_:f(),begin=at,end=at)
# Paired UI gates, scope, rollback, actual Produce constructor.
for mode in (0,3,4,5):
 m,s=fixture(mode);s.flags=4096;s.techAllowed[1]=1;write(m,s);s=issue(m,10,1)
 assert s.result==1 and s.data.techOpen==1 and s.data.hookInstalled==2,(mode,s.result,s.data.techError,s.data.hookInstalled)
 assert m.sidebar_add_calls==[(40,0)]
 before=bytes(m.u.mem_read(m.t,0x2000));original=bytes(m.game.get_data(0x4f7870-0x400000,8))
 assert bytes(m.u.mem_read(0x4f7870,8))==original
 entry=0x6AA781+5+struct.unpack('<i',m.u.mem_read(0x6AA782,4))[0]
 m.canbuild_full_result=0;m.canbuild_limit_result=1
 m.run(entry,(m.t,0,1),m.house);assert m.canbuild_calls[-1]==(m.house,m.t,1,1)
 m.clock+=4000;m.w(m.SH+m.Shared.heartbeat.offset,1);m.run(entry,(m.t,0,1),m.house);assert m.canbuild_calls[-1]==(m.house,m.t,1,1)
 m.w(m.SH+m.Shared.heartbeat.offset,m.clock)
 m.run(entry,(m.t,0,1),m.enemy);assert m.canbuild_calls[-1]==(m.enemy,m.t,0,1)
 m.run(entry,(m.engT,0,1),m.house);assert m.canbuild_calls[-1]==(m.house,m.engT,0,1)
 s=issue(m,12,1);assert s.result==1 and s.count==1,(s.result,s.data.lastSubmitError)
 at=0x8b4204 if mode in (0,5) else 0xa802d4;packet=bytes(m.u.mem_read(at,111));assert packet[0]==0x0e and packet[7:19]==struct.pack('<iii',40,0,0)
 assert bytes(m.u.mem_read(m.t,0x2000))==before
 s=issue(m,10,0);assert s.result==1 and s.data.hookInstalled==0
 for site in (0x6A97D2,0x6AA781):assert bytes(m.u.mem_read(site,5))==m.game.get_data(site-0x400000,5)
 record(f'tech_{mode}_paired_gates_payload_scope_restore')
# Actual original sidebar prune branch: old behavior cancels, paired gate retains.
for enabled in (False,True):
 m,s=fixture();s.flags=4096;s.techAllowed[1]=1;write(m,s)
 if enabled:s=issue(m,10,1)
 m.canbuild_full_result=0;m.canbuild_limit_result=1
 reached=[]
 for at in (0x6AA7B6,0x6AAA68):addhook(m,at,lambda at=at:(reached.append(at),m.u.emu_stop()))
 sp=m.STACK+0x1e000;m.u.mem_write(sp,b'\0'*256);m.u.reg_write(UC_X86_REG_ESP,sp);m.u.reg_write(UC_X86_REG_EBX,m.t);m.u.reg_write(UC_X86_REG_EDI,0)
 m.u.emu_start(0x6AA75F,m.STOP,count=100000)
 assert reached==[0x6AAA68 if enabled else 0x6AA7B6],reached
 record('native_prune_'+('retains' if enabled else 'cancel_branch'))
# Changed/foreign patch must not be overwritten; paired rollback.
m,s=fixture();m.u.mem_write(0x6AA781,b'\x90'*5);s.flags=4096;s.techAllowed[1]=1;write(m,s);s=issue(m,10,1)
assert C.c_int32(s.result).value==-11 and s.data.techOpen==0
assert bytes(m.u.mem_read(0x6AA781,5))==b'\x90'*5
assert bytes(m.u.mem_read(0x6A97D2,5))==m.game.get_data(0x6A97D2-0x400000,5)
record('tech_conflict_group_rollback')
# Switching actual context, old epoch rejection, restoration and neutral rejection.
for case in ('switch_restore','toggle_restore','stop_restore','paused_restore','neutral','passive','spectator','reused_identity','new_round'):
 m,s=fixture();s.switchPlayerEnabled=1;write(m,s)
 if case=='neutral':m.u.mem_write(m.r(m.enemy+0x34)+0x24,b'Special\0')
 if case=='passive':m.b(m.r(m.enemy+0x34)+0x1a6,1)
 if case=='spectator':m.w(0xac1198,m.enemy)
 old=s.data.match;s=issue(m,23,m.enemy,999 if case=='reused_identity' else 102)
 if case in ('neutral','passive','spectator','reused_identity'):
  assert C.c_int32(s.result).value<0 and m.r(0xa83d4c)==m.house
 else:
  assert s.result==1 and m.r(0xa83d4c)==m.enemy
  assert m.u.mem_read(m.house+0x1ec,1)==b'\0' and m.u.mem_read(m.enemy+0x1ec,1)==b'\1'
  s=m.hook();assert s.data.match!=old
  if case=='switch_restore':s=issue(m,24)
  elif case=='toggle_restore':s.switchPlayerEnabled=0;write(m,s);s=m.hook()
  elif case=='stop_restore':s.stop=1;write(m,s);s=m.hook()
  elif case=='paused_restore':
   m.b(0xa8e9a0,0);s=m.hook();assert m.r(0xa83d4c)==m.enemy
   s.switchPlayerEnabled=0;write(m,s);s=m.hook()
  else:
   m.w(0xa8ed84,1);s=m.hook();s.switchPlayerEnabled=0;write(m,s);s=m.hook();assert m.r(0xa83d4c)==m.enemy
  if case!='new_round':assert m.r(0xa83d4c)==m.house and m.u.mem_read(m.house+0x1ec,1)==b'\1' and m.u.mem_read(m.enemy+0x1ec,1)==b'\0'
 record('player_'+case)
# Explicit player order gateway, SP must preserve actor owner instead of local house.
for mode in (0,3,4,5):
 for enabled,neutral in ((True,False),(False,False),(True,True)):
  m,s=fixture(mode);s.controlAll=enabled;s.externalDisplay=1;write(m,s);s=m.hook()
  if neutral:m.u.mem_write(m.r(m.enemy+0x34)+0x24,b'Neutral\0')
  # Stub screen hit testing only; use actual gateway, constructor and transport.
  def click():
   sp=m.u.reg_read(UC_X86_REG_ESP);arg=lambda n:m.r(sp+4+n*4)
   m.u.mem_write(arg(1),struct.pack('<hh',20,30));m.u.mem_write(arg(2),struct.pack('<iii',5120,7680,0));m.w(arg(3),0);m.ret(0,6)
  addhook(m,0x692300,click)
  i=s.pointer.intent;i.match=s.data.match;i.tick=m.clock;i.frame=m.r(0xa8ed84);i.house=m.house;i.operation=9;i.width=800;i.height=600;i.x=20;i.y=30;i.count=1;i.sources[0].id=13;i.sources[0].pointer=m.enemyUnit
  s.pointer.request+=1;write(m,s);s=m.hook()
  n=m.r(0x8b41f8)+m.r(0xa802c8);assert n==int(enabled and not neutral),(mode,enabled,neutral,n,s.pointer.result,s.pointer.accepted)
  if n:
   at=0x8b4204 if mode in (0,5) else 0xa802d4;packet=bytes(m.u.mem_read(at,111));assert packet[2]==1 and packet[0]==4 and packet[12]==2,packet.hex()
  assert m.r(m.enemyUnit+0x21c)==m.enemy and m.r(0xa83d4c)==m.house
  record(f'player_order_{mode}_{enabled}_{neutral}')
# Cross-player sale, gate and target class.
for mode in (0,3,4,5):
 for enabled in (False,True):
  m,s=fixture(mode);m.w(m.building+0x21c,m.enemy);s.flags=33554432 if enabled else 0;write(m,s)
  def click():
   sp=m.u.reg_read(UC_X86_REG_ESP);arg=lambda n:m.r(sp+4+n*4)
   m.u.mem_write(arg(1),struct.pack('<hh',20,30));m.u.mem_write(arg(2),struct.pack('<iii',5120,7680,0));m.w(arg(3),m.building);m.ret(0,6)
  addhook(m,0x692300,click)
  i=s.pointer.intent;i.match=s.data.match;i.tick=m.clock;i.frame=m.r(0xa8ed84);i.house=m.house;i.operation=3;i.bindings=64|2048|8192;i.width=800;i.height=600;i.x=20;i.y=30
  s.pointer.request+=1;write(m,s);s=m.hook();n=m.r(0x8b41f8)+m.r(0xa802c8);assert n==int(enabled),(mode,enabled,n,s.pointer.result)
  if n:
   at=0x8b4204 if mode in (0,5) else 0xa802d4;packet=bytes(m.u.mem_read(at,111));assert packet[0]==0x16 and packet[2]==1 and packet[7:12]==struct.pack('<IB',12,0x34)
  assert m.r(m.building+0x21c)==m.enemy
  record(f'foreign_sale_{mode}_{enabled}')
# Original selection implementation, including mixed-owner list insertion.
m,s=fixture();s.controlAll=1;write(m,s);s=m.hook();assert s.playerControlStatus==1 and s.data.hookInstalled==5
m.execute_native_selection=True;m.b(m.t+0x230,1);m.b(m.engT+0x230,1);m.b(0x822cf2,0)
for obj in (m.unit,m.enemyUnit,m.eng):m.b(obj+0x14,1);m.b(obj+0x83,0)
m.vector(0xa8ecb8,[]);buffer=m.alloc(256);m.w(0xa8ecbc,buffer);m.w(0xa8ecc0,64)
# The synthetic DynamicVector has no C++ vtable. Supply its Find operation;
# original Object::Deselect still removes entries and clears selection flags.
vector_table=m.alloc(32);vector_find=m.STUB+0xef20;m.w(0xa8ecb8,vector_table);m.w(vector_table+16,vector_find)
def find_selected():
 sp=m.u.reg_read(UC_X86_REG_ESP);value=m.r(m.r(sp+4));items=[m.r(buffer+i*4) for i in range(m.r(0xa8ecc8))]
 m.ret(items.index(value) if value in items else 0xffffffff,1)
addhook(m,vector_find,find_selected)
addhook(m,0x40dd70,lambda:m.ret(0,0)) # cell lookup only, selection is native
for actor in (m.enemyUnit,m.unit):
 m.run(0x6fbfa0,(),actor);assert m.u.reg_read(UC_X86_REG_EAX)&255==1
assert m.r(0xa8ecc8)==2 and [m.r(buffer+i*4) for i in range(2)]==[m.enemyUnit,m.unit]
assert m.r(m.enemyUnit+0x21c)==m.enemy and m.r(m.unit+0x21c)==m.house
s.controlAll=0;write(m,s);s=m.hook();assert s.data.hookInstalled==0
assert m.r(0xa8ecc8)==0 and m.u.mem_read(m.unit+0x83,1)==b'\0' and m.u.mem_read(m.enemyUnit+0x83,1)==b'\0'
for site in (0x6FBFB3,0x6FBFEF,0x5F45D2,0x5F45DF,0x5F45EF):assert bytes(m.u.mem_read(site,5))==m.game.get_data(site-0x400000,5)
record('native_mixed_player_selection_and_restore')
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'real_game':False,'real_netplay':False,'results':rows},indent=2)+'\n')
