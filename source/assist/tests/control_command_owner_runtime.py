"""Final DLL native Mega/F owner gates through native sender and decoder.
Synthetic Win32/world and capture-only transport. Original constructors, native
OutList writers, 649CA0/64B660 and 64BDD0 run. Planning tests retain original
ClickedMission construction with one unrelated movement-query boundary stub.
No event effects, real network peer, or complete game scheduler are simulated.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('owner_network',Path(__file__).with_name('network_house_runtime.py'));net=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.network-helpers.json'))]
try:spec.loader.exec_module(net)
except SystemExit as stopped:
 if stopped.code:raise
sys.argv=args;n=net.n
SITES={0x646ef5:bytes.fromhex('813dc802a80080000000'),0x646fd2:bytes.fromhex('813dc802a80080000000'),0x6ffc93:bytes.fromhex('83ec70b91b000000')}
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:
  detail=fn();rows.append({'case':name,'status':'PASS',**(detail or {})});print('PASS',name,flush=True)
 except Exception as error:rows.append({'case':name,'status':'FAIL','error':repr(error)});traceback.print_exc()
def config(m,**kw):
 s=n.state(m)
 for k,v in kw.items():setattr(s,k,v)
 n.save(m,s)
def setup(mode=3,protocol=1,enabled=True,network=True,gateway=True):
 m=net.setup(mode,protocol);config(m,controlAll=1,controlCommandOwner=int(enabled),networkHousePreserve=int(network),nativeMegaGateway=int(gateway))
 n.tick(m);m.targetRef=m.alloc();m.destRef=m.alloc()
 return m
def native(m,kind=4,source=None,mission=2):
 source=m.enemyUnit if source is None else source
 m.u.mem_write(m.targetRef,struct.pack('<IB',0,0));m.u.mem_write(m.destRef,struct.pack('<IB',30020,11))
 m.u.reg_write(UC_X86_REG_EDX,m.targetRef);before=m.r(0xa802c8)
 args=(m.r(source+0x10),0x34,m.destRef)+( (21,37) if kind==5 else () )
 m.run(0x646f70 if kind==5 else 0x646e90,args,mission)
 result=m.u.reg_read(UC_X86_REG_EAX)&255;after=m.r(0xa802c8)
 assert after==before+int(bool(result)),(kind,before,after,result)
 return bytes(m.u.mem_read(0xa802d4+((m.r(0xa802d0)-1)&127)*111,111)) if result else None
def meaningful(packet):
 kind=packet[0]
 if kind==4:return (kind,packet[2],packet[7:13],packet[14:30])
 if kind==5:return (kind,packet[2],packet[7:31])
 return (kind,packet[2],packet[7:12])
def transmit(m,expected):
 before=net.protect(m);count=len(expected);sent,local=net.send(m)
 assert sent==(0 if m.protocol==0 else count) and m.r(0xa802c8)==0
 remote=[x for x in net.decode(m) if x[0]!=0x1c]
 assert [meaningful(x) for x in local]==[meaningful(x) for x in expected]
 assert [meaningful(x) for x in local]==[meaningful(x) for x in remote]
 assert [x[3:7] for x in local]==[x[3:7] for x in remote]
 net.unchanged(m,before)
 return local,remote

for mode in (3,4):
 for protocol in (0,1,2):
  for kind in (4,5):
   def roundtrip(mode=mode,protocol=protocol,kind=kind):
    m=setup(mode,protocol);assert n.state(m).controlCommandOwnerStatus==1,n.state(m).controlCommandOwnerStatus
    before=net.protect(m);event=native(m,kind);assert event and event[2]==m.r(m.r(m.enemyUnit+0x21c)+0x30)==1
    assert n.state(m).controlCommandOwnerAccepted==1
    assert n.state(m).nativeMegaGatewayAccepted==int(kind==4)
    net.unchanged(m,before);local,remote=transmit(m,[event])
    return {'source_owner':1,'outlist_house':event[2],'local_dolist_house':local[0][2],'remote_decoded_house':remote[0][2],'native_kind':kind}
   check(f'native_owner_roundtrip_session_{mode}_protocol_{protocol}_kind_{kind}',roundtrip)
for protocol in (0,1,2):
 def mixed(protocol=protocol):
  m=setup(3,protocol);events=[]
  for source,kind in ((m.unit,4),(m.enemyUnit,5),(m.eng,4),(m.enemyUnit,4),(m.unit,5)):
   event=native(m,kind,source);assert event;events.append(event)
  local,remote=transmit(m,events)
  assert [x[2] for x in remote]==[0,1,0,1,0]
  assert [int.from_bytes(x[7:11],'little') for x in remote]==[10,13,11,13,10]
  return {'house_order':[0,1,0,1,0],'event_order_preserved':True}
 check(f'mixed_selection_keeps_actor_order_protocol_{protocol}',mixed)

for mode in (0,5):
 for kind in (4,5):
  def single(mode=mode,kind=kind):
   m=setup(mode,1,network=False);event=native(m,kind)
   assert event and event[2]==1 and m.r(0xa83d4c)==m.house
  check(f'singleplayer_native_owner_session_{mode}_kind_{kind}',single)

for case in ('network_off','control_off','heartbeat','foreground','chat','wrong_thread','owner_ai','owner_observer','owner_defeated','owner_index','owner_slot','source_dead','source_invalid','object_gate_lost','native_gate_lost','invalid_ring'):
 for kind in (4,5):
  def deny(case=case,kind=kind):
   m=setup();before=m.r(0xa802c8)
   if case=='network_off':config(m,networkHousePreserve=0)
   if case=='control_off':config(m,controlAll=0)
   if case=='heartbeat':m.clock+=3001
   if case=='foreground':config(m,foreground=0)
   if case=='chat':m.b(0xa8bc60+25,1)
   if case=='wrong_thread':m.current_thread+=1
   if case=='owner_ai':m.b(m.enemy+0x1ed,0)
   if case=='owner_observer':m.w(0xac1198,m.enemy)
   if case=='owner_defeated':m.b(m.enemy+0x1f5,1)
   if case=='owner_index':m.w(m.enemy+0x30,2)
   if case=='owner_slot':m.vector(0xa80228,[m.house,m.house])
   if case=='source_dead':m.w(m.enemyUnit+0x6c,0)
   if case=='source_invalid':m.w(m.enemyUnit+0x21c,0)
   if case=='object_gate_lost':m.u.mem_write(0x6ffe2b,b'\x90')
   if case=='native_gate_lost':m.u.mem_write(0x6ffc93,b'\x90')
   if case=='invalid_ring':m.w(0xa802d0,1)
   assert native(m,kind) is None and m.r(0xa802c8)==before
  check(f'foreign_fail_closed_{case}_kind_{kind}',deny)

for kind in (4,5):
 def own(kind=kind):
  m=setup();config(m,networkHousePreserve=0);assert native(m,kind,m.unit)[2]==0
 def full(kind=kind):
  m=setup();m.w(0xa802c8,128);m.w(0xa802d0,0)
  assert native(m,kind) is None and n.state(m).controlCommandOwnerAccepted==0
 def disabled(kind=kind):
  m=setup(enabled=False);assert native(m,kind)[2]==0 and n.state(m).controlCommandOwnerAccepted==0
 def noarbiter(kind=kind):
  m=setup(gateway=False);event=native(m,kind);assert event[2]==1 and n.state(m).nativeMegaGatewayAccepted==0
  transmit(m,[event])
 check(f'own_commands_survive_missing_foreign_transport_kind_{kind}',own)
 check(f'original_full_queue_false_kind_{kind}',full)
 check(f'independent_option_off_preserves_original_kind_{kind}',disabled)
 check(f'owner_option_independent_of_manual_gateway_kind_{kind}',noarbiter)

def planning(m,source):
 m.b(0xac4cf4,1)
 if not getattr(m,'planningQueryStubbed',False):
  m.u.hook_add(UC_HOOK_CODE,lambda u,at,size,_:m.ret(0,0),begin=0x731bf0,end=0x731bf0);m.planningQueryStubbed=True
 before=m.r(0xa802c8);m.run(0x6ffbe0,(2,0,0,0),source)
 assert not(m.u.reg_read(UC_X86_REG_EAX)&255),'vanilla planning returns false even after append'
 after=m.r(0xa802c8)
 return bytes(m.u.mem_read(0xa802d4+((m.r(0xa802d0)-1)&127)*111,111)) if after>before else None
for foreign in (False,True):
 def plan(foreign=foreign):
  m=setup();before=net.protect(m);event=planning(m,m.enemyUnit if foreign else m.unit)
  assert bool(event)==(not foreign)
  if event:assert event[2]==0 and event[29]==1
  else:assert n.state(m).controlCommandOwnerRejected==1
  net.unchanged(m,before)
 check('native_planning_'+('foreign_rejected' if foreign else 'own_kept'),plan)

for kind in (4,5):
 def toggle_transition(kind=kind):
  m=setup();config(m,controlCommandOwner=0) # guard remains installed until next tick
  assert native(m,kind) is None and m.r(0xa802c8)==0
  assert native(m,kind,m.unit)[2]==0 and m.r(0xa802c8)==1
 def player_transition(kind=kind):
  m=setup();m.w(0xa83d4c,m.enemy) # previous lastHouse must not authorize a new CurrentHouse header
  assert native(m,kind,m.unit) is None and m.r(0xa802c8)==0
 check(f'toggle_off_before_maintenance_rejects_foreign_keeps_own_kind_{kind}',toggle_transition)
 check(f'current_house_changed_before_maintenance_rejects_stale_own_kind_{kind}',player_transition)
def planning_toggle_transition():
 m=setup();config(m,controlCommandOwner=0)
 assert planning(m,m.enemyUnit) is None and m.r(0xa802c8)==0
 assert planning(m,m.unit)[2]==0 and m.r(0xa802c8)==1
check('toggle_off_before_maintenance_rejects_foreign_planning_keeps_own',planning_toggle_transition)

def tool(m):
 def source(s,i):i.sources[0]=m.PointerSource(13,m.enemyUnit)
 return n.pointer(m,9,0,source)
for case in ('accepted','network_off','owner_changes_during_clock'):
 def alternative(case=case):
  m=setup()
  if case=='network_off':config(m,networkHousePreserve=0)
  if case=='owner_changes_during_clock':m.timer_side_effect=lambda:m.w(m.enemyUnit+0x21c,m.house)
  s=tool(m);m.timer_side_effect=None
  if case=='accepted':
   assert s.pointer.accepted==1 and m.r(0xa802c8)==1
   event=net.logical_out(m)[0][0];assert event[2]==1;transmit(m,[event])
  else:assert s.pointer.accepted==0 and m.r(0xa802c8)==0,(case,s.pointer.accepted,m.r(0xa802c8))
 check('tool_alt_player_order_'+case,alternative)

def restore():
 m=setup();config(m,controlCommandOwner=0);n.tick(m)
 assert n.state(m).controlCommandOwnerStatus==0
 for at,original in SITES.items():assert bytes(m.u.mem_read(at,len(original)))==original
 assert n.state(m).nativeMegaGatewayStatus==1
check('owner_option_restore_preserves_gateway',restore)
for kind in (4,5):
 def abi(kind=kind):
  snapshots=[]
  for enabled in (False,True):
   m=setup(enabled=enabled,gateway=False)
   regs={UC_X86_REG_EAX:0x12345678,UC_X86_REG_EBX:0x23456789,UC_X86_REG_ESI:0x3456789a,UC_X86_REG_EDI:0x456789ab,UC_X86_REG_EBP:0x56789abc}
   for reg,value in regs.items():m.u.reg_write(reg,value)
   for j,reg in enumerate((UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7)):m.u.reg_write(reg,(0x9000000000000000+j*0x100000000000000,0x4000+j))
   m.u.reg_write(UC_X86_REG_FPSW,0x2800);m.u.reg_write(UC_X86_REG_FPTAG,0)
   m.u.reg_write(UC_X86_REG_FPCW,0x77f);m.u.reg_write(UC_X86_REG_MXCSR,0x3f80);m.u.reg_write(UC_X86_REG_EFLAGS,0x247)
   native(m,kind,m.unit)
   snapshots.append((tuple(m.u.reg_read(r) for r in net.GPRS+(UC_X86_REG_EFLAGS,)),net.fp(m)))
  assert snapshots[0]==snapshots[1],snapshots
 check(f'own_native_abi_unchanged_kind_{kind}',abi)

Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
