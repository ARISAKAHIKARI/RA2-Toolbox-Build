#!/usr/bin/env python3
"""Actual sender DLL with synthetic world; no real game/receiver claims."""
import argparse,ctypes as C,hashlib,json,struct,traceback
from pathlib import Path
from two_peer_sync import Peer
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
def case(name,fn):
 try:details=fn();rows.append({'case':name,'status':'PASS','details':details});print('PASS',name,flush=True)
 except Exception as e:rows.append({'case':name,'status':'FAIL','error':str(e)});traceback.print_exc()
def tick(peer,ms=4,frames=0):
 peer.m.clock+=ms;peer.advance(frames);peer.m.w(peer.m.SH+peer.m.Shared.heartbeat.offset,peer.m.clock)
 peer.m.run(peer.m.entry,(0,1,0));return peer.state()
def coop(peer,count=1):
 m=peer.m;s=peer.state();s.cooperativePlan+=1;s.cooperativeSerial+=1;s.cooperativeTick=m.clock;s.cooperativeFrame=m.r(0xa8ed84);s.cooperativeMatch=s.data.match;s.cooperativeMode=1;s.cooperativeCount=count
 s.cooperativeRequest.serial=s.cooperativeSerial;s.cooperativeRequest.match=s.data.match;s.cooperativeRequest.mode=1;s.cooperativeRequest.tick=m.clock
 for i in range(count):
  o=s.cooperativeOrders[i];o.id=m.r(peer.owned[1]+0x10);o.target=m.r(peer.owned[0]+0x10);o.type=m.t
 peer.save(s);return s

def same_frame():
 q=Peer(a.game,a.dll,0);q.set(flags=1048576,scanMs=1000);q.tick();s=coop(q);result=tick(q)
 assert result.cooperativeAck==s.cooperativePlan,'same-frame request skipped by display scan cadence'
 assert result.cooperativeAccepted==1
 return {'modeled_submit_to_ack_ms':4,'frame_advanced':False}
case('cooperative_same_frame_bypasses_slow_display_scan',same_frame)
def end_round():
 q=Peer(a.game,a.dll,0);q.set(flags=1048576);q.tick();s=coop(q);q.m.w(0xa83d4c,0);result=tick(q,120,1)
 assert result.cooperativeAck==s.cooperativePlan,'ended round left cooperative mailbox pending'
 assert result.cooperativeResult==-3 and result.cooperativeAccepted==0 and result.cooperativeRejected==1
 assert q.m.r(0xa802c8)==0
case('round_end_settles_cooperative_mailbox',end_round)
def population():
 q=Peer(a.game,a.dll,0);m=q.m;miners=[];soldiers=[]
 m.b(m.t+0xe0e,1);m.vector(m.t+0x3e8,[m.buildT]);m.w(m.t+0x800,28);m.w(m.t+0x5e0,128)
 for i in range(24):
  unit=m.obj(0x7f5c70,m.t,m.house,100+i);m.u.mem_write(unit+0x33c,struct.pack('<4f',28,0,0,0));miners.append(unit)
  soldier=m.obj(0x7eb058,m.engT,m.house,200+i);soldiers.append(soldier)
 q.objects+=miners+soldiers;m.vector(0xa8ec78,q.objects);m.vector(0xa83d10,[q.owned[0]]+miners);m.vector(0xa83de8,[q.owned[1]]+soldiers)
 q.set(flags=32768|1048576);q.advance();q.tick();q.clear_queues();s=coop(q,24)
 for i,unit in enumerate(soldiers):s.cooperativeOrders[i].id=m.r(unit+0x10)
 s.flags|=64;s.repairMs=500;s.planRequest+=1;s.planMatch=s.data.match;s.planTick=m.clock;s.planFrame=m.r(0xa8ed84);s.planCount=24
 for i,unit in enumerate(miners):
  o=s.plan[i];o.unit=m.r(unit+0x10);o.destination=m.r(q.owned[2]+0x10);o.refinery=o.destination;o.mission=7;o.destinationTag=0x34;o.purpose=0
 q.save(s);return q

def fair():
 q=population();s=tick(q,4,1);packets=q.drain();types=[bytes.fromhex(x['packet'])[0] for x in packets]
 assert s.cooperativeAccepted>0,'cooperative channel starved by tactical plan'
 assert s.data.repairTotal>0,'repair starved by two independent planning budgets'
 assert len(packets)<=24
 assert s.planAck!=s.planRequest and s.cooperativeAck!=s.cooperativePlan
 return {'packets':len(packets),'cooperative_accepted':s.cooperativeAccepted,'repair_count':s.data.repairTotal,'types':types}
