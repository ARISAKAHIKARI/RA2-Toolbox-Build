#pragma once
#include <windows.h>
#include <mutex>
#include <mmsystem.h>
#include "precise_clock.h"
#include "key_gesture.h"
#include "game_entry_api.h"
#include "runtime_protocol.h"
#include "message_query.h"
#include "../../external_reader/source/shared_reader.h"
// EXE-owned subscriptions and cadence. Features never choose a DLL, callback,
// Win32 hook kind, or injection path. One GETMESSAGE + POST pair per game.
namespace gameinput {
using Subscription=uint64_t;
struct Health {
 bool subscribed=false,diagnosticValid=false,diagnosticCached=false;DWORD postError=0;
 LONG enabled=0,ready=0,wakePending=0,dispatches=0,callbacks=0,stage=0;
 DWORD callbackTick=0,callbackThread=0,error=0,runtimeBase=0,hookRva=0;
 DWORD faultSerial=0,faultCode=0,faultAddress=0,faultOperation=0,faultAccess=0,faultBoundary=0;
 DWORD faultThread=0,faultTick=0,faultCommand=0,faultPhase=0,faultSource=0,faultTarget=0;
 bool nativeValid=false;
 LONG observeState=0,observeError=0;
 DWORD nativeSerial=0,nativeCode=0,nativeAddress=0,nativeOperation=0,nativeAccess=0;
 DWORD nativeThread=0,nativeTick=0,nativeEax=0,nativeEcx=0,nativeEsi=0,nativeEbp=0;
};
inline bool heldBinding(uint32_t binding){
 const unsigned vk=binding&255,mods=binding>>8;if(!vk)return false;
 if(!(GetAsyncKeyState(vk)&0x8000))return false;if(binding==16||binding==17||binding==18)return true;
 return bool(GetAsyncKeyState(16)&0x8000)==bool(mods&1)&&bool(GetAsyncKeyState(17)&0x8000)==bool(mods&2)&&bool(GetAsyncKeyState(18)&0x8000)==bool(mods&4);
}
struct PointerPress {DWORD tick=0,serial=0;uint32_t match=0;int operation=0;POINT point{};int width=0,height=0;uint8_t keys[256]{};};
struct PointerQueue {PointerPress items[16]{};unsigned head=0,count=0;DWORD dropped=0,cancelSerial=0;bool enabled=false,focused=false,rightArmed=false,leftArmed=false;uint32_t match=0;DWORD contextAt=0;uint32_t down=0;uint8_t rightKeys[256]{},leftKeys[256]{};};
struct Target {keygesture::Tracker keys;PointerQueue pointer{};Health lastDiagnostic{};DWORD pid=0,thread=0;HWND window=nullptr;HHOOK message=nullptr,post=nullptr;HANDLE mapping=nullptr;runtime::Control*control=nullptr;uint64_t cursorAt=0,tacticalAt=0;DWORD lastPost=0,lastPostError=0;bool fastTactical=false;};
struct Lease {Subscription id=0;gameentry::Client client{};unsigned target=0;};
struct Host {std::recursive_mutex mutex;Target targets[8]{};Lease leases[32]{};Subscription next=0;HMODULE module=nullptr;HANDLE timer=nullptr;volatile LONG pulseBusy=0;bool preciseTimer=false;const wchar_t* installStage=L"not-started";};
inline Host& host(){static Host*value=new Host;return *value;}
inline const wchar_t* installationStage(){auto&h=host();std::lock_guard<std::recursive_mutex> lock(h.mutex);return h.installStage;}
inline uint32_t mask(gameentry::Client c){return c==gameentry::Client::Assist?runtime::Tactical:c==gameentry::Client::Cursor?runtime::Cursor:0;}
inline HMODULE runtimeModule(){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);if(h.module)return h.module;
 wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768)return nullptr;
 while(n&&path[n-1]!=L'\\'&&path[n-1]!=L'/')--n;
 const wchar_t name[]=L"GameRuntime.dll";if(!n||n+sizeof(name)/sizeof(wchar_t)>32768)return nullptr;
 for(unsigned i=0;i<sizeof(name)/sizeof(wchar_t);++i)path[n+i]=name[i];
 HMODULE m=LoadLibraryW(path);if(!m)return nullptr;
 auto info=(const uint32_t*)GetProcAddress(m,"GameRuntimeInfo");
 auto entry=(const uint32_t*)GetProcAddress(m,"GameEntryManagerInfo");
 if(!info||info[0]!=runtime::Version||info[1]!=sizeof(runtime::Control)||!entry||entry[0]!=gameentry::Version||entry[1]!=sizeof(gameentry::Request)||!GetProcAddress(m,"GameRuntimeHook")||!GetProcAddress(m,"GameRuntimePostHook")){FreeLibrary(m);SetLastError(ERROR_REVISION_MISMATCH);return nullptr;}
 h.module=m;return m;
}
inline void post(Target&t,uint32_t bits){
 if(!t.control)return;
 InterlockedOr(&t.control->ready,LONG(bits));t.control->heartbeat=GetTickCount();
 if(t.control->wakePending&&DWORD(GetTickCount()-t.lastPost)>=250)InterlockedExchange(&t.control->wakePending,0);
 if(!InterlockedCompareExchange(&t.control->wakePending,1,0)){
  t.lastPost=GetTickCount();DWORD actual=0;
  if(!t.window||GetWindowThreadProcessId(t.window,&actual)!=t.thread||actual!=t.pid){
   t.lastPostError=ERROR_INVALID_PARAMETER;InterlockedExchange(&t.control->wakePending,0);return;
  }
  // Window-addressed WM_NULL also reaches window-filtered game message pumps.
  // The same Runtime GETMESSAGE hook consumes it; no second execution route.
  if(PostMessageW(t.window,WM_NULL,runtime::WakeTag,GetCurrentProcessId()))t.lastPostError=0;
  else{t.lastPostError=GetLastError();InterlockedExchange(&t.control->wakePending,0);}
 }
}
inline Health health(Subscription id){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(id&&l.id==id){auto&t=h.targets[l.target];if(t.control){
  auto*c=t.control;Health value=t.lastDiagnostic;value.diagnosticCached=value.diagnosticValid;value.diagnosticValid=false;value.subscribed=true;value.postError=t.lastPostError;
  value.enabled=c->enabled;value.ready=c->ready;value.wakePending=c->wakePending;value.dispatches=c->dispatches;
  value.observeState=c->observeState;value.observeError=c->observeError;
  value.nativeValid=false;
  for(int retry=0;retry<4;++retry){
   LONG seq=InterlockedCompareExchange(&c->nativeSeq,0,0);if(seq&1)continue;
   Health next=value;
   next.nativeSerial=c->nativeSerial;next.nativeCode=c->nativeCode;next.nativeAddress=c->nativeAddress;
   next.nativeOperation=c->nativeOperation;next.nativeAccess=c->nativeAccess;next.nativeThread=c->nativeThread;next.nativeTick=c->nativeTick;
   next.nativeEax=c->nativeEax;next.nativeEcx=c->nativeEcx;next.nativeEsi=c->nativeEsi;next.nativeEbp=c->nativeEbp;
   if(seq==InterlockedCompareExchange(&c->nativeSeq,0,0)){next.nativeValid=true;value=next;break;}
  }
  for(int retry=0;retry<4;++retry){
   LONG seq=InterlockedCompareExchange(&c->diagnosticSeq,0,0);if(seq&1)continue;
   Health candidate=value;
   candidate.callbacks=c->callbacks;candidate.callbackTick=c->callbackTick;candidate.callbackThread=c->callbackThread;
   candidate.stage=c->stage;candidate.error=c->error;candidate.runtimeBase=c->runtimeBase;candidate.hookRva=c->hookRva;
   candidate.faultSerial=c->faultSerial;candidate.faultCode=c->faultCode;candidate.faultAddress=c->faultAddress;
   candidate.faultOperation=c->faultOperation;candidate.faultAccess=c->faultAccess;candidate.faultBoundary=c->faultBoundary;
   candidate.faultThread=c->faultThread;candidate.faultTick=c->faultTick;candidate.faultCommand=c->faultCommand;
   candidate.faultPhase=c->faultPhase;candidate.faultSource=c->faultSource;candidate.faultTarget=c->faultTarget;
   if(seq==InterlockedCompareExchange(&c->diagnosticSeq,0,0)){
    candidate.diagnosticValid=true;candidate.diagnosticCached=false;t.lastDiagnostic=candidate;return candidate;
   }
  }
  return value;
 }}
 return {};
}
inline bool clear(Target&t){
 if(t.post){if(!UnhookWindowsHookEx(t.post))return false;t.post=nullptr;}
 if(t.message){if(!UnhookWindowsHookEx(t.message))return false;t.message=nullptr;}
 if(t.control)UnmapViewOfFile(t.control);if(t.mapping)CloseHandle(t.mapping);t={};return true;
}
// Capture identity belongs to the producer, never to the dequeueing feature.
inline void disarmPointer(PointerQueue&q){q.focused=false;q.rightArmed=false;q.leftArmed=false;q.count=q.head=0;++q.cancelSerial;}
inline void setPointerContext(Subscription id,uint32_t match,bool enabled){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(id&&l.id==id&&l.client==gameentry::Client::Assist){auto&q=h.targets[l.target].pointer;
  enabled=enabled&&match!=0;if(q.match!=match||q.enabled!=enabled)disarmPointer(q);
  q.match=match;q.enabled=enabled;q.contextAt=GetTickCount();return;
 }
}
// Polling belongs to the EXE manager. No low-level mouse/keyboard hooks and no
// game input is swallowed or rewritten. Right UP follows the native click.
inline void keySnapshot(Subscription id,uint8_t*out,uint32_t*serial=nullptr){auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);memset(out,0,256);for(auto&l:h.leases)if(id&&l.id==id){auto&t=h.targets[l.target];memcpy(out,t.keys.held,256);if(serial)memcpy(serial,t.keys.serial,256*sizeof(uint32_t));return;}}
inline void pollPointer(Target&t,DWORD now){
 auto&q=t.pointer;DWORD fg=0;GetWindowThreadProcessId(GetForegroundWindow(),&fg);
 const int keys[]={VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2,VK_RBUTTON,'S','D','X',VK_LBUTTON};
 uint32_t down=0;for(unsigned i=0;i<8;++i)if(GetAsyncKeyState(keys[i])&0x8000)down|=1u<<i;
 if(q.enabled&&DWORD(now-q.contextAt)>150){q.enabled=false;disarmPointer(q);}
 if(!q.enabled||fg!=t.pid||!t.window||IsIconic(t.window)){
  if(q.focused||q.rightArmed||q.count)disarmPointer(q);t.keys.clear();q.down=down;return;
 }
 uint8_t physical[256]{};for(unsigned i=0;i<256;++i)physical[i]=(GetAsyncKeyState(i)&0x8000)!=0;
 if(!q.focused){q.focused=true;q.rightArmed=false;q.down=down;t.keys.clear();memcpy(t.keys.held,physical,256);return;}
 t.keys.sample(physical,now);t.keys.decorate(physical);
 POINT hit{};if(GetCursorPos(&hit)){DWORD hitPID=0;GetWindowThreadProcessId(WindowFromPoint(hit),&hitPID);if(hitPID!=t.pid){if(q.rightArmed||q.leftArmed||q.count)disarmPointer(q);q.down=down;return;}}
 uint32_t pressed=down&~q.down,released=q.down&~down;q.down=down;
 bool follow=(released&8)&&q.rightArmed,left=(released&128)&&q.leftArmed;
 if(released&128)q.leftArmed=false;
 if(pressed&128){q.leftArmed=true;for(unsigned i=0;i<256;++i)q.leftKeys[i]=physical[i];}
 if(released&8)q.rightArmed=false;
 if(pressed&8){q.rightArmed=false;for(unsigned i=0;i<256;++i)q.rightKeys[i]=physical[i];}
 int operation=(pressed&~135u)?5:left?6:(pressed&128)?7:follow?4:(pressed&1)?1:(pressed&2)?2:(pressed&4)?3:0;
 if(!operation)return;if(operation!=4&&operation!=6)++q.cancelSerial;
 PointerPress e{};e.tick=now;e.serial=q.cancelSerial;e.match=q.match;e.operation=operation;RECT rect{};
 if(!GetCursorPos(&e.point)||!ScreenToClient(t.window,&e.point)||!GetClientRect(t.window,&rect))return;
 e.width=rect.right;e.height=rect.bottom;
 if(e.width<=0||e.height<=0||e.point.x<0||e.point.y<0||e.point.x>=e.width||e.point.y>=e.height)return;
 if(pressed&8)q.rightArmed=true;
 for(unsigned i=0;i<256;++i)e.keys[i]=operation==4?q.rightKeys[i]:operation==6?q.leftKeys[i]:physical[i];
 // Modifier authorization belongs to the configured host binding resolver.
 if(q.count==16){++q.dropped;return;}q.items[(q.head+q.count++)%16]=e;
}
inline DWORD pointerCancelSerial(Subscription id){auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);for(auto&l:h.leases)if(id&&l.id==id)return h.targets[l.target].pointer.cancelSerial;return 0;}
inline bool takePointer(Subscription id,PointerPress&out,DWORD&dropped){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(id&&l.id==id&&l.client==gameentry::Client::Assist){auto&q=h.targets[l.target].pointer;dropped=q.dropped;q.dropped=0;if(!q.count)return false;out=q.items[q.head];q.head=(q.head+1)%16;--q.count;return true;}return false;
}
inline void updateTimer(){
 auto&h=host();bool fast=false;
 for(auto&t:h.targets)if(t.pid&&t.fastTactical&&t.pointer.enabled&&t.pointer.focused)fast=true;
 if(fast==h.preciseTimer||!h.timer)return;
 if(fast&&timeBeginPeriod(1)!=TIMERR_NOERROR)return;
 if(!ChangeTimerQueueTimer(nullptr,h.timer,fast?1:4,fast?1:4)){if(fast)timeEndPeriod(1);return;}
 if(!fast)timeEndPeriod(1);h.preciseTimer=fast;
}
inline void setFastTactical(Subscription id,bool enabled){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(id&&l.id==id&&l.client==gameentry::Client::Assist){h.targets[l.target].fastTactical=enabled;updateTimer();return;}
}
inline void CALLBACK pulse(void*,BOOLEAN){
 auto&h=host();if(InterlockedCompareExchange(&h.pulseBusy,1,0))return;
 struct Reset {volatile LONG*value;~Reset(){InterlockedExchange(value,0);}} reset{&h.pulseBusy};
 std::lock_guard<std::recursive_mutex>lock(h.mutex);DWORD now=GetTickCount();const uint64_t precise=preciseclock::micros();
 for(unsigned i=0;i<8;++i){auto&t=h.targets[i];if(!t.pid)continue;
  uint32_t enabled=0;for(auto&l:h.leases)if(l.id&&l.target==i)enabled|=mask(l.client);
  if(!enabled){clear(t);continue;}if(!t.control)continue;
  if(enabled&runtime::Tactical)pollPointer(t,now);
  uint32_t ready=0;if((enabled&runtime::Cursor)&&precise-t.cursorAt>=4000){t.cursorAt=precise;ready|=runtime::Cursor;}
  const uint64_t tacticalPeriod=t.fastTactical&&t.pointer.enabled&&t.pointer.focused?1000:16000;
  if((enabled&runtime::Tactical)&&precise-t.tacticalAt>=tacticalPeriod){t.tacticalAt=precise;ready|=runtime::Tactical;}
  if(ready)post(t,ready);
 }
 updateTimer();
}
// Enumeration is sufficient for read-only discovery, not for binding the sole
// executor. A launcher/renderer thread can stay alive after its startup window
// disappears, which would otherwise permanently occupy the game-thread lease.
struct NativeWindowProbe {bool read=false;uint32_t raw=0;DWORD owner=0,thread=0,error=0;};
inline HWND nativeWindow(DWORD pid,DWORD thread,NativeWindowProbe* detail=nullptr){
 NativeWindowProbe probe{};struct Save{NativeWindowProbe*dest;NativeWindowProbe&source;~Save(){if(dest){source.error=GetLastError();*dest=source;}}}save{detail,probe};
 if(!pid||!thread){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}
 external::ReadSession reader;
 if(!reader.open(pid)){SetLastError(reader.lastOpenError);return nullptr;}
 uint32_t raw=0;
 probe.read=reader.readFresh(assist::messagequery::YrppGameHwndAddr,&raw,sizeof raw);probe.raw=raw;
 if(!probe.read||!raw){SetLastError(ERROR_NOT_READY);return nullptr;}
 HWND window=reinterpret_cast<HWND>(uintptr_t(raw));DWORD owner=0;
 probe.thread=GetWindowThreadProcessId(window,&owner);probe.owner=owner;
 if(probe.thread!=thread||owner!=pid){SetLastError(ERROR_NOT_READY);return nullptr;}
 SetLastError(ERROR_SUCCESS);
 return window;
}
inline Subscription install(gameentry::Client client,DWORD thread,DWORD pid,HWND window){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 h.installStage=L"window-owner-check";
 DWORD actual=0,tid=window?GetWindowThreadProcessId(window,&actual):0;
 if(!mask(client)||!pid||!thread||actual!=pid||tid!=thread){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
 h.installStage=L"native-window-check";window=nativeWindow(pid,thread);if(!window)return 0;
 h.installStage=L"load-runtime";HMODULE m=runtimeModule();if(!m)return 0;
 h.installStage=L"subscription-slots";
 Lease*slot=nullptr;for(auto&l:h.leases)if(!l.id){slot=&l;break;}if(!slot){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
 unsigned at=8;for(unsigned i=0;i<8;++i)if(h.targets[i].pid==pid){at=i;break;}
 if(at<8&&h.targets[at].thread!=thread){
  h.installStage=L"previous-thread-still-alive";
  auto&t=h.targets[at];HANDLE old=OpenThread(SYNCHRONIZE,FALSE,t.thread);
  bool dead=old?WaitForSingleObject(old,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;if(old)CloseHandle(old);
  if(!dead){SetLastError(ERROR_BUSY);return 0;}
  // Keep subscriptions and mapping; replace transport only after old thread exits.
  if(t.post){UnhookWindowsHookEx(t.post);t.post=nullptr;}if(t.message){UnhookWindowsHookEx(t.message);t.message=nullptr;}
  t.pointer={};t.thread=thread;t.window=window;if(t.control)InterlockedExchange(&t.control->wakePending,0);
 }
 if(at==8){for(unsigned i=0;i<8;++i)if(!h.targets[i].pid){at=i;break;}if(at==8){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
  auto&t=h.targets[at];t.pid=pid;t.thread=thread;t.window=window;
  h.installStage=L"shared-mapping";
  wchar_t name[96];runtime::mapName(name,pid,GetCurrentProcessId());t.mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(runtime::Control),name);
  if(t.mapping)t.control=(runtime::Control*)MapViewOfFile(t.mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(runtime::Control));
  if(!t.control){DWORD error=GetLastError();clear(t);SetLastError(error);return 0;}
  // A pinned runtime may retain the same mapping between feature toggles.
  // Initialize identity once; do not reset counters during a reattach.
  t.control->magic=runtime::Magic;t.control->version=runtime::Version;t.control->pid=pid;t.control->owner=GetCurrentProcessId();
  InterlockedExchange(&t.control->wakePending,0);
 }
 auto&t=h.targets[at];
 // The HWND may be recreated on the same thread between loading and play.
 // All subscribers must wake the current native window, including old leases.
 if(t.window!=window){t.window=window;if(t.control)InterlockedExchange(&t.control->wakePending,0);}
 if(!t.message){h.installStage=L"WH_GETMESSAGE";t.message=SetWindowsHookExW(WH_GETMESSAGE,(HOOKPROC)GetProcAddress(m,"GameRuntimeHook"),m,thread);}
 if(t.message&&!t.post){h.installStage=L"WH_CALLWNDPROCRET";t.post=SetWindowsHookExW(WH_CALLWNDPROCRET,(HOOKPROC)GetProcAddress(m,"GameRuntimePostHook"),m,thread);}
 if(!t.message||!t.post){DWORD error=GetLastError();bool used=false;for(auto&l:h.leases)if(l.id&&l.target==at)used=true;if(!used)clear(t);SetLastError(error);return 0;}
 h.installStage=L"host-timer";
 if(!h.timer&&!CreateTimerQueueTimer(&h.timer,nullptr,pulse,nullptr,4,4,WT_EXECUTEDEFAULT)){DWORD error=GetLastError();clear(t);SetLastError(error);return 0;}
 if(!++h.next)++h.next;*slot={h.next,client,at};InterlockedOr(&t.control->enabled,LONG(mask(client)));post(t,mask(client));h.installStage=L"installed-awaiting-callback";return slot->id;
}
inline bool wake(Subscription id){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(l.id==id&&id){post(h.targets[l.target],mask(l.client));return true;}return false;
}
inline bool release(gameentry::Client client,Subscription id){
 if(!id)return true;auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(l.id==id){if(l.client!=client){SetLastError(ERROR_ACCESS_DENIED);return false;}
  unsigned at=l.target;l={};uint32_t enabled=0;for(auto&other:h.leases)if(other.id&&other.target==at)enabled|=mask(other.client);
  auto&t=h.targets[at];if(!(enabled&runtime::Tactical)){t.pointer.enabled=false;disarmPointer(t.pointer);}if(t.control)InterlockedExchange(&t.control->enabled,LONG(enabled));if(!enabled)clear(t);updateTimer();return true;
 }SetLastError(ERROR_INVALID_HANDLE);return false;
}
struct ClickResult {UINT sent=0;DWORD error=0;bool released=false;};
// Host presentation input also belongs to this manager, including no-injection
// cursor modes. Never hold the scheduling mutex while waiting for game WndProc.
inline bool refreshCursor(HWND window,DWORD pid){
 DWORD actual=0,foreground=0;
 const DWORD thread=window?GetWindowThreadProcessId(window,&actual):0;
 GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
 if(!thread||!pid||actual!=pid||foreground!=pid)return false;
 DWORD_PTR result=0;
 return SendMessageTimeoutW(window,WM_SETCURSOR,(WPARAM)window,
  MAKELPARAM(HTCLIENT,WM_MOUSEMOVE),SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT,10,&result)!=0;
}
inline ClickResult clickPair(HWND window,DWORD pid){
 auto& h=host();std::lock_guard<std::recursive_mutex> lock(h.mutex);
 DWORD actual=0,foreground=0;
 GetWindowThreadProcessId(window,&actual);GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
 if(!pid||actual!=pid||foreground!=pid)return {0,ERROR_INVALID_PARAMETER,false};
 INPUT inputs[2]{};inputs[0].type=inputs[1].type=INPUT_MOUSE;
 inputs[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;inputs[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
 inputs[0].mi.dwExtraInfo=inputs[1].mi.dwExtraInfo=0x52413543;
 SetLastError(0);UINT sent=SendInput(2,inputs,sizeof(INPUT));DWORD error=sent==2?0:GetLastError();
 // A partial DOWN must be balanced even if focus changes before cleanup.
 bool released=sent==2||(sent==1&&SendInput(1,&inputs[1],sizeof(INPUT))==1);
 return {sent,error,released};
}
}

