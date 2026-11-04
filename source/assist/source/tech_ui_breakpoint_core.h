#pragma once
#include <windows.h>
#include <stdint.h>
// Dedicated-thread CONTEXT changes; no target-process code, virtual table,
// rules, event queue or executable allocation is written here.
namespace assist { namespace techbp {
enum Status {Off=0,Ready=1,Waiting=2,Signature=-1,Slots=-2,System=-3,Foreign=-4,Thread=-5};
struct Registers {uint32_t address[4]{},control=0,status=0;};
inline uint32_t slotMask(unsigned n){return (3u<<(n*2))|(15u<<(16+n*4));}
inline uint32_t enabled(unsigned n){return 1u<<(n*2);}
struct Lease {
 uint32_t sites[4]{},before[4]{};uint8_t tag[4]{},alternative[4]{};uint32_t mask=0,uncertain=0;
 unsigned actualTag(const Registers&r,unsigned n)const{
  if(!(mask&(1u<<n))||(r.control&slotMask(n))!=enabled(n))return 4;
  if(r.address[n]==sites[tag[n]])return tag[n];
  return (uncertain&(1u<<n))&&r.address[n]==sites[alternative[n]]?alternative[n]:4;
 }
 bool exact(const Registers&r,unsigned n)const{return actualTag(r,n)<4;}
 uint32_t tags()const{uint32_t result=0;for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&!(uncertain&(1u<<n)))result|=1u<<tag[n];return result;}
 int configure(Registers&r,const uint32_t*addresses,uint32_t wanted){
  if(!addresses||wanted&~15u||r.control&(1u<<13))return Slots;
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
  unsigned index=0;for(unsigned t=0;t<4;++t)if(missing&(1u<<t)){
   if(!addresses[t])return Signature;const unsigned n=free[index++];next.before[n]=after.address[n];next.tag[n]=uint8_t(t);next.mask|=1u<<n;
   after.address[n]=addresses[t];after.control=(after.control&~slotMask(n))|enabled(n);after.status&=~(1u<<n);
  }
  for(unsigned t=0;t<4;++t)if(wanted&(1u<<t))next.sites[t]=addresses[t];*this=next;r=after;return wanted?Ready:Off;
 }
 int install(Registers&r){if(mask)return Foreign;const uint32_t requested[4]={sites[0],sites[1],0,0};return configure(r,requested,3);}
 bool intact(const Registers&r)const{if(!mask)return false;for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&!exact(r,n))return false;return true;}
 Lease remaining(const Registers&r)const{Lease kept=*this;for(unsigned n=0;n<4;++n)if(mask&(1u<<n)){
  unsigned actual=actualTag(r,n);if(actual>=4)kept.mask&=~(1u<<n);else kept.tag[n]=uint8_t(actual);
 }kept.uncertain=0;return kept;}
 static Lease possible(const Lease&a,const Lease&b){
  Lease result=a;for(unsigned t=0;t<4;++t)if(b.sites[t])result.sites[t]=b.sites[t];
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
class Service {
public:
 using Callback=bool(*)(void*,uint32_t);
 Lease lease{};Callback callback=nullptr;
 volatile LONG state=Off,lastError=0,exceptions=0,changed=0;
 volatile LONG confirmed=0;
private:
 HANDLE thread_=nullptr,worker_=nullptr;DWORD owner_=0;PVOID handler_=nullptr;
 uint32_t requested_[4]{},wanted_=0,operationGroups_=0;
 volatile LONG job_=0,operation_=0; // job 0 idle, 1 running, 2 completed
 static Service*&instance(){static Service*p=nullptr;return p;}
 static Registers registers(const CONTEXT&c){Registers r;r.address[0]=c.Dr0;r.address[1]=c.Dr1;r.address[2]=c.Dr2;r.address[3]=c.Dr3;r.control=c.Dr7;r.status=c.Dr6;return r;}
 static void registers(CONTEXT&c,const Registers&r){c.ContextFlags=CONTEXT_DEBUG_REGISTERS;c.Dr0=r.address[0];c.Dr1=r.address[1];c.Dr2=r.address[2];c.Dr3=r.address[3];c.Dr7=r.control;c.Dr6=r.status;}
 static DWORD WINAPI worker(void*raw){
  auto&s=*static_cast<Service*>(raw);int result=System;DWORD error=0;
  if(WaitForSingleObject(s.thread_,0)==WAIT_OBJECT_0){s.lease.mask=0;result=Off;}
  else {
   const DWORD prior=SuspendThread(s.thread_);
   if(prior==DWORD(-1))error=GetLastError();
   else {
    // Preserve another debugger's suspension count and leave its state alone.
    if(prior)result=Thread;
    else {
     CONTEXT context{};context.ContextFlags=CONTEXT_DEBUG_REGISTERS;
     if(!GetThreadContext(s.thread_,&context))error=GetLastError();
     else {
      Registers before=registers(context),after=before;const Lease original=s.lease;Lease proposed=original;
      bool retiring=false;
      if(s.operation_==1)result=proposed.install(after);
      else if(s.operation_==4)result=proposed.configure(after,s.requested_,s.wanted_);
      else if(s.operation_==2){retiring=true;result=proposed.restore(after);}
      else if(proposed.intact(before))result=Ready;
      else{retiring=true;result=Foreign;proposed.restore(after);}
      const bool writes=after.control!=before.control||after.status!=before.status||
       after.address[0]!=before.address[0]||after.address[1]!=before.address[1]||after.address[2]!=before.address[2]||after.address[3]!=before.address[3];
      bool committed=!writes;
      if(writes){registers(context,after);committed=SetThreadContext(s.thread_,&context)!=FALSE;if(!committed){error=GetLastError();result=System;}}
      const Lease possible=Lease::possible(original,proposed);s.lease=possible;s.confirmed=0;
      CONTEXT verify{};verify.ContextFlags=CONTEXT_DEBUG_REGISTERS;
      if(!GetThreadContext(s.thread_,&verify)){
       // Keep BOTH old/new candidate owners after an unverified transaction.
       // Neither a successful Set nor failure permits dropping a live VEH.
       error=GetLastError();result=System;
      }else{
       const Registers actual=registers(verify);s.lease=possible.remaining(actual);s.confirmed=1;
       if(retiring){if(s.lease.mask)result=System;}
       else if((s.operation_==1||s.operation_==3||s.operation_==4)&&committed&&result!=Slots&&result!=Signature&&
          s.lease.tags()!=proposed.tags())result=Foreign;
      }
     }
    }
    if(ResumeThread(s.thread_)==DWORD(-1)){error=GetLastError();result=System;}
   }
  }
  InterlockedExchange(&s.lastError,LONG(error));InterlockedExchange(&s.state,result);MemoryBarrier();InterlockedExchange(&s.job_,2);return 0;
 }
 static LONG CALLBACK vectored(EXCEPTION_POINTERS*p){
  Service*s=instance();if(!s||!p||!p->ExceptionRecord||!p->ContextRecord||
   p->ExceptionRecord->ExceptionCode!=EXCEPTION_SINGLE_STEP||GetCurrentThreadId()!=s->owner_)return EXCEPTION_CONTINUE_SEARCH;
  auto&c=*p->ContextRecord;const Registers r=registers(c);uint32_t ours=0,tag=0;
  for(unsigned n=0;n<4;++n)if((r.status&(1u<<n))&&s->lease.exact(r,n)&&c.Eip==s->lease.sites[s->lease.actualTag(r,n)]){ours|=1u<<n;tag=s->lease.actualTag(r,n);}
  if(!ours)return EXCEPTION_CONTINUE_SEARCH;
  // Other hardware/TF/debug-register causes belong to their existing handler.
  // Pass the original CONTEXT through without even changing RF or Dr6.
  if(r.status&(0xE00Fu&~ours))return EXCEPTION_CONTINUE_SEARCH;
  c.Dr6&=~ours;c.EFlags|=0x10000u;InterlockedIncrement(&s->exceptions);
  if(s->callback&&s->callback(&c,tag))InterlockedIncrement(&s->changed);
  return EXCEPTION_CONTINUE_EXECUTION;
 }
 bool start(unsigned operation){
  poll();if(job_)return false;operationGroups_=lease.uncertain?15u:lease.tags();if(operation==1)operationGroups_|=3;else if(operation==4)operationGroups_|=wanted_;operation_=LONG(operation);InterlockedExchange(&job_,1);
  worker_=CreateThread(nullptr,0,worker,this,0,nullptr);
  if(!worker_){lastError=LONG(GetLastError());state=System;job_=0;return false;}return true;
 }
public:
 void poll(){if(job_==2&&worker_&&WaitForSingleObject(worker_,0)==WAIT_OBJECT_0){CloseHandle(worker_);worker_=nullptr;MemoryBarrier();InterlockedExchange(&job_,0);}}
 bool pending(){poll();return job_!=0;}
 bool occupied()const{return lease.mask!=0;}
 bool pendingFor(uint32_t group){poll();return job_&&(operationGroups_&group);}
 uint32_t activeTags()const{return confirmed?lease.tags():0;}
 bool resources()const{return lease.mask||job_||thread_||worker_||handler_;}
 DWORD owner()const{return owner_;}
 bool enable(uint32_t a,uint32_t b,Callback fn){
  poll();if(job_)return false;
  const DWORD current=GetCurrentThreadId();
  if(owner_&&owner_!=current){state=Thread;return false;}
  if(!thread_){thread_=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|SYNCHRONIZE,FALSE,current);if(!thread_){lastError=LONG(GetLastError());state=System;return false;}owner_=current;}
  lease.sites[0]=a;lease.sites[1]=b;callback=fn;
  if(!handler_){
   if(instance()&&instance()!=this){state=Foreign;return false;}
   HMODULE pin=nullptr;
   if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&vectored),&pin)){lastError=LONG(GetLastError());state=System;return false;}
   instance()=this;handler_=AddVectoredExceptionHandler(1,vectored);
   if(!handler_){lastError=LONG(GetLastError());instance()=nullptr;state=System;return false;}
  }
  if(lease.mask)return audit();state=Waiting;return start(1);
 }
 bool configure(const uint32_t*sites,uint32_t wanted,Callback fn){
  poll();if(job_)return false;const DWORD current=GetCurrentThreadId();if(owner_&&owner_!=current){state=Thread;return false;}
  if(!thread_){thread_=OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|SYNCHRONIZE,FALSE,current);if(!thread_){lastError=LONG(GetLastError());state=System;return false;}owner_=current;}
  callback=fn;for(unsigned i=0;i<4;++i)requested_[i]=sites[i];wanted_=wanted;
  if(!handler_){
   if(instance()&&instance()!=this){state=Foreign;return false;}HMODULE pin=nullptr;
   if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&vectored),&pin)){lastError=LONG(GetLastError());state=System;return false;}
   instance()=this;handler_=AddVectoredExceptionHandler(1,vectored);if(!handler_){lastError=LONG(GetLastError());instance()=nullptr;state=System;return false;}
  }
  if(activeTags()==wanted){state=wanted?Ready:Off;return true;}state=Waiting;return start(4);
 }
 bool audit(){if(pending())return false;return start(3);}
 bool retire(){
  poll();if(job_)return false;
  if(lease.mask){start(2);return false;}
  if(handler_){if(!RemoveVectoredExceptionHandler(handler_)){lastError=LONG(GetLastError());state=System;return false;}handler_=nullptr;instance()=nullptr;}
  if(thread_){CloseHandle(thread_);thread_=nullptr;}owner_=0;callback=nullptr;confirmed=0;state=Off;return true;
 }
};
}}
