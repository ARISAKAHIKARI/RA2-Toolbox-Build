"""Actual x86 DLL and native event constructors, pure C++ host calculator.
Synthetic OS/world/virtual functions. Does not run Windows or real netplay.
"""
from pathlib import Path
import subprocess,tempfile
exec(compile(Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0],str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
root=Path(__file__).resolve().parents[2];rows=[]
def record(name):rows.append({'name':name,'status':'PASS'});print('PASS',name,flush=True)
def configure(m,inside,flags):
 m.clock+=16;m.w(0xa8ed84,1);s=state(m);s.computeRequested=960 if inside else 192;s.flags=flags;save(m,s);s=m.hook();assert s.computeApplied==s.computeRequested
def host_result(m,host):
 s=state(m);assert s.unloadTask!=s.unloadComputed
 data=subprocess.run([str(host)],input=bytes(s.unloadInput),stdout=subprocess.PIPE,check=True).stdout
 assert len(data)==C.sizeof(m.UnloadResult)
 s.unloadResult=m.UnloadResult.from_buffer_copy(data);s.unloadComputed=s.unloadTask;s.unloadResultSeq+=2;save(m,s)
 return s
def setup(mode,inside,eject=False):
 m=fixture(mode);configure(m,inside,1024|(2048 if eject else 16384))
 m.vector(0xa83de8,[m.eng]);m.vector(0xa8ecb8,[m.unit]);m.b(m.unit+0x83,1)
 m.w(m.unit+0x114,1);m.w(m.unit+0x118,m.eng);m.w(m.eng+0x11c,m.unit);m.b(m.eng+0x81,1);m.b(m.eng+0x90,0);m.w(m.eng+0xac,7);m.w(m.eng+0x5a4,m.unit)
 m.w(m.enemy+0x34,m.alloc());m.w(m.building+0x21c,m.enemy)
 m.w(m.eng+0x9c,5000);m.w(m.eng+0xa0,5000);m.w(m.building+0x9c,5400);m.w(m.building+0xa0,5000)
 far=m.obj(0x7e3ebc,m.buildT,m.enemy,99);m.w(far+0x9c,9000);m.w(far+0xa0,5000)
 m.vector(0xa8eb40,[far,m.building]);m.vector(0xa8ec78,[m.unit,m.eng,far,m.building]);m.action_overrides[(m.eng,m.building)]=9;m.action_overrides[(m.eng,far)]=9
 if eject:tick(m);m.w(m.unit+0x6c,0);m.b(m.unit+0x74,0)
 else:
  s=state(m);s.command=17;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook();assert s.request==s.ack and count(m)==1
 m.w(m.eng+0x11c,0);m.b(m.eng+0x81,0);m.b(m.eng+0x90,1);m.w(m.eng+0xac,5);m.w(m.eng+0x5a4,0)
 return m
with tempfile.TemporaryDirectory(prefix='unload-compute-') as tmp:
 cpp=Path(tmp)/'host.cpp';host=Path(tmp)/'host'
 cpp.write_text('#include "'+str(root/'assist/source/unload_compute.h')+'"\n#include <cstdio>\n#include <memory>\nint main(){auto i=std::make_unique<assist::unloadcompute::Input>();if(fread(i.get(),sizeof(*i),1,stdin)!=1)return 2;auto r=assist::unloadcompute::calculate(*i);fwrite(&r,sizeof(r),1,stdout);}')
 subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(host)],check=True)
 for mode in (0,3):
  for inside in (False,True):
   for eject in (False,True):
    m=setup(mode,inside,eject);before=bytes(m.u.mem_read(m.eng,0x2000));s=tick(m)
    if not inside:
     assert count(m)==int(not eject);host_result(m,host);s=tick(m)
    assert count(m)==1+int(not eject),(mode,inside,eject,count(m),s.data.unloadPending)
    packet=packets(m)[-111:];assert packet[12]==8 and packet[19:24]==struct.pack('<IB',12,0x34)
    for _ in range(5):tick(m)
    assert count(m)==1+int(not eject) and before==bytes(m.u.mem_read(m.eng,0x2000)) and not m.codeWrites
    record(f'nearest_engineer_{mode}_{inside}_{eject}')
  for cause in ('stale_ms','stale_frame','moved','manual_stop','token','dead_target','target_action','disabled','new_round','torn','queue_full'):
   m=setup(mode,False);tick(m);s=host_result(m,host)
   if cause=='stale_ms':m.clock+=151
   if cause=='stale_frame':m.w(0xa8ed84,m.r(0xa8ed84)+7)
   if cause=='moved':m.w(m.eng+0x9c,5500)
   if cause=='manual_stop':m.w(m.eng+0xac,13)
   if cause=='token':pointer(m,5,mutate=lambda s,i:(setattr(i.sources[0],'id',11),setattr(i.sources[0],'pointer',m.eng)))
   if cause=='dead_target':m.w(m.building+0x6c,0);m.b(m.building+0x74,0)
   if cause=='target_action':m.action_overrides[(m.eng,m.building)]=31
   if cause=='disabled':s.flags=0;save(m,s)
   if cause=='new_round':m.w(0xa8ed84,0)
   if cause=='torn':s.unloadResultSeq|=1;save(m,s)
   if cause=='queue_full':m.w(0x8b41f8 if mode==0 else 0xa802c8,16384 if mode==0 else 112)
   before=count(m);tick(m);assert count(m)==before,(mode,cause,count(m))
   if cause in ('stale_ms','stale_frame','moved','queue_full'):
    if cause=='queue_full':m.w(0x8b41f8 if mode==0 else 0xa802c8,1)
    # Expiry/backpressure must request a fresh calculation, not drop the job.
    for _ in range(3):
     if state(m).unloadTask!=state(m).unloadComputed:host_result(m,host)
     tick(m)
     if count(m)==2:break
    assert count(m)==2,(mode,cause,'did not recover',state(m).data.unloadPending)
   assert not m.codeWrites
   record(f'reject_external_{mode}_{cause}')
  # Five survivors, two complete immediately; three interrupted animations
  # scatter and independently retry. Real commands are observed and reflected
  # in the synthetic world, as the engine would do after event execution.
  for inside in (False,True):
   m=fixture(mode);configure(m,inside,1024|2048);m.b(m.engT+0xec8,1);m.b(m.unit+0x83,1)
   people=[m.eng]+[m.obj(0x7eb058,m.engT,m.house,20+n) for n in range(4)]
   m.vector(0xa83de8,people);m.vector(0xa8ec78,[m.unit]+people)
   for p in people:m.w(p+0x11c,m.unit);m.b(p+0x81,1);m.b(p+0x90,0);m.w(p+0xac,7);m.w(p+0x5a4,m.unit)
   for offset in (0x314,0x554):
    address=m.r(0x7eb058+offset)
    m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_,off=offset:m.ret(1 if off==0x314 else int(27<=m.r(m.u.reg_read(UC_X86_REG_ECX)+0x6c4)<=30),0),begin=address,end=address)
   tick(m);m.w(m.unit+0x6c,0);m.b(m.unit+0x74,0)
   for p in people:m.w(p+0x11c,0);m.b(p+0x81,0);m.b(p+0x90,1);m.w(p+0xac,5)
   attempts={m.r(p+0x10):0 for p in people};seen=0;first=None
   for age in range(65):
    for n,p in enumerate(people):
     num=attempts[m.r(p+0x10)]
     if num and n>=2 and num==1 and age>(first or 0)+4:
      m.w(p+0x6c4,0);moving=age<15+n*3;m.w(p+0xac,2 if moving else 5)
      if moving:m.w(p+0x9c,m.r(p+0x9c)+16)
    if not inside and state(m).unloadTask!=state(m).unloadComputed:host_result(m,host)
    s=tick(m)
    for n in range(seen,count(m)):
     packet=packets(m)[n*111:(n+1)*111];ident=struct.unpack_from('<I',packet,7)[0]
     assert packet[12]==16;attempts[ident]+=1;assert attempts[ident]<=2
     p=next(p for p in people if m.r(p+0x10)==ident);index=people.index(p)
     m.w(p+0x6c4,28 if index<2 or attempts[ident]==2 else 27);m.w(p+0xac,16)
     if first is None:first=age
    seen=count(m)
   assert list(attempts.values())==[1,1,2,2,2],(mode,inside,attempts)
   assert s.data.unloadPending==0 and not m.codeWrites
   record(f'five_interrupted_survivors_{mode}_{inside}')
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
