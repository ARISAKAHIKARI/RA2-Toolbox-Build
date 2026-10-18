"""Actual Runtime install/callback/cleanup with mocked Win32/world/mask blitters."""
import argparse,ctypes as C,importlib.util,json,hashlib,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',default='RENDER_RUNTIME_VERIFICATION.json');a=p.parse_args()
def fixture():
 spec=importlib.util.spec_from_file_location('render_fixture',Path(__file__).with_name('entry_runtime_regression.py'))
 m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 s=m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)));s.flags=0;s.externalDisplay=1;m.u.mem_write(m.SH,bytes(s));m.hook()
 return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
sites={0x47EFE0:bytes.fromhex('55 8B EC 83 EC 34'),0x47F250:bytes.fromhex('A0 C6 E7 89 00')}
results=[]
for scenario in ('enabled','off_before_tick','stale_host','wrong_thread','stopped_host','foreign_before_install','foreign_after_install'):
 m=fixture();assert state(m).data.renderRevealStatus==0
 if scenario=='foreign_before_install':m.u.mem_write(0x47F250,b'\x90')
 s=state(m);s.renderReveal=1;save(m,s);s=m.hook()
 if scenario=='foreign_before_install':
  assert s.data.renderRevealStatus==-2
  assert bytes(m.u.mem_read(0x47EFE0,6))==sites[0x47EFE0]
  assert bytes(m.u.mem_read(0x47F250,1))==b'\x90'
 else:
  assert s.data.renderRevealStatus==1,(scenario,s.data.renderRevealStatus)
  assert all(m.u.mem_read(at,1)==b'\xe9' for at in sites)
  if scenario=='foreign_after_install':
   m.u.mem_write(0x47F250,b'\x90');s=m.hook()
   assert s.data.renderRevealStatus==-2 and m.u.mem_read(0x47F250,1)==b'\x90'
   assert bytes(m.u.mem_read(0x47EFE0,6))==sites[0x47EFE0]
  else:
   calls=[]
   def original(uc,at,size,_):
    calls.append(at)
    if at==0x47EFE6:
     ebp=m.u.reg_read(UC_X86_REG_EBP);m.u.reg_write(UC_X86_REG_EBP,m.r(ebp));m.u.reg_write(UC_X86_REG_ESP,ebp+4)
    m.ret(0,3)
   for at in (0x47EFE6,0x47F255):m.u.hook_add(UC_HOOK_CODE,original,begin=at,end=at)
   s=state(m)
   if scenario=='off_before_tick':s.renderReveal=0
   if scenario=='stale_host':s.heartbeat=m.clock-4000
   if scenario=='stopped_host':s.stop=1
   save(m,s)
   if scenario=='wrong_thread':m.current_thread=778
   before=(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)),m.r(0xA802C8),m.r(0x8B41F8))
   for at in sites:m.run(at,(0,0,15),self=m.crateCell)
   assert calls==([] if scenario=='enabled' else [0x47EFE6,0x47F255]),(scenario,calls)
   after=(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)),m.r(0xA802C8),m.r(0x8B41F8))
   assert before==after and not any(c[0]==0x577D90 for c in m.calls)
   m.current_thread=777;s=state(m);s.renderReveal=0;s.stop=0;save(m,s);s=m.hook()
   assert s.data.renderRevealStatus==0
   assert all(bytes(m.u.mem_read(at,len(sig)))==sig for at,sig in sites.items())
 results.append({'name':scenario,'pass':True});print('PASS',scenario,flush=True)
Path(a.output).write_text(json.dumps({'version':'1.0.6','dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),
 'results':results,'real_windows_or_rendering_or_netplay':False},indent=2)+'\n')
