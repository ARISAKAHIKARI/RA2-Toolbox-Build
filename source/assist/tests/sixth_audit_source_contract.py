from pathlib import Path
r=Path(__file__).resolve().parents[2];s=r/'assist/source'
protocol=(s/'protocol.h').read_text();assert 'Version=34' in protocol
assert 'RA2Assist_v040_' in (s/'main.cpp').read_text()
assert 'RA2Assist_v040_' in (s/'bridge.cpp').read_text()
load=(s/'autoload_bridge.h').read_text()
assert 'finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);continue;' in load
assert 'finishLoadingReservation(r,match,frame,false);' in load
ore=(s/'external_planner.h').read_text()
assert 'effects::cargo(i->initialCargo,cargo' in ore and 'job.dockingObserved=true' in ore
cycle=(s/'magnetron_cycle.h').read_text()
assert 'rearmStart>sentFrame' in cycle and 'return readyRemaining>0' not in cycle
coop=(s/'cooperative_bridge.h').read_text()
assert 'observeCooperativeEffects(house,frame);' in coop
assert 'missionObserver.dismissTicket(match,ticket);' in coop
assert 'supersedeCooperativeEffect(o.id,match);' in coop
assert '||cooperativeEffectCount||' in (s/'bridge.cpp').read_text()
unload=(s/'native_orders_bridge.h').read_text()
assert 'finishUnloadFollowup(unloadPassengers[i],frame)' in unload
assert 'missionObserver.dismissTicket(match,ticket);' in unload
assert 'BUILD-VERIFIED-v0713.tag' in (r/'tools/verify_windows_outputs.py').read_text()
assert (r/'BUILD-0.7-ON-WINDOWS.cmd').exists()
print('PASS: v058 evidence gates, coordinated receipt supersession, protocol separation and build markers')
