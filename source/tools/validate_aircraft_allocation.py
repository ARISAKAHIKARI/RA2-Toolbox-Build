"""Aircraft policy integration guards; behavior is exercised by C++/Runtime tests."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
read = lambda p: (root/p).read_text(encoding='utf-8')
types = read('assist/source/cooperative_types.h')
assert 'AirDamage=4' in types and 'AirSingle=5' in types
assert 'double expectedDamage=0' in types and 'retrySourcePool' in types
assert re.search(r'Version\s*=\s*80\b', read('assist/source/protocol.h'))
settings = read('assist/source/extra_settings_io.h')
assert settings.count('L"AircraftAllocationPolicy"') == 2
assert 'cooperative::reserveAccepted(input,acceptedCooperative)' in settings
assert 'cooperative::retrySourcePool(' in settings
bridge = read('assist/source/cooperative_bridge.h')
assert 'cooperative::reserveAccepted(input,runtimeCoopAccepted)' in bridge
assert 'cooperative::retrySourcePool(' in bridge
assert 'singleDamageCommitted*5<double(get<int>(t+0x6C))*6' in bridge
assert 'if(mode!=cooperative::AirSingle)cancelNative(p)' in bridge
assert bridge.index('if(!nativeMission(house,o.id,mission,') < bridge.index('deselectDispatchedAircraft(p)')
helper = read('assist/source/aircraft_selection_bridge.h')
for forbidden in ('unselect()', '0x48DC90', 'WriteProcessMemory', 'VirtualProtect'):
    assert forbidden not in helper, forbidden
assert 'dispatchedAircraftSelectionReady' in helper and 'call<void>(0x5F44A0,p)' in helper
runtime = read('game_runtime/source/runtime.cpp')
assert 'msg->message==WM_RBUTTONDBLCLK' in runtime
policy = read('assist/source/native_message_policy.h')
assert 'singleCooperative' in policy and 'WM_RBUTTONDBLCLK' in policy
gateway = read('assist/source/bridge.cpp')
assert 'bool cooperativeTakeover=false' in gateway
assert 'cooperativeTakeover&&owner==taskarbiter::Owner::Cooperative' in gateway
assert gateway.index('if(takeover){int old=actorLeaseArbiter.find(id)') > gateway.index('else if(!submitMegaMission(house,id,mission,')
print('PASS: IPC80, both allocation locations, accepted-damage reservations, post-submit single-object deselection, explicit ownership takeover and double-click forwarding')
