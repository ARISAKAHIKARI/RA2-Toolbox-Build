#pragma once
#include <windows.h>
#include <stdint.h>
#include "../../shared/source/hardware_slot_budget.h"
#include "../../shared/source/runtime_seh_scope.h"
#include "tech_ui_breakpoint_hot.h"
extern "C" int __cdecl RuntimeBreakpointCall(void*,void(__cdecl*)(void*),int(__cdecl*)(void*,void*));
// Owner-thread exception CONTEXT transactions; no target-process code, virtual
// table, rules, event queue or executable allocation is written here.
namespace assist { namespace techbp {
inline constexpr unsigned SiteCount=6;
inline constexpr uint32_t AllTags=(1u<<SiteCount)-1;
enum Status {Off=0,Ready=1,Waiting=2,Signature=-1,Slots=-2,System=-3,Foreign=-4,Thread=-5};
struct Registers {uint32_t address[4]{},control=0,status=0;};
inline uint32_t slotMask(unsigned n){return (3u<<(n*2))|(15u<<(16+n*4));}
inline uint32_t enabled(unsigned n){return 1u<<(n*2);}
struct Lease {
 uint32_t sites[SiteCount]{},before[4]{};uint8_t tag[4]{},alternative[4]{};uint32_t mask=0,uncertain=0;
 unsigned actualTag(const Registers&r,unsigned n)const{
  if(!(mask&(1u<<n))||(r.control&slotMask(n))!=enabled(n))return SiteCount;
  if(r.address[n]==sites[tag[n]])return tag[n];
  return (uncertain&(1u<<n))&&r.address[n]==sites[alternative[n]]?alternative[n]:SiteCount;
 }
 bool exact(const Registers&r,unsigned n)const{return actualTag(r,n)<SiteCount;}
 uint32_t tags()const{uint32_t result=0;for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&!(uncertain&(1u<<n)))result|=1u<<tag[n];return result;}
 int configure(Registers&r,const uint32_t*addresses,uint32_t wanted){
  if(!addresses||wanted&~AllTags||r.control&(1u<<13))return Slots;
  if(mask&&!intact(r))return Foreign;
  for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&(wanted&(1u<<actualTag(r,n)))&&addresses[actualTag(r,n)]!=sites[actualTag(r,n)])return Foreign;
  Lease next=*this;Registers after=r;
  for(unsigned n=0;n<4;++n)if(next.mask&(1u<<n)){
   const unsigned current=next.actualTag(after,n);
   if(!(wanted&(1u<<current))||addresses[current]!=next.sites[current]){
    after.address[n]=next.before[n];after.control&=~slotMask(n);after.status&=~(1u<<n);next.mask&=~(1u<<n);next.uncertain&=~(1u<<n);
   }else{next.tag[n]=uint8_t(current);next.uncertain&=~(1u<<n);}
  }
  uint32_t missing=wanted&~next.tags();unsigned free[4]{},count=0;
  for(unsigned n=0;n<4;++n)if(!after.address[n]&&!(after.control&slotMask(n)))free[count++]=n;
  if(unsigned(__builtin_popcount(missing))>count)return Slots; // transaction: no changes at all
  unsigned index=0;for(unsigned t=0;t<SiteCount;++t)if(missing&(1u<<t)){
   if(!addresses[t])return Signature;const unsigned n=free[index++];next.before[n]=after.address[n];next.tag[n]=uint8_t(t);next.mask|=1u<<n;
   after.address[n]=addresses[t];after.control=(after.control&~slotMask(n))|enabled(n);after.status&=~(1u<<n);
  }
  for(unsigned t=0;t<SiteCount;++t)if(wanted&(1u<<t))next.sites[t]=addresses[t];*this=next;r=after;return wanted?Ready:Off;
 }
 int install(Registers&r){if(mask)return Foreign;const uint32_t requested[SiteCount]={sites[0],sites[1]};return configure(r,requested,3);}
 bool intact(const Registers&r)const{if(!mask)return false;for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&!exact(r,n))return false;return true;}
 Lease remaining(const Registers&r)const{Lease kept=*this;for(unsigned n=0;n<4;++n)if(mask&(1u<<n)){
  unsigned actual=actualTag(r,n);if(actual>=SiteCount)kept.mask&=~(1u<<n);else kept.tag[n]=uint8_t(actual);
 }kept.uncertain=0;return kept;}
 static Lease possible(const Lease&a,const Lease&b){
  Lease result=a;for(unsigned t=0;t<SiteCount;++t)if(b.sites[t])result.sites[t]=b.sites[t];
  for(unsigned n=0;n<4;++n)if(b.mask&(1u<<n)){
   if(!(result.mask&(1u<<n))){result.mask|=1u<<n;result.tag[n]=b.tag[n];result.before[n]=b.before[n];}
   else if(result.tag[n]!=b.tag[n]){result.uncertain|=1u<<n;result.alternative[n]=b.tag[n];}
  }return result;
 }
 int restore(Registers&r){
  bool foreign=false;for(unsigned n=0;n<4;++n)if(mask&(1u<<n)){
   if(exact(r,n)){r.address[n]=before[n];r.control&=~slotMask(n);r.status&=~(1u<<n);}else foreign=true;
  }mask=uncertain=0;return foreign?Foreign:Off;
 }
};
// These two instructions live in this Runtime image. No game instruction is
// patched: the INT3 is a synchronous owner rendezvous and the NOP is a hardware
// execution witness used before the first feature lease on an owner thread.
static __declspec(naked) void ownerRendezvousTrap(){__asm {
 int 3
 ret
}}
static __declspec(naked) void ownerRendezvousWitness(){__asm {
 nop
 ret
}}
class Service {
public:
 using Callback=bool(*)(void*,uint32_t);
 Lease lease{};Callback callback=nullptr;
 volatile LONG state=Off,lastError=0,exceptions=0,changed=0;
 volatile LONG confirmed=0;
private:
 HANDLE thread_=nullptr;DWORD owner_=0;PVOID handler_=nullptr;
 uint32_t requested_[SiteCount]{},wanted_=0,operationGroups_=0;
 volatile LONG job_=0,handlerDepth_=0;
 volatile LONG budget_=0;DWORD budgetAt_=0;
 Lease witnessLease_{};bool witnessed_=false;volatile LONG witnessSeen_=0;
 enum RequestMode {ReadContext=1,WriteContext=2};
 struct Request {
  Registers before{},after{},observed{};uint32_t token=0,reply=0,clearStatus=0;
  unsigned mode=0;DWORD error=0;bool valid=false,writeIssued=false;
 } request_;
 uint32_t serial_=0;
 static uint32_t budget(const Registers&r,const Lease&lease){
  if(r.control&(1u<<13))return 0;unsigned free=0,owned=0;uint32_t tags=0;
  for(unsigned n=0;n<4;++n){if(!r.address[n]&&!(r.control&slotMask(n)))++free;else if(lease.exact(r,n)){++owned;tags|=1u<<lease.actualTag(r,n);}}
  return slotbudget::pack(free,owned,4-free-owned,tags);
 }
 static Service*&instance(){static Service*p=nullptr;return p;}
 static uint32_t trapAddress(){return uint32_t(uintptr_t(&ownerRendezvousTrap));}
 static uint32_t witnessAddress(){return uint32_t(uintptr_t(&ownerRendezvousWitness));}
 static Registers registers(const CONTEXT&c){Registers r;r.address[0]=c.Dr0;r.address[1]=c.Dr1;r.address[2]=c.Dr2;r.address[3]=c.Dr3;r.control=c.Dr7;r.status=c.Dr6;return r;}
 static bool same(const Registers&a,const Registers&b){return a.control==b.control&&a.address[0]==b.address[0]&&a.address[1]==b.address[1]&&a.address[2]==b.address[2]&&a.address[3]==b.address[3];}
 static bool contextPresent(const CONTEXT&c){return (c.ContextFlags&CONTEXT_DEBUG_REGISTERS)==CONTEXT_DEBUG_REGISTERS;}
 bool privateTrap(const EXCEPTION_POINTERS*p)const{
  return request_.token&&job_&&p&&p->ExceptionRecord&&p->ContextRecord&&GetCurrentThreadId()==owner_&&
   p->ExceptionRecord->ExceptionCode==EXCEPTION_BREAKPOINT&&
   uint32_t(uintptr_t(p->ExceptionRecord->ExceptionAddress))==trapAddress();
 }
 struct DispatchRoute {
  enum Kind {Ignore,PrivateTrap,Witness,Feature} kind=Ignore;
  Hit hit{};
 };
 DispatchRoute classify(const EXCEPTION_POINTERS*p)const{
  // This phase only reads the supplied CONTEXT and our owner-thread lease.
  // Foreign/native exceptions need neither a cleanup frame nor a depth write.
  // No game-memory read, callback, CONTEXT edit or statistic belongs here.
  const auto&c=*p->ContextRecord;
  if(p->ExceptionRecord->ExceptionCode==EXCEPTION_BREAKPOINT)
   return privateTrap(p)?DispatchRoute{DispatchRoute::PrivateTrap,{}}:DispatchRoute{};
  if(p->ExceptionRecord->ExceptionCode!=EXCEPTION_SINGLE_STEP||!contextPresent(c))return {};
  const Registers r=registers(c);
  if(witnessLease_.mask){
   const Hit hit=ownedHit(witnessLease_,r,c.Eip,SiteCount);
   if(hit.slots)return {DispatchRoute::Witness,hit};
  }
  const Hit hit=ownedHit(lease,r,c.Eip,SiteCount);
  return hit.slots?DispatchRoute{DispatchRoute::Feature,hit}:DispatchRoute{};
 }
 LONG dispatch(EXCEPTION_POINTERS*p,const DispatchRoute&route){
  auto&c=*p->ContextRecord;
  if(route.kind==DispatchRoute::PrivateTrap){
   // CONTEXT_CONTROL is needed to skip our exact, one-byte INT3. No native
   // game EIP is redirected, and an unexpected context goes to the local SEH.
   if((c.ContextFlags&CONTEXT_CONTROL)!=CONTEXT_CONTROL||(c.Eip!=trapAddress()&&c.Eip!=trapAddress()+1))return EXCEPTION_CONTINUE_SEARCH;
   c.Eip=trapAddress()+1;request_.reply=request_.token;
   if(!contextPresent(c)){request_.error=ERROR_NOT_SUPPORTED;return EXCEPTION_CONTINUE_EXECUTION;}
   request_.observed=registers(c);request_.valid=true;
   if(request_.mode==WriteContext){
    if(!same(request_.observed,request_.before)){request_.error=ERROR_BUSY;return EXCEPTION_CONTINUE_EXECUTION;}
    c.Dr0=request_.after.address[0];c.Dr1=request_.after.address[1];c.Dr2=request_.after.address[2];c.Dr3=request_.after.address[3];
    c.Dr7=request_.after.control;c.Dr6&=~request_.clearStatus;request_.writeIssued=true;
   }
   return EXCEPTION_CONTINUE_EXECUTION;
  }
  const Hit hit=route.hit;
  if(route.kind==DispatchRoute::Witness){c.Dr6&=~hit.slots;c.EFlags|=0x10000u;witnessSeen_=1;return EXCEPTION_CONTINUE_EXECUTION;}
  if(route.kind!=DispatchRoute::Feature)return EXCEPTION_CONTINUE_SEARCH;
  // An uncertain owned slot still needs a receiver. Its feature callback is
  // disabled until an independent post-continuation read confirmed the lease.
  c.Dr6&=~hit.slots;c.EFlags|=0x10000u;countOwner(exceptions);
  if(handlerDepth_==1&&confirmed&&(state==Ready||state==Slots||state==Signature)&&callback&&callback(&c,hit.tag))countOwner(changed);
  return EXCEPTION_CONTINUE_EXECUTION;
 }
 static LONG CALLBACK vectored(EXCEPTION_POINTERS*p){
  Service*s=instance();if(!s||!p||!p->ExceptionRecord||!p->ContextRecord||GetCurrentThreadId()!=s->owner_)return EXCEPTION_CONTINUE_SEARCH;
  const DispatchRoute route=s->classify(p);if(route.kind==DispatchRoute::Ignore)return EXCEPTION_CONTINUE_SEARCH;
  LONG result=EXCEPTION_CONTINUE_SEARCH;++s->handlerDepth_;
  runtimeunwind::run([&]{result=s->dispatch(p,route);},[&]{--s->handlerDepth_;});
  return result;
 }
 int localTrapFilter(EXCEPTION_POINTERS*p){return privateTrap(p)?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH;}
 static void __cdecl trapBody(void*){ownerRendezvousTrap();}
 static int __cdecl trapFilter(void*raw,void*p){return static_cast<Service*>(raw)->localTrapFilter(static_cast<EXCEPTION_POINTERS*>(p));}
 // The only SEH recovery here is our exact Runtime INT3 while its request is
 // active. A debugger/earlier VEH may consume it: the missing reply then fails
 // closed. Foreign breakpoint, single-step and native faults are not handled.
 void invokeTrap(){
  if(!RuntimeBreakpointCall(this,trapBody,trapFilter))request_.error=ERROR_NOT_SUPPORTED;
 }
 bool rendezvous(unsigned mode,const Registers*before=nullptr,const Registers*after=nullptr,uint32_t clearStatus=0){
  if(handlerDepth_||!job_||GetCurrentThreadId()!=owner_){lastError=ERROR_BUSY;return false;}
  request_={};request_.mode=mode;request_.token=++serial_;if(!request_.token)request_.token=++serial_;
  if(before)request_.before=*before;if(after)request_.after=*after;request_.clearStatus=clearStatus;
  invokeTrap();const bool replied=request_.reply==request_.token;request_.token=0;
  if(!replied||!request_.valid||request_.error){lastError=request_.error?LONG(request_.error):LONG(ERROR_NOT_SUPPORTED);return false;}
  return true;
 }
 bool sample(Registers&r){if(!rendezvous(ReadContext))return false;r=request_.observed;return true;}
 void observedBudget(const Registers&r){budgetAt_=GetTickCount();InterlockedExchange(&budget_,LONG(budget(r,lease)));}
 // Only the owner can run this transaction. "writeIssued" means our handler
 // supplied a continuation CONTEXT, not that Windows has committed it. Keep
 // both possible owners until a separate, subsequent trap observes the DRs.
 int transfer(Lease&owned,const Lease&proposed,const Registers&before,const Registers&after,int result,bool retiring){
  if(same(before,after)){owned=proposed.remaining(before);confirmed=1;observedBudget(before);return result;}
  // A previous failed readback may represent either A or B in a slot. This
  // fresh sample resolves it before proposing C, so B cannot be overwritten
  // in the uncertainty journal and forgotten if the next write is ignored.
  const Lease original=owned.remaining(before),possible=Lease::possible(original,proposed);owned=possible;confirmed=0;
  const bool accepted=rendezvous(WriteContext,&before,&after,original.mask|proposed.mask);
  const bool issued=request_.writeIssued;
  if(!accepted&&!issued){
   owned=original;
   if(request_.valid){owned=original.remaining(request_.observed);confirmed=1;observedBudget(request_.observed);}
   return request_.error==ERROR_BUSY?Foreign:System;
  }
  Registers actual{};if(!sample(actual))return System;
  owned=possible.remaining(actual);confirmed=1;observedBudget(actual);
  if(!same(actual,after)){const bool ignored=same(actual,before);lastError=ignored?ERROR_NOT_SUPPORTED:ERROR_INVALID_DATA;return ignored||(retiring&&owned.mask)?System:Foreign;}
  if(retiring&&owned.mask){lastError=ERROR_INVALID_DATA;return System;}
  if(!retiring&&owned.tags()!=proposed.tags()){lastError=ERROR_INVALID_DATA;return Foreign;}
  return result;
 }
 int clean(Lease&owned){
  if(!owned.mask)return Off;
  Registers before{};if(!sample(before)){confirmed=0;return System;}
  Registers after=before;Lease removed=owned;const int result=removed.restore(after);
  return transfer(owned,removed,before,after,result,true);
 }
 void recoverOnce(Lease&owned,int failure,LONG error){
  // One bounded owned-only attempt, never a wait/retry loop. Failure keeps
  // the possible lease and handler for a later owner maintenance call.
  if(owned.mask)clean(owned);state=failure;lastError=error?error:LONG(ERROR_INVALID_DATA);
 }
 bool witness(){
  if(witnessed_)return true;
  if(witnessLease_.mask||lease.mask){state=System;lastError=ERROR_BUSY;return false;}
  Registers before{};if(!sample(before)){state=System;return false;}
  Registers after=before;Lease proposed{};uint32_t address[SiteCount]{};address[0]=witnessAddress();
  int result=proposed.configure(after,address,1);if(result!=Ready){state=result;return false;}
  witnessSeen_=0;result=transfer(witnessLease_,proposed,before,after,Ready,false);
  if(result!=Ready){recoverOnce(witnessLease_,result,lastError);return false;}
  // A readback alone could hide an unsupported/sanitized DR continuation.
  // Require the CPU execution breakpoint at our own harmless NOP as well.
  ownerRendezvousWitness();const bool hit=witnessSeen_!=0;
  result=clean(witnessLease_);
  if(result!=Off||witnessLease_.mask){state=System;if(!lastError)lastError=ERROR_INVALID_DATA;return false;}
  Registers restored{};if(!sample(restored)){state=System;return false;}
  if(!same(restored,before)){state=Foreign;lastError=ERROR_INVALID_DATA;return false;}
  if(!hit){state=System;lastError=ERROR_NOT_SUPPORTED;return false;}
  witnessed_=true;return true;
 }
 bool ensureHandler(){
  if(handler_)return true;
  if(instance()&&instance()!=this){state=Foreign;return false;}
  HMODULE pin=nullptr;
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&vectored),&pin)){lastError=LONG(GetLastError());state=System;return false;}
  instance()=this;handler_=AddVectoredExceptionHandler(1,vectored);
  if(!handler_){lastError=LONG(GetLastError());instance()=nullptr;state=System;return false;}return true;
 }
 bool ensureOwner(){
  const DWORD current=GetCurrentThreadId();
  if(thread_){
   const DWORD alive=WaitForSingleObject(thread_,0);
   if(alive==WAIT_TIMEOUT)return owner_==current;
   if(alive!=WAIT_OBJECT_0){lastError=LONG(GetLastError());state=System;return false;}
   // A reused numeric ID is not the old kernel thread. Only a signaled held
   // object proves that every old DR/exception context has ceased to exist.
   endedOwner();if(!CloseHandle(thread_)){lastError=LONG(GetLastError());state=System;return false;}
   thread_=nullptr;owner_=0;callback=nullptr;
  }
  // Duplicate the current-thread pseudo handle, so identity is the actual
  // kernel object rather than an ID reopened after possible ID reuse. This
  // service deliberately never requests SUSPEND_RESUME or SET_CONTEXT rights.
  HANDLE owned=nullptr;
  if(!DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&owned,SYNCHRONIZE,FALSE,0)){lastError=LONG(GetLastError());state=System;return false;}
  thread_=owned;owner_=current;witnessed_=false;return true;
 }
 void endedOwner(){lease={};witnessLease_={};confirmed=0;witnessed_=false;InterlockedExchange(&budget_,0);state=Off;lastError=0;}
 void transact(unsigned operation){
  if(witnessLease_.mask){state=System;lastError=ERROR_BUSY;return;}
  if((operation==1||(operation==4&&wanted_))&&!witness()){return;}
  Registers before{};if(!sample(before)){confirmed=0;state=System;return;}
  Registers after=before;const Lease original=lease;Lease proposed=original;bool retiring=false;int result=Off;
  if(operation==5){observedBudget(before);state=Off;lastError=0;return;}
  if(operation==1)result=proposed.install(after);
  else if(operation==4)result=proposed.configure(after,requested_,wanted_);
  else if(operation==2){retiring=true;result=proposed.restore(after);}
  else if(proposed.intact(before))result=Ready;
  else {retiring=true;result=Foreign;proposed.restore(after);}
  result=transfer(lease,proposed,before,after,result,retiring);state=result;
  if(result==System||result==Foreign){const LONG error=lastError;recoverOnce(lease,result,error);}
  else lastError=0;
 }
 bool start(unsigned operation){
  // An owned hardware callback already has an older continuation CONTEXT.
  // It must never start a second configuration, even on the same thread.
  if(handlerDepth_||job_||!thread_)return false;
  const DWORD alive=WaitForSingleObject(thread_,0);
  if(alive==WAIT_OBJECT_0){endedOwner();return true;}
  if(alive!=WAIT_TIMEOUT){lastError=LONG(GetLastError());state=System;return false;}
  if(GetCurrentThreadId()!=owner_)return false;
  if(!ensureHandler())return false;
  operationGroups_=lease.uncertain?AllTags:lease.tags();if(operation==1)operationGroups_|=3;else if(operation==4)operationGroups_|=wanted_;
  InterlockedExchange(&budget_,0);InterlockedExchange(&job_,1);bool completed=false;
  runtimeunwind::run([&]{transact(operation);completed=true;},[&]{
   request_.token=0;
   if(!completed){confirmed=0;state=System;lastError=ERROR_OPERATION_ABORTED;}
   MemoryBarrier();InterlockedExchange(&job_,0);
  });
  return true;
 }
