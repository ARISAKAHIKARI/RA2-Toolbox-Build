#pragma once
#include "world_frame_capture.h"
#include "world_frame_command_policy.h"
#include "world_frame_command_index.h"
#include "world_frame_publication.h"
#include "world_frame_transport.h"
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
 static constexpr DWORD MaxWorkMs=120,MaxHandoffMs=80,MaxCandidateAgeMs=500;
 struct Work {Key key{};uint32_t demand=Base,epoch=0,ticket=0;uint64_t requestRevision=0;bool globalVehicles=false;std::vector<uint32_t>pages;};
 std::mutex gate_;
 std::atomic<uint32_t>epoch_{1},latestTicket_{1};
 std::atomic<bool>stopping_{false},capturing_{false};
 HANDLE thread_=nullptr,event_=nullptr;
 HMODULE module_=nullptr;
 Work queued_{};
 Snapshot staging_,completed_;
 CommandRoster stagingRoster_,completedRoster_;
 CaptureScratch scratch_;
 Key completedKey_{};
 uint32_t completedDemand_=0,completedEpoch_=0;uint64_t completedRevision_=0;
 bool queuedReady_=false,completedReady_=false,completedExact_=false;
 DWORD lastRequestedAt_=0;
 Key lastRequestedKey_{};
 uint32_t lastRequestedDemand_=0;uint64_t lastRequestedRevision_=0;bool lastRequestedGlobal_=false;
 std::atomic<uint32_t>accepted_{0},rejected_{0};
 // Diagnostic counters are written only by the worker. The Game callback
 // loads atomics at most 4x/s, never locks gate_, waits, or touches RPM.
 std::atomic<uint32_t>jobsStarted_{0},jobsPublished_{0},jobsExact_{0},jobsCandidate_{0};
 std::atomic<uint32_t>stale_{0},worldChanged_{0},budget_{0},rosterFailed_{0},incomplete_{0};
 std::atomic<uint32_t>lastDurationMs_{0},maxDurationMs_{0},lastCalls_{0},lastBytes_{0},lastPages_{0};
 std::atomic<int32_t>lastFirstFrame_{-1},lastLastFrame_{-1};
 // One worker also publishes sealed full graphs directly into the existing
 // Host shared-memory transport. No 3MB copy, RPM, or page traversal occurs
 // on the game callback. This diagnostic snapshot is acquire/release stable:
 // publish serial is stored last, after all corresponding counters.
 uint32_t hostSequence_=0; // worker-thread only
 std::atomic<uint32_t>hostPublishedSerial_{0},hostPublishedPages_{0},
                       hostPublishedCalls_{0},hostPublishedBytes_{0},
                       hostPublishedMatch_{0};
 std::atomic<int32_t>hostPublishedFrame_{-1};
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
  // PublicationOnly never retires the control-side observed identity. It
  // shares the existing double-buffer data channel with Host readers.
  // In async mode the old exact-frame publisher does not write this channel.
  {
   Server hostWriter(Server::Role::PublicationOnly);
   for(;;){
   if(self->stopping_.load(std::memory_order_acquire))break;
   if(WaitForSingleObject(self->event_,50)==WAIT_FAILED)break;
   if(self->stopping_.load(std::memory_order_acquire))break;
   Work job;bool found=false;
   {
    std::lock_guard<std::mutex>lock(self->gate_);
    if(self->queuedReady_){
     job=std::move(self->queued_);self->queuedReady_=false;
     self->capturing_.store(true,std::memory_order_release);found=true;
    }
   }
   // Every aborted job releases its busy latch.
   struct CaptureGuard {
    CommandCaptureWorker*worker;bool engaged;
    ~CaptureGuard(){if(engaged)worker->capturing_.store(false,std::memory_order_release);}
   } running{self,found};
   if(!found||job.epoch!=self->epoch_.load(std::memory_order_acquire)||
      job.ticket!=self->latestTicket_.load(std::memory_order_acquire))continue;
   // All requested page sorting, dedup, real RPM and graph traversal happen
   // exclusively here. No worker ever reads Runtime Shared/House structures.
   // Equivalent ascending page set, O(4N), retained worker-only buffer.
   sortUniquePageAddresses(job.pages,self->scratch_.warmSortScratch);
   const DWORD began=GetTickCount();unsigned calls=0;size_t bytes=0;
   ++self->jobsStarted_;
   // A worker started after its 80ms strict command deadline may still
   // produce a useful *candidate* world index. Do not throw the whole graph
   // away merely because the game frame advanced while the worker was busy.
   if(uint32_t(began-job.key.tick)>MaxCandidateAgeMs){
    ++self->stale_;++self->rejected_;continue;
   }
   const Key before=currentKey(job.key);
   if(!before.pid||!before.sameWorld(job.key)){
    ++self->worldChanged_;++self->rejected_;continue;
   }
   self->staging_.reset(before);
   const auto checkedRead=[&](uint32_t at,void*out,size_t n){
    if(self->stopping_.load(std::memory_order_acquire)||
       job.epoch!=self->epoch_.load(std::memory_order_acquire)||
       job.ticket!=self->latestTicket_.load(std::memory_order_acquire)||
       uint32_t(GetTickCount()-began)>MaxWorkMs||
       calls>=MaxCalls||
       n>MaxBytes-bytes)return false;
    ++calls;bytes+=n;return live(at,out,n);
   };
   CaptureGraph<decltype(checkedRead)>graph(self->staging_,checkedRead,self->scratch_);
   const bool complete=graph.capture(job.demand,job.pages);
   if(!complete)++self->incomplete_;
   const Key after=currentKey(job.key);
   const bool stable=before.sameWorld(after)&&before.frame<=after.frame&&
       uint32_t(after.tick-began)<=MaxWorkMs;
   // An optional combat/Host page can fail while the complete Techno UID
   // index is still readable. Keep such a graph ONLY as an observation;
   // incomplete graphs never pass strict native command authority.
   const bool exact=complete&&stable&&before.sameFrame(job.key)&&before.sameFrame(after)&&
       uint32_t(after.tick-job.key.tick)<=MaxHandoffMs;
   const bool sealed=self->staging_.seal();
   if(!stable||!sealed||self->staging_.overflow()||
      job.epoch!=self->epoch_.load(std::memory_order_acquire)||
      job.ticket!=self->latestTicket_.load(std::memory_order_acquire)){
    if(uint32_t(GetTickCount()-began)>MaxWorkMs||self->staging_.overflow())
     ++self->budget_;
    else if(!before.sameWorld(after))++self->worldChanged_;
    ++self->rejected_;continue;
   }
   // Build the full identity hash on this SAME worker. A missing UID or
   // unreadable roster is UNKNOWN, not an empty world or an owned actor.
   const bool rosterBuilt=self->stagingRoster_.build(self->staging_,
                           job.globalVehicles,self->scratch_.list);
   const bool overtime=uint32_t(GetTickCount()-before.tick)>MaxWorkMs;
   if(!rosterBuilt||overtime){
    if(!rosterBuilt)++self->rosterFailed_;
    if(overtime)++self->budget_;
    ++self->rejected_;continue;
   }
   // The worker has the *only* mutable ownership of staging_.pages here.
   // Copy the already sealed, fully indexed graph directly into shared IPC
   // on this worker. This fixes pre.6 where ~60 completed worker graphs/sec
   // produced almost zero Host publications and starved reservation panels.
   // The original source frame/tick is preserved; NO newer game identity is
   // fabricated, and the observational bit cannot become native authority.
   if(complete&&uint32_t(GetTickCount()-before.tick)<=250&&
      !self->stopping_.load(std::memory_order_acquire)&&
      job.epoch==self->epoch_.load(std::memory_order_acquire)&&
      job.ticket==self->latestTicket_.load(std::memory_order_acquire)&&
      hostWriter.wanted(job.key.owner,GetTickCount())&&
      hostWriter.observes(before)){
    Publication publication{};
    publication.key=before;
    if(!++self->hostSequence_)++self->hostSequence_;
    publication.key.serial=self->hostSequence_;
    publication.key.demand=ObservationOnlyKeyBit|
       (uint32_t(job.requestRevision>>32)&~(BoundaryOnlyKeyBit|ObservationOnlyKeyBit));
    publication.counters=self->staging_.counters;
    publication.pages={self->staging_.pages.data(),
                       uint32_t(self->staging_.pages.size())};
    const bool copied=hostWriter.publish(publication,[&]{
      return !self->stopping_.load(std::memory_order_acquire)&&
        job.epoch==self->epoch_.load(std::memory_order_acquire)&&
        job.ticket==self->latestTicket_.load(std::memory_order_acquire)&&
        hostWriter.observes(before);
    });
    if(copied){
     self->hostPublishedPages_.store(publication.pages.count,std::memory_order_relaxed);
     self->hostPublishedCalls_.store(calls,std::memory_order_relaxed);
     self->hostPublishedBytes_.store(uint32_t(bytes),std::memory_order_relaxed);
     self->hostPublishedFrame_.store(before.frame,std::memory_order_relaxed);
     self->hostPublishedMatch_.store(before.match,std::memory_order_relaxed);
     self->hostPublishedSerial_.store(publication.key.serial,std::memory_order_release);
    }
   }
   {
    std::lock_guard<std::mutex>lock(self->gate_);
    if(job.epoch!=self->epoch_.load(std::memory_order_acquire)||
       job.ticket!=self->latestTicket_.load(std::memory_order_acquire)||
       self->stopping_.load(std::memory_order_acquire))continue;
    self->staging_.swap(self->completed_); // page ownership, not page copies
    self->stagingRoster_.swap(self->completedRoster_); // hash owner swap, no 16K rehash
    self->completedKey_=before;self->completedDemand_=job.demand;
    self->completedEpoch_=job.epoch;self->completedRevision_=job.requestRevision;
    self->completedExact_=exact;self->completedReady_=true;
    ++self->jobsPublished_;
    if(exact)++self->jobsExact_;else ++self->jobsCandidate_;
    const uint32_t ms=uint32_t(GetTickCount()-began);
    self->lastDurationMs_.store(ms,std::memory_order_relaxed);
    self->maxDurationMs_.store(std::max(ms,self->maxDurationMs_.load(std::memory_order_relaxed)),
                               std::memory_order_relaxed);
    self->lastCalls_.store(calls,std::memory_order_relaxed);
    self->lastBytes_.store(uint32_t(bytes),std::memory_order_relaxed);
    self->lastPages_.store(uint32_t(self->completed_.pages.size()),std::memory_order_relaxed);
    self->lastFirstFrame_.store(before.frame,std::memory_order_relaxed);
    self->lastLastFrame_.store(after.frame,std::memory_order_relaxed);
   }
   }
  } // Close publication-only mappings BEFORE releasing our pinned DLL.
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
 // Async command-world capture is the production default. Explicitly set
 // RA2TOOLBOX_ASYNC_COMMAND_CAPTURE=0 before launching to restore the prior
 // synchronous capture route on platforms with poor exact-frame handoff.
 // Late/stale results are never native EventClass authorization.
 static bool enabled(){
#ifdef WORLD_FRAME_TEST_SYNC_CAPTURE
  return false;
#else
  return true;
#endif
 }
 // The dispatcher calls this BEFORE assembling up to 16K page addresses.
 // A busy same-world request can be retained without making any stale result
 // authoritative. Both the lock and read-only check are nonblocking.
 bool deferBusy(const Key&key,uint32_t demand,uint64_t revision,bool globalVehicles=false){
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  // A contended worker gate is not permission to do another 16K-page
  // dependency-list assembly on the game callback. A request cannot be
  // submitted under this gate anyway: postpone it and try again next wake.
  // This is ONLY request admission; take() remains strictly exact-frame.
  if(!lock.owns_lock())return true;
  if(stopping_.load(std::memory_order_acquire)){
   // Critical lifecycle boundary: a stopped worker MUST be restartable.
   // If the pinned old DLL thread is still draining, do not rebuild a huge
   // page list just to fail request(). Once it exits (or was never created),
   // allow request() to perform its zero-wait reap()/launch() instead of
   // coalescing forever after a new game/session begins.
   return thread_&&WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0;
  }
  const bool outstanding=queuedReady_||capturing_.load(std::memory_order_acquire);
  if(coalesceInFlightWorld(lastRequestedKey_,key,lastRequestedDemand_,demand,
      lastRequestedRevision_,revision,lastRequestedGlobal_,globalVehicles,
      uint32_t(GetTickCount()-lastRequestedAt_),outstanding,MaxWorkMs))return true;
  // Even after the coalescing deadline, do not rebuild 16K dependencies
  // while a still-running same-world scan is exhausting its own budget.
  // Demand revisions and optional global-vehicle mode can change while the
  // old worker is already copying pages. Keep that *candidate* read alive;
  // the next request after completion will include the new requirements.
  // Strict native take() still checks demand, revision and global mode.
  return capturing_.load(std::memory_order_acquire)&&
      lastRequestedKey_.sameWorld(key);
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
  // Coalesce only busy same-world work, NEVER the completed graph's authority.
  // take() still checks matching simulation frame before any native EventClass.
  const bool outstanding=queuedReady_||capturing_.load(std::memory_order_acquire);
  if(coalesceInFlightWorld(lastRequestedKey_,key,lastRequestedDemand_,demand,
     lastRequestedRevision_,revision,lastRequestedGlobal_,globalVehicles,
     uint32_t(now-lastRequestedAt_),outstanding,MaxWorkMs))return true;
  // Do not continuously cancel a full-world read after each 16ms game tick.
  // A same-world in-flight scan is allowed to finish into a candidate.
  if(capturing_.load(std::memory_order_acquire)&&
     lastRequestedKey_.sameWorld(key))return true;
  if(!launch())return false;
  // A changed world/demand/revision/global bit supersedes the running ticket.
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
 bool take(const Key&current,uint32_t demand,uint64_t revision,Snapshot&into,CommandRoster*roster=nullptr,bool globalVehicles=false){
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  if(!lock.owns_lock()||!completedReady_)return false;
  if(completedEpoch_!=epoch_.load(std::memory_order_acquire)||
     !completedExact_||
     !acceptCommandCapture(completedKey_,current,completedDemand_,demand,
                           completedRevision_,revision,GetTickCount(),MaxHandoffMs)){
   // The completed immutable graph may still be an observational candidate.
   // Do not discard it here; takeObservation() has its own provenance test.
   return false;
  }
  // A second same-frame request cannot reuse an already taken sample:
  // completed_ now holds only the previous game thread's owned storage.
  // Global candidate demand is an independent, same-frame configuration bit.
  // A mode switch cannot adopt a graph indexed under the previous setting.
  if(completedRoster_.global!=globalVehicles||
     (roster&&!completedRoster_.current(completedKey_))){
   completedReady_=false;++rejected_;return false;
  }
  into.swap(completed_);
  if(roster)roster->swap(completedRoster_);
  completedReady_=false;++accepted_;return true;
 }
 // Returns a candidate only; this API MUST NOT be used as EventClass
 // authority. A candidate can contain pages sampled across frame boundaries.
 // The dispatcher may use its UID index for discovery, with mandatory live
 // per-object validation at the native submission gateway.
 bool takeObservation(const Key&current,uint32_t requiredDemand,
                      Snapshot&into,CommandRoster&roster,bool globalVehicles=false){
  std::unique_lock<std::mutex>lock(gate_,std::try_to_lock);
  if(!lock.owns_lock()||!completedReady_)return false;
  if(completedEpoch_!=epoch_.load(std::memory_order_acquire)||
     !acceptObservationCandidate(completedKey_,current,completedDemand_,
                                  requiredDemand,GetTickCount(),MaxCandidateAgeMs)||
     completedRoster_.global!=globalVehicles||
     !completedRoster_.current(completedKey_)){
   completedReady_=false;++rejected_;return false;
  }
  into.swap(completed_);
  roster.swap(completedRoster_);
  completedReady_=false;++accepted_;return true;
 }
 void invalidate(){
  epoch_.fetch_add(1,std::memory_order_acq_rel);
  latestTicket_.fetch_add(1,std::memory_order_acq_rel);
  lastRequestedAt_=0;lastRequestedKey_={};lastRequestedDemand_=lastRequestedRevision_=0;lastRequestedGlobal_=false;
  if(event_)SetEvent(event_);
 }
 struct HostPublication {
  uint32_t serial=0,pages=0,calls=0,bytes=0,match=0;
  int32_t frame=-1;
 };
 HostPublication hostPublication()const{
  HostPublication data{};
  for(unsigned n=0;n<2;++n){
   const uint32_t before=hostPublishedSerial_.load(std::memory_order_acquire);
   data.pages=hostPublishedPages_.load(std::memory_order_relaxed);
   data.calls=hostPublishedCalls_.load(std::memory_order_relaxed);
   data.bytes=hostPublishedBytes_.load(std::memory_order_relaxed);
   data.frame=hostPublishedFrame_.load(std::memory_order_relaxed);
   data.match=hostPublishedMatch_.load(std::memory_order_relaxed);
   const uint32_t after=hostPublishedSerial_.load(std::memory_order_acquire);
   if(before==after){data.serial=after;return data;}
  }
  return {};
 }
 struct Diagnostics {
  uint32_t started=0,published=0,exact=0,candidate=0;
  uint32_t stale=0,worldChanged=0,budget=0,rosterFailed=0,incomplete=0,rejected=0;
  uint32_t lastDurationMs=0,maxDurationMs=0,lastCalls=0,lastBytes=0,lastPages=0;
  int32_t firstFrame=-1,lastFrame=-1;
 };
 Diagnostics diagnostics()const{
  const auto order=std::memory_order_relaxed;
  return {jobsStarted_.load(order),jobsPublished_.load(order),jobsExact_.load(order),
   jobsCandidate_.load(order),stale_.load(order),worldChanged_.load(order),
   budget_.load(order),rosterFailed_.load(order),incomplete_.load(order),
   rejected_.load(order),lastDurationMs_.load(order),maxDurationMs_.load(order),
   lastCalls_.load(order),lastBytes_.load(order),lastPages_.load(order),
   lastFirstFrame_.load(order),lastLastFrame_.load(order)};
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
