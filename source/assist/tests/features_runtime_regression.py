"""Actual DLL: external-display planning and managed native-sidebar lifecycle.
Game functions/Win32 are modeled except the original event constructors/queues.
This is NOT whole-game or real-netplay validation.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,subprocess,tempfile
from pathlib import Path
from unicorn.x86_const import *
P=argparse.ArgumentParser();P.add_argument('game');P.add_argument('dll');P.add_argument('--baseline');P.add_argument('--output',default='features-runtime.json');a=P.parse_args()
root=Path(__file__).resolve().parents[2];results=[]
def fixture(dll=None):
 spec=importlib.util.spec_from_file_location('feature_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,dll or a.dll];spec.loader.exec_module(m);return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def issue(m,cmd,arg=0):
 s=state(m);s.command=cmd;s.arg=arg;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);s=m.hook();assert s.ack==s.request;return s

def tech(m,mode=0,flags=0):
 m.w(0xa8b238,mode);v=m.alloc(256);m.w(v+0x94,0x5f7900)
 for t in (m.t,m.engT,m.buildT):m.w(t,v)
 s=state(m);s.externalDisplay=1;s.flags=flags;save(m,s);s=m.hook();s.techAllowed[1]=1;save(m,s);return s

def refresh_branch(m,typ=None):
 # Original strip refresh instructions: resolve factory then call eligibility.
 # Stop BEFORE the original cleanup or retain branches; never simulate their
 # production/economy effects as if the complete game had been run.
 m.u.reg_write(UC_X86_REG_EAX,typ or m.t);m.u.reg_write(UC_X86_REG_EDI,0);sp=m.STACK+0x1c000;m.u.reg_write(UC_X86_REG_ESP,sp)
 m.u.emu_start(0x6aa759,0x6aaa68,count=10000)
 ip=m.u.reg_read(UC_X86_REG_EIP);return ip

def check(name,fn):
 fn();results.append({'name':name,'status':'PASS'});print('PASS',name,flush=True)
# A tiny host executable uses the production planner AND its freshness policy.
with tempfile.TemporaryDirectory(prefix='ra2-feature-') as tmp:
 cpp=Path(tmp)/'host.cpp';exe=Path(tmp)/'host'
 cpp.write_text('#include "'+str(root/'assist/source/external_planner.h')+'"\n#include "'+str(root/'assist/source/plan_freshness.h')+'"\n#include <cstdio>\n#include <memory>\nint main(){uint32_t h[3];auto w=std::make_unique<assist::planner::World>();if(fread(h,sizeof h,1,stdin)!=1||fread(w.get(),sizeof(*w),1,stdin)!=1)return 2;std::vector<assist::planner::Order> o;if(assist::freshness::valid(w->match,h[0],w->tick,w->frame,h[1],int(h[2]),assist::freshness::Tactical)){assist::planner::Planner p;o=p.step(*w,assist::AutoOreReturn,200,300,90);}uint32_t n=o.size();fwrite(&n,4,1,stdout);fwrite(o.data(),sizeof(assist::planner::Order),n,stdout);}')
 subprocess.run(['g++','-std=c++17','-O2',str(cpp),'-o',str(exe)],check=True)
 def prepare(m,mode,amount=40,mission=10,dock=True):
  m.w(0xa8b238,mode);m.w(m.unit+0xac,mission);m.b(m.t+0xe0e,1);m.w(m.t+0x800,40);m.u.mem_write(m.unit+0x33c,struct.pack('<4f',amount,0,0,0));m.vector(m.t+0x3e8,[m.buildT] if dock else []);s=state(m);s.flags=32768;s.externalDisplay=1;save(m,s);return m.hook()
 def host(m,s):return subprocess.check_output([str(exe)],input=struct.pack('<III',s.data.match,m.clock,s.data.frame)+bytes(s.world))
 baseline=None
 if a.baseline:
  m=fixture(a.baseline);s=prepare(m,0);m.w(0xa8ed84,150);m.clock+=20;s=m.hook();assert s.data.frame==0 and s.world.frame==150 and struct.unpack_from('<I',host(m,s))[0]==0
  baseline={'dll_sha256':hashlib.sha256(Path(a.baseline).read_bytes()).hexdigest(),'control_frame':s.data.frame,'world_frame':s.world.frame,'host_orders':0};print('REPRODUCED 0.7.9 externalDisplay control frame frozen at zero',flush=True)
 for mode in (0,3):
  for label,amount,mission,dock,expected in [('full',40,10,True,1),('not_full',36,10,True,0),('manual_move',40,2,True,0),('no_dock',40,10,False,0)]:
   def case(mode=mode,amount=amount,mission=mission,dock=dock,expected=expected):
    m=fixture();s=prepare(m,mode,amount,mission,dock);assert s.data.frame==s.world.frame==100
    # New world within display throttle (<16ms) must still publish control.
    m.w(0xa8ed84,101);m.clock+=8;s=m.hook();assert s.data.frame==s.world.frame==101
    raw=host(m,s);count=struct.unpack_from('<I',raw)[0];assert count==expected
    if count:
     m.w(0x8b41f8,0);m.w(0x8b4200,0);m.w(0xa802c8,0);m.w(0xa802d0,0)
     s.planRequest+=1;s.planMatch=s.world.match;s.planTick=s.world.tick;s.planFrame=s.world.frame;s.planCount=count
     for i in range(count):s.plan[i]=m.Order.from_buffer_copy(raw[4+i*C.sizeof(m.Order):4+(i+1)*C.sizeof(m.Order)])
     save(m,s);s=m.hook();assert s.planAck==s.planRequest and s.planAccepted==1
     assert m.r(0x8b41f8 if mode==0 else 0xa802c8)==1
   check('external_display_ore_'+str(mode)+'_'+label,case)
 for mode in (0,3):
  def expiry(mode=mode):
   m=fixture();s=prepare(m,mode);raw=host(m,s);assert struct.unpack_from('<I',raw)[0]==1
   s.planRequest+=1;s.planMatch=s.world.match;s.planTick=s.world.tick;s.planFrame=s.world.frame;s.planCount=1;s.plan[0]=m.Order.from_buffer_copy(raw[4:]);save(m,s)
   m.w(0xa8ed84,107);m.clock+=20;s=m.hook();assert C.c_int32(s.planResult).value==-7 and s.planAccepted==0
  check('six_frame_deadline_retained_'+str(mode),expiry)

# Hook function to stop exactly at cleanup before side effects.
from unicorn import UC_HOOK_CODE

def branch(m,typ=None):
 h=m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,user:uc.emu_stop(),begin=0x6aa7b6,end=0x6aa7b6)
 try:return refresh_branch(m,typ)
 finally:m.u.hook_del(h)

def native_sidebar():
 m=fixture();tech(m);original=bytes(m.u.mem_read(0x4f7870,64));s=issue(m,10,1)
 assert s.result==1 and s.data.techVisible==1 and m.sidebar_add_calls==[(40,0)]
 assert m.u.mem_read(0x6a97d2,5)!=b'\xe8\x99\xe0\xe4\xff' and m.u.mem_read(0x6aa781,5)!=b'\xe8\xea\xd0\xe4\xff'
 assert branch(m)==0x6aaa68 and m.canbuild_calls[-1][2:]==(1,1)
 for _ in range(3):m.clock+=1001;m.w(0xa8ed84,m.r(0xa8ed84)+1);m.hook()
 assert m.sidebar_add_calls==[(40,0)] and bytes(m.u.mem_read(0x4f7870,64))==original
 # Explicit production still uses the unified SP queue, with repeated purchases.
 for _ in range(2):
  m.w(0xa8ed84,m.r(0xa8ed84)+1);s=issue(m,12,1);assert s.result==1
 assert m.r(0x8b41f8)==2
 # Disabling restores BOTH call sites; global simulation code was untouched.
 s=issue(m,10,0);assert s.result==1
 assert bytes(m.u.mem_read(0x6a97d2,5))==b'\xe8\x99\xe0\xe4\xff' and bytes(m.u.mem_read(0x6aa781,5))==b'\xe8\xea\xd0\xe4\xff'
 assert branch(m)==0x6aa7b6
check('native_sidebar_add_retain_repeat_purchase_disable',native_sidebar)

def no_factory():
 m=fixture();tech(m);m.factory_available=False;s=issue(m,10,1);assert s.result==1 and s.data.techVisible==0 and s.data.techUnavailable==1 and not m.sidebar_add_calls
 m.clock+=1001;m.hook();assert not m.sidebar_add_calls
 m.factory_available=True;m.clock+=1001;s=m.hook();assert s.data.techVisible==1 and branch(m)==0x6aaa68
 m.factory_available=False;assert branch(m)==0x6aa7b6
check('missing_factory_wait_rebuild_and_loss_cleanup',no_factory)

def whitelist():
 m=fixture();tech(m);issue(m,10,1);assert branch(m,m.engT)==0x6aa7b6
 s=state(m);s.techAllowed[1]=0;save(m,s);issue(m,10,1);assert branch(m)==0x6aa7b6
check('unlisted_type_and_removed_exception_native_cleanup',whitelist)

def denied():
 m=fixture();tech(m);issue(m,10,1);m.current_thread=998
 assert branch(m)==0x6aa7b6 and m.canbuild_calls[-1][2:]==(0,1)
check('denied_manager_capture_preserves_original_call',denied)

def click_site():
 m=fixture();tech(m);issue(m,10,1)
 raw=bytes(m.u.mem_read(0x6a97d2,5));target=0x6a97d7+struct.unpack_from('<i',raw,1)[0]
 m.run(target,(m.t,0,0),m.house)
 assert m.u.reg_read(UC_X86_REG_EAX)==1 and m.canbuild_calls[-1][2:]==(1,0)
 m.run(target,(m.engT,0,0),m.house)
 assert m.u.reg_read(UC_X86_REG_EAX)==0 and m.canbuild_calls[-1][2:]==(0,0)
 m.run(target,(m.t,0,0),m.enemy)
 assert m.u.reg_read(UC_X86_REG_EAX)==0 and m.canbuild_calls[-1][0]==m.enemy and m.canbuild_calls[-1][2:]==(0,0)
check('native_click_ABI_local_whitelist_and_other_house',click_site)

for at in (0x6a97d2,0x6aa781):
 def conflict(at=at):
  m=fixture();tech(m);m.u.mem_write(at,b'\x90'*5);s=issue(m,10,1)
  assert C.c_int32(s.result).value==-11 and not s.data.techOpen and bytes(m.u.mem_read(at,5))==b'\x90'*5 and not m.sidebar_add_calls
 check('foreign_sidebar_patch_preserved_'+hex(at),conflict)

def changed_site():
 m=fixture();tech(m);issue(m,10,1);m.u.mem_write(0x6aa781,b'\x90'*5);m.clock+=1001;s=m.hook()
 assert not s.data.techOpen and s.data.techError<0 and bytes(m.u.mem_read(0x6aa781,5))==b'\x90'*5
 assert bytes(m.u.mem_read(0x6a97d2,5))==b'\xe8\x99\xe0\xe4\xff'
check('foreign_replacement_after_install_restores_only_owned_site',changed_site)
def build_limit():
 m=fixture();tech(m);m.canbuild_limit_result=-1;s=issue(m,10,1)
 assert s.data.techVisible==0 and not m.sidebar_add_calls
 raw=bytes(m.u.mem_read(0x6a97d2,5));target=0x6a97d7+struct.unpack_from('<i',raw,1)[0]
 m.run(target,(m.t,0,0),m.house);assert C.c_int32(m.u.reg_read(UC_X86_REG_EAX)).value==-1
check('native_build_limit_rejection_preserved',build_limit)

def capacity():
 m=fixture();tech(m);strip=0x87f7e8+0x1544+3*3988;m.w(strip+84,75)
 for i in range(75):m.w(strip+88+i*52,100+i);m.w(strip+92+i*52,40)
 guard=bytes(m.u.mem_read(strip+88+75*52,16));s=issue(m,10,1)
 assert s.data.techVisible==0 and s.data.techUnavailable==1 and not m.sidebar_add_calls
 assert m.r(strip+84)==75 and bytes(m.u.mem_read(strip+88+75*52,16))==guard
check('native_sidebar_75_slot_boundary',capacity)

def together():
 m=fixture();sites=(0x646e90,0x731bf0,0x6ffe00,0x4d74e0,0x692610,0x4ab9b0,0x464ac0,0x6a97d2,0x6aa781)
 originals={at:bytes(m.u.mem_read(at,8)) for at in sites}
 flags=512|131072|8192|1024|262144|1048576|2097152|4194304|8388608|16777216|65536
 tech(m,0,flags);s=issue(m,10,1);assert s.result==1 and s.data.hookConflicts==0 and s.data.inputPatchMask==63
 assert branch(m)==0x6aaa68
 s=issue(m,10,0);s.flags=0;save(m,s);m.w(0xa8ed84,101);m.clock+=20;s=m.hook()
 assert s.data.hookConflicts==0 and s.data.inputPatchMask==0
 for at in sites:assert bytes(m.u.mem_read(at,8))==originals[at],hex(at)
check('technology_and_all_input_hooks_share_manager_and_restore',together)

for flags,expected in ((0,-13),(4096,1)):
 def mp(flags=flags,expected=expected):
  m=fixture();tech(m,3,flags);s=issue(m,10,1);assert C.c_int32(s.result).value==expected
  assert bool(m.sidebar_add_calls)==(expected==1)
 check('multiplayer_technology_existing_experimental_guard_'+str(flags),mp)
Path(a.output).write_text(json.dumps({'inputs':{'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest()},'baseline_reproduction':baseline,'results':results,'limitations':'Native DLL and original constructors/branch instructions; synthetic world, mocked sidebar AddCameo/CanBuild/FindFactory and Win32. No full game, real network or economy verification.'},indent=2)+'\n')
