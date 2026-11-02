#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "entry_policy.h"
#include "../../assist/source/hook_registry.h"
using namespace gameentry;
static_assert(!CodeWritesAllowed,"Strict Runtime must not permit game code or vtable writes");
static Policy policy;
static HANDLE boundThreadHandle=nullptr;
static uint32_t boundThreadId=0;
static volatile LONG busy,activeThread,activeModule,entries,reentrant,contended,wrongThread;
// Diagnostic ownership metadata only. No product Patch path commits writes.
static assist::hookregistry::Registry<> patches;
static uint32_t patchModules[decltype(patches)::CapacityLimit];
extern "C" __declspec(dllexport) const uint32_t GameEntryManagerInfo[2]={Version,sizeof(Request)};
extern "C" __declspec(dllexport) const uint32_t GameCodeWritePolicyInfo[3]={1,0,0};
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
// Strict build intentionally contains no executable/rdata write implementation.
// Start a fresh game instead of trying to restore another Runtime's patches.
extern "C" __declspec(dllexport) int32_t __stdcall ManageGameEntry(Request*r){
 if(!r||r->size!=sizeof(Request)||r->version!=Version)return int32_t(Result::Invalid);
 // Includes Install and Restore, code, virtual tables and immediate operands.
 // Reject before target reads, lease mutation or any Windows API call.
 if(r->operation==Operation::Patch)return int32_t(Result::PatchDenied);
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
