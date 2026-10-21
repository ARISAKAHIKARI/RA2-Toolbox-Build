"""Actual native receiver, planning-node construction, commit and next-node extraction.
Synthetic network-ID resolver, CRT heap, local-owner/audio and locomotor readiness;
original planning functions and QueueMission execute. No full pathfinding/arrival/netplay.
"""
import argparse,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('helper',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args
for case in ('ordinary','engineer','spy','c4','append_existing','busy_waits','native_key'):
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
  m.w(m.eng+0xac,2 if case=='busy_waits' else 5);m.w(m.eng+0xb4,0xffffffff)
  m.b(m.engT+0xec3,case=='engineer');m.b(m.engT+0xebe,case=='spy');m.b(m.engT+0xec2,case=='c4');m.w(m.building+0x21c,m.enemy)
  def receive(data):
   p=m.alloc();m.u.mem_write(p,data);m.run(0x4c6cb0,(),p)
  if case=='append_existing':
   prior=m.alloc();m.run(0x4c6860,(0,11,0x34,2,0,0,14,0x34,0,0),prior);m.b(prior+29,1);receive(bytes(m.u.mem_read(prior,111)))
  s=n.state(m);s.c4Test=1;n.save(m,s);m.queryObject=m.building
  if case=='native_key':m.b(0x87f7e8+0x11b3,1)
  def source(s,i):i.sources[0]=m.PointerSource(11,m.eng)
  n.pointer(m,7,8192,source);data=n.packets(m);assert data[29]==1
  receive(data[:111]);token=m.r(m.eng+0x514);assert token and m.r(token+20)==(2 if case=='append_existing' else 1)
  assert m.r(0xac4b58)==0,'append alone must not start execution'
  if case=='native_key':
   assert n.count(m)==1
   commit=m.alloc();m.run(0x4c66c0,(0,43),commit);receive(bytes(m.u.mem_read(commit,111)))
  else:assert data[111]==43;receive(data[111:222])
  if case=='busy_waits':
   assert m.r(0xac4b58)==0
   m.w(m.eng+0xac,5);m.run(0x6385c0,(),m.eng)
  assert m.r(0xac4b58)==1
  if case=='append_existing':
   first=m.r(m.r(0xac4b4c));assert bytes(m.u.mem_read(first+12,1))==b'\x02'
   m.b(token+28,0) # received prior waypoint acknowledgement at boundary
   m.run(0x6385c0,(),m.eng);assert m.r(0xac4b58)==2
  q=m.r(m.r(0xac4b4c)+(m.r(0xac4b58)-1)*4);out=bytes(m.u.mem_read(q,111));mission=8 if case=='engineer' else 17 if case=='c4' else 7
  assert out[0]==4 and out[12]==mission and out[29]==2 and out[19:24]==struct.pack('<IB',12,0x34)
  assert m.r(m.eng+0xb4)==28 and not m.codeWrites
  rows.append({'case':case,'status':'PASS','released_mission':mission})
 except Exception as e:rows.append({'case':case,'status':'FAIL','error':repr(e)});traceback.print_exc()
 print(rows[-1],flush=True)
Path(a.output).write_text(json.dumps({'scope':__doc__,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))
