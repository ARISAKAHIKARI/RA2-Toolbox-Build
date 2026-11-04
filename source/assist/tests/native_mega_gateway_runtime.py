"""Compiled success adapter with original QueueMegaMission/constructor/OutList.
Synthetic Win32 and world; manager installation and capture lease are real DLL
paths. No receiver, simulation mission execution, or multiplayer certification.
"""
import argparse,ctypes as C,hashlib,importlib.util,json,struct,sys,traceback
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('game');p.add_argument('dll');p.add_argument('--output',required=True);p.add_argument('--filter',default='');a=p.parse_args();rows=[]
spec=importlib.util.spec_from_file_location('mega_support',Path(__file__).with_name('no_interception_runtime.py'));n=importlib.util.module_from_spec(spec)
args=sys.argv;sys.argv=[str(spec.origin),a.game,a.dll,'--filter','__fixture__','--output',str(Path(a.output).with_suffix('.helpers.json'))];spec.loader.exec_module(n);sys.argv=args
SITE=0x646f4e;ORIGINAL=bytes.fromhex('b0 01 5f 5e 83 c4 70')
def check(name,fn):
 if a.filter and a.filter not in name:return
 try:fn();rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
 except Exception as error:rows.append({'case':name,'status':'FAIL','error':repr(error)});traceback.print_exc()
def config(m,**kw):
 s=n.state(m)
 for k,v in kw.items():setattr(s,k,v)
 n.save(m,s)
def setup(mode=3,enabled=True,conflict=None):
 m=n.fixture(mode);m.vector(0xa8da74,[])
 if conflict:m.u.mem_write(conflict,b'\x90')
 config(m,nativeMegaGateway=int(enabled));n.tick(m)
 m.targetRef=m.alloc();m.destRef=m.alloc()
 return m
def native(m,mission=2,actor=10,target=0,targetTag=0,destination=30020,destinationTag=11):
 m.u.mem_write(m.targetRef,struct.pack('<IB',target,targetTag));m.u.mem_write(m.destRef,struct.pack('<IB',destination,destinationTag))
 m.u.reg_write(UC_X86_REG_EDX,m.targetRef)
 before=m.r(0xa802c8);m.run(0x646e90,(actor,0x34,m.destRef),mission)
 after=m.r(0xa802c8);success=m.u.reg_read(UC_X86_REG_EAX)&255
 assert after==before+int(bool(success)),(before,after,success)
 return bytes(m.u.mem_read(0xa802d4+((m.r(0xa802d0)-1)&127)*111,111)) if success else None
def command(m,which):
 s=n.state(m);s.command=which;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;n.save(m,s);return m.hook()
def miner(m):
 m.b(m.t+0xe0e,1);m.w(m.t+0x800,40);config(m,plannerTracking=32768);n.tick(m)
def actor(m):
 s=n.state(m);return next(x for x in s.world.actors[:s.world.count] if x.id==10)

for mode in (0,5,3,4):
 for mission in (0,1,2,5,6,7,8,9,10,12,13,16,23,29):
  def accepted(mode=mode,mission=mission):
   m=setup(mode);assert n.state(m).nativeMegaGatewayStatus==1
   before=bytes(m.u.mem_read(m.unit,0x2000));cur=m.r(0xa83d4c);queued=n.state(m).data.queuedEvents
   packet=native(m,mission)
   assert packet[0]==4 and packet[2]==0 and packet[7:12]==struct.pack('<IB',10,0x34) and packet[12]==mission
   assert packet[24:29]==struct.pack('<IB',10,0),'vanilla None-tagged nonzero nearest ID must survive'
   assert m.r(0xa802c8)==1 and m.r(0xa83a54)==m.clock and m.r(0x8b41f8)==0
   assert n.state(m).nativeMegaGatewayAccepted==1 and n.state(m).data.queuedEvents==queued
   assert before==bytes(m.u.mem_read(m.unit,0x2000)) and cur==m.r(0xa83d4c)
  check(f'original_success_session_{mode}_mission_{mission}',accepted)

for case in ('off','expired_heartbeat','wrong_thread','foreign_actor','dead_actor','unknown_actor','unknown_mission','constructor_slot_mismatch'):
 def bypass(case=case):
  m=setup(enabled=case!='off');before=bytes(m.u.mem_read(m.unit,0x2000))
  if case=='expired_heartbeat':m.clock+=3001
  if case=='wrong_thread':m.current_thread+=1
  if case=='foreign_actor':m.w(m.unit+0x21c,m.enemy);before=bytes(m.u.mem_read(m.unit,0x2000))
  if case=='dead_actor':m.w(m.unit+0x6c,0);m.b(m.unit+0x74,0);before=bytes(m.u.mem_read(m.unit,0x2000))
  if case=='constructor_slot_mismatch':m.timer_side_effect=lambda:m.b(0xa802d4+12,5)
  assert native(m,255 if case=='unknown_mission' else 2,999 if case=='unknown_actor' else 10)
  assert m.r(0xa802c8)==1 and n.state(m).nativeMegaGatewayAccepted==0
  assert before==bytes(m.u.mem_read(m.unit,0x2000))
 check('native_input_survives_'+case,bypass)

for count in (112,126,127,128):
 def queue(count=count):
  m=setup();m.w(0xa802c8,count);m.w(0xa802d0,127)
  oldClock=m.r(0xa83a54+127*4);packet=native(m)
  assert bool(packet)==(count<128)
  assert m.r(0xa802d0)==(0 if count<128 else 127)
  assert n.state(m).nativeMegaGatewayAccepted==int(count<128)
  assert m.r(0xa83a54+127*4)==(m.clock if count<128 else oldClock)
 check(f'native_capacity_and_wrap_{count}',queue)

