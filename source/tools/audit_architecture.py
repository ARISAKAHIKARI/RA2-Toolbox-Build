#!/usr/bin/env python3
"""Source-boundary audit. Structural checks are not a proof of game correctness."""
from pathlib import Path
import hashlib,json,re
ROOT=Path(__file__).resolve().parent.parent
MODULES=('assist','cursor','unit_monitor','shared','external_reader','suite','game_runtime')
FILES={p.relative_to(ROOT).as_posix():p for m in MODULES for p in (ROOT/m/'source').rglob('*') if p.suffix in ('.cpp','.h','.hpp','.c','.asm')}
def includes(path):
 for inc in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"',path.read_text(),re.M):
  target=(path.parent/inc).resolve()
  if not target.is_file():target=ROOT/'suite/source'/inc
  assert target.is_file(),(path,inc)
  yield target

def closure(roots):
 seen=set();todo=[ROOT/x for x in roots]
 while todo:
  p=todo.pop().resolve()
  if p in seen:continue
  seen.add(p);todo.extend(includes(p))
 return {p.relative_to(ROOT).as_posix() for p in seen}
HOST=closure(['suite/source/main.cpp','assist/source/main.cpp','cursor/source/main.cpp','unit_monitor/source/main.cpp'])
RUNTIME=closure(['game_runtime/source/runtime.cpp','shared/source/game_entry_manager.cpp','assist/source/bridge.cpp','assist/source/guard.cpp','cursor/source/hide.cpp','cursor/source/resource_guard.cpp'])
assert 'shared/source/game_entry_client.h' not in HOST
assert 'external_reader/source/process_reader.h' not in RUNTIME
assert 'shared/source/automatic_log.h' not in RUNTIME
assert all(n not in HOST for n in ('assist/source/bridge.cpp','cursor/source/hide.cpp','assist/source/input_patch.h'))
api_sites=[]
for n,p in FILES.items():
 s=p.read_text()
 # Enumerate nested sources too, including files not reachable in this build.
 assert not re.search(r'\b(?:WriteProcessMemory|NtWriteVirtualMemory|NtReadVirtualMemory|VirtualAllocEx|CreateRemoteThread|QueueUserAPC|mouse_event|keybd_event)\s*\(',s),n
 if re.search(r'\bReadProcessMemory\s*\(',s):
  assert n in ('external_reader/source/process_reader.h','assist/source/bridge.cpp','cursor/source/hide.cpp'),n
  if n!='external_reader/source/process_reader.h':assert 'ReadProcessMemory(GetCurrentProcess(),' in s,n
 if re.search(r'\bSendInput\s*\(|\bSendMessageTimeout[AW]?\s*\(',s):assert n=='shared/source/host_game_input.h',n
 if re.search(r'\bLoadLibrary(?:Ex)?[AW]?\s*\(',s):
  assert n in ('shared/source/host_game_input.h','cursor/source/main.cpp'),n
  if n=='cursor/source/main.cpp':assert re.findall(r'LoadLibraryW\(([^)]+)\)',s)==['L"shcore.dll"'],n
 if 'gameentry::patch(' in s:assert n=='assist/source/placement_bridge.h',n
 if re.search(r'\bVirtualProtect\s*\(',s):assert n=='shared/source/game_entry_manager.cpp',n
 if 'constructors::fill(' in s or re.search(r'\benqueue\(',s):assert n=='assist/source/bridge.cpp',n
 for hook in re.findall(r'SetWindowsHookExW\(([^,]+)',s):
  assert n=='shared/source/host_game_input.h' and hook in ('WH_GETMESSAGE','WH_CALLWNDPROCRET'),(n,hook)
 if n in HOST:
  assert 'gameentry::active(' not in s and 'gameentry::Scope' not in s,n
  assert not re.search(r'\bshared->(?:ack|result|selectAck|planAck|cooperativeAck)\s*=(?!=)',s),n
  assert not re.search(r'\([^()]*\*\)\s*0x[0-9a-fA-F]+',s),n
 assert 'ReadScope' not in s and 'readHub()' not in s,n
 for api in ('ReadProcessMemory','SendInput','SendMessageTimeoutW','SetWindowsHookExW','VirtualProtect','LoadLibraryW'):
  for m in re.finditer(r'\b'+api+r'\s*\(',s):api_sites.append({'file':n,'line':s.count('\n',0,m.start())+1,'api':api})
bridge=FILES['assist/source/bridge.cpp'].read_text()
segment=bridge.split('static void dispatchFromTrigger(dispatchrouter::Trigger trigger,DWORD now,const MSG* message){')[1].split('extern "C" unsigned BeginAssistCapture')[0]
assert segment.index('if(trigger!=dispatchrouter::Trigger::Message)return;')<segment.index('InterlockedExchange(&hostWorkPending,0)')<segment.index('  GuardedAssistTick();')
message=bridge.split('extern "C" void AssistRuntimeMessage')[1].split('extern "C" void AssistRuntimeWatchdog')[0]
assert 'InterlockedExchange(&hostWorkPending,1)' in message
assert bridge.count('InterlockedExchange(&hostWorkPending,1)')==1
for module in ('assist','cursor','unit_monitor'):
 text=FILES[module+'/source/main.cpp'].read_text();assert 'external::ReadSession' in text,module
