"""Shipped DLL direct orders plus the original route-cancellation function.
Native planning receiver creates an existing route for replacement cases; the
original 6386E0 then consumes the emitted direct packet. Synthetic world, heap,
network-ID and audio interfaces; no full mission receiver/pathfinding/netplay.
"""
import argparse,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('helper',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args
for case in ('ordinary','engineer','spy','c4','replace_existing','busy','native_key','native_key_existing'):
 try:
  m=n.fixture(0)
  def hook(at,fn):m.u.hook_add(UC_HOOK_CODE,lambda uc,a,size,_:fn(),begin=at,end=at)
  def reg(r):return m.u.reg_read(r)
  def arg(i):return m.r(reg(UC_X86_REG_ESP)+4+4*i)
  hook(0x7c8e17,lambda:m.ret(m.alloc(arg(0)),0));hook(0x7c8b3d,lambda:m.ret(0,0))
  hook(0x50b6f0,lambda:m.ret(0,0));hook(0x750920,lambda:m.ret(0,2));hook(0x521b60,lambda:m.ret(0,0))
  other=m.obj(0x7e3ebc,m.buildT,m.enemy,14);objects={11:m.eng,12:m.building,14:other}
  for at in (0x6e6f20,0x6e6e20,0x6e6ff0):hook(at,lambda:m.ret(objects.get(m.r(reg(UC_X86_REG_ECX)),0),0))
  # Equivalent initialized pointer vectors, with native type-specific vtables.
  for va,vt in ((0xac4b30,0x7efe64),(0xac4b48,0x7efe24),(0xac4c40,0x7e180c),(0xac4c78,0x7efea4),(0xac4c98,0x7efe64),(0xac4c18,0x7efe64)):
   m.vector(va,[0]*32);m.w(va,vt);m.w(va+16,0)
  m.w(m.eng+0xac,2 if case=='busy' else 5);m.w(m.eng+0xb4,0xffffffff)
  m.b(m.engT+0xec3,case=='engineer');m.b(m.engT+0xebe,case=='spy');m.b(m.engT+0xec2,case=='c4');m.w(m.building+0x21c,m.enemy)
  def receive(data):
   p=m.alloc();m.u.mem_write(p,data);m.run(0x4c6cb0,(),p)
  if case in ('replace_existing','native_key_existing'):
   prior=m.alloc();m.run(0x4c6860,(0,11,0x34,2,0,0,14,0x34,0,0),prior);m.b(prior+29,1);receive(bytes(m.u.mem_read(prior,111)))
  s=n.state(m);s.c4Test=1;n.save(m,s);m.queryObject=m.building
  if case in ('native_key','native_key_existing'):m.b(0x87f7e8+0x11b3,1)
  def source(s,i):i.sources[0]=m.PointerSource(11,m.eng)
  s=n.pointer(m,7,8192,source);data=n.packets(m)
  assert s.pointer.accepted==1 and s.pointer.ack==s.pointer.request and n.count(m)==1
  mission=8 if case=='engineer' else 7 if case=='spy' else 17
  assert data[0]==4 and data[12]==mission and data[29]==0 and data[19:24]==struct.pack('<IB',12,0x34)
  token=m.r(m.eng+0x514)
  if case in ('replace_existing','native_key_existing'):assert token and m.r(token+20)==1
  else:assert not token,'direct submission must not create a planning token'
  packet=m.alloc();m.u.mem_write(packet,data)
  m.u.reg_write(UC_X86_REG_EDX,packet);m.run(0x6386e0,(),m.eng)
  assert not token or m.r(token+20)==0,'original routine must remove the existing route'
  assert m.r(0xac4b58)==0,'no waypoint released into the original planning queue'
  assert m.r(m.eng+0xac)==(2 if case=='busy' else 5) and m.r(m.eng+0xb4)==0xffffffff
  assert bool(m.u.mem_read(0x87f7e8+0x11b3,1)[0])==(case in ('native_key','native_key_existing'))
  for _ in range(3):n.tick(m)
  assert n.count(m)==1,'no deferred commit or duplicate order'
  assert all(any(site<=at and at+size<=site+6 for site in (0x51a4d8,0x51c32b)) for at,size in m.codeWrites)
  rows.append({'case':case,'status':'PASS','direct_mission':mission})
 except Exception as e:rows.append({'case':case,'status':'FAIL','error':repr(e)});traceback.print_exc()
 print(rows[-1],flush=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))
