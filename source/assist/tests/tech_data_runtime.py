"""Compiled Runtime rule-data technology lifecycle with synthetic world/Win32/native UI.
The actual native CanBuild/FindFactory predicates are tested separately by
 tools/audit_tech_data_native.py. This is not a running game or netplay test.
"""
import argparse, ctypes as C, hashlib, importlib.util, json, struct, traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
initial_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest()
FIELDS=((0x634,4,0),(0x648,4,0),(0x6CC,4,0xffffffff),(0xDA0,4,0xffffffff),(0xDA4,4,0),(0xC98,1,0),(0xD9B,1,0),(0xD9C,1,0),(0xD9D,1,0))
ORIGINAL=(5,1,1,2,1,1,1,1,1)

def state(m):return m.Shared.from_buffer_copy(m.u.mem_read(m.SH,C.sizeof(m.Shared)))
def save(m,s):m.u.mem_write(m.SH,bytes(s))
def result(s):return C.c_int32(s.result).value
def tick(m):m.w(0xa8ed84,m.r(0xa8ed84)+1);return m.hook()
def value(m,typ,f):return int.from_bytes(m.u.mem_read(typ+f[0],f[1]),'little')
def fields(m,typ):return tuple(value(m,typ,f) for f in FIELDS)
def codehash(m):
 h=hashlib.sha256()
 for sec in m.game.sections:
  if sec.Characteristics&0x20000000:h.update(m.u.mem_read(m.game.OPTIONAL_HEADER.ImageBase+sec.VirtualAddress,sec.Misc_VirtualSize))
 return h.hexdigest()
def typehash(s):
 h=1469598103934665603
 data=struct.pack('<i',s.data.typeCount)
 for t in s.data.types[:s.data.typeCount]:data+=bytes(t)[:24]+struct.pack('<ii',t.kind,t.index)
 for v in data:h=((h^v)*1099511628211)&0xffffffffffffffff
 return h or 1

def fixture(session=3):
 spec=importlib.util.spec_from_file_location('tech_fixture',Path(__file__).with_name('entry_runtime_regression.py'));m=importlib.util.module_from_spec(spec);m.fixture_paths=[a.game,a.dll];spec.loader.exec_module(m)
 m.w(0xa8b238,session);m.w(0xa8b54c,1);m.w(0xa8b230,m.alloc())
 m.types=(m.engT,m.t,m.buildT)
 for uid,(typ,vt) in enumerate(((m.t,0x7F6218),(m.engT,0x7EB610),(m.buildT,0x7E4570)),100):
  m.w(typ,vt);m.w(typ+0x10,uid);m.w(typ+0xDF8,0)
  for f,v in zip(FIELDS,ORIGINAL):m.u.mem_write(typ+f[0],int(v).to_bytes(f[1],'little'))
  m.w(typ+0x63C,m.alloc(16));m.w(typ+0x640,1);m.w(typ+0x3B8,3)
 for tab in range(4):m.w(0x87f7e8+0x1544+tab*3988+84,0)
 m.full_override=None;m.full_before_data=[]
 m.u.hook_del(m.native_stub_hooks[0x4f7870])
 def canbuild(uc,at,size,_):
  sp=uc.reg_read(UC_X86_REG_ESP);args=tuple(m.r(sp+4+i*4) for i in range(3));m.canbuild_calls.append((uc.reg_read(UC_X86_REG_ECX),*args))
  if args[1]:result=1
  else:
   result=int(fields(m,args[0])==tuple(f[2] for f in FIELDS)) if m.full_override is None else m.full_override
   m.full_before_data.append((args[0],fields(m,args[0])))
  m.ret(result,3)
 m.u.hook_add(UC_HOOK_CODE,canbuild,begin=0x4f7870,end=0x4f7870)
 s=state(m);s.flags=0;s.externalDisplay=1;save(m,s);m.original_code=codehash(m);return m

