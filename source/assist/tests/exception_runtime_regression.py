"""Actual DLL filters/handlers and command continuation under modeled x86 SEH delivery.
The OS dispatcher and game MouseOver are synthetic; this is not native Windows SEH validation.
"""
import argparse,importlib.util,ctypes as C,json,hashlib,struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def passed(name,**data):rows.append({'case':name,'status':'PASS',**data});print('PASS',name,data,flush=True)
def fixture():
 spec=importlib.util.spec_from_file_location('exception_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[str(a.game),str(a.dll)];spec.loader.exec_module(m)
 m.u.mem_write(m.engT+0x380,struct.pack('<d',1.));m.u.mem_write(m.t+0x388,struct.pack('<d',3.))
 m.w(m.unit+0x114,0);m.w(m.unit+0x118,0);m.w(0x8b41f8,0);m.w(0x8b41fc,0);m.w(0x8b4200,0)
 m.hook();return m
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def issue(m,command,mode=0):
 s=state(m);s.command=command;s.loadMode=mode;s.request+=1;s.requestMatch=s.data.match;s.requestTick=m.clock;m.u.mem_write(m.SH,bytes(s));return m.hook()
def fields(m):return struct.unpack('<12I',m.u.mem_read(m.SH+0x3e0000+72,48))
def inject(m,code=0xc0000005):
 # Model Windows passing EXCEPTION_POINTERS to registered __except_handler3
 # scopes. Execute the real compiler-emitted filters and selected handler.
 record=m.SH+0x3d0000;ptr=record+128;stub=m.STUB+0xe000
 active={'frame':None,'handler':None,'fired':False,'filters':0}
 def start(frame):
  assert frame not in (0,0xffffffff)
  level=m.r(frame+12);scope=m.r(frame+8);assert level<16
  _,filt,handler=struct.unpack('<3I',m.u.mem_read(scope+12*level,12));assert filt and handler
  m.w(frame-4,ptr);active.update(frame=frame,handler=handler)
  # The MSVC filter/handler ABI uses registration-frame + 16 as EBP.
  entry=stub+active['filters']*32
  m.u.mem_write(entry,b'\xbd'+struct.pack('<I',frame+16)+b'\xb8'+struct.pack('<I',filt)+b'\xff\xd0\x90')
  m.u.hook_add(UC_HOOK_CODE,complete,begin=entry+12,end=entry+12)
  m.u.reg_write(UC_X86_REG_EIP,entry)
 def complete(uc,address,size,_):
  active['filters']+=1
  if uc.reg_read(UC_X86_REG_EAX)==1:
   uc.reg_write(UC_X86_REG_EBP,active['frame']+16);uc.reg_write(UC_X86_REG_EIP,active['handler'])
  else:start(m.r(active['frame']))
 def fail(address):
  if active['fired']:return False
  active['fired']=True;m.u.mem_write(record,struct.pack('<20I',code,0,0,address,2,0,0xdead0000,*([0]*13)));m.w(ptr,record);m.w(ptr+4,0)
  start(m.r(0));return True
 m.native_query_exception=fail;return active
for mode in (0,1):
 m=fixture();depth=[0];top=m.STACK+0x1e000
 def stack(uc,address,size,_):depth[0]=max(depth[0],top-uc.reg_read(UC_X86_REG_ESP))
 token=m.u.hook_add(UC_HOOK_CODE,stack);s=issue(m,15,mode);m.u.hook_del(token)
 assert s.result==1 and s.ack==s.request and s.count==1 and m.r(0x8b41f8)==1
 assert depth[0]<16384,depth
 passed('autoload_mode_'+str(mode)+'_single_submit_bounded_game_thread_stack',stack_high_water_bytes=depth[0])
 m=fixture();inj=inject(m);s=issue(m,15,mode)
 assert inj['fired'] and inj['filters']==1 and C.c_int32(s.result).value==-15 and s.ack==s.request and s.count==0
 assert s.data.status==1 and s.data.commandStatus==1 and not m.r(0x8b41f8)
 d=fields(m);assert d[:7]==(1,0xc0000005,0x51e3b0,0,0xdead0000,9,777),d
 assert d[8:]==(15,2,m.eng,m.unit),d
 passed('autoload_mode_'+str(mode)+'_query_AV_reports_context_rejects_before_queue')
 s=issue(m,1);assert s.result==1 and s.count==1 and m.r(0x8b41f8)==1
 s=issue(m,15,mode);assert s.result==1 and s.count==1 and m.r(0x8b41f8)==2
 assert fields(m)==d
 passed('autoload_mode_'+str(mode)+'_repair_and_next_load_continue_without_fault_replay')
m=fixture();inj=inject(m,0xc000001d);s=issue(m,15)
assert inj['filters']==2 and C.c_int32(s.result).value==-10 and s.data.status==-2 and s.data.commandStatus==0
assert fields(m)[1]==0xc000001d and fields(m)[5]==1 and not m.r(0x8b41f8)
ack=s.ack;s=issue(m,1);assert s.ack==ack and not m.r(0x8b41f8)
passed('non_access_exception_reaches_outer_latch_and_never_replays_unknown_work')
a.output.write_text(json.dumps({'scope':__doc__,'inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')
