#pragma once
#include <windows.h>
#include <mutex>
#include <cwchar>
#include <mmsystem.h>
#include "precise_clock.h"
#include "key_gesture.h"
#include "game_entry_api.h"
#include "runtime_protocol.h"
#include "frame_signal_client.h"
#include "message_query.h"
#include "../../external_reader/source/shared_reader.h"
#ifdef _WIN64
#include "runtime_broker_client.h"
#endif
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
inline bool heldBinding(uint32_t binding);
struct PointerPress {DWORD tick=0,serial=0;uint32_t match=0,viewportGeneration=0;int operation=0;POINT point{};int width=0,height=0;uint8_t keys[256]{};};
struct PointerQueue {PointerPress items[128]{};unsigned head=0,count=0;DWORD dropped=0,cancelSerial=0;bool enabled=false,focused=false,rightArmed=false,leftArmed=false,rightDragged=false;POINT rightOrigin{};uint32_t match=0,viewportGeneration=0;DWORD contextAt=0;uint32_t down=0;uint8_t rightKeys[256]{},leftKeys[256]{};};
struct Target {keygesture::Tracker keys;PointerQueue pointer{};Health lastDiagnostic{};DWORD pid=0,thread=0;HWND window=nullptr;HHOOK message=nullptr,post=nullptr;HANDLE mapping=nullptr,process=nullptr;runtime::Control*control=nullptr;uint64_t cursorAt=0,tacticalAt=0;DWORD lastPost=0,lastPostError=0;bool fastTactical=false,frameSampling=false,diagnosticFrame=false;std::shared_ptr<framesignal::Client> frameSignal;
#ifdef _WIN64
 uint32_t brokerToken=0,brokerGeneration=0;DWORD brokerRetry=0;
#endif
};
struct Lease {Subscription id=0;gameentry::Client client{};unsigned target=0;};
struct Host {std::recursive_mutex mutex;Target targets[32]{};Lease leases[256]{};Subscription next=0;HMODULE module=nullptr;HANDLE timer=nullptr;volatile LONG pulseBusy=0;bool preciseTimer=false;const wchar_t* installStage=L"not-started";};
inline Host& host(){static Host*value=new Host;return *value;}
inline const wchar_t* installationStage(){auto&h=host();std::lock_guard<std::recursive_mutex> lock(h.mutex);return h.installStage;}
struct Capacity {unsigned targets=0,subscriptions=0;};
inline Capacity capacity(){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);Capacity value;
 for(auto&t:h.targets)if(t.pid)++value.targets;
 for(auto&l:h.leases)if(l.id)++value.subscriptions;
 return value;
}
inline uint32_t mask(gameentry::Client c){return c==gameentry::Client::Assist?runtime::Tactical:c==gameentry::Client::Cursor?runtime::Cursor:0;}
inline HMODULE runtimeModule(){
#ifdef _WIN64
 // A PE32 runtime is intentionally never loaded as executable code into the
 // PE32+ host. The paired x86 broker validates exports and owns the HHOOKs.
 return nullptr;
#else
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
#endif
}
inline bool runtimeAvailable(){
#ifdef _WIN64
 return runtimebroker::client().transport(runtime::Version,sizeof(runtime::Control),gameentry::Version,sizeof(gameentry::Request));
#else
 return runtimeModule()!=nullptr;
#endif
}
inline bool assistIdentity(HMODULE module,uint32_t version,uint32_t bytes,const wchar_t*prefix){
#ifdef _WIN64
 uint32_t actualVersion=0,actualBytes=0;
 if(!runtimebroker::client().identity(actualVersion,actualBytes,prefix))return false;
 bool valid=actualVersion==version&&actualBytes==bytes;if(!valid)SetLastError(ERROR_REVISION_MISMATCH);return valid;
#else
 auto info=reinterpret_cast<const uint32_t*>(GetProcAddress(module,"AssistProtocolInfo"));
 auto text=reinterpret_cast<const wchar_t*>(GetProcAddress(module,"AssistMappingPrefix"));
 return info&&info[0]==version&&info[1]==bytes&&text&&wcscmp(text,prefix)==0;
#endif
}
inline uint32_t windowToken(HWND window){
 // HWND values are interoperable across Windows bitness, unlike pointers.
 // Verify the low-width handle still names exactly this window's process and
 // thread before publishing it to the unchanged x86 mailbox.
 uint32_t raw=uint32_t(uintptr_t(window));DWORD owner=0,roundOwner=0;
 DWORD thread=GetWindowThreadProcessId(window,&owner),roundThread=GetWindowThreadProcessId((HWND)(uintptr_t)raw,&roundOwner);
 return raw&&thread&&thread==roundThread&&owner==roundOwner?raw:0;
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
inline bool releaseHook(HHOOK&hook){
 if(!hook)return true;
 // Windows may already have removed a hook when its target thread exits.
 // Treat that exact result as detached; retain real failures for a retry.
 if(!UnhookWindowsHookEx(hook)&&GetLastError()!=ERROR_INVALID_HOOK_HANDLE)return false;
 hook=nullptr;return true;
}
inline bool clear(Target&t){
#ifdef _WIN64
 if(t.brokerToken&&!runtimebroker::client().remove(t.pid,t.brokerToken,t.brokerGeneration))return false;
#endif
 if(!releaseHook(t.post)||!releaseHook(t.message))return false;
 if(t.control)UnmapViewOfFile(t.control);if(t.mapping)CloseHandle(t.mapping);if(t.process)CloseHandle(t.process);t={};return true;
}
inline void reapTargets(){
 auto&h=host();
 // Use the retained process object, not a fresh PID lookup or a transient
 // HWND. A failed wait/denied query never proves that a live target exited.
 for(unsigned i=0;i<32;++i){auto&t=h.targets[i];if(!t.pid)continue;
  bool dead=t.process&&WaitForSingleObject(t.process,0)==WAIT_OBJECT_0,used=false;
  for(auto&l:h.leases)if(l.id&&l.target==i){if(dead)l={};else used=true;}
  if(!used){if(t.control)InterlockedExchange(&t.control->enabled,0);clear(t);}
 }
}
// Capture identity belongs to the producer, never to the dequeueing feature.
inline void disarmPointer(PointerQueue&q){q.focused=false;q.rightArmed=false;q.leftArmed=false;q.count=q.head=0;++q.cancelSerial;}
inline void setPointerContext(Subscription id,uint32_t match,bool enabled,uint32_t viewportGeneration=0){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(id&&l.id==id&&l.client==gameentry::Client::Assist){auto&q=h.targets[l.target].pointer;
  enabled=enabled&&match!=0;if(q.match!=match||q.enabled!=enabled||q.viewportGeneration!=viewportGeneration)disarmPointer(q);
  q.match=match;q.enabled=enabled;q.viewportGeneration=viewportGeneration;q.contextAt=GetTickCount();return;
 }
}
// Polling belongs to the EXE manager. No low-level mouse/keyboard hooks and no
// game input is swallowed or rewritten. Right UP follows the native click.
inline void keySnapshot(Subscription id,uint8_t*out,uint32_t*serial=nullptr){auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);memset(out,0,256);for(auto&l:h.leases)if(id&&l.id==id){auto&t=h.targets[l.target];memcpy(out,t.keys.held,256);if(serial)memcpy(serial,t.keys.serial,256*sizeof(uint32_t));return;}}
inline bool heldBinding(uint32_t binding){
 if(!(binding&255)||!keygesture::isHold(binding))return false;
 uint8_t keys[256]{};for(unsigned code:{binding&255,16u,17u,18u})keys[code]=(GetAsyncKeyState(code)&0x8000)!=0;
 if(binding&keygesture::Double){
  DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);
  auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
  for(auto&t:h.targets)if(t.pid==foreground&&t.pointer.focused&&(t.keys.held[binding&255]&2)){keys[binding&255]|=keys[binding&255]?2:0;break;}
 }
 return keygesture::matchesHold(keys,binding);
}
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
 POINT hit{};bool located=GetCursorPos(&hit);if(located){DWORD hitPID=0;GetWindowThreadProcessId(WindowFromPoint(hit),&hitPID);if(hitPID!=t.pid){if(q.rightArmed||q.leftArmed||q.count)disarmPointer(q);q.down=down;return;}}
 uint32_t pressed=down&~q.down,released=q.down&~down;q.down=down;
 if(q.rightArmed&&located&&(hit.x-q.rightOrigin.x>5||q.rightOrigin.x-hit.x>5||hit.y-q.rightOrigin.y>5||q.rightOrigin.y-hit.y>5))q.rightDragged=true;
 bool follow=(released&8)&&q.rightArmed&&!q.rightDragged,left=(released&128)&&q.leftArmed;
 if(released&128)q.leftArmed=false;
 if(pressed&128){q.leftArmed=true;for(unsigned i=0;i<256;++i)q.leftKeys[i]=physical[i];}
 if(released&8)q.rightArmed=false;
 // A right press may become camera panning. It cancels stale input captures,
 // but must not emit a gameplay Cancel before the gesture is known.
 if(pressed&8){q.rightArmed=located;q.rightDragged=false;q.rightOrigin=hit;++q.cancelSerial;for(unsigned i=0;i<256;++i)q.rightKeys[i]=physical[i];}
 int operation=(pressed&112u)?5:left?6:(pressed&128)?7:follow?4:(pressed&1)?1:(pressed&2)?2:(pressed&4)?3:0;
 if(!operation)return;if(operation!=4&&operation!=6&&!(pressed&8))++q.cancelSerial;
 PointerPress e{};e.tick=now;e.serial=q.cancelSerial;e.match=q.match;e.viewportGeneration=q.viewportGeneration;e.operation=operation;RECT rect{};
 if(!GetCursorPos(&e.point)||!ScreenToClient(t.window,&e.point)||!GetClientRect(t.window,&rect))return;
 e.width=rect.right;e.height=rect.bottom;
 if(e.width<=0||e.height<=0||e.point.x<0||e.point.y<0||e.point.x>=e.width||e.point.y>=e.height)return;
 for(unsigned i=0;i<256;++i)e.keys[i]=operation==4?q.rightKeys[i]:operation==6?q.leftKeys[i]:physical[i];
 // Modifier authorization belongs to the configured host binding resolver.
 if(q.count==128){++q.dropped;return;}q.items[(q.head+q.count++)%128]=e;
}
inline DWORD pointerCancelSerial(Subscription id){auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);for(auto&l:h.leases)if(id&&l.id==id)return h.targets[l.target].pointer.cancelSerial;return 0;}
inline bool takePointer(Subscription id,PointerPress&out,DWORD&dropped){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(auto&l:h.leases)if(id&&l.id==id&&l.client==gameentry::Client::Assist){auto&q=h.targets[l.target].pointer;dropped=q.dropped;q.dropped=0;if(!q.count)return false;out=q.items[q.head];q.head=(q.head+1)%128;--q.count;return true;}return false;
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
inline bool nativeFrameSampling(Subscription id){auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);for(auto&l:h.leases)if(id&&l.id==id){auto&t=h.targets[l.target];framesignal::Stamp stamp;if(t.frameSignal&&t.frameSignal->current(stamp)&&stamp.state==framesignal::Native&&stamp.serial>0&&stamp.frequency>0){LARGE_INTEGER q{};QueryPerformanceCounter(&q);return double(q.QuadPart-stamp.qpc)*1000./stamp.frequency<100.;}}return false;}
inline void setFrameSampling(Subscription id,bool enabled,bool diagnostic=false){auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);for(auto&l:h.leases)if(id&&l.id==id&&l.client==gameentry::Client::Assist){auto&t=h.targets[l.target];if(!enabled||t.diagnosticFrame!=diagnostic)t.frameSignal.reset();t.frameSampling=enabled;t.diagnosticFrame=enabled&&diagnostic;return;}}
inline void CALLBACK pulse(void*,BOOLEAN){
 auto&h=host();if(InterlockedCompareExchange(&h.pulseBusy,1,0))return;
 struct Reset {volatile LONG*value;~Reset(){InterlockedExchange(value,0);}} reset{&h.pulseBusy};
 std::lock_guard<std::recursive_mutex>lock(h.mutex);reapTargets();DWORD now=GetTickCount();const uint64_t precise=preciseclock::micros();
 for(unsigned i=0;i<32;++i){auto&t=h.targets[i];if(!t.pid)continue;
  uint32_t enabled=0;for(auto&l:h.leases)if(l.id&&l.target==i)enabled|=mask(l.client);
  if(!enabled){clear(t);continue;}if(!t.control)continue;
#ifdef _WIN64
  if(!runtimebroker::client().alive(t.brokerGeneration)){
   InterlockedExchange(&t.control->enabled,0);
   if(t.brokerRetry&&int32_t(now-t.brokerRetry)<0)continue;t.brokerRetry=now+1000;
   if(!runtimebroker::client().install(t.pid,t.thread,uint32_t(uintptr_t(t.window)),t.brokerToken,t.brokerGeneration)){t.lastPostError=GetLastError();continue;}
   InterlockedExchange(&t.control->enabled,LONG(enabled));t.lastPostError=0;
  }
#endif
  bool nativeFrames=false;
  // Normal frame sampling keeps the 4 ms message pulse below. It reads the
  // existing frame counter and does not request a game-code notification hook.
  if((enabled&runtime::Tactical)&&t.frameSampling&&t.diagnosticFrame){if(!t.frameSignal)t.frameSignal=std::make_shared<framesignal::Client>();if(t.frameSignal->open(t.pid,framesignal::Compute|framesignal::ExplicitDiagnostic)){t.frameSignal->heartbeat();framesignal::Stamp stamp;if(t.frameSignal->current(stamp)&&stamp.state==framesignal::Native&&stamp.frequency>0&&stamp.serial>0){LARGE_INTEGER q{};QueryPerformanceCounter(&q);nativeFrames=double(q.QuadPart-stamp.qpc)*1000./stamp.frequency<100.;}}}
  else t.frameSignal.reset();
  t.control->heartbeat=now;
  if(enabled&runtime::Tactical)pollPointer(t,now);
  uint32_t ready=0;if((enabled&runtime::Cursor)&&precise-t.cursorAt>=4000){t.cursorAt=precise;ready|=runtime::Cursor;}
  const uint64_t tacticalPeriod=nativeFrames?100000:t.frameSampling?4000:t.fastTactical&&t.pointer.enabled&&t.pointer.focused?1000:16000;
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
// Recreated HWND on the existing game thread is a delivery-address change,
// not a reason to stop Runtime or release either thread Hook/subscriber.
inline bool rebindWindow(Subscription id,HWND window){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 for(const auto&lease:h.leases)if(id&&lease.id==id){
  auto&t=h.targets[lease.target];DWORD owner=0;
  if(!window||GetWindowThreadProcessId(window,&owner)!=t.thread||owner!=t.pid||nativeWindow(t.pid,t.thread)!=window)return false;
  if(t.window!=window){t.window=window;t.pointer={};if(t.control)InterlockedExchange(&t.control->wakePending,0);}
  return true;
 }
 return false;
}
inline Subscription install(gameentry::Client client,DWORD thread,DWORD pid,HWND window){
 auto&h=host();std::lock_guard<std::recursive_mutex>lock(h.mutex);
 h.installStage=L"window-owner-check";
 DWORD actual=0,tid=window?GetWindowThreadProcessId(window,&actual):0;
 if(!mask(client)||!pid||!thread||actual!=pid||tid!=thread){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
 h.installStage=L"native-window-check";window=nativeWindow(pid,thread);if(!window)return 0;
 h.installStage=L"load-runtime";if(!runtimeAvailable())return 0;HMODULE m=runtimeModule();
 // Also recover forgotten leases of exited games and partial installations
 // before the host timer existed. Live subscribers keep their transport.
 reapTargets();
 h.installStage=L"subscription-slots";
 Lease*slot=nullptr;for(auto&l:h.leases)if(!l.id){slot=&l;break;}if(!slot){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
 unsigned at=32;for(unsigned i=0;i<32;++i)if(h.targets[i].pid==pid){at=i;break;}
 if(at<32&&h.targets[at].thread!=thread){
  h.installStage=L"previous-thread-still-alive";
  auto&t=h.targets[at];HANDLE old=OpenThread(SYNCHRONIZE,FALSE,t.thread);
  bool dead=old?WaitForSingleObject(old,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;if(old)CloseHandle(old);
  if(!dead){SetLastError(ERROR_BUSY);return 0;}
  // Keep subscriptions and mapping; replace transport only after old thread exits.
#ifdef _WIN64
  if(t.brokerToken&&!runtimebroker::client().remove(t.pid,t.brokerToken,t.brokerGeneration))return 0;
  t.brokerToken=t.brokerGeneration=0;
#endif
  if(!releaseHook(t.post)||!releaseHook(t.message))return 0;
  t.pointer={};t.thread=thread;t.window=window;if(t.control)InterlockedExchange(&t.control->wakePending,0);
 }
 if(at==32){h.installStage=L"target-slots";for(unsigned i=0;i<32;++i)if(!h.targets[i].pid){at=i;break;}if(at==32){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
  auto&t=h.targets[at];t.pid=pid;t.thread=thread;t.window=window;
  // Best-effort synchronization rights only; existing platforms that deny
  // this handle retain the normal explicit-release path.
  t.process=OpenProcess(SYNCHRONIZE,FALSE,pid);
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
#ifdef _WIN64
 h.installStage=L"x86-hook-broker";
 if(!runtimebroker::client().install(pid,thread,uint32_t(uintptr_t(window)),t.brokerToken,t.brokerGeneration)){
  DWORD error=GetLastError();bool used=false;for(auto&l:h.leases)if(l.id&&l.target==at)used=true;if(!used)clear(t);SetLastError(error);return 0;
 }
#else
 if(!t.message){h.installStage=L"WH_GETMESSAGE";t.message=SetWindowsHookExW(WH_GETMESSAGE,(HOOKPROC)GetProcAddress(m,"GameRuntimeHook"),m,thread);}
 if(t.message&&!t.post){h.installStage=L"WH_CALLWNDPROCRET";t.post=SetWindowsHookExW(WH_CALLWNDPROCRET,(HOOKPROC)GetProcAddress(m,"GameRuntimePostHook"),m,thread);}
 if(!t.message||!t.post){DWORD error=GetLastError();bool used=false;for(auto&l:h.leases)if(l.id&&l.target==at)used=true;if(!used)clear(t);SetLastError(error);return 0;}
#endif
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

