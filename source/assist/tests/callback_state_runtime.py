"""Compiled DLL callbacks with synthetic Win32 error/exception delivery.
Real x86 game/DLL instructions, not a Windows crash dump or netplay test.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--baseline',action='store_true');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
def fixture():
 spec=importlib.util.spec_from_file_location('state_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.u.reg_write(UC_X86_REG_MXCSR,0x1f80);m.u.reg_write(UC_X86_REG_FPCW,0x27f);m.hook();return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def draw_setup(m):
 rules=m.alloc();m.w(0xa8b230,rules)
 s=state(m);s.renderReveal=1;m.u.mem_write(m.SH,bytes(s));m.hook()
 m.b(0x89e7c5,1);m.b(0x89e7c6,1)
 for at in (0x89e794,0x89e790,0x89e760):m.w(at,rules)
def case(name,fn):
 try:detail=fn() or {};rows.append({'case':name,'status':'PASS',**detail});print('PASS',name,detail,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
def preserve(kind):
 m=fixture()
 if kind!='message':draw_setup(m)
 seen=[]
 if kind=='message':
  nxt=next(at for at,n in m.imports.items() if n=='CallNextHookEx')
  m.u.hook_add(UC_HOOK_CODE,lambda *args:seen.append(m.win32_last_error),begin=nxt,end=nxt)
 m.win32_last_error=0x12345678;m.clobber_win32_error=True
 if kind=='message':m.hook()
 elif kind=='rejected':
  # A foreign-thread capture must decline without leaking a Win32 error.
  m.current_thread=778
  continuation=0x47f03d
  def finish(*args):
   bp=m.u.reg_read(UC_X86_REG_EBP);m.u.reg_write(UC_X86_REG_EBP,m.r(bp));m.u.reg_write(UC_X86_REG_ESP,bp+4);m.ret(0,3)
  m.u.hook_add(UC_HOOK_CODE,finish,begin=continuation,end=continuation)
  m.run(0x47efe0,(0,0,15),m.crateCell)
 else:m.run(0x47efe0,(0,0,15),m.crateCell)
 expected=0xdead if a.baseline else 0x12345678
 assert m.win32_last_error==expected,(hex(m.win32_last_error),hex(expected))
 assert all(v==expected for v in seen),seen
 return {'caller_last_error':hex(m.win32_last_error),'chain_observations':seen}
for kind in ('message','draw','rejected'):case('last_error_'+kind,lambda kind=kind:preserve(kind))
def cost(nested):
 m=fixture();draw_setup(m);calls={}
 if nested:
  q=m.request(0);result,lease=m.manager_call(q);assert result==1
 def count(uc,at,size,_):
  n=m.imports.get(at)
  if n:calls[n]=calls.get(n,0)+1
 token=m.u.hook_add(UC_HOOK_CODE,count)
 for _ in range(100):
  if nested:
   q=m.request(5);q.token=lease.token;result,child=m.manager_call(q);assert result==1
   child.operation=1;assert m.manager_call(child)[0]==1
  else:m.run(0x47efe0,(0,0,15),m.crateCell)
 m.u.hook_del(token)
 if nested:lease.operation=1;assert m.manager_call(lease)[0]==1
 assert calls.get('GetCurrentThreadId',0)==(200 if nested or not a.baseline else 300),calls
 assert calls.get('WaitForSingleObject',0)==(0 if nested and not a.baseline else 100),calls
 return {'operations':100,'operation':'nested_capture' if nested else 'draw','api_calls':calls}
for nested in (False,True):case('capture_cost_'+str(nested),lambda nested=nested:cost(nested))
def observer(code=0xc0000005,thread=777,enabled=True,site=0x6f49de):
 m=fixture();assert m.exception_observer
 base=m.SH+0x3e0000;assert m.r(base+120)==1
 record=m.alloc(0x100);context=m.alloc(0x400);ptr=m.alloc(8)
 m.u.mem_write(record,struct.pack('<20I',code,0,0,site,2,0,0x5778,*([0]*13)))
 for off,val in ((160,m.eng),(172,0),(176,0),(180,m.unit)):m.w(context+off,val)
 m.w(ptr,record);m.w(ptr+4,context)
 before=bytes(m.u.mem_read(record,80))+bytes(m.u.mem_read(context,0x300))
 m.current_thread=thread;m.w(base+16,1 if enabled else 0);m.win32_last_error=1234;m.clobber_win32_error=True
 # Its own publication remains possible even when a Runtime fault report is
 # in progress. It must not make the original diagnostic seqlock look stable.
 m.w(base+40,1)
 m.run(m.exception_observer,(ptr,),0);result=m.u.reg_read(UC_X86_REG_EAX)
 assert result==0 and m.win32_last_error==1234
 assert before==bytes(m.u.mem_read(record,80))+bytes(m.u.mem_read(context,0x300))
 assert m.r(base+40)==1
 hit=code in (0xc0000005,0xc0000006,0xc000001d,0xc0000094) and thread==777 and enabled
 assert m.r(base+132)==int(hit)
 if hit:
  assert m.r(base+136)==code and m.r(base+140)==site
  assert m.r(base+152)==777 and m.r(base+168)==m.eng
 return {'reported':hit,'continued_exception_search':True,'exception_context_unchanged':True}
if not a.baseline:
 for site in (0x6f49de,0x70af6c,0x65dc17,0x47f050):case('native_fault_'+hex(site),lambda site=site:observer(site=site))
 case('ignore_other_thread',lambda:observer(thread=778))
 case('ignore_disabled',lambda:observer(enabled=False))
 case('ignore_breakpoint',lambda:observer(code=0x80000003))
 def registration_failure():
  spec=importlib.util.spec_from_file_location('failure_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
  m.observer_registration_fails=True;m.win32_last_error=5;s=m.hook();base=m.SH+0x3e0000
  assert m.r(base+120)==0xffffffff and m.r(base+124)==5 and s.data.commandStatus==1
  return {'bridge_remains_usable':True}
 case('observer_registration_failure_is_nonfatal',registration_failure)
Path(a.output).write_text(json.dumps({'scope':__doc__,'baseline':a.baseline,'game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'results':rows,'real_netplay':False},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
