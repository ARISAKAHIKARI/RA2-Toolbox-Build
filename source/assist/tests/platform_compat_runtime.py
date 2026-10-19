"""Actual DLL and supplied EXE constructors; synthetic world/Win32/click hit testing.
No platform launcher, extensions.dll, sockets, rendered frames or real netplay.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode):
 spec=importlib.util.spec_from_file_location('compat_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,mode);s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);m.hook();return m
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
def pointer(m,operation=5,bindings=0):
 s=state(m);i=s.pointer.intent;i.match=s.data.match;i.tick=m.clock;i.frame=m.r(0xa8ed84);i.house=m.house;i.operation=operation;i.bindings=bindings;i.width=800;i.height=600;i.x=20;i.y=30;i.count=1;i.sources[0].id=10;i.sources[0].pointer=m.unit
 s.pointer.request+=1;save(m,s);s=m.hook();assert s.pointer.ack==s.pointer.request and s.pointer.result==1,(s.pointer.ack,s.pointer.request,s.pointer.result);return s
for mode in (0,5,3,4):
 m=fixture(mode);s=state(m);s.flags=0x80000000;save(m,s);s=m.hook();network=mode in (3,4)
 assert s.data.nativeAirStatus==(2 if network else 1) and s.data.hookInstalled==(0 if network else 2)
 for at,n in ((0x416ab6,10),(0x7e2908,4)):
  assert (bytes(m.u.mem_read(at,n))==m.game.get_data(at-0x400000,n))==network
 assert m.r(0xa802c8)==m.r(0x8b41f8)==0
 if network:
  airtype=m.alloc(0x2000);air=m.obj(0x7e22a4,airtype,m.house,40)
  m.vector(0xa8ec78,[m.unit,m.eng,m.building,m.enemyUnit,air]);m.w(0xa8ed84,101);s=m.hook()
  before=bytes(m.u.mem_read(air,0x2000));s.command=21;s.arg=40;s.arg2=30020;s.requestToken=0;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook()
  assert s.result==1 and s.count==1,(s.result,s.data.lastSubmitError)
  assert m.r(0xa802c8)==1 and m.r(0x8b41f8)==0
  event=bytes(m.u.mem_read(0xa802d4,111));assert event[0]==4 and event[2]==0 and event[12]==2,event.hex()
  assert bytes(m.u.mem_read(air,0x2000))==before,'only queue event, no local aircraft state change'
 passed(f'air_{mode}_backend_sites_no_startup_event'+('_ordinary_network_move' if network else ''))
 # Every input starts from the real cancel path; no direct calls to the helper.
 m=fixture(mode);m.b(m.t+0xe0e,1);m.w(m.t+0x800,20);m.vector(m.t+0x3e8,[m.buildT]);m.w(m.unit+0xac,10)
 m.w(0xa8ed84,101);m.hook();pointer(m)
 def tick(mission):m.w(m.unit+0xac,mission);m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
 for mission in (10,12,16,5,10):assert tick(mission).data.oreManual==1
 assert tick(2).data.oreManual==1 and tick(10).data.oreManual==0
 passed(f'ore_{mode}_actual_cancel_observed_move_then_harvest')
 target=[0]
 def click(u,at,size,_):
  sp=m.u.reg_read(UC_X86_REG_ESP);arg=lambda n:m.r(sp+4+n*4)
  m.u.mem_write(arg(1),struct.pack('<hh',20,30));m.u.mem_write(arg(2),struct.pack('<iii',5120,7680,0));m.w(arg(3),target[0]);m.ret(0,6)
 m.u.hook_add(UC_HOOK_CODE,click,begin=0x692300,end=0x692300)
 # Plain ground click: Runtime wires expectation itself, old Return cannot resume.
 pointer(m,4,8192);assert tick(12).data.oreManual==1 and tick(10).data.oreManual==1
 assert tick(2).data.oreManual==1 and tick(10).data.oreManual==0
 passed(f'ore_{mode}_plain_move_expected_mission_is_wired')
 # Ordinary mining/refinery click resumes with Auto CS and auto ore both OFF.
 pointer(m);m.b(m.overlayType+0x2a9,1);pointer(m,4,8192);assert tick(10).data.oreManual==0
 pointer(m);pointer(m,4,8192|131072);assert tick(10).data.oreManual==1
 target[0]=m.building;pointer(m,4,8192);assert tick(10).data.oreManual==0
 # Unrelated own building cannot hand back mining control.
 other=m.alloc(0x2000);m.w(m.building+0x520,other);pointer(m,4,8192);assert tick(10).data.oreManual==1
 assert m.r(0xa802c8)==m.r(0x8b41f8)==0,'native input observer must not generate commands'
 passed(f'ore_{mode}_ore_refinery_handback_force_exclusion_no_extra_events')
Path(a.output).write_text(json.dumps({'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'scope':__doc__,'results':rows},indent=2)+'\n')