runtime=FILES['game_runtime/source/runtime.cpp'].read_text()
assert 'controllerID==owner&&controller&&WaitForSingleObject(controller,0)==WAIT_TIMEOUT' in runtime
assert sum('BOOL WINAPI DllMain(' in p.read_text() for p in FILES.values())==1
for name in ('BUILD_ZIG.cmd','build-zig.sh'):
 build=(ROOT/name).read_text()
 assert build.index('BUILD-VERIFIED-v106.tag')<build.index('game_entry_manager.cpp'),name
 assert 'verification' in build and 'WINDOWS_BUILD_ARTIFACTS.json' in build,name
 assert build.count('-shared ')==1 and '-o GameRuntime.dll' in build,name
assert 'tools\\verify_windows_outputs.py' in (ROOT/'BUILD_ZIG.cmd').read_text()
for p in (ROOT/'engine').rglob('*.py'):
 assert not re.search(r'ReadProcessMemory|WriteProcessMemory|CreateRemoteThread|SetWindowsHookEx|SendInput',p.read_text()),str(p)
known=[
 {'id':'P01','status':'remaining_migration','feature':'AutoLoad allocation','source':'assist/source/autoload_bridge.h','detail':'Candidate collection and assign() still run inside Runtime; final canEnter/game calls must remain there.'},
 {'id':'P02','status':'remaining_migration','feature':'Automatic repair discovery/cadence','source':'assist/source/bridge.cpp','detail':'repairTick and collectRepairs remain game-side; submission already uses the unified event gateway.'},
 {'id':'P03','status':'remaining_migration','feature':'Smart unload/ejection target choice','source':'assist/source/native_orders_bridge.h','detail':'smartUnloadTick and target search still run in Runtime; advanced move/magnetron decisions already use the host planner.'},
 {'id':'P04','status':'remaining_consolidation','feature':'Read snapshots','source':'external_reader/source/shared_reader.h','detail':'One read API/shared connection, independent task snapshots; not one cross-feature atomic world snapshot.'},
]
report={'version':'1.0.6','baseline':'0.7.16','result':'No detected forbidden source-boundary bypass after fixes; remaining migrations explicitly listed. Not full architecture completion.',
 'scope':{'source_files':len(FILES),'host_reachable_files':len(HOST),'runtime_reachable_files':len(RUNTIME),'method':'Recursive local include graph + forbidden API ownership checks + manual review of game-call, queue, patch and scheduling paths. Lexical assertions cannot prove all runtime behavior.'},
 'freshness_contract':{'protocol':45,'tactical_max_ms':150,'tactical_max_frames':6,'cooperative_max_ms':250,'cooperative_max_frames':12,'intent_max_ms':5000,'source_clock_never_renewed_on_publish':True,'partial_batch_deadline_retained':True,'cooperative_retry_only_unprocessed':True,'real_game_latency_measured':False},
 'extended_test_fixes':['Cooperative pending requests bypass display cadence','Round-end settles cooperative ACK with per-serial receipt ownership','Both planning mailboxes share one frame budget and per-channel share'],
 'runtime_diagnostics':{'protocol':3,'control_bytes':120,'transport':'existing unified Runtime mapping','writer':'Runtime','file_io':'host only','native_windows_loader_tested':False},
 'fixes':['Capture-time match identity and context expiry prevent loading/menu input reuse','Loading/menu/invalid-frame quarantine precedes feature initialization; no arbitrary warmup delay','All event backends share live simulationReady preflight; ordinary queue append rechecks after native clock callback','Bounded host startup diagnostics, without DLL file IO','Local native legality query AV boundary remains within Runtime ownership; no private ingress or command replay','Host initial/torn snapshots never replace authenticated state; fresh timestamps avoid false connection timeout','Automatic bounded host logging and callback/handshake evidence without consuming host work','Source time/frame preserved across planning and queueing; per-order expiration; fresh suffix replan preserves aircraft quotas','Cursor WM_SETCURSOR goes through host input manager without holding scheduler mutex','Only the message entry consumes host work; frame detours removed; Patch API permits only three 1.0.4 judgment sites and two 1.0.6 drawing leaves','Recycled host PID cannot reuse a dead retained controller process handle','Host builds reject including the Runtime execution client','Partial rebuilds invalidate old verification markers; Windows build verifies PE outputs before success'],
 'permitted_exceptions':['Two experimental judgment features use three sites; the render-only experiment uses exactly ShroudDraw/FogDraw leaves; defaults off, multiplayer-danger labels. All writes owned by the entry manager, callbacks acquire a bound-thread Capture lease. Explicit host polling feeds Runtime. Optional compatible ground-click suppression uses the existing common message boundary only, default off; no OutList rewriting','Read-only game-side validation/resource reads under Runtime execution','Watchdog stop/dead-host restoration only','Central transport supports verified construct/enqueue and original high-level backends','Local UI refresh fields/selection/cursor use guarded Runtime calls; not every operation is a network event'],
 'remaining':known,'host_sources':sorted(HOST),'runtime_sources':sorted(RUNTIME),'api_sites':api_sites,
 'source_sha256':{n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in sorted(FILES.items())}}
(ROOT/'ARCHITECTURE_AUDIT.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(f'PASS architecture boundaries: {len(FILES)} source files; {len(HOST)} host includes / {len(RUNTIME)} Runtime includes; read, input, injection, patch, event and frame-grant ownership')
print('NOTE: 3 business-planning migrations and cross-feature snapshot consolidation remain; no full-game/real-netplay proof')
