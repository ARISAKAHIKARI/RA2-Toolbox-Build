#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "entry_policy.h"
#include "../../assist/source/hook_registry.h"
using namespace gameentry;
static Policy policy;
static HANDLE boundThreadHandle=nullptr;
static uint32_t boundThreadId=0;
static volatile LONG busy,activeThread,activeModule,entries,reentrant,contended,wrongThread;
static assist::hookregistry::Registry<> patches;
static uint32_t patchModules[16];
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
// No engine code writes in this build. Compatibility callers fail closed.
static Result applyPatch(const Request&){return Result::PatchDenied;}
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
   refreshBoundThreadHandle(tid);
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
