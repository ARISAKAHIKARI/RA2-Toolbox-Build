"""Compiled ClickedEvent House adapter with original Target/constructor/OutList.
Synthetic Win32 and world; real unified manager/capture and native instructions.
No sender, receiver, unit effects, planning execution or real multiplayer test.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
Path(a.output).parent.mkdir(parents=True,exist_ok=True)
spec=importlib.util.spec_from_file_location('object_owner_support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
SITE=0x6ffe2b;ORIGINAL=bytes.fromhex('8b 42 30 51 50');KINDS=(1,2,6,7,9,10,17,21,22)
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as error:rows.append({'case':name,'status':'FAIL','error':repr(error)});traceback.print_exc()
def config(m,**kw):
 s=n.state(m)
 for k,v in kw.items():setattr(s,k,v)
 n.save(m,s)
def setup(mode=3,enabled=True,preserve=True,control=True,conflict=None):
 m=n.fixture(mode);m.vector(0xa8da74,[])
 for house,ident in ((m.house,100),(m.enemy,200)):
  typ=m.alloc(0x2000);m.w(house+0x34,typ);m.w(house+0x10,ident);m.u.mem_write(typ+0x24,b'Americans\0')
 m.enemyType=m.r(m.enemy+0x34);m.b(m.house+0x1ec,1);m.b(m.enemy+0x1ed,mode in (3,4));m.w(0xa8b230,m.alloc(0x4000));m.w(0xa8b24c,1)
 m.target=m.unit;m.w(m.target+0x21c,m.enemy)
 if conflict:m.u.mem_write(conflict,b'\x90')
 config(m,controlAll=int(control),controlCommandOwner=int(enabled),networkHousePreserve=int(preserve),nativeMegaGateway=0)
 n.tick(m);n.tick(m)
 m.ctorCalls=[];m.planningCalls=[]
 def ctor(u,at,size,_):
  sp=u.reg_read(UC_X86_REG_ESP);m.ctorCalls.append((m.r(sp+4),m.r(sp+8)))
 def planning(u,at,size,_):m.planningCalls.append(bytes(u.mem_read(u.reg_read(UC_X86_REG_ECX),111)));m.ret(0,0)
 m.u.hook_add(UC_HOOK_CODE,ctor,begin=0x4c65e0,end=0x4c65e0)
 m.u.hook_add(UC_HOOK_CODE,planning,begin=0x639fd0,end=0x639fd0)
 return m
def native(m,kind=6):
 before=m.r(0xa802c8);m.run(0x6ffe00,(kind,),m.target);after=m.r(0xa802c8)
 assert after in (before,before+1),(before,after)
 packet=bytes(m.u.mem_read(0xa802d4+((m.r(0xa802d0)-1)&127)*111,111)) if after>before else None
 return m.u.reg_read(UC_X86_REG_EAX)&255,packet
def packet_matches(m,packet,kind,house):
 assert packet and packet[0]==kind and packet[2]==house and packet[7:12]==struct.pack('<IB',m.r(m.target+0x10),0x34),packet[:16].hex() if packet else None
 assert m.r(0x8b41f8)==0,'native ClickedEvent owns OutList even in SP'

for mode in (0,5,3,4):
 for kind in KINDS:
  def accepted(mode=mode,kind=kind):
   m=setup(mode);assert n.state(m).controlCommandOwnerStatus==1
   before=bytes(m.u.mem_read(m.target,0x2000));local=m.r(0xa83d4c);queued=n.state(m).data.queuedEvents
   result,packet=native(m,kind);assert result==1;packet_matches(m,packet,kind,1)
   assert m.ctorCalls==[(1,kind)] and n.state(m).controlCommandOwnerAccepted==1
   assert before==bytes(m.u.mem_read(m.target,0x2000)) and local==m.r(0xa83d4c)
   assert queued==n.state(m).data.queuedEvents,'identity preparation is not a toolbox receipt'
  check(f'foreign_object_{mode}_{kind:02x}',accepted)

def exhaustive():
 m=setup()
 for kind in range(256):
  m.w(0xa802c8,0);m.w(0xa802d0,0);m.ctorCalls.clear()
  result,packet=native(m,kind)
  if kind in KINDS:assert result==1;packet_matches(m,packet,kind,1);assert len(m.ctorCalls)==1
  else:assert result==0 and packet is None and not m.ctorCalls,kind
 assert n.state(m).controlCommandOwnerAccepted==len(KINDS)
 assert n.state(m).controlCommandOwnerRejected==256-len(KINDS)
check('all_256_kinds_only_explicit_object_allowlist',exhaustive)

for case in ('control_off','toggle_off_before_tick','network_off','network_conflict','ai_no_scheduler','dead','limbo','unknown_id','zero_id','wrong_tag','owner_invalid','owner_index','owner_slot','passive','neutral','observer','defeated','planning','chat','foreground','expired_heartbeat','wrong_thread','queue_full','bad_tail','bad_head','tail_mismatch','missing_mega_adapter'):
 def denied(case=case):
  m=setup()
  if case=='control_off':config(m,controlAll=0)
  if case=='toggle_off_before_tick':config(m,controlCommandOwner=0)
  if case=='network_off':config(m,networkHousePreserve=0)
  if case=='network_conflict':m.u.mem_write(0x649e1e,b'\x90')
  if case=='ai_no_scheduler':m.b(m.enemy+0x1ec,0);m.b(m.enemy+0x1ed,0)
  if case=='dead':m.w(m.target+0x6c,0);m.b(m.target+0x74,0)
  if case=='limbo':m.b(m.target+0x81,1)
  if case in ('unknown_id','zero_id','wrong_tag'):
   def source(u,at,size,_):
    sp=u.reg_read(UC_X86_REG_ESP)
    if case=='wrong_tag':m.b(sp+4,11)
    else:m.w(sp,999999 if case=='unknown_id' else 0)
   m.u.hook_add(UC_HOOK_CODE,source,begin=SITE,end=SITE)
  if case=='owner_invalid':m.w(m.target+0x21c,0)
  if case=='owner_index':m.w(m.enemy+0x30,32)
  if case=='owner_slot':m.vector(0xa80228,[m.house,m.house])
  if case=='passive':m.b(m.enemyType+0x1a6,1)
  if case=='neutral':m.u.mem_write(m.enemyType+0x24,b'Neutral\0')
  if case=='observer':m.w(0xac1198,m.enemy)
  if case=='defeated':m.b(m.enemy+0x1f5,1)
  if case=='planning':m.b(0xac4cf4,1)
  if case=='chat':m.b(0xa8bc60+25,1)
  if case=='foreground':config(m,foreground=0)
  if case=='expired_heartbeat':m.clock+=3001
  if case=='wrong_thread':m.current_thread+=1
  if case=='queue_full':m.w(0xa802c8,128)
  if case=='bad_tail':m.w(0xa802d0,128)
  if case=='bad_head':m.w(0xa802cc,128)
  if case=='tail_mismatch':m.w(0xa802c8,1)
  if case=='missing_mega_adapter':m.u.mem_write(0x646ef5,bytes.fromhex('81 3d c8 02 a8 00 80 00 00 00'))
  before=bytes(m.u.mem_read(m.target,0x2000));result,packet=native(m)
  assert result==0 and packet is None and not m.ctorCalls and not m.planningCalls
  assert before==bytes(m.u.mem_read(m.target,0x2000))
  assert n.state(m).controlCommandOwnerAccepted==0
 check('reject_'+case,denied)

for case in ('normal','control_off','network_off','toggle_off_before_tick','chat','foreground','planning','unknown_kind'):
 def own(case=case):
  m=setup();m.target=m.building;m.w(m.target+0x21c,m.house)
  if case=='control_off':config(m,controlAll=0)
  if case=='network_off':config(m,networkHousePreserve=0)
  if case=='toggle_off_before_tick':config(m,controlCommandOwner=0)
  if case=='chat':m.b(0xa8bc60+25,1)
  if case=='foreground':config(m,foreground=0)
  if case=='planning':m.b(0xac4cf4,1)
  kind=0x2d if case=='unknown_kind' else 0x16
  result,packet=native(m,kind)
  if case=='planning':assert result==0 and packet is None and len(m.planningCalls)==1 and m.planningCalls[0][2]==0
  else:assert result==1;packet_matches(m,packet,kind,0)
  assert m.ctorCalls==[(0,kind)] and n.state(m).controlCommandOwnerAccepted==0
 check('own_native_unchanged_'+case,own)

def off_baseline():
 m=setup(enabled=False);assert bytes(m.u.mem_read(SITE,5))==ORIGINAL
 result,packet=native(m);assert result==1;packet_matches(m,packet,6,0)
check('option_off_keeps_original_foreign_local_house_behavior',off_baseline)
def restore():
 m=setup();config(m,controlCommandOwner=0);n.tick(m)
 assert bytes(m.u.mem_read(SITE,5))==ORIGINAL
 result,packet=native(m);assert result==1;packet_matches(m,packet,6,0)
check('option_disable_restores_original_site',restore)
def conflict():
 m=setup(conflict=SITE);assert n.state(m).controlCommandOwnerStatus<0
 assert n.state(m).playerControlStatus!=1,'incomplete owner guards must not enable global foreign-control judgment'
 assert bytes(m.u.mem_read(SITE,1))==b'\x90'
check('signature_conflict_preserves_foreign_bytes',conflict)

for owner in ('own','foreign','rejected'):
 def abi(owner=owner):
  m=setup();m.w(m.target+0x21c,m.house if owner=='own' else m.enemy)
  if owner=='rejected':config(m,controlAll=0)
  sp=m.STACK+0x1d000;m.u.mem_write(sp-0x1000,bytes(0x1800));m.u.mem_write(sp,struct.pack('<IB',10,0x34));m.u.reg_write(UC_X86_REG_ESP,sp)
  initial={UC_X86_REG_EAX:0x12345678,UC_X86_REG_EBX:0x23456789,UC_X86_REG_ECX:6,UC_X86_REG_EDX:m.house,UC_X86_REG_ESI:0x3456789a,UC_X86_REG_EDI:0x456789ab,UC_X86_REG_EBP:0x56789abc}
  for reg,value in initial.items():m.u.reg_write(reg,value)
  xmm={reg:0x0123456789abcdeffedcba9876543210+i for i,reg in enumerate((UC_X86_REG_XMM0,UC_X86_REG_XMM1,UC_X86_REG_XMM2,UC_X86_REG_XMM3,UC_X86_REG_XMM4,UC_X86_REG_XMM5,UC_X86_REG_XMM6,UC_X86_REG_XMM7))}
  for reg,val in xmm.items():m.u.reg_write(reg,val)
  m.u.reg_write(UC_X86_REG_FPCW,0x77f);m.u.reg_write(UC_X86_REG_MXCSR,0x3f80);m.u.reg_write(UC_X86_REG_EFLAGS,0x647)
  hit=[]
  def stop(u,at,size,_):hit.append(at);u.reg_write(UC_X86_REG_EIP,m.STOP)
  for at in (0x6ffe30,0x6ffe4b):m.u.hook_add(UC_HOOK_CODE,stop,begin=at,end=at)
  m.u.emu_start(SITE,m.STOP,count=2000000)
  allowed=owner!='rejected';assert hit==[0x6ffe30 if allowed else 0x6ffe4b]
  assert m.u.reg_read(UC_X86_REG_ESP)==sp+(-8 if allowed else 8)
  for reg,val in initial.items():
   if reg==UC_X86_REG_EAX and allowed:continue
   assert m.u.reg_read(reg)==val,('register',reg)
  for reg,val in xmm.items():assert m.u.reg_read(reg)==val,('XMM',reg)
  assert m.u.reg_read(UC_X86_REG_FPCW)==0x77f and m.u.reg_read(UC_X86_REG_MXCSR)==0x3f80
  if allowed:
   expected=int(owner=='foreign');assert m.r(sp-8)==expected and m.r(sp-4)==6 and m.u.reg_read(UC_X86_REG_EAX)==expected
   assert m.u.reg_read(UC_X86_REG_EFLAGS)&0xcd5==0x647&0xcd5
 check('adapter_abi_'+owner,abi)

Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
