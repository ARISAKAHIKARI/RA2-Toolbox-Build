"""Actual x86 runtime: anchored fanout through the existing pointer mailbox.
World/Win32/transport are modeled; this does not run Windows or a real match.
"""
from pathlib import Path
# Reuse the established actual-DLL fixture and native constructor/query stubs.
fixture_source=Path(__file__).with_name('no_interception_runtime.py').read_text().split('for mode in (0,5,3,4):',1)[0]
exec(compile(fixture_source,str(Path(__file__).with_name('no_interception_runtime.py')),'exec'))
START=16384;NEXT=32768;PER=8192;RIGHT=1024;FLAGS=1024|8192|131072
def begin(mode,bindings=4,operation=1):
 m=fixture(mode,FLAGS);s=pointer(m,operation,bindings|PER|START);return m,s,bytes(s.pointer.intent)
def followup(m,capture,bindings=4,operation=1,mutate=None):
 s=state(m);s.pointer.request+=1;C.memmove(C.addressof(s.pointer.intent),capture,len(capture));i=s.pointer.intent;i.operation=operation;i.bindings=bindings|PER|NEXT
 if mutate:mutate(s,i)
 save(m,s);return m.hook()
for mode in (0,5,3,4):
 def aged(mode=mode):
  m,s,original=begin(mode);assert s.pointer.accepted==1 and count(m)==1
  m.clock+=200;m.w(0xa8ed84,m.r(0xa8ed84)+1);m.queryObject=m.unit
  s=followup(m,original,1);assert s.pointer.accepted==1 and s.pointer.ack==s.pointer.request and count(m)==2
  assert m.queryCalls==1 # retains the captured ground target even if an object moves under the pointer
  assert packets(m)[12]==packets(m)[111+12]==29 and not m.codeWrites
 check(f'anchored_capture_and_original_deadline_session_{mode}',aged)
 def throttle(mode=mode):
  m,s,original=begin(mode,1);assert count(m)==1
  m.w(m.unit+0xac,29);m.w(m.unit+0x5a4,m.crateCell) # original engine has applied the first AttackMove
  s=followup(m,original,4);assert s.pointer.request!=s.pointer.ack and count(m)==1 # frame grant remains authoritative
  s=tick(m);assert s.pointer.ack==s.pointer.request and s.pointer.accepted==1 and count(m)==2
  for _ in range(2):s=tick(m)
  assert s.data.advancedUnits==0 # the later direct CS replaces the earlier persistent planner
  assert count(m)==2 and not m.codeWrites
 check(f'same_frame_wait_then_submit_once_session_{mode}',throttle)
 def unanchored(mode=mode):
  m=fixture(mode,FLAGS);s=pointer(m,1,PER|NEXT|4);assert s.pointer.result==-7 and count(m)==0
 check(f'unanchored_continuation_rejected_session_{mode}',unanchored)
 def mismatch(mode=mode):
  m,s,original=begin(mode);s=followup(m,original,1,mutate=lambda s,i:setattr(i.sources[0],'id',11));assert s.pointer.result==-7 and count(m)==1
 check(f'changed_source_capture_rejected_session_{mode}',mismatch)
 def cancel(mode=mode):
  m,s,original=begin(mode);s=state(m);s.pointer.cancelSerial+=1;save(m,s);s=followup(m,original,1);assert s.pointer.result==-7 and count(m)==1
 check(f'manual_cancel_rejects_remaining_matches_session_{mode}',cancel)
 def expired(mode=mode):
  m,s,original=begin(mode);m.clock+=1501;s=followup(m,original,1);assert s.pointer.result==-7 and count(m)==1
 check(f'batch_deadline_never_renewed_session_{mode}',expired)
 def camera(mode=mode):
  m,s,original=begin(mode);m.w(m.tactical+0xb0 if hasattr(m,'tactical') else m.r(0x887324)+0xb0,1)
  s=followup(m,original,1);assert s.pointer.result==-4 and count(m)==1
 check(f'camera_change_rejects_continuation_session_{mode}',camera)
 def reuse(mode=mode):
  m,s,original=begin(mode);m.w(m.unit+0x10,110);m.w(0xa8ed84,m.r(0xa8ed84)+1);s=followup(m,original,1);assert s.pointer.accepted==0 and count(m)==1
 check(f'recycled_actor_id_not_retargeted_session_{mode}',reuse)
 def right(mode=mode):
  m,s,original=begin(mode,4|RIGHT);assert s.pointer.ack!=s.pointer.request and count(m)==0
  m.w(m.unit+0xac,2);m.w(m.unit+0x5a4,m.crateCell);s=tick(m);assert s.pointer.accepted==1 and count(m)==1
  m.w(m.unit+0xac,29);m.w(m.unit+0x5a4,0);m.w(0xa8ed84,m.r(0xa8ed84)+1)
  s=followup(m,original,1|RIGHT);assert s.pointer.accepted==1 and count(m)==2 and m.queryCalls==1
 check(f'right_click_observed_move_shared_by_matches_session_{mode}',right)
 def flags(mode=mode):
  m=fixture(mode,FLAGS);s=pointer(m,1,PER|START|NEXT|4);assert s.pointer.result==-7 and count(m)==0
 check(f'invalid_batch_flags_rejected_session_{mode}',flags)
Path(a.output).write_text(json.dumps({'inputs':{'GameRuntime.dll':hashlib.sha256(Path(a.dll).read_bytes()).hexdigest(),'game':hashlib.sha256(Path(a.game).read_bytes()).hexdigest()},'results':results,'limitations':'Actual x86 DLL, original constructors and existing queue entry. Synthetic Win32/world; no real game, screen or netplay verification.'},indent=2)+'\n')
print('TOTAL',len(results))
