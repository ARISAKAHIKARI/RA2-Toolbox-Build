#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "entry_policy.h"
#include "experimental_patch_policy.h"
#include "patch_context.h"
#include "../../assist/source/hook_registry.h"
using namespace gameentry;
static Policy policy;
static HANDLE boundThreadHandle=nullptr;
static uint32_t boundThreadId=0;
static volatile LONG busy,activeThread,activeModule,entries,reentrant,contended,wrongThread;
static assist::hookregistry::Registry<> patches;
static uint32_t patchModules[32];
extern "C" __declspec(dllexport) const uint32_t GameEntryManagerInfo[2]={Version,sizeof(Request)};
static bool isBoundThreadDead(uint32_t boundTid){
 // Numeric thread IDs can be recycled. Only the retained object handle
 // can prove that the previous binding has exited.
 return boundTid&&boundThreadHandle&&boundThreadId==boundTid&&
        WaitForSingleObject(boundThreadHandle,0)==WAIT_OBJECT_0;
}
static void refreshBoundThreadHandle(uint32_t tid){
 if(!tid)return;
 if(boundThreadHandle&&boundThreadId==tid&&WaitForSingleObject(boundThreadHandle,0)==WAIT_TIMEOUT)return;
 if(boundThreadHandle){CloseHandle(boundThreadHandle);boundThreadHandle=nullptr;boundThreadId=0;}
 boundThreadHandle=OpenThread(SYNCHRONIZE,FALSE,tid);
 if(boundThreadHandle)boundThreadId=tid;
}
// One writer, bound game thread, exact signatures, Runtime-owned targets.
static Result applyPatch(const Request&r){
 if(r.client!=Client::Assist||r.length!=experimentalpatch::size(r.address)||!r.length)return Result::PatchDenied;
 HMODULE self=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
   reinterpret_cast<LPCWSTR>(&applyPatch),&self)||r.module!=uint32_t(uintptr_t(self)))return Result::WrongOwner;
 int slot=patches.find(r.address);
 if(slot>=0&&patchModules[slot]&&patchModules[slot]!=r.module)return Result::WrongOwner;
 const auto transition=patches.proposal(r.address,r.before,r.after,r.length);
 if(transition==assist::hookregistry::Transition::Deny)return Result::PatchDenied;
 if(transition==assist::hookregistry::Transition::Install){
  // Entry bytes alone do not establish the adapter's stack/continuation ABI.
  // Restoration must remain possible after a dependency changes.
  if(!patchcontext::compatible(r.address,[](uint32_t at,void*out,size_t n){
   MEMORY_BASIC_INFORMATION info{};const void* p=(const void*)(uintptr_t)at;
   if(!VirtualQuery(p,&info,sizeof info)||info.State!=MEM_COMMIT||
      info.AllocationBase!=GetModuleHandleW(nullptr)||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
      uintptr_t(p)+n>uintptr_t(info.BaseAddress)+info.RegionSize)return false;
   memcpy(out,p,n);return true;
  }))return Result::PatchDenied;
  const uint32_t target=experimentalpatch::destination(r.address,r.after,r.length);MEMORY_BASIC_INFORMATION info{};
  if(!target||!VirtualQuery((void*)(uintptr_t)target,&info,sizeof info)||info.State!=MEM_COMMIT||
     info.AllocationBase!=self||info.Type!=MEM_IMAGE||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return Result::PatchDenied;
  HMODULE pin=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
    reinterpret_cast<LPCWSTR>(target),&pin))return Result::SystemFailure;
 }
 void* site=(void*)(uintptr_t)r.address;MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery(site,&info,sizeof info)||info.State!=MEM_COMMIT||info.AllocationBase!=GetModuleHandleW(nullptr)||
    uintptr_t(site)+r.length>uintptr_t(info.BaseAddress)+info.RegionSize)return Result::PatchDenied;
 if(memcmp(site,r.before,r.length)){patches.markConflict(r.address);return Result::PatchDenied;}
 DWORD prior=0;if(!VirtualProtect(site,r.length,PAGE_EXECUTE_READWRITE,&prior))return Result::SystemFailure;
 memcpy(site,r.after,r.length);
 const bool flushed=FlushInstructionCache(GetCurrentProcess(),site,r.length)!=FALSE;
 DWORD ignored=0;const bool protectedAgain=VirtualProtect(site,r.length,prior,&ignored)!=FALSE;
 // Record actual bytes even on an OS error, so cleanup retains ownership.
 patches.commit(r.address,r.before,r.after,r.length,transition);
 slot=patches.find(r.address);if(slot>=0)patchModules[slot]=r.module;
 return flushed&&protectedAgain?Result::Ok:Result::SystemFailure;
}
extern "C" __declspec(dllexport) int32_t __stdcall ManageGameEntry(Request*r){
 if(!r||r->size!=sizeof(Request)||r->version!=Version)return int32_t(Result::Invalid);
 const DWORD tid=GetCurrentThreadId();
 if(r->operation==Operation::Leave){
  if(InterlockedCompareExchange(&activeThread,0,0)!=LONG(tid)||
     InterlockedCompareExchange(&activeModule,0,0)!=LONG(r->module))return int32_t(Result::WrongOwner);
  const auto result=policy.leave(tid,*r);
  if(result==Result::Ok&&!policy.module){InterlockedExchange(&activeModule,0);InterlockedExchange(&activeThread,0);InterlockedExchange(&busy,0);}
  return int32_t(result);
 }
 // Scheduling cannot reenter. Only explicit Capture may nest under a live
 // parent token. Mutation requires the current owner and exact lease token.
 const bool owned=InterlockedCompareExchange(&activeThread,0,0)==LONG(tid)&&
                  InterlockedCompareExchange(&activeModule,0,0)==LONG(r->module)&&
                  policy.sameModule(tid,*r);
 if((r->operation==Operation::Enter)||!owned){
  if(InterlockedCompareExchange(&busy,1,0)){
   if(InterlockedCompareExchange(&activeThread,0,0)==LONG(tid))InterlockedIncrement(&reentrant);
   else InterlockedIncrement(&contended);
   return int32_t(Result::Busy);
  }
 }
 Result result=Result::Invalid;bool retain=false;
 if(r->operation==Operation::Enter||r->operation==Operation::Capture){
  const bool priorDead=(policy.thread&&policy.thread!=tid)?isBoundThreadDead(policy.thread):false;
  result=policy.enter(tid,*r,priorDead);
  if(result==Result::Ok){
   // Nested capture already owns a live outer lease on this exact thread.
   // A fresh outer entry still verifies the retained thread object, including
   // numeric TID reuse. No handle wait/open is needed for the nested level.
   if(!owned)refreshBoundThreadHandle(tid);
   InterlockedExchange(&activeThread,LONG(tid));InterlockedExchange(&activeModule,LONG(r->module));
   InterlockedIncrement(&entries);retain=true;
  }
  else if(result==Result::WrongThread)InterlockedIncrement(&wrongThread);
 }else if(r->operation==Operation::Inspect){
  r->gameThread=policy.thread;r->activeModule=policy.module;r->installed=patches.installed();r->conflicts=patches.conflicts();
  r->active=patches.active(r->address);r->conflict=patches.conflict(r->address);
  r->entries=uint32_t(entries);r->reentrant=uint32_t(reentrant);r->contended=uint32_t(contended);r->wrongThread=uint32_t(wrongThread);result=Result::Ok;
 }else if(!policy.owns(tid,*r)){result=Result::WrongOwner;
 }else if(r->operation==Operation::Validate){result=Result::Ok;}
 else if(r->operation==Operation::Patch)result=applyPatch(*r);
 else if(r->operation==Operation::MarkConflict&&r->client==Client::Assist){
  int i=patches.find(r->address);
  if(i>=0&&patchModules[i]&&patchModules[i]!=r->module)result=Result::WrongOwner;
  else{patches.markConflict(r->address);i=patches.find(r->address);if(i>=0)patchModules[i]=r->module;result=Result::Ok;}
 }
 if(!owned&&!retain)InterlockedExchange(&busy,0);
 return int32_t(result);
}
extern "C" void EntryRuntimeDetach(){
 if(boundThreadHandle){CloseHandle(boundThreadHandle);boundThreadHandle=nullptr;boundThreadId=0;}
}
