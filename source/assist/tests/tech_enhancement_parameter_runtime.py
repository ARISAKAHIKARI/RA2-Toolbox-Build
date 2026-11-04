"""Compiled exact TechLevel Runtime + native CanBuild in an isolated x86 world.
Windows debug APIs are modeled; native DR/VEH is a separate own-process fixture.
The compiled classifier changes EFLAGS at actual private CanBuild Jcc boundaries.
No game process or multiplayer session is opened.
"""
import argparse,ctypes as C,gc,hashlib,json,struct,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser(description=__doc__);p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);a=p.parse_args();rows=[]
repo=Path(__file__).resolve().parents[3]
source_paths=['source/assist/tests/tech_enhancement_parameter_runtime.py','source/assist/tests/tech_ui_parameter_runtime.py','source/assist/tests/entry_runtime_regression.py','source/assist/tests/pdb_fixture_symbol.py','source/assist/source/bridge.cpp','source/assist/source/protocol.h','source/assist/source/tech_ui_breakpoint_core.h','source/assist/source/tech_ui_breakpoint_bridge.h','source/assist/source/tech_ui_breakpoint_guard.h','source/assist/source/tech_enhancement_parameter_policy.h','source/assist/source/tech_enhancement.h','source/assist/source/tech_enhancement_bridge.h','source/assist/source/tech_data_bridge.h','source/assist/source/sidebar.h']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();source_hashes={n:sha(repo/n)for n in source_paths};initial_dll=sha(a.dll);initial_pdb=sha(Path(a.dll).with_suffix('.pdb'));initial_game=sha(a.game)

provider_path=Path(__file__).with_name('tech_ui_parameter_runtime.py');source=provider_path.read_text(encoding='utf-8').split('for session in (0,3):check(',1)[0];source=source.replace(' def remove_veh():m.removed_vehs.append(tuple(m.dr));m.vehs.clear();m.ret(1,1)', ' def remove_veh():\n  if getattr(m,\"remove_veh_fails\",False):m.ret(0,1);return\n  m.removed_vehs.append(tuple(m.dr));m.vehs.clear();m.ret(1,1)')
ns={'__file__':str(provider_path),'__name__':'enhancement_provider'};exec(compile(source,str(provider_path),'exec'),ns)
state,save,tick,send,settle,ready,codehash,ruleshash=(ns[x]for x in ('state','save','tick','send','settle','ready','codehash','ruleshash'))
from pdb_fixture_symbol import function_address
reg_offsets={UC_X86_REG_EDI:156,UC_X86_REG_ESI:160,UC_X86_REG_EBX:164,UC_X86_REG_EDX:168,UC_X86_REG_ECX:172,UC_X86_REG_EAX:176,UC_X86_REG_EBP:180,UC_X86_REG_EIP:184,UC_X86_REG_EFLAGS:192,UC_X86_REG_ESP:196}
def check(name,fn):
 try:rows.append({'case':name,'status':'PASS',**(fn()or{})});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':repr(e)});traceback.print_exc()
 finally:gc.collect()
