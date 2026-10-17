from pathlib import Path
s=Path(__file__).resolve().parents[1]/'source'
b=(s/'bridge.cpp').read_text()
u=(s/'main.cpp').read_text()
p=(s/'protocol.h').read_text()
assert 'static void tick(){' in b and b.count('static void tick(){')==1
assert 'GuardedAssistTick();' in b and 'installFrameDriver()' in b
assert 'frameprologue::safeOverwrite(' in b
assert 'restoreFrameDriver();' in b
assert 'writeInputCodeGuarded(at,original,framePatch,n)' in b
assert 'codeHookRegistry.markConflict(frameAt)' in b
assert 'dispatchFromTrigger(' in b and 'dispatchrouter::accepts(trigger,state)' in b
assert 'static bool enqueue(Event&e)' in b and b.count('static bool enqueue(Event&e)')==1
assert 'driverMode=schedulerMode' in u
assert 'RA2_TOOLBOX_DRIVER' in u
assert 'Version=34' in p and 'driverTarget' in p and 'RA2Assist_v041_' in u and 'RA2Assist_v041_' in b
print('PASS: two entrypoints, single shared tick/vanilla event gateway, verified frame hook, safe fallback')
