from pathlib import Path
r=Path(__file__).resolve().parents[2]
h=(r/'assist/source/main.cpp').read_text();b=(r/'assist/source/external_orders_bridge.h').read_text()
assert 'shared->planTick=result->tick;shared->planFrame=result->frame' in h
task=(r/'assist/source/planner_task.h').read_text()
assert 'out->tick=world_->tick;out->frame=world_->frame' in task
assert h.count('freshness::valid(w.match')==1 and h.count('freshness::valid(result->match')==1
assert 'planner::Receipt::Expired' in h and 'planner_.expired(r.order)' in task
assert h.index('freshness::valid(result->match')<h.index('shared->planTick=result->tick')
assert 'shared->planTick=GetTickCount()' not in h
assert '4000' not in b and 'planBatch.next==0' not in b
assert b.count('if(!fresh()){result=-7;break;}')==2
assert b.index('if(!fresh()){result=-7;break;}',b.index('static void externalOrdersTick'))<b.index('int admitted=admitPlannerOrder(o,house)')
assert b.index('if(!externalOrderValid(o,house))return -1;')<b.index('if(!nativeMission(house,o.unit')
c=(r/'assist/source/extra_settings_io.h').read_text();g=(r/'assist/source/cooperative_bridge.h').read_text()
assert c.count('readFresh(0xA8ED84')==2 and c.count('readFresh(0xA83D4C')==2
assert 'frameAfter<frameBefore' in c and 'cooperativeTick=acquired' in c
assert c.index('auto assignments=cooperative::allocate(input)')<c.index('frameAfter<frameBefore')<c.index('InterlockedIncrement((volatile LONG*)&shared->cooperativePlan)')
assert 'cooperative::retryRemaining' in c and 'cooperativeProcessed=cooperativeAt' in g
assert 'cooperativeResult=-7' in g and 'freshness::intentAlive' in g
assert 'DWORD(now-sh->cooperativeTick)>5000' not in g
print('PASS source stamps survive planning; per-order deadlines; uncached read guards; suffix-only fresh replan')
