"""Current release invariants; runtime and host-polling tests provide behavior checks."""
from pathlib import Path
import re
r=Path(__file__).resolve().parents[2]
sources=[p for d in ('assist','cursor','unit_monitor','shared','game_runtime','external_reader','suite') for p in (r/d/'source').rglob('*') if p.suffix in ('.h','.hpp','.cpp','.c')]
for p in sources:
 s=p.read_text()
 assert not re.search(r'\b(?:WriteProcessMemory|VirtualProtect|CreateRemoteThread|VirtualAllocEx)\s*\(',s),p
 for hook in re.findall(r'SetWindowsHookExW\(([^,]+)',s):
  assert p==r/'shared/source/host_game_input.h' and hook in ('WH_GETMESSAGE','WH_CALLWNDPROCRET'),(p,hook)
 assert 'makeInputTrampoline' not in s and 'patchNativeInput' not in s,p
manager=(r/'shared/source/game_entry_manager.cpp').read_text()
assert 'applyPatch(const Request&){return Result::PatchDenied;}' in manager
bridge=(r/'assist/source/bridge.cpp').read_text();pointer=(r/'assist/source/pointer_bridge.h').read_text();host=(r/'assist/source/pointer_host.h').read_text()
assert bridge.count('pointerTick();')==3 and 'snap.inputPatchMask=0' in bridge
assert 'installFrameDriver' not in bridge and 'assistFrameObserver' not in bridge
assert 'i.inputSerial!=b.cancelSerial' in pointer and 'DWORD(now-i.tick)>1500' in pointer
assert 'pointerDone[n]=1' in pointer and 'manualVersions.get(i.sources[n].id)' in pointer
assert 'get<int>(p+0xAC)!=2' in pointer and 'destination!=readMapCell' in pointer
assert 'call<void>(0x692300,0x87F7E8,&point,&cell,&coord,&object,&a,&b)' in pointer
assert 'nativeMission(' in pointer and 'nativeObjectEvent(' in pointer and 'enqueue(' not in pointer
assert 'i.x>=width||i.y>=height' in pointer
assert 'GetAsyncKeyState' not in pointer
assert 'box.ack=' not in host and 'box.result=' not in host
assert 'e.serial!=box.cancelSerial' in host and 'externalReader.readFresh' in host
assert 'in.tick=e.tick' in host and 'in.tick=GetTickCount' not in host
assert 'Version=41' in (r/'assist/source/protocol.h').read_text()
ui=(r/'assist/source/main.cpp').read_text()
assert 'techMode=false;' in ui and 'buildAnywhereMode=false;' in ui and 'schedulerMode=0;schedulerTarget=0' in ui
assert 'extra.sellEnemy=false;' in (r/'assist/source/extra_settings_io.h').read_text()
assert 'owner!=lastHouse)return false;' in (r/'assist/source/cooperative_bridge.h').read_text()
print('PASS no feature hooks/code writes; message manager owns entry; bounded stamped input, native follow, sole event gateway, unsupported rule bypasses closed')
