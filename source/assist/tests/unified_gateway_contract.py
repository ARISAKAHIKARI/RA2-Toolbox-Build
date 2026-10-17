"""Static safety invariants for the unified command gateway (not an ABI test)."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'source'
bridge=(root/'bridge.cpp').read_text(encoding='utf-8')
input_hooks=(root/'native_input_hooks.h').read_text(encoding='utf-8')
cs_hooks=(root/'auto_cs_bridge.h').read_text(encoding='utf-8')
policy=(root/'hook_policy.h').read_text(encoding='utf-8')
load=(root/'autoload_bridge.h').read_text(encoding='utf-8')
native=(root/'native_orders_bridge.h').read_text(encoding='utf-8')
coop=(root/'cooperative_bridge.h').read_text(encoding='utf-8')
ui=(root/'main.cpp').read_text(encoding='utf-8')
protocol=(root/'protocol.h').read_text(encoding='utf-8')
reader=(root/'../../external_reader/source/process_discovery.h').read_text(encoding='utf-8')
assert 'Version=40' in protocol
assert 'RA2Assist_v040_' in bridge and 'RA2Assist_v040_' in ui
assert bridge.count('static bool nativeMission(')==1
assert 'static bool nativeObjectEvent(' in bridge
assert 'static bool nativeProduceEvent(' in bridge
assert 'nativeMission(house,infantry,7,0,vehicle,0x34' in load
assert 'static bool nativeMission(' not in native
assert 'nativeObjectEvent(get<int>(lastHouse+0x30),0x16,' in (root/'pointer_bridge.h').read_text()
assert '++snap.queuedEvents' in bridge and 'snap.observedEvents+=missionObserver.pollWithReceipts(' in bridge
assert 'missionObserver.clear()' in bridge
assert 'unloadCount=0;' not in input_hooks and 'writeInputCodeGuarded' not in input_hooks
for body in (cs_hooks,policy):
    assert '(AutoCS|AdvancedInput|MagnetronInput|CustomCS|AutoOreReturn)' not in body
assert '(NativeOrders|CustomCS|AutoOreReturn)' not in input_hooks
assert 'if(!scan.best.pid)' in ui
assert 'if(!scan.best.pid)' in (root/'../../unit_monitor/source/main.cpp').read_text(encoding='utf-8')
assert 'result.uniqueWindowless=ids.size()==1;' in reader
assert 'snap.bridgeStatus' in ui and 'snap.readStatus' in ui
assert 'DirectProduce",0)' in ui
assert 'gateway::validate(ctx,intent,3500)' in bridge
assert 'actorFrameArbiter.record(' in bridge
assert 'recordReceipt(receipt)' in bridge
assert 'eventdelivery::mayQueue(get<int>(0xA8B238))' in bridge
assert 'repairAckRequest=0' in bridge
print('PASS: one engine order gateway, passive PID reader, hook isolation, version separation and observed/queued telemetry')