def fixture(session=3,foreign=None):
 m=ns['fixture'](session,foreign);m.u.mem_write(m.t+0x24,b'BUS'+bytes(21));m.w(m.t+0x634,-1);m.w(m.t+0xda0,0xffffffff);m.w(m.t+0xda4,0);m.w(m.t+0x3b8,100)
 houseType=m.alloc();m.w(m.house+0x34,houseType);m.w(m.house+0x1d4,10);m.b(m.house+0x1ec,1)
 for off in (0x5500,0x5514,0x5528,0x553c,0x5550,0x55a0,0x55b4,0x55c8,0x55dc):m.w(m.house+off+4,m.alloc(64));m.w(m.house+off+8,16)
 m.u.hook_del(m.native_stub_hooks[0x4f7870]);
 def malloc(uc,pc,size,_):m.ret(m.alloc(m.r(m.u.reg_read(UC_X86_REG_ESP)+4)),0)
 m.u.hook_add(UC_HOOK_CODE,malloc,begin=0x7c8e17,end=0x7c8e17);m.u.hook_add(UC_HOOK_CODE,lambda uc,pc,size,_:m.ret(0,0),begin=0x7c8b3d,end=0x7c8b3d)
 m.hits=[];m.pending_bp=None;m.skip_bp=None
 def hook(uc,pc,size,_):
  if m.skip_bp==pc:m.skip_bp=None;return
  tag={0x6a97d2:0,0x6aa781:1,0x4f7951:2,0x4f7b66:3}.get(pc)
  if tag is None or not any(m.dr[n]==pc and (m.dr[5]&(3<<(2*n))) for n in range(4)):return
  m.pending_bp=tag;uc.emu_stop()
 m.u.hook_add(UC_HOOK_CODE,hook)
 # Pause at an owned DR site, execute the real compiled callback on a separate
 # synthetic stack, then resume the unchanged native Jcc using its new flags.
 # This emulates exception dispatch; true Windows dispatch is tested separately.
 def run(address,args=(),self=0):
  if address==m.entry and args==(0,1,0):m.w(m.SH+0x700000+20,m.r(m.SH+0x700000+20)|1);args=(0,1,m.SH+0x700100)
  sp=m.STACK+0x1e000;m.u.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),m.STOP,*args));m.u.reg_write(UC_X86_REG_ESP,sp);m.u.reg_write(UC_X86_REG_ECX,self)
  for attempts in range(200):
   m.u.emu_start(address,m.STOP,count=2000000)
   if m.pending_bp is None:break
   tag=m.pending_bp;m.pending_bp=None;outer=m.u.context_save();pc=m.u.reg_read(UC_X86_REG_EIP);context=m.alloc(716);before=bytearray(716)
   for reg,off in reg_offsets.items():struct.pack_into('<I',before,off,m.u.reg_read(reg))
   m.u.mem_write(context,bytes(before));callback_sp=m.STACK+0x10000;m.u.mem_write(callback_sp,struct.pack('<3I',m.STOP,context,tag));m.u.reg_write(UC_X86_REG_ESP,callback_sp);m.u.emu_start(m.apply,m.STOP,count=200000)
   assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP
   after=bytes(m.u.mem_read(context,716));changed=m.u.reg_read(UC_X86_REG_EAX);m.hits.append((tag,changed))
   if tag>=2:
    assert after[:192]==bytes(before[:192]) and after[196:]==bytes(before[196:]);flags=struct.unpack_from('<I',after,192)[0]
   m.u.context_restore(outer)
   if tag>=2:m.u.reg_write(UC_X86_REG_EFLAGS,flags)
   m.skip_bp=pc;address=pc
  else:raise AssertionError('too many modeled exceptions')
  assert m.u.reg_read(UC_X86_REG_EIP)==m.STOP,'instruction budget'
  assert m.u.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args),'stack imbalance'
 m.run=run
 m.original_code=codehash(m);m.original_rules=ruleshash(m);return m

def enhance(m,on=True,implementation=1):
 s=state(m);s.techEnhance=int(on);s.techEnhanceImplementation=implementation;save(m,s)
 for _ in range(10):m.flush();s=tick(m)
 return s

def cold(session):
 m=fixture(session);s=enhance(m);assert s.techEnhanceParameterStatus==1 and s.techEnhanceParameterSlots==2,(s.techEnhanceParameterStatus,s.techEnhanceParameterSlots,s.data.techError,m.hits)
 assert m.r(m.t+0x634)==0xffffffff and codehash(m)==m.original_code and ruleshash(m)==m.original_rules
 assert (40,0)in m.sidebar_add_calls and any(tag==2 and changed for tag,changed in m.hits) and any(tag==3 and changed for tag,changed in m.hits),m.sidebar_add_calls
 assert s.techEnhanceStatus==0 # legacy writer stays off for the entire mode
 s=enhance(m,False);assert s.techEnhanceParameterSlots==0 and not m.vehs and not any(m.dr[:4]);assert ruleshash(m)==m.original_rules
 return {'native_private_CanBuild_and_compiled_EFLAGS_classifier':True,'native_cameo_added_without_Type_or_text_writes':True,'full_legacy_writer_blocked':True}
