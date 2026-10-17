#!/usr/bin/env python3
"""Actual x86 Runtime source-age checks, synthetic objects and Win32 stubs."""
import argparse,ctypes as C,hashlib,json,struct
from pathlib import Path
from two_peer_sync import Peer
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
def setup(coop=False,count=1):
 peer=Peer(a.game,a.dll,0);m=peer.m
 if coop:
  peer.set(flags=1048576);peer.tick();s=peer.state()
  s.cooperativePlan=1;s.cooperativeSerial=1;s.cooperativeTick=m.clock;s.cooperativeFrame=m.r(0xa8ed84);s.cooperativeMatch=s.data.match;s.cooperativeMode=1;s.cooperativeCount=count
  s.cooperativeRequest.serial=1;s.cooperativeRequest.match=s.data.match;s.cooperativeRequest.mode=1;s.cooperativeRequest.tick=m.clock
  sources=[peer.owned[1]]
  if count>1:
   extra=m.obj(0x7eb058,m.engT,m.house,91);sources.append(extra);peer.objects.append(extra);m.vector(0xa8ec78,peer.objects);peer.advance()
   s.cooperativeFrame=m.r(0xa8ed84)
  for i in range(count):
   o=s.cooperativeOrders[i];o.id=m.r(sources[i]+0x10);o.target=m.r(peer.owned[0]+0x10);o.type=m.t
 else:
  m.b(m.t+0xe0e,1);m.vector(m.t+0x3e8,[m.buildT]);m.u.mem_write(peer.owned[0]+0x33c,struct.pack('<4f',28,0,0,0));m.w(m.t+0x800,28)
  peer.set(flags=32768);peer.tick();s=peer.state();s.planRequest=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=m.r(0xa8ed84);s.planCount=count
  for i in range(count):
   o=s.plan[i];o.unit=m.r(peer.owned[0]+0x10);o.destination=m.r(peer.owned[2]+0x10);o.refinery=o.destination;o.mission=7;o.destinationTag=0x34;o.purpose=0
 peer.save(s);return peer
for coop in (False,True):
 label='cooperative' if coop else 'tactical';tick='cooperativeTick' if coop else 'planTick';frame='cooperativeFrame' if coop else 'planFrame';req='cooperativePlan' if coop else 'planRequest';ack='cooperativeAck' if coop else 'planAck';result='cooperativeResult' if coop else 'planResult'
 for cause in ('fresh','time','frame','future','rollback','epoch'):
  peer=setup(coop);m=peer.m;s=peer.state()
  if cause=='time':setattr(s,tick,m.clock-251)
  if cause=='frame':peer.advance(13 if coop else 7)
  if cause=='future':setattr(s,tick,m.clock+500)
  if cause=='rollback':setattr(s,frame,m.r(0xa8ed84)+1)
  if cause=='epoch':setattr(s,'cooperativeMatch' if coop else 'planMatch',s.data.match+1)
  peer.save(s);peer.tick();s=peer.state()
  assert getattr(s,ack)==getattr(s,req),(label,cause,'missing ACK')
  if cause=='fresh':assert m.r(0xa802c8)==1,(label,'fresh rejected',getattr(s,result))
  else:assert C.c_int32(getattr(s,result)).value==-7 and m.r(0xa802c8)==0,(label,cause,getattr(s,result))
  passed(label+'_'+cause)
 # Full transport retains the request only until the source expires. The
 # source stamp is deliberately NOT refreshed when transport becomes free.
 peer=setup(coop);m=peer.m;m.w(0xa802c8,128);peer.tick();s=peer.state();assert getattr(s,ack)!=getattr(s,req)
 peer.clear_queues();m.clock+=300;peer.advance();peer.tick();s=peer.state()
 assert getattr(s,ack)==getattr(s,req) and C.c_int32(getattr(s,result)).value==-7 and m.r(0xa802c8)==0
 passed(label+'_expires_under_queue_backpressure')
# Partial batch: one free slot, original source expires; preserve accepted
# prefix and expose only unprocessed suffix for the host's fresh read/replan.
peer=setup(True,2);m=peer.m;m.w(0xa802c8,111);peer.tick();s=peer.state()
assert s.cooperativeAcceptedBits[0]==1
assert s.cooperativeAccepted==1 and s.cooperativeProcessed==1 and s.cooperativeAck!=s.cooperativePlan,(s.cooperativeAccepted,s.cooperativeProcessed,s.cooperativeResult)
peer.clear_queues();m.clock+=300;peer.advance();peer.tick();s=peer.state()
assert s.cooperativeAck==s.cooperativePlan and s.cooperativeResult==-7 and s.cooperativeAccepted==1 and s.cooperativeProcessed==1 and s.cooperativeRejected==1
assert m.r(0xa802c8)==0
# Retry only remaining source with newly acquired source stamp; same original
# intent, fresh capacity validation; accepted first actor is never resubmitted.
s.cooperativePlan+=1;s.cooperativeCount=1;s.cooperativeOrders[0]=s.cooperativeOrders[1];s.cooperativeTick=m.clock;s.cooperativeFrame=m.r(0xa8ed84);peer.save(s);peer.advance();peer.tick();s=peer.state()
assert s.cooperativeAck==s.cooperativePlan and s.cooperativeAccepted==1 and m.r(0xa802c8)==1
packet=bytes(m.u.mem_read(0xa802d4,111));assert packet
passed('partial_cooperative_batch_replans_only_unprocessed_suffix')
a.output.write_text(json.dumps({'scope':'actual compiled GameRuntime in x86 emulator; synthetic world/Win32; not real game/netplay latency', 'inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')