public:
 void poll(){MemoryBarrier();}
 bool pending(){return InterlockedCompareExchange(&job_,0,0)!=0;}
 uint32_t pendingGroups(){return (pending()?operationGroups_:0u)|(witnessLease_.mask?AllTags:0u);}
 bool occupied()const{return lease.mask||witnessLease_.mask;}
 bool pendingFor(uint32_t group){return (pending()&&(operationGroups_&group))||(witnessLease_.mask&&group);}
 uint32_t activeTags()const{return confirmed?lease.tags():0;}
 bool resources()const{return lease.mask||witnessLease_.mask||job_||thread_||handler_;}
 DWORD owner()const{return owner_;}
 bool enable(uint32_t a,uint32_t b,Callback fn){
  if(handlerDepth_||pending()||!ensureOwner())return false;
  if(lease.mask){if(lease.sites[0]!=a||lease.sites[1]!=b){lastError=ERROR_INVALID_PARAMETER;state=Foreign;return false;}callback=fn;return audit();}
  lease.sites[0]=a;lease.sites[1]=b;callback=fn;state=Waiting;return start(1);
 }
 bool configure(const uint32_t*sites,uint32_t wanted,Callback fn){
  if(handlerDepth_||pending())return false;if(!sites||(wanted&~AllTags)){state=Slots;return false;}if(!ensureOwner())return false;
  callback=fn;for(unsigned i=0;i<SiteCount;++i)requested_[i]=(wanted&(1u<<i))?sites[i]:0;wanted_=wanted;
  bool settled=!witnessLease_.mask&&(wanted?(confirmed&&witnessed_&&activeTags()==wanted&&state!=System&&state!=Foreign&&state!=Thread):!lease.mask);
  for(unsigned i=0;i<SiteCount&&settled;++i)if((wanted&(1u<<i))&&requested_[i]!=lease.sites[i])settled=false;
  if(settled){state=wanted?Ready:Off;return true;}state=Waiting;return start(4);
 }
 uint32_t slotBudget()const{return !job_&&budgetAt_&&DWORD(GetTickCount()-budgetAt_)<=1000?uint32_t(InterlockedCompareExchange(const_cast<volatile LONG*>(&budget_),0,0)):0;}
 bool inspect(){
  if(handlerDepth_||pending()||!ensureOwner())return false;if(lease.mask)return audit();return start(5);
 }
 bool audit(){if(handlerDepth_||pending())return false;return start(3);}
 bool retire(){
  if(handlerDepth_||pending())return false;
  if(thread_){
   const DWORD alive=WaitForSingleObject(thread_,0);
   if(alive==WAIT_OBJECT_0)endedOwner();
   else if(alive!=WAIT_TIMEOUT){lastError=LONG(GetLastError());state=System;return false;}
   else if(GetCurrentThreadId()!=owner_)return false;
  }
  if(witnessLease_.mask){
   if(!ensureHandler())return false;operationGroups_=AllTags;InterlockedExchange(&job_,1);bool completed=false;
   runtimeunwind::run([&]{const int result=clean(witnessLease_);state=result;completed=true;},[&]{request_.token=0;if(!completed){confirmed=0;state=System;lastError=ERROR_OPERATION_ABORTED;}InterlockedExchange(&job_,0);});
   return false;
  }
  if(lease.mask){start(2);return false;}
  if(handler_){if(!RemoveVectoredExceptionHandler(handler_)){lastError=LONG(GetLastError());state=System;return false;}handler_=nullptr;instance()=nullptr;}
  if(thread_){if(!CloseHandle(thread_)){lastError=LONG(GetLastError());state=System;return false;}thread_=nullptr;}
  owner_=0;callback=nullptr;confirmed=0;witnessed_=false;state=Off;lastError=0;return true;
 }
};
}}
