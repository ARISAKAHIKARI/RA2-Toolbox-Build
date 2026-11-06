#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../shared/source/runtime_protocol.h"
#include "../../shared/source/runtime_fault.h"
#include "../../shared/source/game_entry_client.h"
#include "../../shared/source/runtime_fp_scope.h"
#include "frame_signal_runtime.h"
#include "native_tooltip_runtime.h"
#include "../../shared/source/debug_entry_policy.h"
extern "C" void AssistRuntimeMessage(int,WPARAM,LPARAM);
extern "C" void CursorRuntimeMessage(int,WPARAM,LPARAM);
extern "C" void CursorRuntimePost(int,WPARAM,LPARAM);
extern "C" void AssistRuntimeWatchdog();
extern "C" void AssistRuntimeFilter(void*);
extern "C" void CursorRuntimeWatchdog();
extern "C" bool AssistRuntimeDetached();
extern "C" bool CursorRuntimeDetached();
extern "C" void EntryRuntimeDetach();
extern "C" __declspec(dllexport) const uint32_t GameRuntimeInfo[2]={runtime::Version,sizeof(runtime::Control)};
static runtime::Control* control;
static HANDLE mapping,controller;
static DWORD controllerID;static HWND frameWakeWindow;
extern "C" void RuntimeFrameReady(){
 auto*c=control;if(!c||!frameWakeWindow||!(c->enabled&runtime::Tactical)||DWORD(GetTickCount()-c->heartbeat)>500)return;
 InterlockedOr(&c->ready,runtime::Tactical);
 if(!InterlockedCompareExchange(&c->wakePending,1,0)&&!PostMessageW(frameWakeWindow,WM_NULL,runtime::WakeTag,controllerID))InterlockedExchange(&c->wakePending,0);
}
static PVOID exceptionObserver;
static DWORD observerThread;
static volatile LONG observingException;
static LONG CALLBACK observeException(EXCEPTION_POINTERS* pointers){
 const DWORD savedError=GetLastError();
 // Observe only the bound game thread. No stack walking, game reads, file IO,
 // allocation, exception recovery or game-state mutation in this callback.
 auto* c=control;const auto* r=pointers?pointers->ExceptionRecord:nullptr;
 const auto* context=pointers?pointers->ContextRecord:nullptr;
 if(c&&r&&context&&(c->enabled&runtime::Tactical)&&GetCurrentThreadId()==observerThread&&
    (r->ExceptionCode==EXCEPTION_ACCESS_VIOLATION||r->ExceptionCode==EXCEPTION_IN_PAGE_ERROR||
     r->ExceptionCode==EXCEPTION_ILLEGAL_INSTRUCTION||r->ExceptionCode==EXCEPTION_INT_DIVIDE_BY_ZERO)&&
    !InterlockedCompareExchange(&observingException,1,0)){
  const bool access=(r->ExceptionCode==EXCEPTION_ACCESS_VIOLATION||r->ExceptionCode==EXCEPTION_IN_PAGE_ERROR)&&r->NumberParameters>=2;
  InterlockedIncrement(&c->nativeSeq);
  ++c->nativeSerial;c->nativeCode=r->ExceptionCode;c->nativeAddress=DWORD(uintptr_t(r->ExceptionAddress));
  c->nativeOperation=access?DWORD(r->ExceptionInformation[0]):~0u;
  c->nativeAccess=access?DWORD(r->ExceptionInformation[1]):0;
  c->nativeThread=observerThread;c->nativeTick=GetTickCount();
  c->nativeEax=context->Eax;c->nativeEcx=context->Ecx;c->nativeEsi=context->Esi;c->nativeEbp=context->Ebp;
  InterlockedIncrement(&c->nativeSeq);
  InterlockedExchange(&observingException,0);
 }
 SetLastError(savedError);
 return EXCEPTION_CONTINUE_SEARCH;
}
static void ensureExceptionObserver(){
 if(!control)return;
 observerThread=GetCurrentThreadId();
 // The watchdog has pinned this DLL before registration. Append our observer
 // to the existing handler chain; do not replace platform exception handling.
 if(!exceptionObserver&&control->observeState==0){
  exceptionObserver=AddVectoredExceptionHandler(0,observeException);
  control->observeError=exceptionObserver?0:LONG(GetLastError());
 }
 control->observeState=exceptionObserver?1:-1;
}
extern "C" void RuntimeReportException(unsigned boundary,unsigned code,unsigned address,unsigned operation,unsigned access,unsigned command,unsigned phase,unsigned source,unsigned target){
 // Called only within the existing Runtime execution/capture scope. No file IO,
 // allocation, game calls, retries or acknowledgements in an exception filter.
 if(!control)return;
 InterlockedIncrement(&control->diagnosticSeq);
 ++control->faultSerial;control->faultCode=code;control->faultAddress=address;
 control->faultOperation=operation;control->faultAccess=access;control->faultBoundary=boundary;
 control->faultThread=GetCurrentThreadId();control->faultTick=GetTickCount();
 control->faultCommand=command;control->faultPhase=phase;control->faultSource=source;control->faultTarget=target;
 InterlockedIncrement(&control->diagnosticSeq);
}
static UINT_PTR watchdog;
static DWORD watchdogThread;
static volatile LONG dispatching;
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimeHook(int,WPARAM,LPARAM);
// The tagged wake uses the existing transport mapping for diagnostics too.
// Before binding, only IPC metadata is touched; no game access is permitted.
struct WakeTrace {
 runtime::Control* view=nullptr;HANDLE handle=nullptr;
 void observe(const MSG* msg){
  if(msg->message!=WM_NULL||msg->wParam!=runtime::WakeTag||!msg->lParam)return;
  DWORD owner=DWORD(msg->lParam);
  if(control&&controllerID==owner)view=control;
  else {
   wchar_t name[96];runtime::mapName(name,GetCurrentProcessId(),owner);
   handle=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
   if(handle)view=(runtime::Control*)MapViewOfFile(handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(runtime::Control));
   if(view&&(view->magic!=runtime::Magic||view->version!=runtime::Version||view->pid!=GetCurrentProcessId()||view->owner!=owner)){UnmapViewOfFile(view);view=nullptr;}
  }
  if(view){
   InterlockedIncrement(&view->diagnosticSeq);
   ++view->callbacks;view->callbackTick=GetTickCount();view->callbackThread=GetCurrentThreadId();
   view->runtimeBase=gameentry::moduleIdentity();
   view->hookRva=DWORD(uintptr_t(&GameRuntimeHook))-view->runtimeBase;
   if(view->stage==runtime::Unseen){view->stage=runtime::CallbackSeen;view->error=0;}
   InterlockedIncrement(&view->diagnosticSeq);
  }
 }
 void retainBeforeReconnect(DWORD owner){
  if(!view||handle)return;
  wchar_t name[96];runtime::mapName(name,GetCurrentProcessId(),owner);
  handle=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
  view=handle?(runtime::Control*)MapViewOfFile(handle,FILE_MAP_ALL_ACCESS,0,0,sizeof(runtime::Control)):nullptr;
 }
 void report(runtime::Stage stage,DWORD error=0){if(view){
  InterlockedIncrement(&view->diagnosticSeq);view->stage=stage;view->error=error;InterlockedIncrement(&view->diagnosticSeq);
 }}
 void release(){if(handle){const HANDLE owned=handle;auto* mapped=view;handle=nullptr;view=nullptr;if(mapped)UnmapViewOfFile(mapped);CloseHandle(owned);}}
 ~WakeTrace(){release();}
};
static void cleanup(){AssistRuntimeWatchdog();CursorRuntimeWatchdog();NativeTooltipMaintain(false);}
static void CALLBACK watchdogProc(HWND,UINT,UINT_PTR,DWORD){
 RuntimeFPScope fp;bool dispatchOwned=false;
 // This timer only restores stale/disconnected clients. It never schedules
 // new user work. Cleanup belongs to this invocation only after its CAS wins.
 runtimeunwind::run([&]{
  if(!InterlockedCompareExchange(&dispatching,1,0)){
   dispatchOwned=true;cleanup();
   frameobserver::maintain(controller&&WaitForSingleObject(controller,0)==WAIT_TIMEOUT?controllerID:0);
  }
 },[&]{if(dispatchOwned)InterlockedExchange(&dispatching,0);fp.restore();});
}
static bool ensureWatchdog(runtime::Stage* failure=nullptr){
 DWORD tid=GetCurrentThreadId();if(watchdog&&watchdogThread==tid)return true;
 // Thread timers die with their creating thread. The execution manager has
 // already proved that a previous binding exited before this can migrate.
 HMODULE pinned=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&watchdogProc,&pinned)){if(failure)*failure=runtime::WatchdogPinFailed;return false;}
 UINT_PTR next=SetTimer(nullptr,0,100,watchdogProc);if(!next){if(failure)*failure=runtime::WatchdogTimerFailed;return false;}
 watchdog=next;watchdogThread=tid;return true;
}
static bool connect(DWORD owner,WakeTrace& trace){
 runtime::Stage failure=runtime::WatchdogTimerFailed;
 // A recycled numeric PID is not the same controller process. Reuse only a
 // live retained process object; otherwise follow the normal reconnect path.
 if(control&&controllerID==owner&&controller&&WaitForSingleObject(controller,0)==WAIT_TIMEOUT){
  bool ok=ensureWatchdog(&failure);if(ok)ensureExceptionObserver();trace.report(ok?runtime::Connected:failure,ok?0:GetLastError());return ok;
 }
 if(!owner)return false;
 // A pinned 0.6.x executor must be removed by restarting the game. Never
 // install a second generation of inline hooks beside the old runtime.
 if(GetModuleHandleW(L"AssistBridge.dll")||GetModuleHandleW(L"CursorHideBridge.dll")||
    GetModuleHandleW(L"UnitMonitorBridge.dll")||GetModuleHandleW(L"GameEntryManager.dll")){trace.report(runtime::LegacyRuntime);return false;}
 if(controller&&WaitForSingleObject(controller,0)!=WAIT_OBJECT_0){trace.report(runtime::ControllerBusy);return false;}
 trace.retainBeforeReconnect(owner);
 cleanup();
 // A watchdog attempt can defer restoration (e.g. unsent foreign events).
 // Retain the OLD controller transport until both services actually detach;
 // attempting cleanup is not proof that it completed.
 if(!AssistRuntimeDetached()||!CursorRuntimeDetached()||!NativeTooltipDetached()){
  trace.report(runtime::ControllerBusy,ERROR_BUSY);return false;
 }
 if(control)UnmapViewOfFile(control);control=nullptr;
 if(mapping)CloseHandle(mapping);mapping=nullptr;
 if(controller)CloseHandle(controller);controller=nullptr;controllerID=0;
 wchar_t name[96];runtime::mapName(name,GetCurrentProcessId(),owner);
 HANDLE m=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);if(!m){trace.report(runtime::MappingFailed,GetLastError());return false;}
 auto* c=(runtime::Control*)MapViewOfFile(m,FILE_MAP_ALL_ACCESS,0,0,sizeof(runtime::Control));
 if(!c||c->magic!=runtime::Magic||c->version!=runtime::Version||c->pid!=GetCurrentProcessId()||c->owner!=owner){trace.report(runtime::MappingFailed,c?ERROR_REVISION_MISMATCH:GetLastError());if(c)UnmapViewOfFile(c);CloseHandle(m);return false;}
 HANDLE p=OpenProcess(SYNCHRONIZE,FALSE,owner);
 if(!p||WaitForSingleObject(p,0)!=WAIT_TIMEOUT){trace.report(p?runtime::OwnerNotAlive:runtime::OwnerOpenFailed,p?0:GetLastError());if(p)CloseHandle(p);UnmapViewOfFile(c);CloseHandle(m);return false;}
 if(!ensureWatchdog(&failure)){trace.report(failure,GetLastError());CloseHandle(p);UnmapViewOfFile(c);CloseHandle(m);return false;}
 mapping=m;control=c;controller=p;controllerID=owner;ensureExceptionObserver();trace.report(runtime::Connected);return true;
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimeHook(int code,WPARAM wp,LPARAM lp){
 // End isolation before chaining into any other module's Windows hook. SEH
 // finally also restores our infrastructure when a platform handler unwinds.
 RuntimeFPScope fp;bool dispatchOwned=false;
 runtimeunwind::run([&]{
  if(code>=0&&wp==PM_REMOVE&&lp){
   const MSG* msg=(const MSG*)lp;
   if(!InterlockedCompareExchange(&dispatching,1,0)){
    dispatchOwned=true;WakeTrace trace;
    runtimeunwind::run([&]{
     trace.observe(msg);
     // Bind once at the real thread-specific message boundary. Services then
     // run consecutively, each releasing its execution scope before the next.
     const bool bound=gameentry::scoped(gameentry::Client::Assist,true,[]{debugentries::clockTick=GetTickCount();debugentries::installation(debugentries::Kind::System,WH_GETMESSAGE,debugentries::Hook::Installed);});
     if(bound){
      if(msg->message==WM_NULL&&msg->wParam==runtime::WakeTag){if(connect(DWORD(msg->lParam),trace))frameWakeWindow=msg->hwnd;}
      if(control&&ensureWatchdog()){
       if(msg->message==WM_NULL&&msg->wParam==runtime::WakeTag&&DWORD(msg->lParam)==controllerID)InterlockedExchange(&control->wakePending,0);
       LONG ready=InterlockedExchange(&control->ready,0)&InterlockedCompareExchange(&control->enabled,0,0);
       if(controller&&WaitForSingleObject(controller,0)==WAIT_TIMEOUT){
        // Cursor work cannot be starved by a stream of tactical commands.
        if(ready&runtime::Cursor)CursorRuntimeMessage(code,wp,lp);
        if(ready&runtime::Tactical)AssistRuntimeMessage(code,wp,lp);
        if(ready)NativeTooltipMaintain(true);
        if((control->enabled&runtime::Tactical)&&(msg->message==WM_MOUSEMOVE||msg->message==WM_KEYDOWN||msg->message==WM_KEYUP||msg->message==WM_SYSKEYDOWN||msg->message==WM_SYSKEYUP||msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONDBLCLK||msg->message==WM_LBUTTONUP||msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONDBLCLK||msg->message==WM_RBUTTONUP||msg->message==WM_MBUTTONDOWN||msg->message==WM_MBUTTONDBLCLK||msg->message==WM_MBUTTONUP||msg->message==WM_XBUTTONDOWN||msg->message==WM_XBUTTONDBLCLK||msg->message==WM_XBUTTONUP))AssistRuntimeFilter((void*)lp);
        frameobserver::maintain(controllerID);
        if(ready)InterlockedIncrement(&control->dispatches);
       }else cleanup();
      }
     }else trace.report(runtime::EntryRejected);
    },[&]{trace.release();});
   }else if(control)InterlockedIncrement(&control->reentrant);
  }
 },[&]{if(dispatchOwned)InterlockedExchange(&dispatching,0);fp.restore();});
 return CallNextHookEx(nullptr,code,wp,lp);
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimePostHook(int code,WPARAM wp,LPARAM lp){
 RuntimeFPScope fp;bool dispatchOwned=false;
 runtimeunwind::run([&]{
  if(code>=0&&control&&(control->enabled&runtime::Cursor)&&!InterlockedCompareExchange(&dispatching,1,0)){
   dispatchOwned=true;gameentry::scoped(gameentry::Client::Cursor,false,[]{debugentries::clockTick=GetTickCount();debugentries::installation(debugentries::Kind::System,WH_CALLWNDPROCRET,debugentries::Hook::Installed);});CursorRuntimePost(code,wp,lp);
  }
 },[&]{if(dispatchOwned)InterlockedExchange(&dispatching,0);fp.restore();});
 return CallNextHookEx(nullptr,code,wp,lp);
}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID){
 if(why==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(h);
 else if(why==DLL_PROCESS_DETACH)EntryRuntimeDetach();
 // No game operations or hook restoration under the loader lock.
 return TRUE;
}