for session in (0,3):check('exact_enhancement_cold_session_'+str(session),lambda session=session:cold(session))

def gated(field,value,byte=False):
 m=fixture();m.w(m.t+field,value) if not byte else m.b(m.t+field,value);m.original_rules=ruleshash(m);s=enhance(m)
 assert s.techEnhanceParameterStatus==1 and not m.sidebar_add_calls,(field,m.sidebar_add_calls,m.hits)
 assert ruleshash(m)==m.original_rules and codehash(m)==m.original_code
 return {'other_native_condition_preserved':hex(field)}
for field,value,byte in ((0xc98,1,True),(0xda0,2,False),(0xda4,1,False),(0xd9b,1,True),(0xd9c,1,True),(0xd9d,1,True),(0x3b8,0,False)):
 check('native_gate_'+hex(field),lambda field=field,value=value,byte=byte:gated(field,value,byte))

def coexist():
 m=fixture();s=ready(m);before=m.dr[:];s=enhance(m);assert s.techParameterStatus==1 and s.techEnhanceParameterStatus==1 and s.techParameterSlots==2 and s.techEnhanceParameterSlots==2
 assert m.dr[0:2]==before[0:2]
 s=enhance(m,False);assert s.techParameterSlots==2 and s.techEnhanceParameterSlots==0 and m.dr[0:2]==before[0:2]
 assert codehash(m)==m.original_code and ruleshash(m)==m.original_rules
 return {'four_sites_share_one_service':True,'partial_enhancement_off_retains_full_slots':True}
check('mode4_and_exact_enhancement_coexist_then_partial_off',coexist)

def slots():
 foreign=[0x12340000,0x12340010,0,0,0,5];m=fixture(foreign=foreign);s=ready(m);before=m.dr[:];s=enhance(m)
 assert s.techParameterStatus==1 and s.techEnhanceParameterStatus==-2 and m.dr==before,(s.techParameterStatus,s.techEnhanceParameterStatus,m.dr,before)
 assert s.techParameterSlots==2 and s.techEnhanceParameterSlots==0
 return {'existing_full_ready_permission_and_foreign_slots_preserved':True}
check('insufficient_slots_refuses_only_new_enhancement',slots)


def cdecl(m,name,args=()):
 address=function_address(m.dll,Path(a.dll).with_suffix('.pdb'),name);stub=m.alloc(128);code=bytearray()
 for arg in reversed(args):code+=b'\x68'+struct.pack('<I',arg&0xffffffff)
 code+=b'\xe8'+struct.pack('<I',(address-(stub+len(code)+5))&0xffffffff)
 if args:code+=b'\x83\xc4'+bytes([len(args)*4])
 code+=b'\xc3';m.u.mem_write(stub,bytes(code));m.run(stub);return m.u.reg_read(UC_X86_REG_EAX)
def probe(m):
 token=cdecl(m,b'_BeginAssistCapture');assert token
 try:return C.c_int32(cdecl(m,b'_GuardedTechEnhancementProbe',(m.house,m.t))).value
 finally:cdecl(m,b'_EndAssistCapture',(token,))
