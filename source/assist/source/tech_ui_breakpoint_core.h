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
 uint32_t sites[2]{},before[4]{};uint8_t tag[4]{};uint32_t mask=0;
 bool exact(const Registers&r,unsigned n)const{return (mask&(1u<<n))&&r.address[n]==sites[tag[n]]&&(r.control&slotMask(n))==enabled(n);}
 int install(Registers&r){
  if(mask)return Foreign;if(r.control&(1u<<13))return Slots;unsigned free[2]{},count=0;
  for(unsigned n=0;n<4&&count<2;++n)if(!r.address[n]&&!(r.control&slotMask(n)))free[count++]=n;
  if(count!=2)return Slots;
  for(unsigned i=0;i<2;++i){unsigned n=free[i];before[n]=r.address[n];tag[n]=uint8_t(i);mask|=1u<<n;r.address[n]=sites[i];r.control=(r.control&~slotMask(n))|enabled(n);r.status&=~(1u<<n);}
  return Ready;
 }
 bool intact(const Registers&r)const{if(!mask)return false;for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&!exact(r,n))return false;return true;}
 Lease remaining(const Registers&r)const{Lease kept=*this;for(unsigned n=0;n<4;++n)if((mask&(1u<<n))&&!exact(r,n))kept.mask&=~(1u<<n);return kept;}
 int restore(Registers&r){
  bool foreign=false;for(unsigned n=0;n<4;++n)if(mask&(1u<<n)){
   if(exact(r,n)){r.address[n]=before[n];r.control&=~slotMask(n);r.status&=~(1u<<n);}else foreign=true;
  }mask=0;return foreign?Foreign:Off;
 }
};
class Service {
public:
 using Callback=bool(*)(void*,uint32_t);
 Lease lease{};Callback callback=nullptr;
 volatile LONG state=Off,lastError=0,exceptions=0,changed=0;
private:
 HANDLE thread_=nullptr,worker_=nullptr;DWORD owner_=0;PVOID handler_=nullptr;
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
      else if(s.operation_==2){retiring=true;result=proposed.restore(after);}
      else if(proposed.intact(before))result=Ready;
      else{retiring=true;result=Foreign;proposed.restore(after);}
      const bool writes=after.control!=before.control||after.status!=before.status||
       after.address[0]!=before.address[0]||after.address[1]!=before.address[1]||after.address[2]!=before.address[2]||after.address[3]!=before.address[3];
      bool committed=!writes;
      if(writes){registers(context,after);committed=SetThreadContext(s.thread_,&context)!=FALSE;if(!committed){error=GetLastError();result=System;}}
      if(!committed)s.lease=s.operation_==1?proposed:original.remaining(before);
      else {
       CONTEXT verify{};verify.ContextFlags=CONTEXT_DEBUG_REGISTERS;
       if(!GetThreadContext(s.thread_,&verify)){
        // SetThreadContext success is not retirement evidence. Retain every
        // possibly installed own slot/VEH until a later confirmed readback.
        s.lease=s.operation_==1?proposed:original.remaining(before);error=GetLastError();result=System;
       }else if(retiring){
        s.lease=original.remaining(registers(verify));
        if(s.lease.mask)result=System; // still installed: keep handler and retry
       }else{
        s.lease=proposed;if(result==Ready&&!s.lease.intact(registers(verify)))result=Foreign;
       }
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
  for(unsigned n=0;n<4;++n)if((r.status&(1u<<n))&&s->lease.exact(r,n)&&c.Eip==s->lease.sites[s->lease.tag[n]]){ours|=1u<<n;tag=s->lease.tag[n];}
  if(!ours)return EXCEPTION_CONTINUE_SEARCH;
  // Other hardware/TF/debug-register causes belong to their existing handler.
  // Pass the original CONTEXT through without even changing RF or Dr6.
  if(r.status&(0xE00Fu&~ours))return EXCEPTION_CONTINUE_SEARCH;
  c.Dr6&=~ours;c.EFlags|=0x10000u;InterlockedIncrement(&s->exceptions);
  if(s->callback&&s->callback(&c,tag))InterlockedIncrement(&s->changed);
  return EXCEPTION_CONTINUE_EXECUTION;
 }
 bool start(unsigned operation){
  poll();if(job_)return false;operation_=LONG(operation);InterlockedExchange(&job_,1);
  worker_=CreateThread(nullptr,0,worker,this,0,nullptr);
  if(!worker_){lastError=LONG(GetLastError());state=System;job_=0;return false;}return true;
 }
public:
 void poll(){if(job_==2&&worker_&&WaitForSingleObject(worker_,0)==WAIT_OBJECT_0){CloseHandle(worker_);worker_=nullptr;MemoryBarrier();InterlockedExchange(&job_,0);}}
 bool pending(){poll();return job_!=0;}
 bool occupied()const{return lease.mask!=0;}
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
 bool audit(){if(pending())return false;return start(3);}
 bool retire(){
  poll();if(job_)return false;
  if(lease.mask){start(2);return false;}
  if(handler_){if(!RemoveVectoredExceptionHandler(handler_)){lastError=LONG(GetLastError());state=System;return false;}handler_=nullptr;instance()=nullptr;}
  if(thread_){CloseHandle(thread_);thread_=nullptr;}owner_=0;callback=nullptr;state=Off;return true;
 }
};
}}
