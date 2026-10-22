"""Shipped DLL + native FindFactory in synthetic worlds; CanBuild, AddCameo and
Win32 are fixture substitutes. This is not a peer simulation or netplay test.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *

p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def fixture(session=5):
 spec=importlib.util.spec_from_file_location('f',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,session);s=m.hook();s.flags=4096;s.techAllowed[1]=1;m.u.mem_write(m.SH,bytes(s))
 m.execute_native_find_factory=True;m.find_calls=[]
 for typ,kind in ((m.t,40),(m.buildT,7)):
  vt=m.alloc(0x600);m.u.mem_write(vt,bytes(m.u.mem_read(m.r(typ),0x600)));m.w(typ,vt)
  for offset,value in ((0x2c,kind),(0x70,1)):
   fn=m.STUB+0xe100+(0 if typ==m.t else 0x100)+offset;m.w(vt+offset,fn)
   m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_,v=value:m.ret(v,0),begin=fn,end=fn)
  m.w(vt+0x94,0x5f7900)
 m.w(m.buildT+0xeb8,40);m.b(m.building+0x660,1);m.b(m.building+0x3d3,1);m.vector(m.house+0x68,[m.building])
 m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_:m.ret(0,0),begin=0x65ae30,end=0x65ae30)
 def trace(uc,addr,size,_):
  sp=uc.reg_read(UC_X86_REG_ESP);m.find_calls.append((uc.reg_read(UC_X86_REG_ECX),*[m.r(sp+4+i*4) for i in range(4)]))
 m.u.hook_add(UC_HOOK_CODE,trace,begin=0x5f7900,end=0x5f7900)
 return m
def tick(m):
 m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def issue(m,command,arg=0,implementation=0):
 s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
 s.command=command;s.arg=arg;s.arg2=implementation;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;m.u.mem_write(m.SH,bytes(s));return tick(m)
def count(m):return m.r(0x8b41f8)+m.r(0xa802c8)
def record(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)

for session in (0,3,4,5):
 for implementation in (0,1):
  m=fixture(session);before=bytes(m.u.mem_read(m.t,0x2000));s=issue(m,10,1,implementation)
  assert s.data.techOpen==implementation+1 and s.data.hookInstalled==2 and s.result==1
  assert m.find_calls[-1]==(m.t,1,0,implementation,m.house),m.find_calls
  assert m.sidebar_add_calls==[(40,0)] and count(m)==0
  entry=0x6AA781+5+struct.unpack('<i',m.u.mem_read(0x6AA782,4))[0]
  m.run(entry,(m.t,0,1),m.house);assert m.canbuild_calls[-1]==(m.house,m.t,1,1)
  m.run(entry,(m.t,0,1),m.enemy);assert m.canbuild_calls[-1]==(m.enemy,m.t,0,1)
  m.run(entry,(m.engT,0,1),m.house);assert m.canbuild_calls[-1]==(m.house,m.engT,0,1)
  n=len(m.find_calls);s=issue(m,12,1);assert s.result==1 and s.count==1
  assert (m.t,int(implementation!=1),1,1,m.house) in m.find_calls[n:],m.find_calls[n:]
  packet=bytes(m.u.mem_read(0x8b4204 if session in (0,5) else 0xa802d4,111))
  assert packet[0]==0x0e and packet[7:19]==struct.pack('<iii',40,0,0)
  assert bytes(m.u.mem_read(m.t,0x2000))==before
  s=issue(m,10,0);assert s.data.techOpen==0 and s.data.hookInstalled==0
  for site in (0x6A97D2,0x6AA781):assert bytes(m.u.mem_read(site,5))==m.game.get_data(site-0x400000,5)
  record(f'mode_{session}_implementation_{implementation}_native_factory_scope_payload_restore')

for implementation in (0,1):
 m=fixture();m.canbuild_limit_result=0;s=issue(m,10,1,implementation)
 assert m.sidebar_add_calls==([] if implementation else [(40,0)]),m.sidebar_add_calls
 s=issue(m,12,1);assert C.c_int32(s.result).value==-9 and count(m)==0
 record(f'implementation_{implementation}_native_build_limit_rejection')

m=fixture();s=issue(m,10,1,1);start=m.clock;n=len(m.find_calls)
while m.clock-start+110<1000:
 tick(m);assert len(m.find_calls)==n
tick(m);assert len(m.find_calls)==n+1 and len(m.sidebar_add_calls)==1 and count(m)==0
s=issue(m,10,1,0);assert s.data.techOpen==1 and m.find_calls[-1][1:4]==(1,0,0)
s=issue(m,10,1,1);assert s.data.techOpen==2 and m.find_calls[-1][1:4]==(1,0,1)
s=issue(m,10,1,9);assert C.c_int32(s.result).value==-4 and s.data.techOpen==2 and count(m)==0
record('one_second_refresh_switch_and_invalid_selection_no_events')

m=fixture();m.w(0x87f7e8+0x1544+3*3988+84,75);s=issue(m,10,1,1)
assert not m.sidebar_add_calls and s.data.techUnavailable==1 and count(m)==0
record('classic_sidebar_capacity_guard')

m=fixture();m.u.mem_write(0x6AA781,b'\x90'*5);s=issue(m,10,1,1)
assert C.c_int32(s.result).value==-11 and not s.data.techOpen
assert bytes(m.u.mem_read(0x6AA781,5))==b'\x90'*5
assert bytes(m.u.mem_read(0x6A97D2,5))==m.game.get_data(0x6A97D2-0x400000,5)
record('classic_foreign_patch_rollback')

# Regression for the user's early-version flicker/cancel report: run the
# original prune branch while the host heartbeat is stale, after a periodic
# refresh, and after switching implementations. Do not merely inspect a flag.
def native_prune_keeps(m):
 reached=[];handles=[]
 for address in (0x6AA7B6,0x6AAA68):
  def stop(uc,addr,size,_):reached.append(addr);uc.emu_stop()
  handles.append(m.u.hook_add(UC_HOOK_CODE,stop,begin=address,end=address))
 sp=m.STACK+0x1e000;m.u.mem_write(sp,b'\0'*256)
 m.u.reg_write(UC_X86_REG_ESP,sp);m.u.reg_write(UC_X86_REG_EBX,m.t);m.u.reg_write(UC_X86_REG_EDI,0)
 m.u.emu_start(0x6AA75F,m.STOP,count=100000)
 for h in handles:m.u.hook_del(h)
 assert reached==[0x6AAA68],reached
for implementation in (0,1):
 m=fixture();issue(m,10,1,implementation);native_prune_keeps(m)
 m.clock+=4000;m.w(m.SH+m.Shared.heartbeat.offset,1);native_prune_keeps(m)
 tick(m);native_prune_keeps(m)
 issue(m,10,1,1-implementation);native_prune_keeps(m)
 assert count(m)==0 and m.sidebar_add_calls==[(40,0)]
 record(f'implementation_{implementation}_native_prune_keeps_during_stall_refresh_switch')
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'real_game':False,'real_netplay':False,'results':rows},indent=2)+'\n')
