from pathlib import Path
import runpy
r=Path(__file__).resolve().parents[2]
runpy.run_path(str(r/'tests/screen_capture_test.py'),run_name='__main__')
b=(r/'assist/source/bridge.cpp').read_text();e=(r/'assist/source/external_orders_bridge.h').read_text();c=(r/'assist/source/cooperative_bridge.h').read_text()
assert 'bool cmd=sh->request!=sh->ack||selectionPending||planPending||cooperativePending||sh->pointer.request!=sh->pointer.ack;' in b
assert 'if(!active){cancelCooperativePlan(-3);if(lastHouse||snap.status!=0)reset();' in b
assert 'static planningbudget::Budget sharedPlanningBudget;' in b
assert 'planningRoom(0)' in e and 'consumePlanning(0)' in e
assert 'planningRoom(1)' in c and 'consumePlanning(1)' in c
assert 'remainingBudget' not in e and 'recurringBudget(EventLane::Planning)' not in c
assert 'sh->cooperativePlan!=cooperativePlanSeen' in c
print('PASS shared planning allowance, same-frame ingress, end-of-round receipt ownership')
