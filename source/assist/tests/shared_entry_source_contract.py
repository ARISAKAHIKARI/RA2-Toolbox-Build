from pathlib import Path
root=Path(__file__).resolve().parents[2]
manager=(root/'shared/source/game_entry_manager.cpp').read_text()
client=(root/'shared/source/game_entry_client.h').read_text()
writer=(root/'assist/source/input_patch.h').read_text()
assert 'static assist::hookregistry::Registry<>' in manager
assert 'patches.proposal' in manager and 'patches.commit' in manager
assert 'VirtualProtect' not in writer.split('// Immutable trampoline:')[0]
assert 'gameentry::patch' in writer
assert 'return &ManageGameEntry;' in client
assert 'LoadLibraryW(' not in client and 'GetProcAddress(' not in client
assert 'InterlockedCompareExchange(&busy,1,0)' in manager
assert 'patchModules[prior]!=r.module' in manager
assert 'patches.slots[i].owner==Owner::Frame' in manager
for relative in ('assist/source/bridge.cpp','cursor/source/hide.cpp','game_runtime/source/runtime.cpp'):
 s=(root/relative).read_text();assert 'gameentry::Scope' in s, relative
 assert 'game_entry_client.h' in s or 'input_patch.h' in s
cursor=(root/'cursor/source/hide.cpp').read_text()
for signature in ('void CursorRuntimeMessage','void CursorRuntimeWatchdog','void CursorRuntimePost'):
 body=cursor.split(signature,1)[1].split('extern "C"',1)[0];assert 'gameentry::Scope' in body
assert 'SetTimer(' not in cursor
boundary=(root/'assist/source/guard.cpp').read_text()
assert boundary.count('BeginAssistCapture()')==9
assert boundary.count('__finally {')==8
assert 'gameentry::Scope processEntry' in (root/'assist/source/bridge.cpp').read_text()
for d in ('assist',):
 assert 'ManageGameEntry(' not in (root/d/'source'/'bridge.cpp').read_text() if d!='cursor' else True
print('PASS: one manager, process-global ownership, guarded DLL callbacks, capture finally-release and central game-code writes')

tech=boundary.split('void GuardedTechEligibility(unsigned* args)')[1]
assert 'BeginAssistCapture()' in tech and '__finally {EndAssistCapture(entry);}' in tech