case('tactical_cooperative_repair_share_budget_without_starvation',fair)
def blocked_selection():
 from unicorn import UC_HOOK_CODE
 q=Peer(a.game,a.dll,0);m=q.m;q.set(flags=1048576);q.tick();s=coop(q)
 address=m.r(m.r(m.unit)+0x13c);m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,ctx:m.ret(1,0),begin=address,end=address)
 s.selectRequest+=1;s.selectTick=m.clock;s.selectMatch=s.data.match;s.selectIndex=next(i for i in range(s.data.typeCount) if bytes(s.data.types[i].id).split(b'\0')[0]==b'HTNK');s.selectGlobal=1;q.save(s)
 m.w(0xa802c8,128);s=tick(q,4,1)
 assert s.selectAck==s.selectRequest and s.selectResult==1
 assert s.cooperativeAck!=s.cooperativePlan and m.r(0xa802c8)==128
 return {'selection_acknowledged':True,'simulation_mailbox_preserved':True}
case('selection_remains_live_with_full_simulation_queue',blocked_selection)
def hooks_toggle():
 q=Peer(a.game,a.dll,0);m=q.m;sites=[0x646e90,0x731bf0,0x6ffe00,0x4d74e0,0x692610,0x4ab9b0]
 originals={at:bytes(m.u.mem_read(at,8)) for at in sites};flags=512|131072|8192|1024|262144|1048576|2097152|4194304|8388608|16777216
 for i in range(40):
  q.set(flags=flags if i%2==0 else 0);s=tick(q,20,1)
  assert s.data.hookConflicts==0
  rc,lease=m.manager_call(m.request(3));assert rc==1 and lease.activeModule==0
  if i%2:
   tick(q,120,1);assert all(bytes(m.u.mem_read(at,8))==b for at,b in originals.items())
 assert m.r(0xa802c8)==0
 return {'toggle_cycles':20,'sites_checked':len(sites)}
case('all_input_features_toggle_restore_without_patch_conflict',hooks_toggle)
def competing_cursor():
 q=Peer(a.game,a.dll,0);m=q.m;cs=0x22000000;m.u.mem_map(cs,0x500000);m.cursor_mapping_enabled=True
 m.u.mem_write(cs,struct.pack('<7I',0x4355523a,123,456,3,99,0,m.clock));ctrl=m.SH+0x3e0000;msg=m.SH+0x3e0100
 q.set(flags=1048576);q.tick();s=coop(q);m.w(ctrl+16,3);m.w(ctrl+20,3)
 m.run(m.entry,(0,1,msg));s=q.state();assert s.cooperativeAccepted==1 and m.r(cs+12)==0 and m.r(cs+16)==0
 rc,lease=m.manager_call(m.request(3));assert rc==1 and lease.activeModule==0 and lease.reentrant==0
case('cursor_capture_and_cooperative_send_share_one_wake',competing_cursor)
def pause_resume():
 q=Peer(a.game,a.dll,0);q.set(flags=1048576);q.tick();s=coop(q);q.m.b(0xa8e9a0,0);s=tick(q,4,1)
 assert s.cooperativeAck!=s.cooperativePlan and s.data.commandStatus==0
 q.m.b(0xa8e9a0,1);s=tick(q,4,1);assert s.cooperativeAccepted==1 and s.cooperativeAck==s.cooperativePlan
case('short_pause_preserves_fresh_cooperative_request',pause_resume)
def long_pause():
 q=Peer(a.game,a.dll,0);q.set(flags=1048576);q.tick();s=coop(q);q.m.b(0xa8e9a0,0);tick(q,4,1)
 q.m.b(0xa8e9a0,1);s=tick(q,300,1);assert s.cooperativeResult==-7 and s.cooperativeAck==s.cooperativePlan and q.m.r(0xa802c8)==0
case('long_pause_expires_plan_without_late_send',long_pause)
def partial_end():
 q=Peer(a.game,a.dll,0);q.set(flags=1048576);q.tick();s=coop(q,2);q.m.w(0xa802c8,111);s=tick(q,4,1)
 assert s.cooperativeAccepted==1 and s.cooperativeAck!=s.cooperativePlan
 q.clear_queues();q.m.w(0xa83d4c,0);s=tick(q,4,1)
 assert s.cooperativeAck==s.cooperativePlan and s.cooperativeResult==-3 and s.cooperativeAccepted==1 and s.cooperativeRejected==1 and s.cooperativeAcceptedBits[0]==1
 assert q.m.r(0xa802c8)==0
case('round_end_preserves_partial_send_receipts',partial_end)
def next_serial_end():
 q=Peer(a.game,a.dll,0);q.set(flags=1048576);q.tick();coop(q);s=tick(q,4,1);assert s.cooperativeAccepted==1
 q.clear_queues();coop(q);q.m.w(0xa83d4c,0);s=tick(q,4,1)
 assert s.cooperativeAck==s.cooperativePlan and s.cooperativeAccepted==0 and s.cooperativeProcessed==0 and s.cooperativeRejected==1 and s.cooperativeAcceptedBits[0]==0
case('new_serial_does_not_inherit_previous_accepted_receipt',next_serial_end)
a.output.write_text(json.dumps({'scope':'actual x86 DLL; modeled clock/objects/Win32; no real screen or network execution','inputs':{'GameRuntime.dll':hashlib.sha256(a.dll.read_bytes()).hexdigest()},'results':rows},indent=2)+'\n')
raise SystemExit(any(row['status']!='PASS' for row in rows))
