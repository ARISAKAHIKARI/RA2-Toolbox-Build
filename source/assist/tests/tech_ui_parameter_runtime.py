"""Compiled mode4 lifecycle/stack-only UI interception in an x86 synthetic world.

Win32 thread/debug-register installation is modeled here; the separate native
tech_ui_breakpoint_windows.cpp fixture runs real own-process Windows DR/VEH.
No game process, platform detection or multiplayer session is exercised.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from pdb_fixture_symbol import function_address
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args()
initial_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();rows=[]

def data_address(pe,path,name):
 raw=Path(path).read_bytes();bs,_,_,ds,_,bm=struct.unpack_from('<6I',raw,32);count=lambda n:(n+bs-1)//bs
 read=lambda blocks,n:b''.join(raw[i*bs:(i+1)*bs] for i in blocks)[:n]
 directory=read(struct.unpack_from('<%dI'%count(ds),raw,bm*bs),ds);n=struct.unpack_from('<I',directory)[0];sizes=struct.unpack_from('<%dI'%n,directory,4);off=4+4*n;found=set()
 for size in sizes:
  if size==0xffffffff:continue
  blocks=struct.unpack_from('<%dI'%count(size),directory,off);off+=4*count(size);s=read(blocks,size);start=0
  while True:
   at=s.find(name+b'\0',start)
   if at<0:break
   start=at+len(name)+1
   if at>=14 and struct.unpack_from('<H',s,at-12)[0] in (0x110c,0x110d):
    value,segment=struct.unpack_from('<IH',s,at-6)
    if 0<segment<=len(pe.sections):found.add(pe.OPTIONAL_HEADER.ImageBase+pe.sections[segment-1].VirtualAddress+value)
 assert len(found)==1,(name,found);return found.pop()
def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def result(s):return C.c_int32(s.result).value
def codehash(m):
 h=hashlib.sha256()
 for sec in m.game.sections:
  if sec.Characteristics&0x20000000:h.update(m.u.mem_read(0x400000+sec.VirtualAddress,sec.Misc_VirtualSize))
 return h.hexdigest()
def ruleshash(m):return hashlib.sha256(b''.join(bytes(m.u.mem_read(t,0x2000)) for t in m.types)).hexdigest()
def typehash(s):
 h=1469598103934665603;data=struct.pack('<i',s.data.typeCount)
 for t in s.data.types[:s.data.typeCount]:data+=bytes(t)[:24]+struct.pack('<ii',t.kind,t.index)
 for v in data:h=((h^v)*1099511628211)&0xffffffffffffffff
 return h or 1
def fixture(session=3,foreign=None):
 spec=importlib.util.spec_from_file_location('parameter_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 pdb=Path(a.dll).with_suffix('.pdb');m.apply=function_address(m.dll,pdb,b'_AssistTechUiParameter')
 m.policy=data_address(m.dll,pdb,b'techparameters::policy');m.implementation=data_address(m.dll,pdb,b'techImplementation')
 m.types=(m.engT,m.t,m.buildT);m.w(0xa8b238,session);m.w(0xa8b230,m.alloc());m.w(m.house+0x10,77)
 for uid,(typ,vt) in enumerate(((m.t,0x7F6218),(m.engT,0x7EB610),(m.buildT,0x7E4570)),100):m.w(typ,vt);m.w(typ+0x10,uid)
 for tab in range(4):m.w(0x87f7e8+0x1544+tab*3988+84,0)
 m.dr=list(foreign or [0,0,0,0,0,0]);m.dr_writes=[];m.jobs=[];m.vehs=set();m.removed_vehs=[]
 def hook(at,fn):return m.u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,_:fn(),begin=at,end=at)
 def override(name,fn):
  for at,n in m.imports.items():
   if n==name:m.u.hook_del(m.native_stub_hooks[at]);hook(at,fn)
 def arg(i):return m.r(m.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 def add_veh():
  if arg(0)==0:m.exception_observer=arg(1);m.ret(0x12345678,2)
  else:assert arg(0)==1;m.vehs.add(arg(1));m.ret(0x7171,2)
 def remove_veh():m.removed_vehs.append(tuple(m.dr));m.vehs.clear();m.ret(1,1)
 def create():
  handle=0x61000000+len(m.jobs)*16;m.jobs.append({'handle':handle,'entry':arg(2),'param':arg(3),'done':False});m.ret(handle,6)
 def wait():
  job=next((j for j in m.jobs if j['handle']==arg(0)),None);m.ret(0 if job and job['done'] else 258,2)
 def getcontext():
  for i,v in enumerate(m.dr):m.w(arg(1)+4+i*4,v)
  m.ret(1,2)
 def setcontext():m.dr=[m.r(arg(1)+4+i*4) for i in range(6)];m.dr_writes.append(tuple(m.dr));m.ret(1,2)
 override('AddVectoredExceptionHandler',add_veh);override('RemoveVectoredExceptionHandler',remove_veh);override('CreateThread',create);override('WaitForSingleObject',wait)
 override('SuspendThread',lambda:m.ret(0,1));override('ResumeThread',lambda:m.ret(1,1));override('GetThreadContext',getcontext);override('SetThreadContext',setcontext)
 def flush():
  for j in m.jobs:
   if not j['done']:m.run(j['entry'],(j['param'],));j['done']=True
 m.flush=flush
 s=state(m);s.flags=4096;s.externalDisplay=1;save(m,s);m.hook();m.original_code=codehash(m);m.original_rules=ruleshash(m);return m
def tick(m):m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def send(m,mode=4,on=True,command=10,arg=None):
 s=state(m);s.command=command;s.arg=(1 if on else 0) if arg is None else arg;s.arg2=mode;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;s.requestTypeHash=typehash(s)
 for i in range(512):s.techAllowed[i]=int(i<s.data.typeCount)
 save(m,s);s=tick(m);assert s.ack==s.request,('ack',s.ack,s.request);return s
def settle(m):
 for _ in range(8):m.flush();s=tick(m)
 return s
def ready(m):
 s=send(m);assert result(s)==-45 and s.data.techOpen==0 and s.techParameterStatus==2,(result(s),s.data.techOpen,s.techParameterStatus)
 s=settle(m);assert s.data.techOpen==5 and s.data.techError==0 and s.techParameterStatus==1 and s.techParameterSlots==2,(s.data.techOpen,s.data.techError,s.techParameterStatus)
 return s
def invoke(m,tag=0,house=None,pointer=None,limit=0,include=None,site=None):
 context=m.alloc();args=m.alloc();site=(0x6a97d2 if tag==0 else 0x6aa781) if site is None else site
 m.w(context+172,m.house if house is None else house);m.w(context+184,site);m.w(context+196,args)
 values=(m.t if pointer is None else pointer,limit,tag if include is None else include)
 m.u.mem_write(args,struct.pack('<3I',*values));before=bytes(m.u.mem_read(context,716));sp=m.STACK+0x1d000
 m.u.mem_write(sp,struct.pack('<3I',m.STOP,context,tag));m.u.reg_write(UC_X86_REG_ESP,sp);m.u.emu_start(m.apply,m.STOP,count=200000)
 assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP and m.u.reg_read(UC_X86_REG_ESP)==sp+4
 assert bytes(m.u.mem_read(context,716))==before,'callback changed CONTEXT'
 after=struct.unpack('<3I',m.u.mem_read(args,12));assert after[0]==values[0] and after[2]==values[2]
 return m.u.reg_read(UC_X86_REG_EAX),after
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**(fn() or {})});print('PASS',name,flush=True)
 except Exception as ex:rows.append({'case':name,'status':'FAIL','error':repr(ex)});traceback.print_exc()
def cold(session):
 m=fixture(session);s=ready(m);epoch=s.data.match
 assert len(m.sidebar_add_calls)==3 and s.data.techVisible==3
 assert codehash(m)==m.original_code and ruleshash(m)==m.original_rules
 for command in (12,22):assert result(send(m,command=command,arg=0))==-4
 for tag in (0,1):assert invoke(m,tag)==(1,(m.t,1,tag))
 s.foreground=0;save(m,s);s=tick(m);assert s.techParameterSlots==2 and s.data.techOpen==5 and s.data.match==epoch
 s=send(m,on=False);assert s.data.techOpen==0;s=settle(m);assert s.techParameterSlots==0 and not m.vehs and not any(m.dr[:4])
 assert invoke(m)[0]==0;ready(m);assert codehash(m)==m.original_code and ruleshash(m)==m.original_rules
 return {'native_icons':3,'same_world_background_retained':True,'only_stack_bool_changed':True,'text_and_type_bytes_unchanged':True,'toolbox_produce_blocked':True}
for session in (0,3):check('cold_native_parameters_session_'+str(session),lambda session=session:cold(session))
def gates():
 m=fixture();ready(m)
 for kwargs in ({'house':m.enemy},{'pointer':m.enemyUnit},{'limit':1},{'include':1},{'tag':1,'include':0},{'site':0x4f7870}):assert invoke(m,**kwargs)[0]==0,kwargs
 for address,value in ((m.house+0x10,78),(m.t+0x10,999),(m.t,0x12345678),(0xa8b230,m.alloc()),(0xa8b238,5),(0xa8ed84,0),(0xa8e9a0,0),(m.implementation,0),(m.SH+m.Shared.stop.offset,1),(m.SH+m.Shared.flags.offset,0)):
  old=m.r(address);m.w(address,value);assert invoke(m)[0]==0,hex(address);m.w(address,old)
 old=bytes(m.u.mem_read(0x6a97c3,1));m.u.mem_write(0x6a97c3,b'\x90');assert invoke(m)[0]==0 and m.r(m.policy)==0;m.u.mem_write(0x6a97c3,old)
 return {'wrong_house_type_uid_vtable_args_site_world_and_stop_rejected':True,'exact_full_argument_instruction_signature_checked':True}
check('compiled_callback_identity_and_immediate_gates',gates)
def occupied():
 foreign=[0x12340000,0x12340010,0x12340020,0x12340030,0,1];m=fixture(foreign=foreign);send(m);s=settle(m)
 assert s.techParameterStatus==-2 and s.techParameterSlots==0 and m.dr==foreign and not m.dr_writes and not m.sidebar_add_calls
 assert codehash(m)==m.original_code and ruleshash(m)==m.original_rules
 return {'no_free_slots_refused_without_any_debug_context_write':True}
check('occupied_debug_slots_fail_closed',occupied)
def signature():
 m=fixture();m.u.mem_write(0x6a97c3,b'\x90');changed=codehash(m);s=send(m)
 assert s.techParameterStatus==-1 and not m.jobs and not m.dr_writes and not m.sidebar_add_calls
 assert codehash(m)==changed and ruleshash(m)==m.original_rules
check('full_instruction_signature_incompatible_refused',signature)
def production(old,new):
 m=fixture()
 if old==4:ready(m)
 else:s=send(m,old);assert s.data.techOpen==old+1
 f=m.alloc();m.w(m.house+0x53b4,f);m.w(f+0x6c,m.house);m.w(f+0x58,m.unit)
 before_code=codehash(m);before_dr=tuple(m.dr);before_rules=ruleshash(m);s=send(m,new)
 assert result(s)==-43 and s.data.techOpen==old+1 and tuple(m.dr)==before_dr
 assert codehash(m)==before_code and ruleshash(m)==before_rules
 # A native-eligible product is allowed; the exact pending legacy product
 # is what blocks the asynchronous eligibility gap, not all busy factories.
 m.canbuild_full_result=1;s=send(m,new);assert result(s)!=-43
 return {'old':old,'new':new,'non_native_production_retains_previous_mode':True,'native_eligible_busy_product_allowed':True}
for old,new in ((0,4),(1,4),(2,4),(4,0),(4,1),(4,2),(4,3)):check('production_transition_%d_to_%d'%(old,new),lambda old=old,new=new:production(old,new))
def enhancement():
 m=fixture();m.u.mem_write(m.t+0x24,b'HOWI'+bytes(21));m.w(m.t+0x634,-1);s=state(m);s.techEnhance=1;save(m,s);tick(m);assert m.r(m.t+0x634)==1
 s=ready(m);assert s.techEnhance==1 and m.r(m.t+0x634)==0xffffffff and s.techEnhanceStatus==0
 for _ in range(12):s=tick(m);assert m.r(m.t+0x634)==0xffffffff and s.techEnhance==1
 send(m,on=False);settle(m)
 for _ in range(8):s=tick(m)
 assert m.r(m.t+0x634)==1 and s.techEnhance==1
 return {'saved_enhance_preference_preserved':True,'rule_writer_blocked_for_entire_parameter_mode':True,'preference_resumes_after_exit':True}
check('independent_enhancement_full_lifecycle_handoff',enhancement)
def stop():
 m=fixture();ready(m);s=state(m);s.stop=1;save(m,s);s=tick(m);assert s.stop!=2 and m.vehs
 assert invoke(m)[0]==0;m.flush();s=tick(m);assert s.stop==2 and not m.vehs and not any(m.dr[:4])
 assert m.removed_vehs and not any(m.removed_vehs[-1][:4]);assert ruleshash(m)==m.original_rules
 return {'stop_ack_waits_for_confirmed_dr_restore_and_own_veh_removal':True}
check('stop_waits_for_hardware_resources',stop)
final_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();assert initial_hash==final_hash,'DLL changed during fixture'
report={'scope':__doc__,'dll_sha256':final_hash,'hash_stable_during_test':True,'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'windows_debug_api_modeled':True,'game_process_tested':False,'multiplayer_tested':False,'results':rows}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise SystemExit(any(r['status']!='PASS' for r in rows))
