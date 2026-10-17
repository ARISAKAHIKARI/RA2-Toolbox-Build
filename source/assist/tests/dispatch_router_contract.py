from pathlib import Path
s=Path(__file__).resolve().parents[1]/'source'
b=(s/'bridge.cpp').read_text(encoding='utf-8')
r=(s/'dispatch_router.h').read_text(encoding='utf-8')
assert b.count('static bool enqueue(Event&e)')==1
assert b.count('static void tick(){')==1
assert b.count('static void dispatchFromTrigger(')==2 # declaration + definition
assert 'dispatchFromTrigger(dispatchrouter::Trigger::GameFrame,now);' in b
assert 'dispatchFromTrigger(dispatchrouter::Trigger::Message,GetTickCount(),' in b
assert 'dispatchrouter::accepts(trigger,state)' in b
assert b.count('GuardedAssistTick();')>=3 # declaration(s) and a single invocation
assert b.count('  GuardedAssistTick();')==1
assert 'maintainScheduler();' in b
assert 'schedulerBusy' in b
assert 'e.frame=uint32_t(frame)' not in b
assert 'timeGetTime()' not in b
assert 'state.frameInstalled && state.frameValidated' in r
assert 'state.matchingGameThread' in r
print('PASS: selected callback only wakes unified tick; event gateway is independent')
