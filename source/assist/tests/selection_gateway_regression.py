#!/usr/bin/env python3
"""Real DLL selection mailbox: priority does not bypass the common lease."""
import argparse, ctypes as C, hashlib, json
from pathlib import Path
from two_peer_sync import Peer
from unicorn import UC_HOOK_CODE
p=argparse.ArgumentParser();p.add_argument('game',type=Path);p.add_argument('dll',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
peer=Peer(a.game,a.dll,0);m=peer.m;rows=[]
selectable=m.r(m.r(m.unit)+0x13c)
m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,ctx:m.ret(1,0),begin=selectable,end=selectable)
def intent():
 s=peer.state();s.selectRequest+=1;s.selectTick=m.clock;s.selectMatch=s.data.match;s.selectIndex=next(i for i in range(s.data.typeCount) if bytes(s.data.types[i].id).split(b"\0")[0]==b"HTNK");s.selectGlobal=1;peer.save(s);return s.selectRequest
def passed(name):rows.append({'case':name,'status':'PASS'});print('PASS',name,flush=True)
intent();s=peer.state();s.command=1;s.request+=1;s.requestTick=m.clock;s.requestMatch=s.data.match;peer.save(s)
m.w(0xa802c8,128);peer.tick();s=peer.state()
assert s.selectAck==s.selectRequest and s.selectResult==1 and s.selectCount>0, (s.selectAck,s.selectRequest,C.c_int32(s.selectResult).value,s.selectCount,s.selectIndex)
assert s.count==0 and m.r(0xa802c8)==128
passed('selection_succeeds_while_gameplay_queue_is_full')
peer.clear_queues();intent();before=peer.state().selectAck
q=m.request(0,2,m.cursor_dll.OPTIONAL_HEADER.ImageBase);rc,lease=m.manager_call(q);assert rc==1
peer.tick();assert peer.state().selectAck==before
passed('cursor_owner_blocks_selection_mailbox_execution')
lease.operation=1;assert m.manager_call(lease)[0]==1
peer.tick();assert peer.state().selectAck==peer.state().selectRequest
passed('selection_resumes_after_same_manager_releases_owner')
intent();before=peer.state().selectAck;m.current_thread=778;peer.tick();assert peer.state().selectAck==before
passed('wrong_thread_cannot_consume_selection_mailbox')
m.current_thread=777;s=peer.state();s.selectMatch+=1;peer.save(s);peer.tick();assert C.c_int32(peer.state().selectResult).value==-7
passed('stale_match_selection_rejected')
# Group union is selected in one transaction, not successive clear/select calls.
for global_mode in (False,True):
 peer=Peer(a.game,a.dll,0);m=peer.m
 for addr in {m.r(m.r(p)+0x13c) for p in peer.owned[:2]}:
  m.u.hook_add(UC_HOOK_CODE,lambda uc,addr,size,ctx:m.ret(1,0),begin=addr,end=addr)
 if not global_mode:
  # One owned engineer is outside the viewport; enemies of matching types stay excluded.
  m.projection_visible_bounds=(200,200);m.w(peer.owned[1]+0x9c,300*256)
 s=peer.state();s.selectRequest+=1;s.selectTick=m.clock;s.selectMatch=s.data.match;s.selectIndex=-3;s.selectGlobal=int(global_mode)
 for i in range(s.data.typeCount):s.selectTypes[i]=int(bytes(s.data.types[i].id).split(b"\0")[0] in (b"HTNK",b"ENGINEER"))
 peer.save(s);m.calls.clear();peer.tick();s=peer.state()
 calls=[c for c in m.calls if c[0] in (0x48dc90,0x6fbfa0)]
 selected=[c[1] for c in calls if c[0]==0x6fbfa0]
 assert selected==list(peer.owned[:2] if global_mode else peer.owned[:1]),(global_mode,selected,peer.owned)
 assert len([c for c in calls if c[0]==0x48dc90])==1 and s.selectCount==len(selected) and s.selectAck==s.selectRequest
 assert m.r(0xa802c8)==0
 passed('group_union_whole_map' if global_mode else 'group_union_visible_only')
 # Empty and invalid masks preserve the user's old selection.
 for invalid in ('empty','building','out_of_range','stale','chat'):
  s=peer.state();s.selectRequest+=1;s.selectTick=m.clock;s.selectMatch=s.data.match;s.selectIndex=-3
  for i in range(512):s.selectTypes[i]=0
  if invalid=='building':s.selectTypes[next(i for i in range(s.data.typeCount) if s.data.types[i].kind==7)]=1
  elif invalid=='out_of_range':s.selectTypes[511]=1
  else:s.selectTypes[0]=int(invalid in ('stale','chat'))
  if invalid=='stale':s.selectMatch+=1
  if invalid=='chat':m.b(0xa8bc60+25,1)
  peer.save(s);m.calls.clear();peer.tick();s=peer.state()
  assert not [c for c in m.calls if c[0] in (0x48dc90,0x6fbfa0)] and C.c_int32(s.selectResult).value<0
  m.b(0xa8bc60+25,0)
  if global_mode:passed('group_reject_'+invalid)
a.output.write_text(json.dumps({'scope':'real DLL and manager; native selection methods stubbed; no game rendering',
 'inputs':{n:hashlib.sha256(a.dll.with_name(n).read_bytes()).hexdigest() for n in ('GameRuntime.dll',)},'results':rows},indent=2)+'\n')