def send(m,mode=3,on=True,command=10,arg=None):
 s=state(m);s.command=command;s.arg=(1 if on else 0) if arg is None else arg;s.arg2=mode;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;s.requestTypeHash=typehash(s)
 for i in range(512):s.techAllowed[i]=int(i<s.data.typeCount)
 save(m,s);s=tick(m);assert s.ack==s.request,('ack',s.ack,s.request,s.data.commandStatus);return s

def activate(m):
 s=state(m);s.flags=4096;save(m,s);s=tick(m);assert s.data.typeCount==3,('catalogue',s.data.typeCount,s.data.status,s.data.commandStatus)
 s=send(m);ready(m,s);return s

def ready(m,s):
 assert s.result==1 and s.data.techOpen==4 and s.data.techError==0,(s.result,s.data.techOpen,s.data.techError,s.data.commandStatus)
 assert all(fields(m,t)==tuple(f[2] for f in FIELDS) for t in m.types)
 assert all(m.r(t+0x3B8)==3 and m.r(t+0x610)==500 and m.r(t+0x640)==1 for t in m.types)
 assert codehash(m)==m.original_code,'rule-data mode changed executable section'

def installed(m):
 at=m.SH+0x700000;saved=bytes(m.u.mem_read(at,108));q=m.request(3);result,stats=m.manager_call(q);m.u.mem_write(at,saved);assert result==1;return stats.installed

def original(m):return all(fields(m,t)==ORIGINAL for t in m.types)
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**(fn() or {})});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()

def cold(session):
 m=fixture(session);m.w(m.SH+0x700000+16,0);m.w(0xa8ed84,5000);s=tick(m);assert s.seq==0 and original(m)
 m.w(m.SH+0x700000+16,1);s=activate(m);epoch=s.data.match;assert len(m.sidebar_add_calls)==3 and s.data.techVisible==3 and installed(m)==0
 assert all(row[1]==tuple(f[2] for f in FIELDS) for row in m.full_before_data)
 for command in (12,22):assert result(send(m,command=command,arg=0))==-4
 assert m.r(0x8b41f8)==0 and m.r(0xa802c8)==0
 s=send(m,on=False);assert s.result==1 and s.data.techOpen==0 and original(m)
 s=send(m);ready(m,s);assert s.data.match==epoch and installed(m)==0
 return {'same_epoch_on_off_on':True,'native_icons':3,'feature_code_patches':0,'toolbox_production_rejected':True}
for session in (0,3):check('cold_rule_data_session_'+str(session),lambda session=session:cold(session))

def switches():
 m=fixture();activate(m)
 for mode,count in ((0,2),(1,2),(2,5)):
  s=send(m,mode);assert s.result==1 and s.data.techOpen==mode+1 and original(m) and installed(m)==count,(mode,s.result,s.data.techError,installed(m))
  s=send(m);ready(m,s);assert installed(m)==0
 return {'data_to_all_three_old_modes_and_back':True,'old_patch_counts':[2,2,5]}
check('all_retained_mode_transitions',switches)

def gates():
 m=fixture();m.full_override=0;s=activate(m);assert s.data.techVisible==0 and not m.sidebar_add_calls
 m.full_override=None;m.factory_available=False;s=send(m);assert s.data.techVisible==0 and not m.sidebar_add_calls
 m.factory_available=True;s=send(m);ready(m,s);assert s.data.techVisible==3 and len(m.sidebar_add_calls)==3
 return {'full_native_rule_and_factory_admission_required':True}
check('native_icon_admission',gates)

def factory(m,product):
 f=m.alloc();m.w(m.house+0x53B4,f);m.w(f+0x6C,m.house);m.w(f+0x58,product);return f

def pending():
 m=fixture();s=state(m);s.flags=4096;save(m,s);tick(m);s=send(m,0);assert s.data.techOpen==1;f=factory(m,m.unit)
 s=send(m);assert result(s)==-43 and s.data.techOpen==1 and installed(m)==2 and original(m)
 m.w(f+0x58,0)
 for _ in range(12):s=tick(m)
 assert s.data.techError==0
 s=send(m);ready(m,s);m.w(f+0x58,m.unit);s=send(m,2);assert result(s)==-43 and s.data.techOpen==4 and installed(m)==0
 m.w(f+0x58,0)
 for _ in range(12):s=tick(m)
 assert s.data.techError==0;s=send(m,2);assert s.result==1 and s.data.techOpen==3 and original(m)
 return {'pending_production_preserves_actual_mode_both_directions':True,'cleared_production_allows_explicit_retry':True}
