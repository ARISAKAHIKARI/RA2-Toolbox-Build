"""Actual x86 Runtime render callbacks across play/menu/round boundaries.
Native mask blitters and dirty-area implementation are mocked; no real rendering.
"""
import argparse,ctypes as C,importlib.util,json,hashlib,traceback,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
sites={0x47F038:bytes.fromhex('8B 45 10 53 56'),0x47F274:bytes.fromhex('8B 44 24 38 53')}
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture():
 spec=importlib.util.spec_from_file_location('render_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);m.hook();m.original=[];m.redraw=[]
 def original(uc,at,size,_):
  m.original.append(at)
  if at==0x47F03D:
   ebp=m.u.reg_read(UC_X86_REG_EBP);m.u.reg_write(UC_X86_REG_EBP,m.r(ebp));m.u.reg_write(UC_X86_REG_ESP,ebp+4)
  else:m.u.reg_write(UC_X86_REG_ESP,m.u.reg_read(UC_X86_REG_ESP)+0x30)
  m.ret(0,3)
 def redraw(uc,at,size,_):
  sp=m.u.reg_read(UC_X86_REG_ESP);area=struct.unpack('<4i',m.u.mem_read(sp+4,16));assert area==(0,0,640,480);assert m.r(sp+20)==0
  assert m.u.reg_read(UC_X86_REG_ECX)==m.r(0x887324);m.redraw.append(area);m.ret(0,5)
 for at in (0x47F03D,0x47F279):m.u.hook_add(UC_HOOK_CODE,original,begin=at,end=at)
 m.u.hook_add(UC_HOOK_CODE,redraw,begin=0x6D2790,end=0x6D2790)
 m.u.mem_write(0x886FA0,struct.pack('<4i',0,0,640,480))
 s=state(m);s.renderReveal=1;save(m,s);s=m.hook();assert s.data.renderRevealStatus==1
 return m
for boundary in ('menu','dialog','tactical_inactive','observer','defeated','house','frame_back','frame_ahead','mode','run_flag','chat'):
 try:
  m=fixture();assert len(m.redraw)==1
  if boundary=='menu':m.w(0xa8eda0,1)
  elif boundary=='dialog':m.w(0xa8ed8c,1)
  elif boundary=='tactical_inactive':m.b(0xa8e378,0)
  elif boundary=='observer':m.w(0xac1198,m.house)
  elif boundary=='defeated':m.b(m.house+0x1f5,1)
  elif boundary=='house':m.w(0xa83d4c,m.enemy)
  elif boundary=='frame_back':m.w(0xa8ed84,0)
  elif boundary=='frame_ahead':m.w(0xa8ed84,m.r(0xa8ed84)+1)
  elif boundary=='mode':m.w(0xa8b238,2)
  elif boundary=='run_flag':m.b(0xa8e9a0,0)
  elif boundary=='chat':m.b(0xa8bc60+25,1)
  enabled=boundary in ('frame_ahead','chat')
  before=(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)),m.r(0xA802C8),m.r(0x8B41F8))
  for at in (0x47EFE0,0x47F250):m.run(at,(0,0,15),self=m.crateCell)
  assert m.original==([] if enabled else [0x47F03D,0x47F279]),(boundary,m.original)
  after=(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)),m.r(0xA802C8),m.r(0x8B41F8));assert before==after
  if boundary in ('menu','dialog','tactical_inactive'):
   m.original.clear();s=m.hook()
   for at in (0x47EFE0,0x47F250):m.run(at,(0,0,15),self=m.crateCell)
   assert m.original==[0x47F03D,0x47F279]
   m.w(0xa8eda0,0);m.w(0xa8ed8c,0);m.b(0xa8e378,1);m.hook();m.original.clear()
   for at in (0x47EFE0,0x47F250):m.run(at,(0,0,15),self=m.crateCell)
   assert m.original==[]
  rows.append({'case':boundary,'status':'PASS'});print('PASS',boundary,flush=True)
 except Exception as e:rows.append({'case':boundary,'status':'FAIL','error':repr(e)});traceback.print_exc()
for stop in (False,True):
 try:
  m=fixture();assert len(m.redraw)==1
  s=state(m);s.renderReveal=0
  if stop:s.stop=1
  save(m,s);m.hook()
  assert all(bytes(m.u.mem_read(at,len(sig)))==sig for at,sig in sites.items())
  assert len(m.redraw)==2,('redraw_after_stop',stop,len(m.redraw))
  rows.append({'case':'stop' if stop else 'disable','status':'PASS'})
 except Exception as e:rows.append({'case':'stop' if stop else 'disable','status':'FAIL','error':repr(e)});traceback.print_exc()
Path(a.output).write_text(json.dumps({'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'scope':__doc__,'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
