"""Enforce the permitted mutation/installation surfaces, not test-name claims."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
files=[p for d in ('assist','cursor','unit_monitor','shared','external_reader','suite','game_runtime') for p in (root/d/'source').glob('*') if p.suffix in ('.h','.cpp')]
for p in files:
 text=p.read_text();relative=p.relative_to(root).as_posix()
 for inc in re.findall(r'^#include "([^"\n]+)"',text,re.M):
  assert (p.parent/inc).exists() or (root/'suite/source'/inc).exists(), (relative,inc)
 assert not re.search(r'\b(?:WriteProcessMemory|CreateRemoteThread|QueueUserAPC|mouse_event|keybd_event)\s*\(',text),relative
 if re.search(r'\bSendInput\s*\(',text):assert relative=='shared/source/host_game_input.h',relative
 for target in re.findall(r'UnhookWindowsHookEx\(([^)]+)\)',text):
  assert relative=='shared/source/host_game_input.h' or (relative,target) in (('cursor/source/main.cpp','clickHook'),('unit_monitor/source/main.cpp','keyHook')),(relative,target)
 for hook in re.findall(r'SetWindowsHookExW\(([^,]+)',text):
  assert relative=='shared/source/host_game_input.h' or hook in ('WH_KEYBOARD_LL','WH_MOUSE_LL'),(relative,hook)
 assert not re.search(r'if\s*\(\s*(?:false|true)\s*(?:\)|&&|\|\|)',text),relative
 if 'constructors::fill(' in text or re.search(r'\benqueue\(',text):assert relative=='assist/source/bridge.cpp',relative
 if 'VirtualProtect(' in text:assert relative in ('shared/source/game_entry_manager.cpp','assist/source/input_patch.h','assist/source/bridge.cpp'),relative
 if re.search(r'gameentry::patch\(',text):assert relative=='assist/source/input_patch.h',relative
 if 'resumeOriginalInput<' in text:assert relative in ('assist/source/cooperative_bridge.h','assist/source/sidebar.h'),relative
bridge=(root/'assist/source/bridge.cpp').read_text()
assert not re.search(r'if\s*\(\s*(true\s*\|\||false\s*&&)',bridge)
assert 'gameentry::active(gameentry::Client::Assist)' in bridge.split('static bool enqueue(Event&e){')[1].split('static entrygate::Snapshot')[0]
assert 'gameentry::capture(gameentry::Client::Assist,r)' in bridge
hook=bridge.split('void AssistRuntimeMessage(')[1].split('extern "C" void AssistRuntimeWatchdog')[0]
assert 'messageGameThread' not in hook and 'sh->' not in hook
host=(root/'assist/source/main.cpp').read_text()
assert not re.search(r'shared->ack\s*=',host), 'Host must not fabricate command ACK'
timeout=host.split('static bool uninitializedBridgeTimedOut(){')[1].split('}')[0]
assert 'shared->request==shared->ack' in timeout and 'GetTickCount()-attachAt' in timeout and '!commandMatch()' in timeout
assert 'if(uninitializedBridgeTimedOut()){' in host
assert 'GuardedPlacementAllowed' in (root/'assist/source/placement_bridge.h').read_text()
manager=(root/'shared/source/game_entry_manager.cpp').read_text()
assert manager.index('!policy.owns(tid,*r)')<manager.index('else if(r->operation==Operation::Patch)')
print('PASS: complete local includes; only central command/patch/host-input owners; no fake ACK, forced backend or unguarded placement/message ingress')

# A denied native cursor capture must use this invocation's return value.
native=(root/'assist/source/native_input_hooks.h').read_text()
guard=(root/'assist/source/guard.cpp').read_text()
assert 'cursorForced' not in native
assert 'extern "C" int GuardedCursorCapture' in native
assert 'test eax, eax\n  jz cursor_original_path' in native
segment=guard.split('int GuardedCursorCapture(unsigned* saved)')[1].split('GuardedNativeEventCapture')[0]
assert 'if(!entry)return 0;' in segment and 'result=CursorCapture(saved)' in segment
policy=(root/'shared/source/entry_policy.h').read_text()
assert 'r.bindThread&&priorThreadDead' in policy
assert 'boundClient!=' not in policy
print('PASS: denied cursor capture resumes original predicate; no live binding promotion exception')
runtime=(root/'game_runtime/source/runtime.cpp').read_text()
client=(root/'shared/source/game_entry_client.h').read_text()
inputhost=(root/'shared/source/host_game_input.h').read_text()
assert 'inline volatile LONG activeToken=0;' in client
assert 'LoadLibraryW(' not in client and 'return &ManageGameEntry;' in client
assert 'DllMain' not in bridge and 'DllMain' not in (root/'cursor/source/hide.cpp').read_text()
assert not (root/'unit_monitor/source/bridge.cpp').exists()
assert 'SetTimer(' not in (root/'cursor/source/hide.cpp').read_text()
for module in ('assist','cursor'):
 src=(root/module/'source/main.cpp').read_text()
 assert not re.search(r'LoadLibraryW\([^\n]*(?:Bridge|GameRuntime|GameEntryManager)',src)
 assert 'gameinput::install(' in src
exports=(root/'game_runtime/source/runtime.def').read_text()
assert all(x not in exports for x in ('AssistHook','CursorHook','UnitMonitorHook'))
assert 'GameRuntimeHook' in exports and 'GameRuntimePostHook' in exports
assert 'if(ready&runtime::Cursor)CursorRuntimeMessage' in runtime
assert runtime.index('CursorRuntimeMessage(code')<runtime.index('AssistRuntimeMessage(code')
assert 'CreateTimerQueueTimer' in inputhost and 'wakePending' in inputhost
print('PASS single-runtime boundary: one shared token, no component DLL loaders/exports/timers, host-only subscriptions and readiness')

loading=(root/'assist/source/autoload_bridge.h').read_text()
query=loading.split('static bool canEnter(uint32_t p,uint32_t target){')[1].split('static int passengerSlots')[0]
assert query.index('!gameentry::active(gameentry::Client::Assist)')<query.index('GuardedEnterAction(')
assert guard.count('int GuardedEnterAction(')==1 and '_exception_code()==0xc0000005u?' in guard
assert 'GuardedEnterAction' not in exports
print('PASS native legality isolation remains under existing Runtime lease and is not an exported/private ingress')

sidebar=(root/'assist/source/sidebar.h').read_text()
assert sidebar.count('resumeOriginalInput<')==1
assert 'GuardedTechEligibility(args)' in sidebar
assert '0x6AA931' not in sidebar
