"""Execute compiled Runtime and vanilla event constructors. World/UI/network send callbacks are modeled; not live netplay."""
import argparse,ctypes as C,hashlib,importlib.util,json,traceback,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def fixture(mode=3):
 spec=importlib.util.spec_from_file_location('fixture128',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xA8B238,mode);s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=268435456;s.externalDisplay=1;m.u.mem_write(m.SH,bytes(s));m.hook();return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def request(m,command,arg=0):
 s=state(m);s.command=command;s.arg=arg;s.arg2=0;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;m.u.mem_write(m.SH,bytes(s));return m.hook()
def case(name,run):
 try:run();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def cheer(mode):
 m=fixture(mode);m.w(0x84555C,0);m.w(0xA8ED84,3000);rules=m.alloc(0x2000);m.w(0x8871E0,rules);m.w(rules+0x580,15)
 before=(bytes(m.u.mem_read(m.house,0x6000)),bytes(m.u.mem_read(m.buildT,0x2000)))
 s=request(m,25);assert s.result==1 and s.count==1,(C.c_int32(s.result).value,s.data.lastSubmitError)
 at=0x8B4204 if mode in (0,5) else 0xA802D4;packet=bytes(m.u.mem_read(at,111));assert packet[0]==0x2d
 s=request(m,25);assert C.c_int32(s.result).value==-22 and m.r(0x8B41F8)+m.r(0xA802C8)==1
 assert before==(bytes(m.u.mem_read(m.house,0x6000)),bytes(m.u.mem_read(m.buildT,0x2000)))
for mode in (0,3,4,5):case(f'cheer_{mode}_native_packet_cooldown',lambda mode=mode:cheer(mode))
def taunt(enabled=True):
 m=fixture();m.b(0xA8D110,int(enabled));m.w(0x825C88,0xffffffff)
 obj=m.alloc();m.w(obj,0x7EBEDC);m.w(obj+4,3);m.vector(0x87F658,[obj])
 calls=[]
 def native(uc,at,size,_):calls.append((uc.reg_read(UC_X86_REG_ECX),m.r(uc.reg_read(UC_X86_REG_ESP)+4)));m.ret(0,1)
 m.u.hook_add(UC_HOOK_CODE,native,begin=0x536350,end=0x536350)
 s=request(m,26,3);assert C.c_int32(s.result).value==(1 if enabled else -23),C.c_int32(s.result).value
 assert calls==([(obj,0)] if enabled else []) and m.r(0xA802C8)+m.r(0x8B41F8)==0
case('taunt_registered_command',lambda:taunt(True));case('taunt_session_disabled',lambda:taunt(False))
def produce(kind,blocked):
 m=fixture();m.canbuild_full_result=not blocked
 s=state(m);slot=next(i for i in range(s.data.typeCount) if s.data.types[i].kind==kind)
 t=s.data.types[slot];tab=0 if kind==7 else 2 if kind==16 else 3;strip=0x87F7E8+0x1544+tab*3988;m.w(strip+84,1);m.w(strip+88,t.index);m.w(strip+92,kind)
 before=bytes(m.u.mem_read(m.house,0x6000));s=request(m,27,slot)
 assert C.c_int32(s.result).value==(-9 if blocked else 1),(kind,blocked,C.c_int32(s.result).value)
 assert m.r(0xA802C8)==(0 if blocked else 1)
 if not blocked:
  packet=bytes(m.u.mem_read(0xA802D4,111));assert packet[0]==0x0e
 assert bytes(m.u.mem_read(m.house,0x6000))==before
for kind in (7,16,40):
 for blocked in (False,True):case(f'deferred_{kind}_prereq_{blocked}',lambda kind=kind,blocked=blocked:produce(kind,blocked))
def missing_factory():
 m=fixture();m.canbuild_full_result=1;m.factory_available=False;s=state(m);slot=next(i for i in range(s.data.typeCount) if s.data.types[i].kind==16)
 strip=0x87F7E8+0x1544+2*3988;m.w(strip+84,1);m.w(strip+88,0);m.w(strip+92,16)
 s=request(m,27,slot);assert C.c_int32(s.result).value==-9 and m.r(0xA802C8)==0
case('deferred_no_factory_no_event',missing_factory)
def half_mask(fog):
 m=fixture();s=state(m);s.renderReveal=2;m.u.mem_write(m.SH,bytes(s));s=m.hook();assert s.data.renderRevealStatus==1
 point=m.alloc();clip=m.alloc();pixels=m.alloc();alpha=m.alloc();dest=m.alloc();shape=m.alloc()
 m.u.mem_write(point,struct.pack('<ii',10,20));m.u.mem_write(clip,struct.pack('<4i',10,20,4,1));m.u.mem_write(pixels,bytes([0,64,127,254]));m.u.mem_write(dest,struct.pack('<4H',127,127,127,127))
 m.w(alpha+0x1C,dest+8);m.w(alpha+0x20,8);m.w(alpha+0x28,4);m.w(0x87E8A4,alpha);m.w(0x89E760,shape);m.w(0x89E790,shape);m.w(0x89E794,shape)
 def bounds(uc,at,size,_):
  esp=uc.reg_read(UC_X86_REG_ESP);out=m.r(esp+4);m.u.mem_write(out,struct.pack('<4i',0,0,4,1));m.ret(out,2)
 def data(uc,at,size,_):m.ret(pixels,1)
 def target(uc,at,size,_):m.ret(dest,2)
 for at,fn in ((0x69E7E0,bounds),(0x69E740,data),(0x4114B0,target)):m.u.hook_add(UC_HOOK_CODE,fn,begin=at,end=at)
 before=(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)),m.r(0xA802C8))
 m.run(0x47F250 if fog else 0x47EFE0,(point,clip,15),self=m.crateCell)
 assert struct.unpack('<4H',m.u.mem_read(dest,8))==(64,96,127,127),list(m.u.mem_read(dest,8))
 assert before==(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)),m.r(0xA802C8))
case('half_shroud_real_boundary',lambda:half_mask(False));case('half_fog_real_boundary',lambda:half_mask(True))
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(r['status']!='PASS' for r in rows))
