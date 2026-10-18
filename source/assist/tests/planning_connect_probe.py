"""Trace native PlanConnect copying and dispatch in an unpatched reference EXE.

The map/object registry and allocator are synthetic. Original event constructors,
PlanConnect execution, token copying, and token commit/next dispatch execute as
machine code. This is NOT a completed C4 explosion, movement, or netplay test.
"""
import argparse, hashlib, importlib.util, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import *

p=argparse.ArgumentParser()
p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True)
args=p.parse_args();root=Path(__file__).resolve().parents[2]

def fixture():
 spec=importlib.util.spec_from_file_location('f',root/'assist/tests/entry_runtime_regression.py')
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[args.game,args.dll];spec.loader.exec_module(m)
 return m

rows=[]
for name,mission in [('tanya_sabotage',17),('engineer_capture',8),('spy_enter',7)]:
 m=fixture();u=m.u;donor=m.obj(0x7eb058,m.alloc(0x2000),m.house,21)
 follower=m.eng;target=m.building;m.b(m.engT+0xec2,0);m.b(m.engT+0xec3,0)
 registry={11:follower,12:target,21:donor};hooks=[];path_calls=[];writes=[]
 def hook(at,fn):
  h=u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:fn(),begin=at,end=at);hooks.append(h)
 def argument(i):return m.r(u.reg_read(UC_X86_REG_ESP)+4+i*4)
 # Synthetic allocator and object lookup only; no task/capability result mocks.
 hook(0x7c8e17,lambda:m.ret(m.alloc(argument(0)),0))
 hook(0x7c8b3d,lambda:m.ret(0,0))
 for at in (0x6e6e20,0x6e6f20,0x6e7a80):
  hook(at,lambda:m.ret(registry.get(m.r(u.reg_read(UC_X86_REG_ECX)),0),0))
 hook(0x50b6f0,lambda:m.ret(0,0)) # no local UI selection side effects
 # Record unmocked path function entry points.
 for at in (0x637e00,0x639800,0x6367b0,0x636590,0x6385c0):
  u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:path_calls.append(hex(a)),begin=at,end=at)
 def vec(at,values,capacity=32):
  m.vector(at,values);m.w(at,0x7efe44);old=m.r(at+4);data=m.alloc(capacity*4)
  u.mem_write(data,bytes(u.mem_read(old,len(values)*4)))
  m.w(at+4,data);m.w(at+8,capacity)
 def token(obj):
  t=m.alloc(0x9c);m.run(0x635f20,(obj,),t)
  vec(t+4,[]);m.w(t+4,0x7efe44);m.w(obj+0x514,t)
  return t
 loco=m.alloc(0x100);lv=m.alloc(0x100);m.w(loco,lv);m.w(lv+0x80,m.STUB+0xe900)
 hook(m.STUB+0xe900,lambda:m.ret(0,1)) # Is_Moving query in a stationary synthetic world
 for ob in (donor,follower):m.w(ob+0x674,loco);m.w(ob+0x6c4,0xffffffff)
 dt=token(donor);ft=token(follower)
 event=m.alloc(0x70)
 m.run(0x4c6860,(0,21,0x34,mission,12,0x34,12,0x34,0,0),event)
 m.b(event+0x1d,1)
 original=bytes(u.mem_read(event,111))
 # Existing user-created planning node containing the leader's native event.
 binding=m.alloc(16);m.w(binding,donor);m.w(binding+4,event);m.w(binding+8,0xffffffff)
 node=m.alloc(0x100);vec(node,[binding]);m.w(node,0x7efe44);m.w(node+0x18,100)
 vec(node+0x20,[]);m.w(node+0xac,0xffffffff);m.w(node+0xb0,0xffffffff);m.w(node+0xb4,0xffffffff)
 vec(dt+4,[node]);m.w(dt+4,0x7efe44);vec(0xac4b30,[node])
 vec(0xac4b48,[]);vec(0xac4c40,[]);vec(0xac4c78,[dt,ft]);vec(0xac4c98,[])
 # Preserve genuine vtables and task methods. Object target coordinates query
 # is replaced only because the synthetic building has no map footprint.
 vt=m.alloc(0x600);u.mem_write(vt,bytes(u.mem_read(m.r(target),0x600)));m.w(target,vt)
 coords=m.alloc(12);u.mem_write(coords,struct.pack('<iii',25600,25600,0))
 stub=m.STUB+0xe800;m.w(vt+0x58,stub)
 def getcoords():u.mem_write(argument(0),bytes(u.mem_read(coords,12)));m.ret(argument(0),1)
 hook(stub,getcoords)
 def protect(uc,access,at,size,value,_):
  if 0x401000<=at<0x7e0000 or m.engT<=at<m.engT+0x2000:writes.append(hex(at))
 u.hook_add(UC_HOOK_MEM_WRITE,protect)
 connect=m.alloc(0x70);m.run(0x4c6780,(0,0x2a,11,0x34,12,0x34),connect)
 m.run(0x4c6cb0,(),connect)
 assert m.r(ft+0x14)==1,('not connected',name,path_calls)
 assert m.r(node+0x10)==2
 newbinding=m.r(m.r(node+4)+4);copied=m.r(newbinding+4);payload=bytes(u.mem_read(copied,111))
 assert m.r(newbinding)==follower
 assert payload[7:12]==struct.pack('<IB',11,0x34)
 assert payload[:7]==original[:7] and payload[12:]==original[12:]
 # Execute native PlanCommit -> TryNextPlanningTokenNode -> next-event list.
 # QueueMission(Wait) changes only synthetic unit state; the real method runs.
 vec(0xa8ec78,[donor,follower]);commit=m.alloc(0x70);m.run(0x4c66c0,(0,0x2b),commit)
 from collections import deque
 trace=deque(maxlen=50)
 def tracing(uc,a,size,_):
  if a<0x700000:trace.append((hex(a),hex(uc.reg_read(UC_X86_REG_ECX)),hex(uc.reg_read(UC_X86_REG_ESP))))
  if a<0x400000:raise AssertionError('invalid call')
 tracehook=u.hook_add(UC_HOOK_CODE,tracing)
 try:m.run(0x4c6cb0,(),commit)
 except Exception:
  print(list(trace));raise
 u.hook_del(tracehook)
 emitted=[]
 for i in range(m.r(0xac4b58)):
  ep=m.r(m.r(0xac4b4c)+i*4);emitted.append(bytes(u.mem_read(ep,111)))
 mine=[e for e in emitted if e[7:12]==struct.pack('<IB',11,0x34)]
 assert len(mine)==1,(name,'no dispatched follower',len(emitted),path_calls)
 assert mine[0][12]==mission and mine[0][14:29]==original[14:29] and mine[0][29]==2
 assert not writes,('code/type write',writes)
 row={'case':name,'status':'PASS','connect_event':bytes(u.mem_read(connect,17)).hex(),
      'copied_event':payload[:30].hex(),'emitted_event':mine[0][:30].hex(),
      'mission':mission,'planning_flag':2,'calls':path_calls,
      'code_or_infantry_type_writes':writes,'final_gameplay_effect':'NOT TESTED'}
 rows.append(row);print(name,'PASS',row['emitted_event'])
Path(args.output).write_text(json.dumps({'scope':__doc__,'reference_sha256':hashlib.sha256(Path(args.game).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
