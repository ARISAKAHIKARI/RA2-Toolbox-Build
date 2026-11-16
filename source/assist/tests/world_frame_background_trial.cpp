// Test real CaptureGraph acquisition on a worker thread; only original game
// read() and world-boundary checks are synthetic. Nothing reaches EventClass.
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
#include "../../shared/source/world_frame_background_trial.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cassert>
#include <cstdio>
int main(){
 using namespace fixture;
 using worldframe::BackgroundTrialStatus;
 Memory memory;setup(memory);
 std::atomic<int>frame{50};
 std::thread::id gameThread=std::this_thread::get_id(),captureThread;
 auto boundary=[&]{auto k=key(frame.load(std::memory_order_acquire));return k;};
 auto workerRead=[&](uint32_t p,void*out,size_t n){
  captureThread=std::this_thread::get_id();
  return memory.read(p,out,n);
 };
 worldframe::BackgroundTrialResult result{};
 std::thread worker([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 worker.join();
 assert(captureThread!=gameThread);
 assert(result.status==BackgroundTrialStatus::CandidateOnly&&result.pages>0);
 assert(result.counters.calls>0&&result.frameChecks==2);
 assert(!result.usableForCommands());
 std::printf("PASS background full graph: %u pages, %u native reads, %u bytes, zero main-thread graph calls\n",
   result.pages,result.counters.calls,result.counters.bytes);

 // Frame transition in the middle of the exact real CaptureGraph execution
 // must discard its entire candidate even when both inventories were readable.
 bool switched=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(!switched&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){
   switched=true;frame.fetch_add(1,std::memory_order_release);
  }
 };
 std::thread changed([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 changed.join();memory.beforeRead={};
 assert(switched&&result.status==BackgroundTrialStatus::WorldChanged);
 assert(!result.usableForCommands());
 // An identity/generation transition without a frame change is also rejected.
 std::atomic<uint32_t>generation{1};bool replaced=false;
 auto generationBoundary=[&]{auto k=boundary();k.generationLow=generation.load();return k;};
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(!replaced&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){
   replaced=true;generation.fetch_add(1,std::memory_order_release);
  }
 };
 std::thread changedWorld([&]{result=worldframe::backgroundTrial(generationBoundary,workerRead,worldframe::Base);});
 changedWorld.join();memory.beforeRead={};
 assert(replaced&&result.status==BackgroundTrialStatus::WorldChanged);

 // Blocking the entire background graph must leave game-thread boundary
 // checks available and must not stall the authoritative thread or hold an
 // engine lock. A bounded queue and actual DMA/CPU throttle come later.
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(p==0xA80208u&&n==worldframe::PageSize){
   std::unique_lock<std::mutex>guard(mutex);
   entered=true;cv.notify_one();
   cv.wait(guard,[&]{return release;});
  }
 };
 std::thread blocked([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 {
  std::unique_lock<std::mutex>guard(mutex);
  cv.wait(guard,[&]{return entered;});
 }
 const auto mainRead=boundary();
 assert(mainRead.frame==51);
 {std::lock_guard<std::mutex>guard(mutex);release=true;}
 cv.notify_one();blocked.join();memory.beforeRead={};
 assert(result.status==BackgroundTrialStatus::CandidateOnly);
 assert(!result.usableForCommands());

 // Same frame/identity still CANNOT certify an atomic snapshot. Hooks,
 // input, UI and other threads may update state without incrementing frame.
 auto current=boundary();assert(current.sameFrame(boundary()));
 assert(!result.usableForCommands());
 std::puts("PASS shadow-only capture: frame/epoch rejection, thread isolation, blocked worker never blocks game, no command adoption");
}
