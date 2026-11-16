#pragma once
#include "world_frame_background_trial.h"
#include <windows.h>
#include <atomic>
#include <mutex>
#include <cstdio>

// An opt-in production shadow sampler. This performs *actual* graph reads on
// its own pinned Win32 thread. It never publishes a Snapshot and is NOT an
// alternate native-command sample. The main control sampler remains authoritative.
namespace worldframe {
class ShadowGraphWorker {
 static constexpr size_t MaxTrialPages=4096,MaxTrialBytes=16*1024*1024;
 static constexpr unsigned MaxTrialReads=4096;
 struct Request {Key key{};uint32_t demand=Base,epoch=0;};
 std::mutex gate_;
 std::atomic<uint32_t> epoch_{1};
 std::atomic<bool> stopping_{false};
 HANDLE thread_=nullptr,event_=nullptr;
 HMODULE pin_=nullptr;
 Request queued_{};
 BackgroundTrialResult result_{};
 uint32_t resultEpoch_=0;
 bool queuedReady_=false,resultReady_=false;
 uint32_t requestedAt_=0;
 BackgroundGraphOrder deliveryOrder_;
 static bool live(uint32_t p,void*out,size_t n){
  if(!range(p,n)||!out)return false;
  SIZE_T got=0;
  return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(uintptr_t(p)),out,n,&got)&&got==n;
 }
 static Key stamp(const Key&source){
  Key key=source;
  int count=-1,frame=-1,session=-1;
  uint32_t scenario=0,cells=0,house=0;uint8_t running=0;
  if(!live(0xA8B230,&scenario,4)||!live(0xA8B238,&session,4)||
     !live(0x87F924,&cells,4)||!live(0x87F928,&count,4)||
     !live(0xA8ED84,&frame,4)||!live(0xA8E9A0,&running,1)||
     !live(0xA83D4C,&house,4)||scenario<0x10000||cells<0x10000||
     house<0x10000||count<=0||count>512*512||frame<0||
     !(session==0||session==3||session==4||session==5)){
   key.pid=0;return key;
  }
  key.scenario=scenario;key.cells=cells;key.cellCount=uint32_t(count);
  key.house=house;key.frame=frame;key.session=session;key.tick=GetTickCount();
  return key;
 }
 static DWORD WINAPI threadMain(void*opaque){
  auto*self=static_cast<ShadowGraphWorker*>(opaque);
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
  {
  // One allocator/storage owner per pinned worker lifetime. A failed
  // allocation ends this diagnostic graph only; the command sampler owns
  // separate pages and cannot be harmed by shadow memory pressure.
  PageAllocator bounded{};
  bounded.allocate=[](size_t size,void*)noexcept->void*{
   return size<=MaxTrialPages*sizeof(Page)?std::malloc(size):nullptr;
  };
  Snapshot retainedSample(bounded);CaptureScratch retainedScratch(bounded);
  for(;;){
   if(self->stopping_.load(std::memory_order_acquire))break;
   if(WaitForSingleObject(self->event_,250)==WAIT_FAILED)break;
   if(self->stopping_.load(std::memory_order_acquire))break;
   Request job{};bool found=false;
   {
    std::lock_guard<std::mutex>lock(self->gate_);
    if(self->queuedReady_){job=self->queued_;self->queuedReady_=false;found=true;}
   }
   if(!found||job.epoch!=self->epoch_.load(std::memory_order_acquire))continue;
   // One Snapshot and graph scratch belong exclusively to this thread.
   // Real RPM work is bounded by time, calls and bytes. Once exhausted,
   // the graph fails closed with CaptureFailure; it NEVER publishes an
   // incomplete authoritative command sample.
   const DWORD began=GetTickCount();size_t bytes=0;unsigned calls=0;
   const auto result=backgroundTrialWithStorage([&]{return stamp(job.key);},
     [&](uint32_t at,void*out,size_t n){
      if(self->stopping_.load(std::memory_order_acquire)||
         job.epoch!=self->epoch_.load(std::memory_order_acquire)||
         uint32_t(GetTickCount()-began)>BackgroundGraphPolicy::MaxCaptureMs||
         calls>=MaxTrialReads||n>MaxTrialBytes-bytes)return false;
      ++calls;bytes+=n;return live(at,out,n);
     },job.demand,retainedSample,retainedScratch);
   if(job.epoch!=self->epoch_.load(std::memory_order_acquire)||
      self->stopping_.load(std::memory_order_acquire))continue;
   {
    std::lock_guard<std::mutex>lock(self->gate_);
    if(job.epoch!=self->epoch_.load(std::memory_order_acquire))continue;
    if(self->resultReady_&&self->result_.after.sameWorld(result.after)&&
       result.after.frame<self->result_.after.frame)continue;
    self->result_=result;self->resultEpoch_=job.epoch;self->resultReady_=true;
   }
  }
  } // Release Snapshot/PageBuffer/CaptureScratch before the DLL pin.
  const HMODULE module=self->pin_;
  FreeLibraryAndExitThread(module,0);return 0;
 }
 void reap(){
  if(!thread_||WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0)return;
  CloseHandle(thread_);CloseHandle(event_);thread_=event_=nullptr;pin_=nullptr;
 }
 bool launch(){
  reap();
  if(thread_)return !stopping_.load(std::memory_order_acquire);
  HMODULE pin=nullptr;
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
       reinterpret_cast<LPCWSTR>(&threadMain),&pin))return false;
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  if(!event){FreeLibrary(pin);return false;}
  stopping_.store(false,std::memory_order_release);
  pin_=pin;event_=event;
  thread_=CreateThread(nullptr,0,threadMain,this,0,nullptr);
  if(!thread_){CloseHandle(event_);event_=nullptr;pin_=nullptr;FreeLibrary(pin);return false;}
  return true;
 }