check('production_safe_mode_switch',pending)

def foreign():
 m=fixture();activate(m);m.w(m.t+0xDA4,77);s=send(m,on=False);assert result(s)==-44 and s.data.techError==-44 and m.r(m.t+0xDA4)==77
 s.stop=1;save(m,s);s=tick(m);assert s.stop!=2 and s.data.techError==-44 and m.r(m.t+0xDA4)==77,(s.stop,s.data.techError)
 assert installed(m)==0
 m.w(m.t+0xDA4,ORIGINAL[4]);s=tick(m);assert s.stop==2 and original(m),(s.stop,s.data.techError)
 return {'foreign_value_not_overwritten':True,'reset_preserves_minus44_until_real_restore':True}
check('foreign_field_stop_pending_and_recovery',foreign)

def focus():
 m=fixture();s=activate(m);epoch=s.data.match;s.foreground=0;save(m,s);s=tick(m);assert s.data.techOpen==4 and fields(m,m.t)==tuple(f[2] for f in FIELDS)
 m.w(0xa8eda0,1);s=tick(m);assert s.data.commandStatus==10 and s.data.techOpen==4
 m.w(0xa8eda0,0);m.b(0xa8e9a0,0);s.flags=0;save(m,s);s=tick(m);assert s.data.commandStatus==9 and s.data.techOpen==0 and original(m)
 assert s.data.match==epoch and codehash(m)==m.original_code
 return {'background_menu_retains_data':True,'paused_explicit_disable_restores':True}
check('focus_menu_and_paused_off',focus)

def enhancement():
 m=fixture();m.u.mem_write(m.t+0x24,b'HOWI'+bytes(21));m.w(m.t+0x634,-1);s=state(m);s.techEnhance=1;s.flags=4096;save(m,s);s=tick(m);assert m.r(m.t+0x634)==1 and s.data.typeCount==3,(m.r(m.t+0x634),s.techEnhanceStatus,s.data.typeCount)
 s=send(m);ready(m,s);assert s.techEnhance==1 and s.techEnhanceStatus==0
 s=send(m,on=False);assert s.result==1 and m.r(m.t+0x634)==0xffffffff
 for _ in range(8):s=tick(m)
 assert m.r(m.t+0x634)==1 and s.techEnhance==1
 s.techEnhance=0;save(m,s);s=tick(m);assert m.r(m.t+0x634)==0xffffffff
 return {'enhancement_restored_before_data_original_capture':True,'saved_preference_resumes_after_data_release':True}
check('enhancement_ownership_handoff',enhancement)

def invalid():
 m=fixture();s=state(m);s.flags=4096;save(m,s);tick(m);m.w(m.t,0x12345678);s=send(m)
 assert result(s)==-44 and s.data.techOpen==0 and not m.sidebar_add_calls and original(m) and installed(m)==0,(s.result,s.data.techOpen,s.data.techError)
 m.w(m.t,0x7F6218)
 for _ in range(12):s=tick(m)
 assert s.data.techOpen==4 and s.data.techError==0 and s.data.techVisible==3
 assert codehash(m)==m.original_code
 return {'partial_group_rolls_back_before_icons':True,'identity_recovery_retries_pending_mode':True}
check('invalid_type_partial_group_and_retry',invalid)
final_hash=hashlib.sha256(Path(a.dll).read_bytes()).hexdigest();assert initial_hash==final_hash,'DLL changed during fixture'
report={'scope':__doc__,'dll_sha256':final_hash,'hash_stable_during_test':True,'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
raise SystemExit(any(r['status']!='PASS' for r in rows))
