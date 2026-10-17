"""Reproduce original sidebar cleanup. No DLL correctness claim.
Actual EXE cleanup loop/constructors; synthetic types, CanBuild, factory queries.
Proves cancellation is generated before removal, not full economy simulation.
"""
import argparse,importlib.util,struct,json,hashlib
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',default='technology-cleanup.json');a=p.parse_args();results=[]
for name,legal,link,queued,present in [('active_and_queued',False,True,True,True),('active_only',False,True,False,True),('queued_without_icon_link',False,False,True,True),('idle_removed',False,False,False,True),('legal_retained',True,True,True,True),('external_only_no_cameo',False,True,True,False)]:
 spec=importlib.util.spec_from_file_location('cleanup_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,3);m.hook();m.canbuild_full_result=int(legal)
 vt=m.alloc(0x400);m.w(m.t,vt);m.w(vt+0x94,0x5f7900)
 kind=m.STUB+0xe300;m.w(vt+0x2c,kind)
 m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(40,0),begin=kind,end=kind)
 m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:m.ret(int(queued),3),begin=0x500510,end=0x500510)
 strip=0x87f7e8+0x1544+3*3988;cameo=strip+88
 m.w(strip+84,int(present));m.w(cameo,0);m.w(cameo+4,40);m.w(cameo+12,m.alloc() if link else 0)
 sp=m.STACK+0x1e000;m.w(sp+0x30,0)
 m.u.reg_write(UC_X86_REG_EBP,strip);m.u.reg_write(UC_X86_REG_EDI,0);m.u.reg_write(UC_X86_REG_ESP,sp)
 m.u.hook_add(UC_HOOK_CODE,lambda uc,at,size,_:uc.reg_write(UC_X86_REG_EIP,m.STOP),begin=0x6aaab3,end=0x6aaab3)
 m.u.emu_start(0x6aa711,m.STOP,count=2000000);assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP
 events=[]
 for i in range(m.r(0xa802c8)):
  raw=bytes(m.u.mem_read(0xa802d4+111*i,111));events.append({'kind':raw[0],'house':raw[2],'type':list(struct.unpack_from('<iii',raw,7))})
 expected=([16] if link else [])+([46] if queued else []) if present and not legal else []
 assert [e['kind'] for e in events]==expected,(name,events,expected)
 assert m.r(strip+84)==int(present and legal)
 assert all(e['house']==0 and e['type']==[40,0,0] for e in events)
 results.append({'name':name,'status':'PASS','native_cancellation_events':events,'cameos_remaining':m.r(strip+84)})
 print('PASS',name,'native events:',expected,flush=True)
 m.u=None
Path(a.output).write_text(json.dumps({'inputs':{'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()},'scope':__doc__,'results':results},indent=2)+'\n')