def invoke_exact(m,tag=2,return_address=0x6aa786,include=1,house=None,pointer=None,limit=0):
 house=m.house if house is None else house;pointer=m.t if pointer is None else pointer;context=m.alloc(716);stack=m.alloc(256);m.w(stack+0x10,house);m.w(stack+0x4c,return_address);m.w(stack+0x50,pointer);m.w(stack+0x54,limit);m.w(stack+0x58,include)
 for offset,value in ((156,pointer),(164,10),(172,pointer),(180,house),(184,0x4f7951 if tag==2 else 0x4f7b66),(192,0x10202),(196,stack)):m.w(context+offset,value)
 before=bytes(m.u.mem_read(context,716));result=cdecl(m,b'_GuardedTechUiParameter',(context,tag));after=bytes(m.u.mem_read(context,716))
 assert before[:192]==after[:192]and before[196:]==after[196:]
 if not result:assert before==after
 else:assert (m.r(context+192)&~0x8d5)==(int.from_bytes(before[192:196],'little')&~0x8d5)
 return result

def exact_gates():
 m=fixture();enhance(m)
 assert invoke_exact(m)==1 and invoke_exact(m,3)==1
 for kwargs in ({'house':m.enemy},{'pointer':m.engT},{'limit':1},{'return_address':0x20200000},{'include':0}):assert invoke_exact(m,**kwargs)==0,kwargs
 for address,value in ((m.house+0x10,78),(m.t+0x10,999),(m.t,0x12345678),(m.t+0x634,1),(0xa8b230,m.alloc()),(0xa8b238,5),(0xa8ed84,0),(0xa8e9a0,0),(m.SH+m.Shared.techEnhance.offset,0),(m.SH+m.Shared.stop.offset,1)):
  original=m.r(address);m.w(address,value);assert invoke_exact(m)==0,hex(address);m.w(address,original)
 assert ruleshash(m)==m.original_rules and codehash(m)==m.original_code
 return {'House_Type_UID_level_world_caller_args_stop_gates':True,'only_arithmetic_EFLAGS_changed':True}
check('compiled_exact_callback_identity_world_caller_and_flags_gates',exact_gates)

def reverse_slots():
 m=fixture(foreign=[0x12340000,0x12340010,0,0,0,5]);s=enhance(m);before=m.dr[:];assert s.techEnhanceParameterStatus==1
 send(m);s=settle(m);assert s.techEnhanceParameterStatus==1 and s.techParameterStatus==-2 and m.dr==before,(s.techEnhanceParameterStatus,s.techParameterStatus,m.dr)
 assert probe(m)==1
 return {'existing_exact_enhancement_preserved_when_new_full_cannot_get_two_slots':True}
check('insufficient_slots_refuses_only_new_full_permission',reverse_slots)

def mode_hold():
 m=fixture();enhance(m);f=m.alloc();m.w(m.house+0x53b4,f);m.w(f+0x6c,m.house);m.w(f+0x58,m.unit)
 s=enhance(m,True,0);assert s.techEnhanceParameterStatus==-7 and s.techEnhanceParameterSlots==2 and m.r(m.t+0x634)==0xffffffff,(s.techEnhanceParameterStatus,s.techEnhanceParameterSlots,m.r(m.t+0x634))
 assert probe(m)==1
 m.w(f+0x58,0)
 for _ in range(12):m.flush();s=tick(m)
 assert s.techEnhanceParameterSlots==0 and m.r(m.t+0x634)==1
 s=enhance(m,False,0);assert m.r(m.t+0x634)==0xffffffff
 return {'mode_switch_keeps_original_permission_until_dependent_product_finishes':True,'legacy_writer_waits_for_confirmed_hardware_retirement':True}
check('exact_to_legacy_switch_waits_for_dependent_production_then_restores',mode_hold)

def enhancement_stop():
 m=fixture();enhance(m);s=state(m);s.stop=1;save(m,s);assert invoke_exact(m)==0;s=tick(m);assert s.stop!=2 and m.vehs
 for _ in range(8):m.flush();s=tick(m)
 assert s.stop==2 and not m.vehs and not any(m.dr[:4]) and ruleshash(m)==m.original_rules and codehash(m)==m.original_code
 return {'stop_ack_after_confirmed_all_groups_and_VEH_retirement':True}
