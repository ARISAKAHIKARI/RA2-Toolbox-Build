from pathlib import Path
p=Path(__file__).resolve().parents[1]/'source'
ui=(p/'main.cpp').read_text(encoding='utf-8')
bridge=(p/'bridge.cpp').read_text(encoding='utf-8')
planner=(p/'external_planner.h').read_text(encoding='utf-8')
assert 'if(ejectMode){hookEject=true;' in ui
assert 'hookEject=ejectMode;' in ui
assert 'if(LOBYTE(ejectKey)&&!registerKey(1696,ejectKey))' in ui
assert 'installFrameDriver()' not in bridge
assert 'maintainScheduler()' in bridge
assert 'if(trigger!=dispatchrouter::Trigger::Message)return;' in bridge
assert 'get<int>(0xA8ED84)!=lastFrame' in bridge
assert 'sh->plannerTracking&&(sh->flags&(AdvancedInput|MagnetronInput|AutoOreReturn))' in bridge
assert 'magnetcycle::shotObserved' in planner
assert 'task.skipTarget=task.target;' in planner
assert 'eventdelivery::isSingleplayer(get<int>(0xA8B238))' in bridge and 'eventdelivery::mayQueue(get<int>(0xA8B238))' in bridge
assert '0x8B4204u+unsigned(next)*sizeof(Event)' in bridge
assert '0xA70204u+unsigned(next)*4u' in bridge
assert 'local.executed,local.house,local.frame' in bridge
native=(p/'native_orders_bridge.h').read_text(encoding='utf-8')
assert 'if(snap.nativeHookStatus!=1&&nativeCount)' in native
assert 'if((!(sh->flags&NativeOrders)&&!unloadCount)||snap.nativeHookStatus!=1)' not in native
assert 'snap.nativeHookStatus!=1||!loadInterfaces()' not in native
assert 'if(!(sh->flags&SmartUnloadOrders)||!loadInterfaces())return -12;' in native
assert 'nativeMission(house' in native
print('PASS: single message dispatcher, no frame detours, eject toggle connection, magnetic cycle and vanilla event ordering and SP/MP queue isolation')
