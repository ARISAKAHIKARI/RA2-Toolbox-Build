"""Actual DLL/manager/stubs/native constructors with synthetic world; not game or multiplayer simulation."""
import argparse,ctypes as C,importlib.util,json,struct,sys,traceback,hashlib
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
# Reuse the read-only fixture helpers without running its unrelated cases.
spec=importlib.util.spec_from_file_location('support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture_only__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
def check(name,fn):
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def issue(m,c,arg=0):
 s=n.state(m);s.command=c;s.arg=arg;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s);s=m.hook();assert s.ack==s.request;return s
for mode in (0,3,4,5):
 for case in ('allowed','disabled','no_factory','limit','building','cancel'):
  def tech(mode=mode,case=case):
   m=n.fixture(mode,4096 if case!='disabled' else 0);s=n.state(m);slot=next(i for i in range(s.data.typeCount) if s.data.types[i].kind==(7 if case=='building' else 40))
   vt=m.alloc(0x100);m.w(m.t,vt);m.w(vt+0x94,0x5f7900);s.techAllowed[slot]=1;n.save(m,s);issue(m,10,1);m.canbuild_full_result=0;m.canbuild_limit_result=case!='limit';m.factory_available=case!='no_factory'
   before=bytes(m.u.mem_read(m.t,0x2000));s=issue(m,22 if case=='cancel' else 12,slot);expected=case in ('allowed','cancel')
   assert n.count(m)==int(expected),(case,s.result,n.count(m));assert bytes(m.u.mem_read(m.t,0x2000))==before and not m.sidebar_add_calls and not m.codeWrites
   if expected:
    packet=n.packets(m);assert packet[0]==(0x10 if case=='cancel' else 0xe);assert packet[7:19]==struct.pack('<iii',40,0,0)
    if case=='allowed':assert m.canbuild_calls[-1][2:]==(1,1)
    for _ in range(3):n.tick(m)
    assert n.count(m)==1
  check(f'tech_{mode}_{case}',tech)
 for case in ('off','on','conflict','restore'):
  def c4(mode=mode,case=case):
   m=n.fixture(mode,0);site=0x51a4d8;original=bytes(m.u.mem_read(site,6))
   if case=='conflict':m.u.mem_write(site,b'\x90'*6)
   s=n.state(m);s.c4Test=case!='off';n.save(m,s);s=n.tick(m)
   if case=='on':assert s.data.c4TestStatus==1 and bytes(m.u.mem_read(site,1))==b'\xe9'
   if case=='off':assert bytes(m.u.mem_read(site,6))==original and not m.codeWrites
   if case=='conflict':assert s.data.c4TestStatus==-2 and bytes(m.u.mem_read(site,6))==b'\x90'*6
   if case=='restore':
    assert s.data.c4TestStatus==1;s.c4Test=0;n.save(m,s);s=n.tick(m);assert s.data.c4TestStatus==0 and bytes(m.u.mem_read(site,6))==original
   # Plain infantry without C4 capability; enemy building with CanC4.
   m.b(m.engT+0xec2,0);m.b(m.buildT+0x1577,1);m.w(m.building+0x21c,m.enemy);m.queryObject=m.building
   s=n.pointer(m,7,8192,lambda s,i:setattr(i.sources[0],'id',11) or setattr(i.sources[0],'pointer',m.eng))
   assert n.count(m)==int(case=='on'),(case,s.pointer.result,s.pointer.accepted,n.count(m))
   if case=='on':
    b=n.packets(m);assert b[12]==17 and b[14:19]==struct.pack('<IB',12,0x34) and b[19:24]==struct.pack('<IB',12,0x34),b[:30].hex()
   if case!='conflict':
    # Execute the actual installed callback, including manager Capture lease.
    reached=[]
    def stop(u,at,size,_):reached.append(at);u.reg_write(UC_X86_REG_EIP,m.STOP)
    for at in (0x51a4e6,0x51a7f4):m.u.hook_add(UC_HOOK_CODE,stop,begin=at,end=at)
    m.u.reg_write(UC_X86_REG_ESI,m.eng);m.u.reg_write(UC_X86_REG_ESP,m.STACK+0x1e000);m.u.emu_start(0x51a4d2,m.STOP,count=200000)
    assert reached==[0x51a4e6 if case=='on' else 0x51a7f4],(case,reached)
    assert m.u.mem_read(m.engT+0xec2,1)==b'\0' # type rule never mutated
  check(f'c4_{mode}_{case}',c4)
 for case in ('deployer','engineer','manual_stop','disabled','off_after','scatter_timeout'):
  def unload(mode=mode,case=case):
   m=n.fixture(mode,1024|(0 if case=='disabled' else 16384));m.vector(0xa8ecb8,[m.unit]);m.vector(0xa83de8,[m.eng]);m.w(m.unit+0x114,1);m.w(m.unit+0x118,m.eng)
   m.w(m.eng+0x11c,m.unit);m.b(m.eng+0x81,1);m.b(m.eng+0x90,0);m.w(m.eng+0xac,7);m.w(m.eng+0x5a4,m.unit)
   m.w(m.enemy+0x34,m.alloc());m.w(m.building+0x21c,m.enemy);m.action_overrides[(m.eng,m.building)]=9
   if case=='deployer':
    m.b(m.engT+0xec8,1)
    for off in (0x314,0x554):
     addr=m.r(0x7eb058+off);m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_,off=off:m.ret(int(off==0x314),0),begin=addr,end=addr)
   s=issue(m,17);assert n.count(m)==int(case!='disabled')
   if case=='disabled':return
   assert n.packets(m)[12]==16;s=n.tick(m);assert s.data.unloadPending==1
   m.w(m.eng+0x11c,0);m.b(m.eng+0x81,0);m.b(m.eng+0x90,1);m.w(m.eng+0xac,2)
   for _ in range(190 if case=='scatter_timeout' else 20):s=n.tick(m);assert n.count(m)==1 # no Sleep/Deploy interrupts exit walking
   if case=='off_after':s.flags=0;n.save(m,s)
   m.w(m.eng+0xac,13 if case=='manual_stop' else 5);s=n.tick(m)
   expected=case in ('deployer','engineer');assert n.count(m)==(2 if expected else 1),(case,s.data.unloadPending,n.count(m))
   if expected:assert n.packets(m)[111+12]==(16 if case=='deployer' else 8)
   assert not m.codeWrites
  check(f'smart_unload_{mode}_{case}',unload)
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(row['status']=='FAIL' for row in rows))
