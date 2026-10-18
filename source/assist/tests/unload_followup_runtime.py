"""Real x86 Runtime + native event constructors; synthetic world and virtual legality query. Not live gameplay/netplay."""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture_only__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
for mode in (0,3):
 for case in ('nearest','damage_action','neutral','virtual_override','guard_sticky','exit_transition','long_scatter','blocked','manual_stop','disabled'):
  try:
   m=n.fixture(mode,1024|16384);m.vector(0xa8ecb8,[m.unit]);m.vector(0xa83de8,[m.eng]);m.w(m.unit+0x114,1);m.w(m.unit+0x118,m.eng)
   m.w(m.eng+0x11c,m.unit);m.b(m.eng+0x81,1);m.b(m.eng+0x90,0);m.w(m.eng+0xac,7);m.w(m.eng+0x5a4,m.unit)
   country=m.alloc();m.w(m.enemy+0x34,country);m.w(m.building+0x21c,m.enemy);m.b(country+0x1a6,case=='neutral')
   far=m.alloc();m.u.mem_write(far,bytes(m.u.mem_read(m.building,0x2000)));m.w(far+0x10,99)
   m.w(m.eng+0x9c,5000);m.w(m.eng+0xa0,5000);m.w(m.building+0x9c,5400);m.w(m.building+0xa0,5000);m.w(far+0x9c,9000);m.w(far+0xa0,5000)
   m.vector(0xa8eb40,[far,m.building]);m.vector(0xa8ec78,[m.unit,m.eng,m.building,far])
   query=m.alloc(16);m.u.mem_write(query,b'\xc2\x08\x00');m.w(0x7eb058+0x74,query);queries=[]
   def legal(u,at,size,_):
    source=u.reg_read(UC_X86_REG_ECX);target=m.r(u.reg_read(UC_X86_REG_ESP)+4);queries.append((source,target))
    m.ret(31 if case=='blocked' else 28 if case=='damage_action' else 9,2)
   m.u.hook_add(UC_HOOK_CODE,legal,begin=query,end=query)
   # Vanilla fixed entry deliberately rejects: the actual virtual override
   # must be called, matching mod/launcher dispatch semantics.
   m.action_overrides[(m.eng,m.building)]=31;m.action_overrides[(m.eng,far)]=31
   s=n.state(m);s.command=17;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s);s=m.hook();assert s.ack==s.request and n.count(m)==1,(case,s.result,n.count(m))
   s=n.tick(m);assert s.data.unloadPending==1
   m.w(m.eng+0x11c,0);m.b(m.eng+0x81,0);m.b(m.eng+0x90,1);m.w(m.eng+0x5a4,0)
   m.w(m.eng+0xac,7 if case=='exit_transition' else 2)
   if case=='exit_transition':s=n.tick(m);assert s.data.unloadPending==1
   m.w(m.eng+0xac,2)
   for _ in range(120 if case=='long_scatter' else 10):s=n.tick(m);assert n.count(m)==1
   if case=='disabled':s.flags=0;n.save(m,s)
   m.w(m.eng+0xac,13 if case=='manual_stop' else 6 if case=='guard_sticky' else 5);s=n.tick(m)
   expected=case not in ('blocked','manual_stop','disabled')
   assert n.count(m)==(2 if expected else 1),(case,n.count(m),s.data.unloadPending,s.data.lastSubmitError)
   if expected:
    packet=n.packets(m)[111:222];assert packet[12]==8 and packet[19:24]==struct.pack('<IB',12,0x34),packet[:24].hex()
    assert queries and queries[-1][0]==m.eng
    for _ in range(5):n.tick(m)
    assert n.count(m)==2
   assert not m.codeWrites
   rows.append({'case':f'{mode}_{case}','status':'PASS'});print('PASS',mode,case,flush=True)
  except Exception as e:rows.append({'case':f'{mode}_{case}','status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
