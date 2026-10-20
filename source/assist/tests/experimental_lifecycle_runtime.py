"""Actual Runtime SP five-site / MP three-site lifecycle and shared capture guard.
Synthetic Win32/world; native planning and graphics are not executed here.
"""
import argparse,ctypes as C,importlib.util,json,hashlib,struct
from pathlib import Path
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
sites={0x4A8EB0:bytes.fromhex('A1 4C 3D A8 00'),0x7E2908:bytes.fromhex('50 CB 41 00'),0x416AB6:bytes.fromhex('C7 86 BC 00 00 00 03 00 00 00'),0x47F038:bytes.fromhex('8B 45 10 53 56'),0x47F274:bytes.fromhex('8B 44 24 38 53')}
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode):
 spec=importlib.util.spec_from_file_location('risk_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);m.w(0xa8ed84,0);s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);m.hook()
 blob=m.dll.get_memory_mapped_image();pattern=bytes.fromhex('6A 00 6A 00 6A 03 E8');offset=blob.index(pattern)+len(pattern)-1
 m.guard=m.dll.OPTIONAL_HEADER.ImageBase+offset+5+struct.unpack_from('<i',blob,offset+1)[0]
 return m
def guarded(m,op):
 sp=m.STACK+0x1e000;m.u.mem_write(sp,struct.pack('<4I',m.STOP,op,0,0));m.u.reg_write(UC_X86_REG_ESP,sp)
 m.u.emu_start(m.guard,m.STOP,count=200000)
 assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP and m.u.reg_read(UC_X86_REG_ESP)==sp+4 # cdecl, caller owns args
 return m.u.reg_read(UC_X86_REG_EAX)
def enable(m):s=state(m);s.flags=0x80010000;s.renderReveal=1;save(m,s);return m.hook()
def restored(m,addresses):return all(bytes(m.u.mem_read(at,len(sites[at])))==sites[at] for at in addresses)
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
for mode in (0,5,3,4):
 m=fixture(mode);s=enable(m);air=mode in (0,5);total=5 if air else 3;assert (s.data.placementStatus,s.data.nativeAirStatus,s.data.renderRevealStatus)==(1,1 if air else 2,1)
 assert s.data.hookInstalled==total and not m.r(0xa802c8) and not m.r(0x8b41f8)
 assert guarded(m,1)==int(air) and guarded(m,3)==1
 for boundary,address,value in [('menu',0xa8eda0,1),('dialog',0xa8ed8c,1),('inactive',0xa8e378,0)]:
  if boundary=='inactive':m.b(address,value)
  else:m.w(address,value)
  assert guarded(m,1)==guarded(m,3)==0
  if boundary=='inactive':m.b(address,1)
  else:m.w(address,0)
  assert guarded(m,1)==int(air) and guarded(m,3)==1
 # Turns do not need a new host sample to stay valid; new rounds do.
 m.w(0xa8ed84,5);assert guarded(m,1)==int(air) and guarded(m,3)==1;m.hook();m.w(0xa8ed84,0);assert guarded(m,1)==guarded(m,3)==0
 s=m.hook();assert s.data.match==2 and s.data.hookInstalled==total
 for which in ('render','placement','air'):
  s=state(m)
  if which=='render':s.renderReveal=0
  elif which=='placement':s.flags &= ~65536
  else:s.flags &= ~0x80000000
  save(m,s);s=m.hook()
  assert s.data.hookInstalled=={'render':total-2,'placement':total-3,'air':0}[which]
 assert restored(m,sites)
 enable(m);s=state(m);s.stop=1;save(m,s);m.hook();assert restored(m,sites)
 passed(f'all_five_install_independent_disable_menu_round_stop_{mode}')
 for changed in (0x416AB6,0x47F274):
  m=fixture(mode);m.u.mem_write(changed,b'\x90');s=enable(m)
  assert m.u.mem_read(changed,1)==b'\x90' and s.data.placementStatus==1
  if changed==0x416AB6:assert s.data.nativeAirStatus==(-2 if air else 2) and s.data.renderRevealStatus==1 and restored(m,[0x7E2908]) and s.data.hookInstalled==3
  else:assert s.data.renderRevealStatus==-2 and s.data.nativeAirStatus==(1 if air else 2) and restored(m,[0x47F038]) and s.data.hookInstalled==total-2
  s=state(m);s.stop=1;save(m,s);m.hook();assert m.u.mem_read(changed,1)==b'\x90' and restored(m,[at for at in sites if at!=changed])
  passed(f'foreign_{changed:x}_group_rollback_other_features_preserved_{mode}')
Path(a.output).write_text(json.dumps({'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'scope':__doc__,'results':rows},indent=2)+'\n')
