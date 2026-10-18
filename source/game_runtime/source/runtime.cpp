#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../../shared/source/runtime_protocol.h"
#include "../../shared/source/runtime_fault.h"
#include "../../shared/source/game_entry_client.h"
extern "C" void AssistRuntimeMessage(int,WPARAM,LPARAM);
extern "C" void CursorRuntimeMessage(int,WPARAM,LPARAM);
extern "C" void CursorRuntimePost(int,WPARAM,LPARAM);
extern "C" void AssistRuntimeWatchdog();
extern "C" void AssistRuntimeFilter(void*);
extern "C" void CursorRuntimeWatchdog();
extern "C" void EntryRuntimeDetach();
extern "C" __declspec(dllexport) const uint32_t GameRuntimeInfo[2]={runtime::Version,sizeof(runtime::Control)};
static runtime::Control* control;
static HANDLE mapping,controller;
static DWORD controllerID;
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
 explicit WakeTrace(const MSG* msg){
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
 ~WakeTrace(){if(handle){if(view)UnmapViewOfFile(view);CloseHandle(handle);}}
};
static void cleanup(){AssistRuntimeWatchdog();CursorRuntimeWatchdog();}
static void CALLBACK watchdogProc(HWND,UINT,UINT_PTR,DWORD){
 __declspec(align(16)) BYTE fx[512];__asm {fxsave fx}
 // This timer only restores stale/disconnected clients. It never schedules
 // new user work. The host owns normal cadence and the command mailboxes.
 if(!InterlockedCompareExchange(&dispatching,1,0)){cleanup();InterlockedExchange(&dispatching,0);}
 __asm {fxrstor fx}
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
  bool ok=ensureWatchdog(&failure);trace.report(ok?runtime::Connected:failure,ok?0:GetLastError());return ok;
 }
 if(!owner)return false;
 // A pinned 0.6.x executor must be removed by restarting the game. Never
 // install a second generation of inline hooks beside the old runtime.
 if(GetModuleHandleW(L"AssistBridge.dll")||GetModuleHandleW(L"CursorHideBridge.dll")||
    GetModuleHandleW(L"UnitMonitorBridge.dll")||GetModuleHandleW(L"GameEntryManager.dll")){trace.report(runtime::LegacyRuntime);return false;}
 if(controller&&WaitForSingleObject(controller,0)!=WAIT_OBJECT_0){trace.report(runtime::ControllerBusy);return false;}
 trace.retainBeforeReconnect(owner);
 cleanup();
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
 mapping=m;control=c;controller=p;controllerID=owner;trace.report(runtime::Connected);return true;
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimeHook(int code,WPARAM wp,LPARAM lp){
 __declspec(align(16)) BYTE fx[512];__asm {fxsave fx}
 if(code>=0&&wp==PM_REMOVE&&lp){
  const MSG* msg=(const MSG*)lp;
  if(!InterlockedCompareExchange(&dispatching,1,0)){
   WakeTrace trace(msg);
   // Bind once at the real thread-specific message boundary. Services then
   // run consecutively, each releasing its execution scope before the next.
   bool bound=false;{gameentry::Scope entry(gameentry::Client::Assist,true);bound=bool(entry);}
   if(bound){
    if(msg->message==WM_NULL&&msg->wParam==runtime::WakeTag)connect(DWORD(msg->lParam),trace);
    if(control&&ensureWatchdog()){
     if(msg->message==WM_NULL&&msg->wParam==runtime::WakeTag&&DWORD(msg->lParam)==controllerID)InterlockedExchange(&control->wakePending,0);
     LONG ready=InterlockedExchange(&control->ready,0)&InterlockedCompareExchange(&control->enabled,0,0);
     if(controller&&WaitForSingleObject(controller,0)==WAIT_TIMEOUT){
      // Cursor work cannot be starved by a stream of tactical commands.
      if(ready&runtime::Cursor)CursorRuntimeMessage(code,wp,lp);
      if(ready&runtime::Tactical)AssistRuntimeMessage(code,wp,lp);
      if((control->enabled&runtime::Tactical)&&(msg->message==WM_LBUTTONDOWN||msg->message==WM_LBUTTONUP||msg->message==WM_RBUTTONDOWN||msg->message==WM_RBUTTONUP))AssistRuntimeFilter((void*)lp);
      if(ready)InterlockedIncrement(&control->dispatches);
     }else cleanup();
    }
   }else trace.report(runtime::EntryRejected);
   InterlockedExchange(&dispatching,0);
  }else if(control)InterlockedIncrement(&control->reentrant);
 }
 __asm {fxrstor fx}
 return CallNextHookEx(nullptr,code,wp,lp);
}
extern "C" __declspec(dllexport) LRESULT CALLBACK GameRuntimePostHook(int code,WPARAM wp,LPARAM lp){
 __declspec(align(16)) BYTE fx[512];__asm {fxsave fx}
 if(code>=0&&control&&(control->enabled&runtime::Cursor)&&!InterlockedCompareExchange(&dispatching,1,0)){
  CursorRuntimePost(code,wp,lp);InterlockedExchange(&dispatching,0);
 }
 __asm {fxrstor fx}
 return CallNextHookEx(nullptr,code,wp,lp);
}
extern "C" BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID){
 if(why==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(h);
 else if(why==DLL_PROCESS_DETACH)EntryRuntimeDetach();
 // No game operations or hook restoration under the loader lock.
 return TRUE;
}

