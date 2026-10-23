"""Actual Runtime, native weapon selection/GetWeapon/range and event constructors. Controlled object getters, map and input; not live game/netplay."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,subprocess,sys,tempfile,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec);args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helper.json'))];spec.loader.exec_module(n);sys.argv=args
root=Path(__file__).resolve().parents[2];tmp=tempfile.TemporaryDirectory();cpp=Path(tmp.name)/'host.cpp';host=Path(tmp.name)/'host'
cpp.write_text('#include "'+str(root/'assist/source/external_planner.h')+'"\n#include <cstdio>\n#include <memory>\nint main(){using namespace assist;using namespace planner;Planner p;auto w=std::make_unique<World>();std::vector<Order> old;unsigned ack[3];while(fread(ack,sizeof ack,1,stdin)==1&&fread(w.get(),sizeof(World),1,stdin)==1){for(unsigned i=0;i<old.size();++i){if(ack[1]&(1u<<i))p.accepted(old[i],ack[0]);else if(ack[2]&(1u<<i))p.rejected(old[i],ack[0]);}old=p.step(*w,AdvancedInput,200,300,90,10,0,2);int count=old.size();fwrite(&count,4,1,stdout);fwrite(old.data(),sizeof(Order),count,stdout);fflush(stdout);}}')
subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(host)],check=True)
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def setup(mode,inside,case):
 m=n.fixture(mode,8192|1024);s=n.state(m);s.computeApplied=1985 if inside else 1984;s.advancedScanHz=10;n.save(m,s)
 m.w(m.enemy+0x34,m.alloc());m.b(m.unit+0x14,7);m.w(m.unit+0x9c,10000);m.w(m.unit+0xa0,10000);m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell)
 air=case in ('flak_air','gatt_air','air_bonus','air_high','negative_bonus')
 targetT=m.alloc();q=m.obj(0x7e22a4 if air else 0x7f5c70,targetT,m.enemy,40);m.b(q+0x14,7);m.b(q+0x134,air);m.w(q+0x9c,10000+7*256);m.w(q+0xa0,10000);m.w(q+0xa4,2*256 if air else 0)
 def weapon(slot,range_,aa=True,ag=True,elite=False):
  w=m.alloc();proj=m.alloc();wh=m.alloc();m.w(w+0xb4,int(range_*256));m.w(w+0xa4,40);m.w(w+0xa0,proj);m.w(w+0xac,wh);m.b(proj+0x2a4,aa);m.b(proj+0x2a5,ag);m.u.mem_write(wh+0xa0,struct.pack('<11d',*([1.0]*11)));m.w(m.t+(0xa94 if elite else 0x898)+slot*28,w);return w
 weapon(0,6);actual=weapon(4,8);expected=4;m.w(m.t+0x808,15);m.w(m.t+0x80c,15);m.w(m.unit+0x138,4)
 if case=='ifv_elite':actual=weapon(4,8,elite=True);weapon(4,6);m.u.mem_write(m.unit+0x150,struct.pack('<f',2.0))
 if case in ('flak_air','gatt_air'):
  m.w(m.t+0x808,0);weapon(0,5,False);actual=weapon(1,8,True,False);expected=1
  if case=='gatt_air':m.b(m.t+0xcd5,1);m.w(m.unit+0x140,2);weapon(4,6,False);actual=weapon(5,10,True,False);expected=5
 if case in ('air_bonus','air_high'):actual=weapon(4,6);m.w(m.t+0x68c,4*256);m.w(q+0x9c,10000+8*256);m.w(q+0xa4,(8 if case=='air_high' else 3)*256)
 if case=='negative_bonus':m.w(m.t+0x68c,-3*256);m.w(q+0x9c,10000+6*256);m.w(q+0xa4,0)
 if case=='minimum':m.w(actual+0xb8,4*256);m.w(q+0x9c,10000+2*256);m.w(m.unit+0xac,5);m.w(m.unit+0x5a4,0)
 if case=='target_enters' or case.startswith('ifv_out_'):m.w(q+0x9c,10000+9*256)
 if case.startswith('ifv_out_'):
  slot=int(case.rsplit('_',1)[1]);actual=weapon(slot,8);expected=slot;m.w(m.unit+0x138,slot);m.w(m.unit+0xac,1);m.w(m.unit+0x2b4,q);m.w(m.unit+0x5a4,0)
 others=[]
 if case=='close_priority':
  far=m.obj(0x7f5c70,targetT,m.enemy,41);m.b(far+0x14,7);m.w(far+0x9c,10000+9*256);m.w(far+0xa0,10000);m.w(targetT+0x610,9000);otherT=m.alloc();m.b(otherT+0x231,1);m.b(otherT+0xd3a,1);m.b(otherT+0xd38,1);m.w(otherT+0x610,100);m.w(q+0x6c4,otherT);others=[far]
 for obj in [q]+others:
  x,y=m.r(obj+0x9c)//256,m.r(obj+0xa0)//256;cell=m.alloc();m.u.mem_write(cell+0x24,struct.pack('<hh',x,y));m.w(m.cells+(y*512+x)*4,cell)
 m.vector(0xa8ec78,[m.unit,q]+others);m.vector(0xa8ecb8,[m.unit]);m.target=q;m.actual=actual;m.expectedSlot=expected
 hooks=set()
 def add(at,fn):
  if at in hooks:return
  hooks.add(at);m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,_:fn(),begin=at,end=at)
 def self():return m.u.reg_read(UC_X86_REG_ECX)
 def arg(i=0):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 def coords():out=arg();m.u.mem_write(out,bytes(m.u.mem_read(self()+0x9c,12)));m.ret(out,1)
 for table in (0x7f5c70,0x7e22a4):
  add(m.r(table+0x48),coords);add(m.r(table+0x54),lambda:m.ret(int(m.r(self()+0x134)&255!=0),0));add(m.r(table+0x50),lambda:m.ret(0,0));add(m.r(table+0x84),lambda:m.ret(m.r(self()+0x6c4),0));add(m.r(table+0x2c),lambda:m.ret(2 if m.r(self())==0x7e22a4 else 1,0));add(m.r(table+0x400),lambda:m.ret(0,0));add(m.r(table+0x1bc),lambda:m.ret(m.crateCell,0))
 add(0x565730,lambda:m.ret(m.crateCell,1));add(0x4cc310,lambda:m.ret(0,2));m.w(m.crateCell+0xec,0)
 m.hits={k:0 for k in ('select','weapon','range')}
 for at,k in [(0x746cd0,'select'),(0x70e140,'weapon'),(0x6f7220,'range')]:
  def hit(uc,addr,size,_,key=k):m.hits[key]+=1
  m.u.hook_add(UC_HOOK_CODE,hit,begin=at,end=at)
 return m

def external(m,h,previous=0):
 s=n.state(m);w=s.world
 h.stdin.write(struct.pack('<III',s.planFrame,s.planAcceptedMask if previous else 0,s.planRejectedMask if previous else 0)+bytes(w));h.stdin.flush();count=struct.unpack('<i',h.stdout.read(4))[0];payload=h.stdout.read(count*C.sizeof(m.Order));assert len(payload)==count*C.sizeof(m.Order)
 s.planCount=count;s.planTick=w.tick;s.planFrame=w.frame;s.planMatch=w.match;s.planRequest+=1
 for i in range(count):s.plan[i]=m.Order.from_buffer_copy(payload[i*C.sizeof(m.Order):(i+1)*C.sizeof(m.Order)])
 n.save(m,s);return count

def case(mode,inside,name):
 m=setup(mode,inside,name);before=bytes(m.u.mem_read(m.unit,0x800));s=n.pointer(m,2,1)
 assert s.pointer.accepted==1,(name,s.pointer.result)
 if not inside:
  assert n.count(m)==0,'advanced intent must not append native AttackMove before target sampling'
  h=subprocess.Popen([str(host)],stdin=subprocess.PIPE,stdout=subprocess.PIPE)
  try:
   external(m,h)
   if name=='weapon_changed':m.w(m.unit+0x138,0)
   if name=='stale':m.clock+=151
   if name=='target_moved':m.w(m.target+0x9c,10000+20*256)
   if name=='target_enters':m.w(m.target+0x9c,10000+7*256)
   if name=='disabled':s=n.state(m);s.flags=0;n.save(m,s)
   s=n.tick(m)
   if name=='target_enters':
    assert n.count(m)==0 and s.planAck==s.planRequest and s.planRejectedMask==1,'superseded Move must settle immediately without blocking the mailbox'
    assert external(m,h,1)==1;s=n.tick(m)
  finally:h.terminate();h.wait()
 if name in ('weapon_changed','stale','target_moved','disabled'):
  assert n.count(m)==0,(name,n.count(m));return
 assert n.count(m)==1,(name,n.count(m),s.world.combatCount,s.world.actors[0].combatFirst,s.data.lastSubmitError)
 packet=n.packets(m)[:111];expect_attack=name not in ('air_high','negative_bonus') and not name.startswith('ifv_out_')
 assert packet[12]==(1 if expect_attack else 2),(name,packet[12],[(x.target,x.flags,x.slot) for x in s.world.combat[:s.world.combatCount]])
 if expect_attack:
  assert packet[14:19]==struct.pack('<IB',40,0x34) and packet[19:24]==bytes(5)
  probe=next(x for x in s.world.combat[:s.world.combatCount] if x.target==40);assert probe.slot==m.expectedSlot and probe.flags&(8 if name=='minimum' else 1),(name,probe.slot)
 else:assert packet[14:19]==bytes(5)
 assert all(m.hits.values()),m.hits
 assert before==bytes(m.u.mem_read(m.unit,0x800)) and not m.codeWrites,'sender must not mutate simulation objects/code'
 # Execute the native event receiver's actual SetTarget / SetDestination block.
 if expect_attack:
  targetCalls=[];destCalls=[];packetAt=m.alloc();m.u.mem_write(packetAt,packet)
  def resolve(uc,at,size,_):
   ref=m.u.reg_read(UC_X86_REG_ECX);m.ret(m.target if m.r(ref)==40 and m.r(ref+4)&255==0x34 else 0,0)
  def setter(uc,at,size,_):
   sp=m.u.reg_read(UC_X86_REG_ESP);v=m.r(sp+4)
   if at==m.r(0x7f5c70+0x3c8):targetCalls.append(v);m.ret(0,1)
   else:destCalls.append(v);m.ret(0,2)
  m.u.hook_add(UC_HOOK_CODE,resolve,begin=0x6e6e20,end=0x6e6e20)
  for at in (m.r(0x7f5c70+0x3c8),m.r(0x7f5c70+0x480)):m.u.hook_add(UC_HOOK_CODE,setter,begin=at,end=at)
  m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(0,0),begin=0x4c7482,end=0x4c7482)
  m.u.reg_write(UC_X86_REG_ESI,packetAt);m.u.reg_write(UC_X86_REG_EDI,m.unit);m.run(0x4c7458)
  assert targetCalls==[m.target] and destCalls==[0],(targetCalls,destCalls)
for mode in (0,3):
 for inside in (True,False):
  for name in ('ifv_7','ifv_elite','flak_air','gatt_air','air_bonus','air_high','negative_bonus','minimum','close_priority','ifv_out_0','ifv_out_1','ifv_out_4','ifv_out_7','ifv_out_14'):check(f'{mode}_{inside}_{name}',lambda m=mode,i=inside,nm=name:case(m,i,nm))
 for name in ('weapon_changed','stale','target_moved','disabled','target_enters'):check(f'{mode}_external_{name}',lambda m=mode,nm=name:case(m,False,nm))
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n');raise SystemExit(any(x['status']!='PASS' for x in rows))
