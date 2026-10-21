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
 # Infantry building-entry cases, including the removed C4 submission gates,
 # are covered by infantry_entry_runtime.py (pre.9); no arrival patch installed.
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
