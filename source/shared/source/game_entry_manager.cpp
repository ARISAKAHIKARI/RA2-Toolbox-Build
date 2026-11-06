#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "entry_policy.h"
#include "experimental_patch_policy.h"
#include "patch_context.h"
#include "master_control_policy.h"
#include "multiplayer_patch_gate.h"
#include "../../assist/source/hook_registry.h"
using namespace gameentry;
static Policy policy;
static HANDLE boundThreadHandle=nullptr;
static uint32_t boundThreadId=0;
static volatile LONG busy,activeThread,activeModule,entries,reentrant,contended,wrongThread;
static assist::hookregistry::Registry<> patches;
static uint32_t patchModules[decltype(patches)::CapacityLimit];
static int32_t patchErrors[decltype(patches)::CapacityLimit];
static bool patchRead(uint32_t at,void*out,size_t n){
 MEMORY_BASIC_INFORMATION info{};const void*p=(const void*)(uintptr_t)at;SIZE_T got=0;
 return n&&VirtualQuery(p,&info,sizeof info)&&info.State==MEM_COMMIT&&
  info.AllocationBase==GetModuleHandleW(nullptr)&&!(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
  uintptr_t(p)+n>=uintptr_t(p)&&uintptr_t(p)+n<=uintptr_t(info.BaseAddress)+info.RegionSize&&
  ReadProcessMemory(GetCurrentProcess(),p,out,n,&got)&&got==n;
}
static void observePatch(uint32_t at){
 using namespace debugentries;const unsigned n=experimentalpatch::size(at);if(!n)return;
 const int i=patches.find(at);const auto*slot=i<0?nullptr:&patches.slots[i];uint8_t actual[16]{};
 if(!patchRead(at,actual,n)){installation(Kind::Patch,at,slot&&slot->active?Hook::Pending:Hook::Unavailable,Reason::Unreadable);return;}
 if(slot&&slot->conflict){installation(Kind::Patch,at,Hook::Conflict,Reason::Foreign);return;}
 if(slot&&slot->active){
  // Ownership is a restoration obligation; only this readback proves bytes.
  if(slot->size!=n||memcmp(actual,slot->patch,n))installation(Kind::Patch,at,Hook::Conflict,Reason::Foreign);
  else installation(Kind::Patch,at,patchErrors[i]?Hook::Pending:Hook::Installed,
    patchErrors[i]?Reason::OwnerPending:Reason::None,patchErrors[i]);
  return;
 }
 auto*o=observation(Kind::Patch,at);
 if(o&&(o->reason==Reason::Signature||o->reason==Reason::Structural||o->reason==Reason::Unsupported)){
  installation(Kind::Patch,at,Hook::Unavailable,o->reason,o->error);return;
 }
 if(!assist::hookregistry::knownOriginal(at,actual,n)){
  installation(Kind::Patch,at,Hook::Unavailable,assist::hookregistry::structuralOriginal(at,actual,n)?Reason::Signature:Reason::Structural);return;
 }
 installation(Kind::Patch,at,Hook::Inactive,Reason::NoRequest);
}
static Result patchDenied(uint32_t at,debugentries::Reason reason=debugentries::Reason::None,Result result=Result::PatchDenied){
 using namespace debugentries;if(reason==Reason::None){auto*o=observation(Kind::Patch,at);reason=o?o->reason:Reason::Structural;
  if(reason==Reason::None||reason==Reason::NoRequest||reason==Reason::AwaitRuntime)reason=Reason::Structural;}
 installation(Kind::Patch,at,patches.active(at)?Hook::Pending:Hook::Unavailable,reason,int(result));return result;
}
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
 if(transition==assist::hookregistry::Transition::Deny)return patchDenied(r.address,patches.conflict(r.address)?debugentries::Reason::Foreign:debugentries::Reason::None);
 if(transition==assist::hookregistry::Transition::Install){
  if(!multiplayerpatch::mayInstall([](uint32_t at,void*out,size_t n){
   SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),(const void*)(uintptr_t)at,out,n,&got)&&got==n;
  }))return patchDenied(r.address,debugentries::Reason::Unsupported);
  // Entry bytes alone do not establish the adapter's stack/continuation ABI.
  // Restoration must remain possible after a dependency changes.
  if(!patchcontext::compatible(r.address,patchRead))return patchDenied(r.address);
  if(auto site=nativesidebarpolicy::find(r.address);site&&site->kind==nativesidebarpolicy::Kind::Bytes){
   if(!nativesidebarpolicy::installed(r.address,r.after,r.length))return patchDenied(r.address,debugentries::Reason::Structural);
  }else if(directpatch::find(r.address)){
   if(!directpatch::installed(r.address,r.after,r.length))return patchDenied(r.address,debugentries::Reason::Structural);
  }else if(mastercontrol::installed(r.address,r.after,r.length)){
   // Do not expose the unconditional predicate until BOTH strict owner calls
   // are owned by this module. Matching foreign bytes are not our ownership.
   if(r.address==experimentalpatch::ControlHouse)for(unsigned i=0;i<2;++i){
    const auto&s=mastercontrol::Sites[i];const int owner=patches.find(s.address);
    if(owner<0||!patches.active(s.address)||patches.conflict(s.address)||patchModules[owner]!=r.module||
       memcmp(patches.slots[owner].patch,s.after,5))return patchDenied(r.address,debugentries::Reason::OwnerPending);
    uint8_t actual[5]{};SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),(const void*)(uintptr_t)s.address,actual,5,&got)||got!=5||memcmp(actual,s.after,5))return patchDenied(r.address,debugentries::Reason::OwnerPending);
   }
   // Only this exact three-site adaptation may target the verified game leaf.
   // Keep ordinary detours restricted to this Runtime module.
   if(!mastercontrol::original(r.address,r.before,r.length)||!mastercontrol::compatible([](uint32_t at,void*out,size_t n){
    MEMORY_BASIC_INFORMATION info{};const void*p=(const void*)(uintptr_t)at;
    if(!VirtualQuery(p,&info,sizeof info)||info.State!=MEM_COMMIT||info.AllocationBase!=GetModuleHandleW(nullptr)||
       (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||uintptr_t(p)+n>uintptr_t(info.BaseAddress)+info.RegionSize)return false;
    memcpy(out,p,n);return true;
   }))return patchDenied(r.address);
  }else{
  const uint32_t target=experimentalpatch::destination(r.address,r.after,r.length);MEMORY_BASIC_INFORMATION info{};
  if(!target||!VirtualQuery((void*)(uintptr_t)target,&info,sizeof info)||info.State!=MEM_COMMIT||
     info.AllocationBase!=self||info.Type!=MEM_IMAGE||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||
     (nativeviewportpolicy::dataOperand(r.address)&&uintptr_t(target)+sizeof(uint32_t)>uintptr_t(info.BaseAddress)+info.RegionSize)||
     (nativeviewportpolicy::dataOperand(r.address)?!(info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)):
       !(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))))return patchDenied(r.address,debugentries::Reason::Structural);
  HMODULE pin=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
    reinterpret_cast<LPCWSTR>(target),&pin))return patchDenied(r.address,debugentries::Reason::OwnerPending,Result::SystemFailure);
  }
 }
 void* site=(void*)(uintptr_t)r.address;MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery(site,&info,sizeof info)||info.State!=MEM_COMMIT||info.AllocationBase!=GetModuleHandleW(nullptr)||
    (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||uintptr_t(site)+r.length>uintptr_t(info.BaseAddress)+info.RegionSize)return patchDenied(r.address,debugentries::Reason::Unreadable);
 if(memcmp(site,r.before,r.length)){patches.markConflict(r.address);return patchDenied(r.address,debugentries::Reason::Foreign);}
 DWORD prior=0;if(!VirtualProtect(site,r.length,PAGE_EXECUTE_READWRITE,&prior))return patchDenied(r.address,debugentries::Reason::OwnerPending,Result::SystemFailure);
 memcpy(site,r.after,r.length);
 const bool flushed=FlushInstructionCache(GetCurrentProcess(),site,r.length)!=FALSE;
 DWORD ignored=0;const bool protectedAgain=VirtualProtect(site,r.length,prior,&ignored)!=FALSE;
 // Record actual bytes even on an OS error, so cleanup retains ownership.
 patches.commit(r.address,r.before,r.after,r.length,transition);
 slot=patches.find(r.address);if(slot>=0){patchModules[slot]=r.module;patchErrors[slot]=flushed&&protectedAgain?0:int32_t(Result::SystemFailure);}
 observePatch(r.address);
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
  const bool priorDead=!owned&&isBoundThreadDead(policy.thread);
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
  if(r->address){const int i=patches.find(r->address);
   // Existing output fields expose the immutable journal without an ABI change.
   // Callers still need the original owner token to perform a restoration.
   if(i>=0){const auto&s=patches.slots[i];r->length=s.size;memcpy(r->before,s.original,s.size);memcpy(r->after,s.patch,s.size);}
   if(r->bindThread)observePatch(r->address); // explicit cold diagnostic sample only
  }
  r->entries=uint32_t(entries);r->reentrant=uint32_t(reentrant);r->contended=uint32_t(contended);r->wrongThread=uint32_t(wrongThread);result=Result::Ok;
 }else if(!policy.owns(tid,*r)){result=Result::WrongOwner;
 }else if(r->operation==Operation::Validate){result=Result::Ok;}
 else if(r->operation==Operation::Patch)result=applyPatch(*r);
 else if(r->operation==Operation::MarkConflict&&r->client==Client::Assist){
  int i=patches.find(r->address);
  if(i>=0&&patchModules[i]&&patchModules[i]!=r->module)result=Result::WrongOwner;
  else{patches.markConflict(r->address);i=patches.find(r->address);if(i>=0)patchModules[i]=r->module;observePatch(r->address);result=Result::Ok;}
 }
 if(!owned&&!retain)InterlockedExchange(&busy,0);
 return int32_t(result);
}
extern "C" void EntryRuntimeDetach(){
 if(boundThreadHandle){CloseHandle(boundThreadHandle);boundThreadHandle=nullptr;boundThreadId=0;}
}
