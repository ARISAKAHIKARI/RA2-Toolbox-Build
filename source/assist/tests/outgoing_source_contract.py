"""Static contract tests for risky bridge changes; not Windows/MP validation."""
from pathlib import Path
s=Path(__file__).parent.parent/'source'
b=(s/'bridge.cpp').read_text()
enqueue=b.split('static bool enqueue(Event&e){',1)[1].split('static bool submitConstructed(',1)[0]
assert 'e.frame=' not in enqueue
assert 'gameSendTiming(timing)' in enqueue and 'timeGetTime()' not in enqueue
assert 'memcpy((void*)(p+12+tail*sizeof(Event)),&e,sizeof e)' in enqueue
assert 'put<uint32_t>(p+12+128*sizeof(Event)+tail*4,timing)' in enqueue
assert 'Version=42' in (s/'protocol.h').read_text()
external=(s/'external_orders_bridge.h').read_text()
check=external.split('static bool externalOrderValid(',1)[1].split('static outgoing::Batch',1)[0]
assert 'snap.csHookStatus!=1' not in check and '!eventPatched' not in check
assert 'if(!planningRoom(0)||!outgoingRoom()){++snap.planDeferred;return;}' in external
assert 'sh->planAck=request' in external
coop=(s/'cooperative_bridge.h').read_text()
assert 'if(!nativeReady()||sh->clickHookStatus!=1||sh->cooperativeSerial' not in coop
print('PASS: event copy / game callback / version lock / independent send / deferred ACK source contract')

# Critical: unhooking the local click handler must never rewind a partially
# submitted cooperative multiplayer command plan and resend accepted orders.
coop=(s/'cooperative_bridge.h').read_text()
restore=coop.split('static void restoreClickHooks(){',1)[1].split('static void clickHooksTick(',1)[0]
assert 'cooperativeAt=0' not in restore and 'cooperativePlanSeen=0' not in restore
assert 'entryReservationCount=0' not in restore
assert 'static void resetCooperativeState()' in coop
assert 'resetCooperativeState();' in b
# A cooperative UI feature needs its local click hook but must not patch the
# unrelated CS/Native outgoing mission interception sites.
for source in ('auto_cs_bridge.h','native_input_hooks.h'):
 text=(s/source).read_text()
 assert 'AutoOreReturn|CooperativeFlags' not in text
assert 'VirtualQuery(' in enqueue or 'VirtualQuery(' in b.split('static bool gameSendTiming',1)[1].split('static bool enqueue',1)[0]
assert 'read(p+0x10,&actual,sizeof actual)' in external
print('PASS: MO input hook isolation / batch continuity / executable callback / ID validation')

assert 'inputFlags=sh->flags|(unloadCount?NativeOrders:0)' in b
assert '(manualVersions.pausedCount?AutoOreReturn:0)' not in b
