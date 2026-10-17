from pathlib import Path
p=Path(__file__).resolve().parents[1]/'source'
bridge=(p/'bridge.cpp').read_text()
registry=(p/'hook_registry.h').read_text()
writer=(p/'input_patch.h').read_text()
manager=(p.parents[1]/'shared/source/game_entry_manager.cpp').read_text()
assert 'Registry<>' in manager and 'patches.proposal' in manager
assert 'static assist::hookregistry::Registry<>' not in writer
assert 'patches.commit' in manager and 'patches.markConflict' in manager
assert 'gameentry::patch' in writer and 'gameentry::inspect' in writer
assert 'knownOriginal(' in registry and 'Owner::Unknown' in registry
assert 'if(s.conflict' in registry and 'Transition::Restore' in registry
assert 'PAGE_READWRITE' in writer and 'PAGE_EXECUTE_READ' in writer
assert 'makeInputTrampoline(' in (p/'auto_cs_bridge.h').read_text()
assert 'makeInputTrampoline(' in (p/'native_input_hooks.h').read_text()
assert 'makeInputTrampoline(' in (p/'placement_bridge.h').read_text()
assert 'writeInputCodeGuarded(at,original,framePatch,n)' in bridge
assert 'writeInputCodeGuarded(frameAt,framePatch,frameOriginal,frameSize)' in bridge
assert 'frameInstalled=!restored;' in bridge and 'frameRestoreConflict=true' in bridge
assert 'InterlockedCompareExchange(&messageGameThread' in bridge
assert 'FrameWrongThread' in bridge and 'dispatchFromTrigger(dispatchrouter::Trigger::GameFrame,now);' in bridge
assert 'state.frameInstalled && state.frameValidated' in (p/'dispatch_router.h').read_text()
assert 'DllMain' not in bridge and 'EntryRuntimeDetach();' in (p.parents[1]/'game_runtime/source/runtime.cpp').read_text()
assert 'magnetShots' in (p/'external_planner.h').read_text()
assert '观察卸货 ' in (p/'main.cpp').read_text() and '磁电出手观测' in (p/'main.cpp').read_text()
print('PASS: source routes all known detours via owned patch registry; protected fallback and business diagnostics')
