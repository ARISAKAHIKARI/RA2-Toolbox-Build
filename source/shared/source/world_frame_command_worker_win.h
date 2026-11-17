#pragma once
#include "world_frame_capture.h"
#include "world_frame_command_policy.h"
#include "world_frame_command_index.h"
#include <windows.h>
#include <atomic>
#include <mutex>
#include <vector>
#include <algorithm>

// Experimental command *capture*, not an executor. ReadProcessMemory and the
// full CaptureGraph run only on this low-priority pinned thread. The original
// game dispatcher alone can adopt an EXACT-FRAME sealed sample and validate
// native order eligibility; a late/missing result cannot make a command valid.
namespace worldframe {
class CommandCaptureWorker {
 static constexpr unsigned MaxCalls=8192;
 static constexpr size_t MaxBytes=64u*1024u*1024u;
 static constexpr DWORD MaxWorkMs=120,MaxHandoffMs=80;
 struct Work {Key key{};uint32_t demand=Base,epoch=0,ticket=0;uint64_t requestRevision=0;bool globalVehicles=false;std::vector<uint32_t>pages;};
 std::mutex gate_;
 std::atomic<uint32_t>epoch_{1},latestTicket_{1};
 std::atomic<bool>stopping_{false};
 HANDLE thread_=nullptr,event_=nullptr;
 HMODULE module_=nullptr;
 Work queued_{};
 Snapshot staging_,completed_;
 CommandRoster stagingRoster_,completedRoster_;
 CaptureScratch scratch_;
 Key completedKey_{};
 uint32_t completedDemand_=0,completedEpoch_=0;uint64_t completedRevision_=0;
 bool queuedReady_=false,completedReady_=false;
 DWORD lastRequestedAt_=0;
 Key lastRequestedKey_{};
 uint32_t lastRequestedDemand_=0;uint64_t lastRequestedRevision_=0;bool lastRequestedGlobal_=false;
 std::atomic<uint32_t>accepted_{0},rejected_{0};
 static bool live(uint32_t address,void*out,size_t bytes){
  if(!out||!range(address,bytes))return false;
  SIZE_T got=0;
  return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<LPCVOID>(uintptr_t(address)),out,bytes,&got)&&got==bytes;
 }
 static Key currentKey(const Key&identity){
  Key candidate=identity;uint32_t scenario=0,cells=0,house=0;int count=-1,frame=-1,session=-1;
  uint8_t running=0;
  if(!live(0xA8B230,&scenario,4)||!live(0x87F924,&cells,4)||
     !live(0x87F928,&count,4)||!live(0xA8ED84,&frame,4)||
     !live(0xA8B238,&session,4)||!live(0xA8E9A0,&running,1)||
     !live(0xA83D4C,&house,4)||running!=1||
     scenario<0x10000||cells<0x10000||house<0x10000||
     count<=0||count>512*512||frame<0||
     !(session==0||session==3||session==4||session==5)){
   candidate.pid=0;return candidate;
  }
  candidate.scenario=scenario;candidate.cells=cells;candidate.cellCount=uint32_t(count);
  candidate.house=house;candidate.frame=frame;candidate.session=session;
  candidate.tick=GetTickCount();return candidate;
 }
 static DWORD WINAPI run(void*opaque){
  auto*self=static_cast<CommandCaptureWorker*>(opaque);
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
  for(;;){
   if(self->stopping_.load(std::memory_order_acquire))break;
   if(WaitForSingleObject(self->event_,50)==WAIT_FAILED)break;
   if(self->stopping_.load(std::memory_order_acquire))break;
   Work job;bool found=false;
   {
    std::lock_guard<std::mutex>lock(self->gate_);
    if(self->queuedReady_){job=std::move(self->queued_);self->queuedReady_=false;found=true;}
   }
   if(!found||job.epoch!=self->epoch_.load(std::memory_order_acquire)||
      job.ticket!=self->latestTicket_.load(std::memory_order_acquire))continue;
   // All requested page sorting, dedup, real RPM and graph traversal happen
   // exclusively here. No worker ever reads Runtime Shared/House structures.
   std::sort(job.pages.begin(),job.pages.end());
   job.pages.erase(std::unique(job.pages.begin(),job.pages.end()),job.pages.end());
   const DWORD began=GetTickCount();unsigned calls=0;size_t bytes=0;
   const Key before=currentKey(job.key);
   if(!before.pid||!before.sameFrame(job.key)){++self->rejected_;continue;}
   self->staging_.reset(job.key);
   const auto checkedRead=[&](uint32_t at,void*out,size_t n){
    if(self->stopping_.load(std::memory_order_acquire)||
       job.epoch!=self->epoch_.load(std::memory_order_acquire)||
       job.ticket!=self->latestTicket_.load(std::memory_order_acquire)||
       uint32_t(GetTickCount()-began)>MaxWorkMs||calls>=MaxCalls||
       n>MaxBytes-bytes)return false;
    ++calls;bytes+=n;return live(at,out,n);
   };
   CaptureGraph<decltype(checkedRead)>graph(self->staging_,checkedRead,self->scratch_);
   const bool complete=graph.capture(job.demand,job.pages);
   const Key after=currentKey(job.key);
   const bool stable=before.sameFrame(after)&&after.sameFrame(job.key)&&
       uint32_t(after.tick-began)<=MaxWorkMs;
   const bool sealed=self->staging_.seal();
   if(!complete||!stable||!sealed||self->staging_.overflow()||
      job.epoch!=self->epoch_.load(std::memory_order_acquire)||
      job.ticket!=self->latestTicket_.load(std::memory_order_acquire)){
    ++self->rejected_;continue;
   }
   // Build the full identity hash on this SAME worker. A missing UID or
   // unreadable roster is UNKNOWN, not an empty world or an owned actor.
   if(!self->stagingRoster_.build(self->staging_,job.globalVehicles,
                                  self->scratch_.list)){
    ++self->rejected_;continue;
   }
   {
    std::lock_guard<std::mutex>lock(self->gate_);
    if(job.epoch!=self->epoch_.load(std::memory_order_acquire)||
       job.ticket!=self->latestTicket_.load(std::memory_order_acquire)||
       self->stopping_.load(std::memory_order_acquire))continue;
    self->staging_.swap(self->completed_); // page ownership, not page copies
    self->stagingRoster_.swap(self->completedRoster_); // hash owner swap, no 16K rehash
    self->completedKey_=job.key;self->completedDemand_=job.demand;
    self->completedEpoch_=job.epoch;self->completedRevision_=job.requestRevision;self->completedReady_=true;
   }
  }
  // Every Snapshot/worker field lives until the Runtime acknowledges exit.
  const HMODULE pin=self->module_;
  FreeLibraryAndExitThread(pin,0);return 0;
 }
 void reap(){
  if(!thread_||WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0)return;
  CloseHandle(thread_);if(event_)CloseHandle(event_);
  thread_=event_=nullptr;module_=nullptr;
 }
 bool launch(){
  reap();
  if(thread_)return !stopping_.load(std::memory_order_acquire);
  HMODULE module=nullptr;
  if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
       reinterpret_cast<LPCWSTR>(&run),&module))return false;
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  if(!event){FreeLibrary(module);return false;}
  stopping_.store(false,std::memory_order_release);
  module_=module;event_=event;
  thread_=CreateThread(nullptr,0,run,this,0,nullptr);
  if(!thread_){CloseHandle(event_);event_=nullptr;module_=nullptr;FreeLibrary(module);return false;}
  return true;
 }
