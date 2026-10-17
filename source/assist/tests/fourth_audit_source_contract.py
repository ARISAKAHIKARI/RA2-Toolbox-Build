"""Source-level guards only; not a claim of Windows ABI or multiplayer validation."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]/'source'
b=(r/'bridge.cpp').read_text()
u=(r/'main.cpp').read_text()
c=(r/'cooperative_bridge.h').read_text()
p=(r/'placement_bridge.h').read_text()
a=(r/'autoload_bridge.h').read_text()
assert 'if(!eventdelivery::mayQueue(get<int>(0xA8B238)))' in b
assert 'eventdelivery::isSingleplayer(get<int>(0xA8B238))' in b
assert 'gateway::validate(ctx,intent,3500)' in b
assert 'actorFrameArbiter.record(' in b
assert 'if(!pendingRepairs.room(id)||!repairEvent(house,id))break;' in b
assert 'pendingRepairs.queued(id,p);' in b
assert 'pendingRepairs.observe(' in b
assert 'if(repairAckPending&&(repairAt>=repairCount||DWORD(now-repairAckStarted)>3000))' in b
assert 'pendingAck=repairAckRequest;pendingAckReady=true;repairAckPending=false;' in b
assert 'if(pendingAckReady){MemoryBarrier();sh->ack=pendingAck;pendingAckReady=false;}' in b
assert 'recordReceipt(queued)' in b and 'recordReceipt(receipt)' in b
assert 'if(!externalValid){motion.clear();labelRasters.clear();return;}' in u
assert 'if(!control){snap.frame=d.frame;snap.session=d.session;snap.chat=d.chat;}' in u
assert 'if(!control){' in u and 'snap.typeCount=std::clamp(d.typeCount' in u
assert 'DllMain' not in b and 'EntryRuntimeDetach();' in (r.parents[1]/'game_runtime/source/runtime.cpp').read_text()
assert 'GET_MODULE_HANDLE_EX_FLAG_PIN' in b and 'GET_MODULE_HANDLE_EX_FLAG_PIN' in (r/'native_input_hooks.h').read_text()
assert 'flags&=~uint32_t(SellEnemy)' in c
assert 'eventdelivery::isSingleplayer' in p
assert 'r.retryAfter=frame+6*r.attempts' in a
assert 'snap.receiptWrite=snap.receiptCount=0' in b
assert 'Version=34' in (r/'protocol.h').read_text()
print('PASS: fourth-audit fail-closed dispatch, repair ACK, passive data isolation, hook shutdown and task receipts')