def repeats():
 m=setup();miner(m);v=actor(m).manualVersion
 for _ in range(3):native(m)
 assert m.r(0xa802c8)==3 and n.state(m).nativeMegaGatewayAccepted==3
 n.tick(m);assert actor(m).manualVersion==v+3
 assert n.state(m).data.receiptCancelled>=2
check('repeated_same_frame_manual_orders_are_never_throttled',repeats)

def ore():
 m=setup();miner(m);native(m,2);n.tick(m);assert n.state(m).data.oreManual==1
 native(m,10);n.tick(m);assert n.state(m).data.oreManual==0
 native(m,5);n.tick(m);assert n.state(m).data.oreManual==1
 native(m,12);n.tick(m);assert n.state(m).data.oreManual==0
check('manual_version_and_explicit_mining_resume',ore)

def boarding(m):
 m.vector(0xa8ecb8,[m.eng]);m.u.mem_write(m.t+0x388,struct.pack('<d',4.0));m.u.mem_write(m.engT+0x380,struct.pack('<d',1.0))
 config(m,megaQueueMode=1)
def own_backend():
 m=setup();boarding(m);s=command(m,15)
 assert s.data.loadIssued==1 and m.r(0xa802c8)==1,(s.data.loadIssued,s.result)
 assert s.nativeMegaGatewayAccepted==0 and s.nativeMegaGatewaySkipped==1
 assert s.data.megaQueueAccepted==1,'tool backend owns its existing acceptance counter'
check('own_native_backend_bypasses_duplicate_manual_arbitration',own_backend)
def manual_after_auto():
 m=setup();boarding(m);s=command(m,15);assert s.data.loadIssued==1
 original=bytes(m.u.mem_read(0xa802d4,111));before=bytes(m.u.mem_read(m.eng,0x2000))
 assert native(m,2,11);assert m.r(0xa802c8)==2
 assert original==bytes(m.u.mem_read(0xa802d4,111)) and before==bytes(m.u.mem_read(m.eng,0x2000))
 s=n.tick(m);assert s.nativeMegaGatewayAccepted==1 and s.data.loadWaiting==0 and s.data.receiptCancelled>=1
check('native_manual_supersedes_existing_auto_lease_and_loading_reservation',manual_after_auto)
def auto_after_manual():
 m=setup();boarding(m);native(m,2,11);s=command(m,15)
 assert m.r(0xa802c8)==1 and not s.data.loadIssued
 n.tick(m);s=command(m,15);assert m.r(0xa802c8)==1 and not s.data.loadIssued
check('manual_claim_and_lease_block_later_autoload',auto_after_manual)

for site in (SITE,0x646e90,0x646eeb,0x646ef5,0x646f30,0x646f43,0x646f55):
 def conflict(site=site):
  m=setup(conflict=site);assert n.state(m).nativeMegaGatewayStatus<0
  assert bytes(m.u.mem_read(site,1))==b'\x90'
  if site!=SITE:assert bytes(m.u.mem_read(SITE,7))==ORIGINAL
 check(f'signature_or_context_conflict_{site:x}',conflict)
def restore():
 m=setup();config(m,nativeMegaGateway=0);n.tick(m)
 assert n.state(m).nativeMegaGatewayStatus==0 and bytes(m.u.mem_read(SITE,7))==ORIGINAL
 native(m);assert n.state(m).nativeMegaGatewayAccepted==0
check('toggle_restores_original_epilogue',restore)

def abi():
 outputs=[]
 for enabled in (False,True):
  m=setup(enabled=enabled)
  regs={UC_X86_REG_EAX:0x12345678,UC_X86_REG_EBX:0x23456789,UC_X86_REG_ESI:0x3456789a,UC_X86_REG_EDI:0x456789ab,UC_X86_REG_EBP:0x56789abc}
  xmms={reg:0x0123456789abcdeffedcba9876543210+i for i,reg in enumerate((UC_X86_REG_XMM0,UC_X86_REG_XMM1,UC_X86_REG_XMM2,UC_X86_REG_XMM3,UC_X86_REG_XMM4,UC_X86_REG_XMM5,UC_X86_REG_XMM6,UC_X86_REG_XMM7))}
  for reg,value in (regs|xmms).items():m.u.reg_write(reg,value)
  fps={reg:(0x9000000000000000+i*0x100000000000000,0x4000+i) for i,reg in enumerate((UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7))}
  for reg,value in fps.items():m.u.reg_write(reg,value)
  m.u.reg_write(UC_X86_REG_FPSW,0x2800);m.u.reg_write(UC_X86_REG_FPTAG,0)
  m.u.reg_write(UC_X86_REG_FPCW,0x77f);m.u.reg_write(UC_X86_REG_MXCSR,0x3f80);m.u.reg_write(UC_X86_REG_EFLAGS,0x247)
  packet=native(m)
  output={reg:m.u.reg_read(reg) for reg in tuple(regs)+tuple(xmms)+tuple(fps)+(UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EFLAGS,UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_MXCSR)}
  outputs.append((packet,output))
 assert outputs[0]==outputs[1],[(k,outputs[0][1][k],outputs[1][1][k]) for k in outputs[0][1] if outputs[0][1][k]!=outputs[1][1][k]]
check('full_native_function_abi_matches_vanilla_registers_flags_x87_sse',abi)

Path(a.output).write_text(json.dumps({'scope':__doc__,'dll_sha256':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game_fixture_sha256':hashlib.sha256(Path(a.game).read_bytes()).hexdigest(),'game_tested':False,'multiplayer_tested':False,'results':rows},indent=2)+'\n')
raise SystemExit(any(x['status']!='PASS' for x in rows))