public:
 CommandCaptureWorker()=default;
 CommandCaptureWorker(const CommandCaptureWorker&)=delete;
 CommandCaptureWorker&operator=(const CommandCaptureWorker&)=delete;
 ~CommandCaptureWorker(){if(thread_)CloseHandle(thread_);if(event_)CloseHandle(event_);}
 // Experimental admission deliberately requires an explicit opt-in while
 // the x86 native and live multiplayer latency/command-success probes run.
 static bool enabled(){
  static const bool value=[]{
   char name[3]{};
   return GetEnvironmentVariableA("RA2TOOLBOX_ASYNC_COMMAND_CAPTURE",name,sizeof name)==1&&name[0]=='1';
  }();
  return value;
 }
 bool request(const Key&key,uint32_t demand,uint64_t revision,const std::vector<uint32_t>&pages,bool globalVehicles=false){
  if(!key.pid||!key.owner||!key.match||key.house<0x10000||
     !key.scenario||!key.cells||key.frame<0||!demand||
     pages.size()>MaxPages)return false;
  const DWORD now=GetTickCount();
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  if(!lock.owns_lock())return false;
  reap(); // A prior stop may have completed after the previous connection.
  if(stopping_.load(std::memory_order_acquire)){
   if(thread_)return false; // The old pinned worker has not exited yet.
   stopping_.store(false,std::memory_order_release);
  }
  if(lastRequestedKey_.sameFrame(key)&&lastRequestedDemand_==demand&&lastRequestedRevision_==revision&&lastRequestedGlobal_==globalVehicles&&
      uint32_t(now-lastRequestedAt_)<16)return true;
  if(!launch())return false;
  // Invalidate a different in-flight frame/House/demand immediately; do
  // not let a 120ms old graph monopolize the worker while a new frame waits.
  // Repeated requests for the SAME work retain its ticket to avoid starvation.
  const bool same=lastRequestedKey_.sameFrame(key)&&
      lastRequestedDemand_==demand&&lastRequestedRevision_==revision&&lastRequestedGlobal_==globalVehicles;
  if(!same)latestTicket_.fetch_add(1,std::memory_order_acq_rel);
  queued_.key=key;queued_.demand=demand;queued_.requestRevision=revision;queued_.globalVehicles=globalVehicles;
  queued_.ticket=latestTicket_.load(std::memory_order_acquire);
  // This O(N) address copy is at most 64 KiB. Sorting, graph traversal,
  // missing-page probing and ALL object bytes are worker-only.
  queued_.pages.assign(pages.begin(),pages.end());
  queued_.epoch=epoch_.load(std::memory_order_acquire);
  queuedReady_=true;lastRequestedAt_=now;lastRequestedKey_=key;
  lastRequestedDemand_=demand;lastRequestedRevision_=revision;lastRequestedGlobal_=globalVehicles;
  SetEvent(event_);return true;
 }
 bool take(const Key&current,uint32_t demand,uint64_t revision,Snapshot&into,CommandRoster*roster=nullptr){
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  if(!lock.owns_lock()||!completedReady_)return false;
  if(completedEpoch_!=epoch_.load(std::memory_order_acquire)||
     !acceptCommandCapture(completedKey_,current,completedDemand_,demand,
                           completedRevision_,revision,GetTickCount(),MaxHandoffMs)){
   completedReady_=false;++rejected_;return false;
  }
  // A second same-frame request cannot reuse an already taken sample:
  // completed_ now holds only the previous game thread's owned storage.
  if(roster&&!completedRoster_.current(completedKey_)){
   completedReady_=false;++rejected_;return false;
  }
  into.swap(completed_);
  if(roster)roster->swap(completedRoster_);
  completedReady_=false;++accepted_;return true;
 }
 void invalidate(){
  epoch_.fetch_add(1,std::memory_order_acq_rel);
  latestTicket_.fetch_add(1,std::memory_order_acq_rel);
  lastRequestedAt_=0;lastRequestedKey_={};lastRequestedDemand_=lastRequestedRevision_=0;lastRequestedGlobal_=false;
  if(event_)SetEvent(event_);
 }
 uint32_t accepted()const{return accepted_.load(std::memory_order_relaxed);}
 uint32_t rejected()const{return rejected_.load(std::memory_order_relaxed);}
 // Poll, never join a worker from GameRuntime's message callback. DLL pin is
 // deliberately retained until worker exits after its own outstanding reads.
 bool stop(){
  stopping_.store(true,std::memory_order_release);invalidate();
  if(!thread_)return true;
  if(WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0)return false;
  reap();return true;
 }
};
} // namespace worldframe
