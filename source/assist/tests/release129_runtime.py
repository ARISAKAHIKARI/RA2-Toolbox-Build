"""Execute actual x86 GameRuntime and original event constructors with controlled world/Win32/virtual queries. Not live netplay."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,subprocess,sys,tempfile,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture_only__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
root=Path(__file__).resolve().parents[2]
tmp=tempfile.TemporaryDirectory();cpp=Path(tmp.name)/'calc.cpp';host=Path(tmp.name)/'calc'
cpp.write_text('#include "'+str(root/'assist/source/unload_compute.h')+'"\n#include "'+str(root/'assist/source/missile_compute.h')+'"\n#include <cstdio>\n#include <memory>\ntemplate<class I,class F>int calc(F f){auto i=std::make_unique<I>();if(fread(i.get(),sizeof(I),1,stdin)!=1)return 2;auto r=f(*i);fwrite(&r,sizeof(r),1,stdout);return 0;}int main(int c,char**){return c>1?calc<assist::missilecompute::Input>(assist::missilecompute::calculate):calc<assist::unloadcompute::Input>(assist::unloadcompute::calculate);}')
subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(host)],check=True)
def compute(m,which):
 s=n.state(m)
 if getattr(s,which+'Task')==getattr(s,which+'Computed'):return
 result=subprocess.run([str(host)]+(['aa'] if which=='missile' else []),input=bytes(getattr(s,which+'Input')),stdout=subprocess.PIPE,check=True).stdout
 typ=m.MissileResult if which=='missile' else m.UnloadResult
 assert len(result)==C.sizeof(typ)
 setattr(s,which+'Result',typ.from_buffer_copy(result));setattr(s,which+'Computed',getattr(s,which+'Task'));setattr(s,which+'ResultSeq',getattr(s,which+'ResultSeq')+2);n.save(m,s)
def tick(m,inside=True,which='unload'):
 if not inside:compute(m,which)
 return n.tick(m)
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def unload(mode,inside):
 m=n.fixture(mode,1024|16384);s=n.state(m);s.computeApplied=1984 if inside else 192;n.save(m,s)
 m.vector(0xa8ecb8,[m.unit]);m.vector(0xa83de8,[m.eng]);m.w(m.unit+0x114,1);m.w(m.eng+0x11c,m.unit);m.b(m.eng+0x81,1);m.b(m.eng+0x90,0);m.w(m.eng+0xac,7);m.w(m.eng+0x5a4,m.unit)
 m.w(m.enemy+0x34,m.alloc());m.w(m.building+0x21c,m.enemy);m.action_overrides[(m.eng,m.building)]=9
 s=n.state(m);s.command=17;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s);s=m.hook();assert n.count(m)==1 and s.ack==s.request
 m.w(m.eng+0x11c,0);m.b(m.eng+0x81,0);m.b(m.eng+0x90,1);m.w(m.eng+0xac,2);m.w(m.eng+0x5a4,0)
 return m
def unload_case(mode,inside,case):
 m=unload(mode,inside)
 # Includes a scatter longer than the former 180-frame cutoff.
 for i in range(185 if case=='long_scatter' else 6):m.w(m.eng+0x9c,m.r(m.eng+0x9c)+8);tick(m,inside);assert n.count(m)==1
 m.w(m.eng+0xac,5)
 # Guard can precede final movement: don't fire Capture during sliding.
 for i in range(4):m.w(m.eng+0x9c,m.r(m.eng+0x9c)+4);tick(m,inside);assert n.count(m)==1
 if case=='manual_stop':m.w(m.eng+0xac,13)
 if case=='disabled':s=n.state(m);s.flags=0;n.save(m,s)
 for i in range(6):tick(m,inside)
 if case in ('manual_stop','disabled'):assert n.count(m)==1;return
 assert n.count(m)==2,(case,n.count(m),n.state(m).data.unloadPending)
 packet=n.packets(m)[111:222];assert packet[12]==8 and packet[19:24]==struct.pack('<IB',12,0x34)
 # Target acquisition must NOT dispose of the follow-up job.
 m.w(m.eng+0xac,8);m.w(m.eng+0x5a4,m.building)
 for i in range(8):m.w(m.eng+0x9c,m.r(m.eng+0x9c)+4);s=tick(m,inside);assert n.count(m)==2 and s.data.unloadPending==1
 if case=='interrupted':
  m.w(m.eng+0xac,2);m.w(m.eng+0x5a4,0)
  for i in range(4):m.w(m.eng+0x9c,m.r(m.eng+0x9c)+4);tick(m,inside)
  m.w(m.eng+0xac,5)
  for i in range(40):tick(m,inside)
  assert n.count(m)==3,('retry',n.count(m),n.state(m).data.unloadPending)
 elif case=='manual_after':
  m.w(m.eng+0xac,13)
  for i in range(40):tick(m,inside)
  assert n.count(m)==2 and n.state(m).data.unloadPending==0
 else:
  m.w(m.building+0x21c,m.house);s=tick(m,inside);assert s.data.unloadPending==0
 assert not m.codeWrites

def aa(mode,inside,case):
 m=n.fixture(mode);m.w(m.enemy+0x34,m.alloc());missileT=m.alloc();m.b(missileT+0xd68,1)
 missiles=[m.obj(0x7e22a4,missileT,m.enemy,40+i,hp=35) for i in range(2)]
 guns=[m.unit,m.obj(0x7f5c70,m.t,m.house,20)]
 if case in ('power_off','antitheft'):
  guns=[m.obj(0x7e3ebc,m.buildT,m.house,10),m.obj(0x7e3ebc,m.buildT,m.house,20)];m.w(m.buildT+0x898,m.weapon)
  m.u.mem_write(m.buildT+0x24,b'YAGGUN\0')
  address=m.r(0x7e3ebc+0x350);m.u.hook_add(UC_HOOK_CODE,lambda u,at,size,_:m.ret(int(case!='power_off'),0),begin=address,end=address)
 for j,q in enumerate(missiles):m.w(q+0x9c,1000+j*200);m.w(q+0xa0,1000);m.w(q+0xa4,200);m.w(q+0x2b4,m.building)
 for p in guns:m.w(p+0x9c,1000);m.w(p+0xa0,1000);m.w(p+0xa4,0)
 projectile=m.alloc();wh=m.alloc();m.w(m.weapon+0xb4,1500);m.w(m.weapon+0xa4,40);m.w(m.weapon+0xa0,projectile);m.w(m.weapon+0xac,wh);m.b(projectile+0x2a4,1);m.u.mem_write(wh+0xa0,struct.pack('<11d',*([1.0]*11)))
 info=m.alloc();m.w(info,m.weapon);queries=[]
 def select(u,at,size,_):m.ret(0,1)
 def weapon(u,at,size,_):m.ret(info,1)
 def in_range(u,at,size,_):
  source=u.reg_read(UC_X86_REG_ECX);q=m.r(u.reg_read(UC_X86_REG_ESP)+4);queries.append((source,q,m.r(q+0xa4)))
  m.ret(int(m.r(q+0xa4)<1000),2)
 for off,fn in ((0x2e4,select),(0x3f8,weapon),(0x3a8,in_range)):
  address=m.r(m.r(guns[0])+off);m.u.hook_add(UC_HOOK_CODE,fn,begin=address,end=address)
 extra=[]
 if case=='antitheft':
  threat=m.obj(0x7eb058,m.engT,m.enemy,60);extra=[threat]
  for p in guns+[m.building]:m.w(p+0x2b4,threat);m.w(p+0xac,1)
  m.vector(0xa8eb40,guns)
  s=n.state(m);s.flags=1073741824;n.save(m,s)
 m.vector(0xa8ec78,guns+missiles+[m.building]+extra);s=n.state(m);s.computeApplied=1984 if inside else 960;s.missileEnabled=1;s.missileScanHz=10;n.save(m,s)
 if case=='height':
  for q in missiles:m.w(q+0xa4,2000)
 if case=='nonmissile':m.b(missileT+0xd68,0)
 if case=='allied':m.w(m.house+0x5788,3)
 if case=='moving':
  for p in guns:m.w(p+0xac,2)
 if case=='disabled':s=n.state(m);s.missileEnabled=0;n.save(m,s)
 if case=='filter':s=n.state(m);s.featureConfig.filters[15].custom=1;s.featureConfigSeq+=2;n.save(m,s)
 if case=='observer':m.b(m.house+0x1f5,1)
 # Only object observation and normal events; compare all relevant gameplay memory.
 before=[bytes(m.u.mem_read(p,0x800)) for p in guns+missiles]
 s=tick(m,inside,'missile')
 if not inside and case in ('stale','dead','range_changed','torn','manual','off_pending'):
  compute(m,'missile');s=n.state(m)
  if case=='stale':m.clock+=151
  if case=='dead':
   for q in missiles:m.w(q+0x6c,0)
  if case=='range_changed':
   for q in missiles:m.w(q+0xa4,2000)
  if case=='torn':s.missileResultSeq|=1;n.save(m,s)
  if case=='manual':
   for p in guns:m.w(p+0xac,2)
  if case=='off_pending':s.missileEnabled=0;n.save(m,s)
  n.tick(m);assert n.count(m)==0,(case,n.count(m));return
 if not inside:s=tick(m,inside,'missile')
 expected=case not in ('height','nonmissile','allied','moving','disabled','filter','observer','power_off','antitheft')
 assert n.count(m)==(2 if expected or case=='height' else 0),(case,inside,n.count(m),s.missileSources,s.missileTargets,s.missileInput.count)
 if expected:
  packets=n.packets(m);targets=set()
  for i in range(2):packet=packets[i*111:(i+1)*111];assert packet[12]==1 and packet[18]==0x34;targets.add(struct.unpack_from('<I',packet,14)[0])
  assert targets=={40,41},targets
  # Once the game observes each assignment, keep both locks instead of spam.
  for i in range(2):packet=packets[i*111:(i+1)*111];ident=struct.unpack_from('<I',packet,7)[0];target=struct.unpack_from('<I',packet,14)[0];p=next(p for p in guns if m.r(p+0x10)==ident);q=next(q for q in missiles if m.r(q+0x10)==target);m.w(p+0x2b4,q);m.w(p+0xac,1)
  for _ in range(8):tick(m,inside,'missile')
  assert n.count(m)==2
 elif case=='height':
  assert queries and all(x[2]==2000 for x in queries)
  assert all(n.packets(m)[j*111+12]==0 for j in range(2)), 'approaching high targets may reserve Sleep; never Attack outside native range'
 if not expected:assert before==[bytes(m.u.mem_read(p,0x800)) for p in guns+missiles]
 assert not m.codeWrites
for mode in (0,3):
 for inside in (True,False):
  for case in ('long_scatter','interrupted','manual_stop','manual_after','disabled'):check(f'unload_{mode}_{inside}_{case}',lambda m=mode,i=inside,c=case:unload_case(m,i,c))
  for case in ('distribute','height','nonmissile','allied','moving','disabled','filter','observer','power_off','antitheft'):check(f'missile_{mode}_{inside}_{case}',lambda m=mode,i=inside,c=case:aa(m,i,c))
 for case in ('stale','dead','range_changed','torn','manual','off_pending'):check(f'missile_external_{mode}_{case}',lambda m=mode,c=case:aa(m,False,c))
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
