"""Actual DLL and supplied EXE instructions; synthetic world/Win32/native resources.
Exercises first shroud/fog draw (including each EXE's real embedded-resource
prefix), tech startup budgets and callback FP isolation. Not a netplay test.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--baseline',action='store_true');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def fixture(mode=3,types=0):
 spec=importlib.util.spec_from_file_location('compat_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.u.reg_write(UC_X86_REG_MXCSR,0x1f80);m.u.reg_write(UC_X86_REG_FPCW,0x27f)
 m.w(0xa8b238,mode);s=state(m);s.flags=0;s.externalDisplay=1;save(m,s)
 if types:
  items=[]
  for i in range(types):
   t=m.alloc(0x2000);m.u.mem_write(t,bytes(m.u.mem_read(m.t,0x2000)));m.u.mem_write(t+0x24,f'TEST{i:03}'.encode()+b'\0');items.append(t)
  m.vector(0xa83ce0,items)
 m.hook();return m
def issue(m,c,arg=0):
 s=state(m);s.command=c;s.arg=arg;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;save(m,s);return m.hook()
def case(name,fn):
 try:
  detail=fn() or {};rows.append({'case':name,'status':'PASS',**detail});print('PASS',name,flush=True)
 except Exception as e:
  rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def setup_draw(m):
 rules=m.alloc();m.w(0xa8b230,rules);m.b(0x89e7c5,0);m.b(0x89e7c6,0)
 for ptr in (0x89e794,0x89e790,0x89e760):m.w(ptr,0)
 # Actual SHP blitters need a Windows graphics device. Stop only after the
 # original resource prefixes and new displaced instructions have executed.
 m.blits=[]
 def continuation(uc,at,size,_):
  m.blits.append(at)
  if at==0x47f03d:
   bp=uc.reg_read(UC_X86_REG_EBP);uc.reg_write(UC_X86_REG_EBP,m.r(bp));uc.reg_write(UC_X86_REG_ESP,bp+4)
  else:uc.reg_write(UC_X86_REG_ESP,uc.reg_read(UC_X86_REG_ESP)+0x30)
  m.ret(0,3)
 for at in (0x47f03d,0x47f279):m.u.hook_add(UC_HOOK_CODE,continuation,begin=at,end=at)
 # Both input EXEs use embedded SHP resources; retain a loader stub for other
 # verified prefixes, without treating that stub as rendered-output evidence.
 m.u.hook_add(UC_HOOK_CODE,lambda u,at,sz,x:m.ret(m.alloc(),0),begin=0x5b40b0,end=0x5b40b0)
def draw_case(mode,enabled):
 m=fixture(mode);setup_draw(m);s=state(m);s.renderReveal=1;save(m,s);s=m.hook();assert s.data.renderRevealStatus==1
 if not enabled:s=state(m);s.renderReveal=0;save(m,s)
 before=(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)))
 for at in (0x47efe0,0x47f250):m.run(at,(0,0,15),m.crateCell)
 initialized=all(m.r(at) for at in (0x89e794,0x89e790,0x89e760))
 assert bool(initialized)==(not(a.baseline and enabled)),(initialized,a.baseline,enabled)
 assert m.blits==([] if enabled else [0x47f03d,0x47f279]),m.blits
 assert before==(bytes(m.u.mem_read(m.crateCell,0x200)),bytes(m.u.mem_read(m.house,0x6000)))
 assert not m.r(0xa802c8) and not m.r(0x8b41f8)
 return {'resource_initialization_retained':bool(initialized),'baseline_skips_init':a.baseline and enabled}
for mode in (0,3,4,5):
 for enabled in (False,True):case(f'first_draw_{mode}_{enabled}',lambda mode=mode,enabled=enabled:draw_case(mode,enabled))
def budget_case(mode):
 m=fixture(mode,40);s=state(m);s.flags=4096
 for i in range(s.data.typeCount):s.techAllowed[i]=1
 save(m,s);s=issue(m,10,1);first=len(m.sidebar_add_calls)
 assert s.data.techOpen==1 and s.data.hookInstalled==2
 assert first==(42 if a.baseline else 8),first
 if a.baseline:return {'first_batch':first,'baseline_unbounded':True}
 # A second request on the SAME frame must not consume another batch.
 m.clock+=20;s=issue(m,10,1);assert len(m.sidebar_add_calls)==first
 per_frame=[]
 for frame in range(101,112):
  m.w(0xa8ed84,frame);m.clock+=20;m.w(m.SH+m.Shared.heartbeat.offset,m.clock)
  before=len(m.sidebar_add_calls);m.hook();per_frame.append(len(m.sidebar_add_calls)-before)
 assert all(n<=8 for n in per_frame) and len(m.sidebar_add_calls)==42,(per_frame,len(m.sidebar_add_calls))
 assert len(set(m.sidebar_add_calls))==42
 # Disabling and changing rounds must stop incomplete work.
 s=issue(m,10,0);assert not s.data.techOpen and not s.data.hookInstalled
 before=len(m.sidebar_add_calls);m.w(0xa8ed84,112);m.clock+=100;m.hook();assert len(m.sidebar_add_calls)==before
 return {'first_batch':first,'subsequent_batches':per_frame,'total':len(m.sidebar_add_calls)}
for mode in (3,4):case(f'tech_bounded_{mode}',lambda mode=mode:budget_case(mode))
def fp_case(kind):
 m=fixture();inside=[];chained=[]
 entry=next(addr for addr,name in m.imports.items() if name=='GetCurrentThreadId')
 m.u.hook_add(UC_HOOK_CODE,lambda u,at,sz,x:inside.append(u.reg_read(UC_X86_REG_MXCSR)),begin=entry,end=entry)
 nxt=next(addr for addr,name in m.imports.items() if name=='CallNextHookEx')
 m.u.hook_add(UC_HOOK_CODE,lambda u,at,sz,x:chained.append(u.reg_read(UC_X86_REG_MXCSR)),begin=nxt,end=nxt)
 incoming=0x7fa0;m.u.reg_write(UC_X86_REG_MXCSR,incoming);m.u.reg_write(UC_X86_REG_FPCW,0x27f)
 if kind=='message':m.hook()
 else:
  setup_draw(m);s=state(m);s.renderReveal=1;save(m,s);m.hook();inside.clear();chained.clear()
  m.u.reg_write(UC_X86_REG_MXCSR,incoming);m.run(0x47efe0,(0,0,15),m.crateCell)
 assert inside and all(x==(incoming if a.baseline else 0x1f80) for x in inside),inside
 assert m.u.reg_read(UC_X86_REG_MXCSR)==incoming and m.u.reg_read(UC_X86_REG_FPCW)==0x27f
 assert all(x==incoming for x in chained),chained
 return {'inside_mxcsr':hex(inside[0]),'caller_mxcsr_restored':hex(incoming)}
for kind in ('message','draw'):case('fp_isolation_'+kind,lambda kind=kind:fp_case(kind))
def callbacks_cost():
 m=fixture();setup_draw(m);s=state(m);s.renderReveal=1;save(m,s);m.hook();calls=[]
 at=next(addr for addr,name in m.imports.items() if name=='GetModuleHandleExW')
 m.u.hook_add(UC_HOOK_CODE,lambda u,at,sz,x:calls.append(at),begin=at,end=at)
 for _ in range(100):m.run(0x47efe0,(0,0,15),m.crateCell)
 assert (len(calls)>=300 if a.baseline else len(calls)==0),len(calls)
 return {'draws':100,'module_lookups':len(calls)}
case('hot_draw_module_identity',callbacks_cost)
Path(a.output).write_text(json.dumps({'scope':__doc__,'baseline':a.baseline,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows,'real_netplay':False},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
