#!/usr/bin/env python3
"""Logical clock latency sweep through actual DLL; not OS/game latency."""
import argparse,ctypes as C,hashlib,json
from pathlib import Path
from two_peer_sync import Peer
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
for background in (False,True):
 q=Peer(a.game,a.dll,0);m=q.m;q.set(flags=1048576,scanMs=1000,foreground=not background);q.tick()
 for delay in (1,4,16,33,64,120,149,150,249,250,251,500):
  for sample in range(10):
   q.clear_queues();s=q.state();s.cooperativePlan+=1;s.cooperativeSerial+=1;s.cooperativeTick=m.clock;s.cooperativeFrame=m.r(0xa8ed84);s.cooperativeMatch=s.data.match;s.cooperativeMode=1;s.cooperativeCount=1
   s.cooperativeRequest.serial=s.cooperativeSerial;s.cooperativeRequest.match=s.data.match;s.cooperativeRequest.mode=1;s.cooperativeRequest.tick=m.clock
   o=s.cooperativeOrders[0];o.id=m.r(q.owned[1]+0x10);o.target=m.r(q.owned[0]+0x10);o.type=m.t;q.save(s)
   m.clock+=delay;q.advance();m.w(m.SH+m.Shared.heartbeat.offset,m.clock);m.run(m.entry,(0,1,0));s=q.state()
   assert s.cooperativeAck==s.cooperativePlan,(background,delay,'unsettled')
   assert s.cooperativeResult==(-4 if background else 1 if delay<=250 else -7),(background,delay,s.cooperativeResult)
   assert m.r(0xa802c8)==(1 if not background and delay<=250 else 0),(background,delay,'wrong packet count')
  rows.append({'background':background,'modeled_wait_ms':delay,'samples':10,'status':'PASS','expected':'foreground_guard' if background else 'accepted' if delay<=250 else 'expired','observed_ack_ms':delay})
print('PASS 240 actual-DLL requests across 24 logical timing/background scenarios')
a.output.write_text(json.dumps({'scope':'actual x86 DLL under modeled GetTickCount; excludes OS scheduling, reads, network, real game execution and capture','inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')