public:
 ShadowGraphWorker()=default;
 ShadowGraphWorker(const ShadowGraphWorker&)=delete;
 ShadowGraphWorker&operator=(const ShadowGraphWorker&)=delete;
 ~ShadowGraphWorker(){if(thread_)CloseHandle(thread_);if(event_)CloseHandle(event_);}
 void invalidate(){
  epoch_.fetch_add(1,std::memory_order_acq_rel);
  deliveryOrder_.reset();requestedAt_=0;
  if(event_)SetEvent(event_);
 }
 // At most one shadow graph per second. The game thread does not wait for
 // a worker read or an in-flight capture; a busy mailbox simply skips a tick.
 bool request(const Key&key,uint32_t demand,uint32_t now){
  // A completed disconnect may be followed by a fresh host connection.
  // Reap the old thread first; never rearm while its DLL-pin is active.
  reap();
  if(stopping_.load(std::memory_order_acquire)&&!thread_)
   stopping_.store(false,std::memory_order_release);
  if(stopping_.load(std::memory_order_acquire)||!key.match||
      !key.pid||!key.scenario||!key.house||!key.cells||
      (requestedAt_&&uint32_t(now-requestedAt_)<1000))return false;
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  if(!lock.owns_lock())return false;
  if(!launch())return false;
  queued_={key,demand,epoch_.load(std::memory_order_acquire)};
  queuedReady_=true;requestedAt_=now;
  SetEvent(event_);return true;
 }
 bool poll(const Key&current,BackgroundTrialResult&out){
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  if(!lock.owns_lock()||!resultReady_)return false;
  const auto candidate=result_;const auto resultEpoch=resultEpoch_;
  resultReady_=false;
  if(resultEpoch!=epoch_.load(std::memory_order_acquire))return false;
  if(!deliveryOrder_.admit(candidate,current))return false;
  out=candidate;return true;
 }
 // Nonblocking stop: Runtime cannot unload/unmap while any pinned worker
 // continues scanning. Do not join from the game's message callback.
 bool stop(){
  stopping_.store(true,std::memory_order_release);
  invalidate();
  if(event_)SetEvent(event_);
  if(!thread_)return true;
  if(WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0)return false;
  reap();return true;
 }
};
} // namespace worldframe