check('exact_enhancement_stop_waits_for_verified_full_cleanup',enhancement_stop)

def background():
 m=fixture();s=enhance(m);before=m.dr[:];s.foreground=0;save(m,s)
 for _ in range(8):m.flush();s=tick(m)
 assert s.techEnhanceParameterStatus==1 and m.dr==before and ruleshash(m)==m.original_rules
 return {'focus_switch_retains_hardware_ownership':True}
check('exact_enhancement_background_keeps_ownership',background)

def signature_conflict():
 m=fixture();m.u.mem_write(0x4f794a,b'\x90');changed=codehash(m);s=enhance(m)
 assert s.techEnhanceParameterStatus==-1 and not any(m.dr[:4]) and not m.dr_writes and not m.sidebar_add_calls
 assert codehash(m)==changed and ruleshash(m)==m.original_rules
 return {'unsupported_native_comparison_refused_before_DR_install':True}
check('exact_comparison_signature_changed_refused_without_game_writes',signature_conflict)


for owned in (False,True):
 def prerequisite(owned=owned):
  m=fixture();items=m.alloc(4);m.w(items,0);m.w(m.t+0x63c,items);m.w(m.t+0x640,1);m.w(m.t+0x648,1);m.w(m.t+0x64c,1)
  if owned:m.w(m.r(m.house+0x5550+4),1)
  m.original_rules=ruleshash(m);s=enhance(m);assert s.techEnhanceParameterStatus==1
  assert ((40,0)in m.sidebar_add_calls)==owned,(owned,m.sidebar_add_calls,m.hits)
  assert ruleshash(m)==m.original_rules
  return {'actual_native_Prerequisite_condition_preserved':True,'genuine_building_owned':owned}
 check('native_prerequisite_owned_'+str(owned),prerequisite)
def no_factory():
 m=fixture();m.factory_available=False;s=enhance(m);assert s.techEnhanceParameterStatus==1 and not m.sidebar_add_calls
 assert ruleshash(m)==m.original_rules and codehash(m)==m.original_code
 return {'no_icon_without_genuine_factory_model':True}
check('missing_factory_prevents_cameo',no_factory)


for queued in (False,True):
 def forward_handoff(queued=queued):
  m=fixture();enhance(m,True,0);assert m.r(m.t+0x634)==1;f=m.alloc();m.w(m.house+0x53b4,f);m.w(f+0x6c,m.house)
  if queued:
   items=m.alloc(4);m.w(items,m.t);m.w(f+0x44,items);m.w(f+0x50,1)
  else:m.w(f+0x58,m.unit)
  s=enhance(m,True,1);assert s.techEnhanceParameterStatus==-7 and s.techEnhanceParameterSlots==0 and m.r(m.t+0x634)==1,(s.techEnhanceParameterStatus,s.techEnhanceParameterSlots,m.r(m.t+0x634))
  assert not m.dr_writes and not m.vehs
  m.w(f+0x58,0);m.w(f+0x50,0)
  for _ in range(12):m.flush();s=tick(m)
  assert s.techEnhanceParameterStatus==1 and s.techEnhanceParameterSlots==2 and m.r(m.t+0x634)==0xffffffff
  assert codehash(m)==m.original_code and ruleshash(m)==m.original_rules
  return {'old_permission_kept_until_dependent_legacy_product_finishes':True,'queued_product':queued,'no_invalid_Type_minus1_without_ready_DR_gap':True}
 check('legacy_to_exact_handoff_owned_queue_'+str(queued),forward_handoff)
def ordinary_product_not_blocked():
 m=fixture();enhance(m,True,0);f=m.alloc();m.w(m.house+0x53b4,f);m.w(f+0x6c,m.house);m.w(f+0x58,m.eng)
 s=enhance(m,True,1);assert s.techEnhanceParameterStatus==1 and m.r(m.t+0x634)==0xffffffff
 return {'ordinary_native_product_does_not_block_legacy_handoff':True}
