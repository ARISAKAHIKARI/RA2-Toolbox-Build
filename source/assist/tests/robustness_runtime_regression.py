#!/usr/bin/env python3
"""Adversarial mailbox inputs against actual x86 DLLs, with synthetic objects."""
import argparse, ctypes as C, hashlib, json
from pathlib import Path
from two_peer_sync import Peer

p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
peer=Peer(a.game,a.dll,0);m=peer.m;rows=[]
def passed(name):
 rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
def submit(**overrides):
 s=peer.state();s.command=1;s.request=(s.request+1)&0xffffffff;s.requestTick=m.clock;s.requestMatch=s.data.match
 for k,v in overrides.items():setattr(s,k,v)
 peer.save(s);peer.tick();s=peer.state();assert s.ack==s.request
 return C.c_int32(s.result).value
def empty():assert m.r(0xa802c8)==0 and m.r(0x8b41f8)==0
before=peer.protected()
assert submit(requestMatch=peer.state().data.match+1)==-7;empty();passed('command_previous_match_rejected')
assert submit(requestTick=(m.clock-3501)&0xffffffff)==-7;empty();passed('command_expired_rejected')
assert submit(requestTick=(m.clock+1000)&0xffffffff)==-7;empty();passed('command_future_timestamp_rejected')
m.b(0xa8bc60+25,1);assert submit()==-8;empty();m.b(0xa8bc60+25,0);passed('chat_blocks_command')
for cmd in (-2147483648,-1,0,2147483647):assert submit(command=cmd)==-4;empty()
passed('unknown_command_extremes_rejected')
# Counts are validated before accessing the fixed 512-element mailbox.
for count in (-2147483648,-1,513,2147483647):
 s=peer.state();s.cooperativePlan+=1;s.cooperativeCount=count;peer.save(s);peer.tick();s=peer.state();assert s.cooperativeAck==s.cooperativePlan;empty()
passed('cooperative_negative_and_oversized_counts_rejected')
assert peer.protected()==before
# Busy is not an acknowledgement: preserve the mailbox until its owning game
# thread can enter. No spin/wait is introduced into the game callback.
s=peer.state();s.request+=1;s.command=1;s.requestTick=m.clock;s.requestMatch=s.data.match;previous=s.ack;peer.save(s)
rc,lease=m.manager_call(m.request(0,2,m.cursor_dll.OPTIONAL_HEADER.ImageBase));assert rc==1
for _ in range(5):peer.advance();peer.tick();assert peer.state().ack==previous;empty()
passed('foreign_DLL_busy_preserves_command_without_ack')
lease.operation=1;assert m.manager_call(lease)[0]==1
peer.tick();assert peer.state().ack==peer.state().request and m.r(0xa802c8)==1
peer.clear_queues();passed('command_resumes_once_after_foreign_DLL_releases')
s=peer.state();s.request+=1;s.command=-1;s.requestTick=m.clock;s.requestMatch=s.data.match;previous=s.ack;peer.save(s)
rc,lease=m.manager_call(m.request(0,2,m.cursor_dll.OPTIONAL_HEADER.ImageBase));assert rc==1
for _ in range(40):peer.advance();peer.tick();assert peer.state().ack==previous;empty()
lease.operation=1;assert m.manager_call(lease)[0]==1
peer.tick();assert peer.state().ack==peer.state().request and C.c_int32(peer.state().result).value==-7;empty()
passed('command_expiring_during_contention_is_rejected_after_release')
# Clear the synthetic pending repair by supplying observed engine evidence.
m.b(peer.owned[2]+0x6e8,1);peer.tick();m.b(peer.owned[2]+0x6e8,0)
# Repeated scheduling of an already acknowledged request must not replay it.
assert submit()==1;assert m.r(0xa802c8)==1
for _ in range(100):peer.advance();peer.tick()
assert m.r(0xa802c8)==1;assert peer.protected()==before
passed('acknowledged_repair_not_replayed_over_100_frames')
peer.clear_queues()
# A new request serial is still subject to pending repair evidence; repairing
# toggles must not be resent merely because frames/time passed.
assert submit()==1;empty();passed('new_repair_request_respects_pending_evidence')
# Wrap request serial over zero: zero is not an unconditionally empty mailbox.
s=peer.state();s.request=s.ack=0xffffffff;peer.save(s)
assert submit(command=-1)==-4;assert peer.state().ack==0;empty();passed('command_request_serial_wrap_is_acknowledged')
# Serial-zero repair must retain its in-flight state while the queue is full.
m.b(peer.owned[2]+0x6e8,1);peer.tick();m.b(peer.owned[2]+0x6e8,0)
s=peer.state();s.request=0;s.ack=0xffffffff;s.command=1;s.requestTick=m.clock;s.requestMatch=s.data.match;peer.save(s)
m.w(0xa802c8,128);peer.tick();assert peer.state().ack==0xffffffff
peer.clear_queues();peer.advance();peer.tick();assert peer.state().ack==0 and m.r(0xa802c8)==1
for _ in range(10):peer.advance();peer.tick()
assert m.r(0xa802c8)==1;passed('zero_serial_repair_survives_queue_backpressure_without_replay')
peer.clear_queues();m.b(0xa8e9a0,0)
s=peer.state();s.request=0;s.ack=0xffffffff;s.command=1;peer.save(s);peer.tick()
assert peer.state().ack==0xffffffff and peer.state().data.status==1;empty();passed('temporary_pause_preserves_zero_serial_request')
m.w(0xa83d4c,0);peer.tick()
assert peer.state().ack==0 and C.c_int32(peer.state().result).value==-3;empty();passed('zero_serial_rejected_and_acknowledged_after_match_ends')
a.output.write_text(json.dumps({'scope':'actual compiled sender/manager with synthetic objects and stubbed Windows APIs; not full game simulation',
 'inputs':{n:hashlib.sha256(a.dll.with_name(n).read_bytes()).hexdigest() for n in ('GameRuntime.dll',)},'results':rows},indent=2)+'\n')
