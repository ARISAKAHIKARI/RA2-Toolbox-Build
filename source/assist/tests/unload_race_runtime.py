"""Real x86 Runtime/events, synthetic transport/OS: native-first hotkey and missed deploy observations. Not live gameplay."""
from pathlib import Path
import subprocess,tempfile
exec(compile(Path(__file__).with_name('unload_locations_runtime.py').read_text().split('with tempfile.TemporaryDirectory',1)[0],str(Path(__file__).with_name('unload_locations_runtime.py')),'exec'))
rows=[]
def loaded(mode,inside,deployer=False):
 m=fixture(mode);configure(m,inside,1024|16384)
 m.vector(0xa83de8,[m.eng]);m.vector(0xa8ecb8,[m.unit]);m.b(m.unit+0x83,1)
 m.w(m.unit+0x114,1);m.w(m.unit+0x118,m.eng);m.w(m.eng+0x11c,m.unit);m.b(m.eng+0x81,1);m.b(m.eng+0x90,0);m.w(m.eng+0xac,7);m.w(m.eng+0x5a4,m.unit)
 m.w(m.enemy+0x34,m.alloc());m.w(m.building+0x21c,m.enemy)
 m.w(m.eng+0x9c,5000);m.w(m.eng+0xa0,5000);m.w(m.building+0x9c,5400);m.w(m.building+0xa0,5000)
 m.vector(0xa8ec78,[m.unit,m.eng,m.building]);m.action_overrides[(m.eng,m.building)]=9
 if deployer:
  m.b(m.engT+0xec8,1);address=m.r(0x7eb058+0x314)
  m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(1,0),begin=address,end=address)
 tick(m);return m
def land(m,mission=2):
 m.w(m.unit+0x114,0);m.w(m.eng+0x11c,0);m.b(m.eng+0x81,0);m.b(m.eng+0x90,1);m.w(m.eng+0xac,mission);m.w(m.eng+0x5a4,0)
def request(m):
 s=state(m);s.command=17;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook();assert s.ack==s.request;return s
with tempfile.TemporaryDirectory(prefix='unload-race-') as tmp:
 cpp=Path(tmp)/'host.cpp';host=Path(tmp)/'host'
 cpp.write_text('#include "'+str(root/'assist/source/unload_compute.h')+'"\n#include <cstdio>\n#include <memory>\nint main(){auto i=std::make_unique<assist::unloadcompute::Input>();if(fread(i.get(),sizeof(*i),1,stdin)!=1)return 2;auto r=assist::unloadcompute::calculate(*i);fwrite(&r,sizeof(r),1,stdout);}')
 subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(host)],check=True)
 def step(m,inside):
  if not inside and state(m).unloadTask!=state(m).unloadComputed:host_result(m,host)
  return tick(m)
 for mode in (0,3):
  for inside in (False,True):
   for case in ('native_first','stale_time','stale_frame','reused_id','manual','disabled','moving_host'):
    m=loaded(mode,inside);land(m);tick(m)
    if case=='stale_time':m.clock+=1501
    if case=='stale_frame':m.w(0xa8ed84,m.r(0xa8ed84)+91)
    if case=='reused_id':m.w(m.eng+0x10,111)
    if case=='manual':pointer(m,5,mutate=lambda s,i:(setattr(i.sources[0],'id',11),setattr(i.sources[0],'pointer',m.eng)))
    if case=='disabled':s=state(m);s.flags=0;save(m,s)
    request(m)
    for _ in range(5):
     if case=='moving_host':m.w(m.eng+0x9c,m.r(m.eng+0x9c)+16)
     step(m,inside)
    expected=case in ('native_first','moving_host')
    assert count(m)==int(expected),(mode,inside,case,count(m),state(m).data.unloadPending)
    if expected:
     packet=packets(m);assert packet[12]==8 and struct.unpack_from('<I',packet,7)[0]==11
    assert not m.codeWrites;record(f'{mode}_{inside}_{case}')
   m=loaded(mode,inside,True);request(m);assert count(m)==1;land(m,5)
   for _ in range(4):step(m,inside)
   assert count(m)==2
   # Event accepted by queue, but no Deploy mission/animation was ever sampled.
   start=m.r(0xa8ed84)
   for _ in range(285):step(m,inside);assert count(m)==2
   for _ in range(24):step(m,inside)
   assert count(m)==3,(mode,inside,'missing retry',count(m))
   m.w(m.eng+0x6c4,28);m.w(m.eng+0xac,16)
   for _ in range(5):s=step(m,inside)
   assert count(m)==3 and s.data.unloadPending==0 and not m.codeWrites
   record(f'{mode}_{inside}_unobserved_deploy_retry')
Path(a.output).write_text(json.dumps({'scope':__doc__,'results':rows},indent=2)+'\n')