check('ordinary_product_does_not_block_legacy_to_exact_handoff',ordinary_product_not_blocked)
for target in (3,4):
 def full_handoff(target=target):
  m=fixture();enhance(m,True,0);f=m.alloc();m.w(m.house+0x53b4,f);m.w(f+0x6c,m.house);m.w(f+0x58,m.unit)
  s=send(m,target);assert C.c_int32(s.result).value==-43 and s.data.techOpen==0 and m.r(m.t+0x634)==1,(target,C.c_int32(s.result).value,s.data.techOpen,m.r(m.t+0x634))
  assert not m.dr_writes
  return {'new_full_mode_request_preserves_legacy_enhancement_until_dependent_product_completes':True}
 check('legacy_enhancement_busy_blocks_full_mode_'+str(target),full_handoff)


def failed_veh_removal():
 m=fixture();enhance(m);m.remove_veh_fails=True;s=enhance(m,False)
 assert not any(m.dr[:4]) and m.vehs and s.techEnhanceParameterStatus==-3 and s.techParameterStatus==0,(s.techEnhanceParameterStatus,s.techParameterStatus,m.dr,m.vehs)
 s.stop=1;save(m,s);s=tick(m);assert s.stop!=2 and m.vehs
 m.remove_veh_fails=False
 for _ in range(8):m.flush();s=tick(m)
 assert s.stop==2 and not m.vehs and ruleshash(m)==m.original_rules
 return {'retained_VEH_error_visible_after_DR_zero':True,'unrequested_full_group_reports_off':True,'stop_waits_for_handler_cleanup':True}
check('failed_VEH_removal_keeps_error_visible_until_true_retirement',failed_veh_removal)
def full_failed_veh_removal():
 m=fixture();ready(m);m.remove_veh_fails=True;send(m,on=False);s=settle(m)
 assert not any(m.dr[:4]) and m.vehs and s.techParameterStatus==-3 and s.techEnhanceParameterStatus==0,(s.techParameterStatus,s.techEnhanceParameterStatus)
 m.remove_veh_fails=False;s=settle(m);assert not m.vehs and s.techParameterStatus==0
 return {'full_group_handler_failure_visible_without_phantom_enhancement_error':True}
check('full_group_failed_VEH_removal_does_not_claim_off_or_blame_unused_enhancement',full_failed_veh_removal)
def shared_handler_not_error():
 m=fixture();ready(m);enhance(m);m.remove_veh_fails=True;s=enhance(m,False)
 assert s.techParameterStatus==1 and s.techEnhanceParameterStatus==0 and s.techParameterSlots==2 and s.techEnhanceParameterSlots==0 and m.vehs
 m.remove_veh_fails=False
 return {'healthy_full_sibling_can_keep_shared_VEH_while_enhancement_is_truly_off':True}
check('healthy_shared_sibling_does_not_make_stopped_group_look_pending_or_failed',shared_handler_not_error)

assert initial_dll==sha(a.dll)and initial_pdb==sha(Path(a.dll).with_suffix('.pdb'))and initial_game==sha(a.game),'input binaries changed during fixture'
assert source_hashes=={n:sha(repo/n)for n in source_paths},'sources changed during fixture'
report={'source_sha256':source_hashes,'hash_stable_during_test':True,'scope':__doc__,'pass':all(x['status']=='PASS'for x in rows),'checks':len(rows),'results':rows,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'pdb_sha256':hashlib.sha256(Path(a.dll).with_suffix('.pdb').read_bytes()).hexdigest(),'private_game_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_process_tested':False,'windows_debug_api_modeled':True}
Path(a.output).parent.mkdir(parents=True,exist_ok=True);Path(a.output).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');raise SystemExit(not report['pass'])
