// Real production Graph sampled off-thread. The candidate is approximate:
// same game, bounded age and monotonic delivery; native commands still require
// independent real-time authoritative admission.
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
 std::atomic<uint32_t>tick{100};
 std::thread::id gameThread=std::this_thread::get_id(),captureThread;
 auto boundary=[&]{auto k=key(frame.load(std::memory_order_acquire));k.tick=tick.load(std::memory_order_acquire);return k;};
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
 worldframe::BackgroundGraphOrder order;
 assert(order.admit(result,boundary()));
 assert(!order.admit(result,boundary())); // duplicate observation
 std::printf("PASS background full graph: %u pages, %u native reads, %u bytes, zero main-thread graph calls\n",
   result.pages,result.counters.calls,result.counters.bytes);

 // Long-lived production worker must reuse allocation capacity but NEVER
 // reuse previously observed bytes. Same frame counter cannot make a cached
 // HP value valid: every new graph observation reacquires mutable fields.
 {
  worldframe::Snapshot retained;
  worldframe::CaptureScratch scratch;
  auto first=worldframe::backgroundTrialWithStorage(boundary,workerRead,worldframe::Base,retained,scratch);
  assert(first.status==BackgroundTrialStatus::CandidateOnly);
  const auto*allocation=retained.pages.data();
  const auto capacity=retained.pages.capacity();
  assert(allocation&&capacity>=retained.pages.size());
  int hp=0;
  assert(retained.read(ActorA+0x6C,&hp,sizeof hp)&&hp==100);
  memory.put(ActorA+0x6C,55);
  auto second=worldframe::backgroundTrialWithStorage(boundary,workerRead,worldframe::Base,retained,scratch);
  assert(second.status==BackgroundTrialStatus::CandidateOnly);
  assert(retained.pages.data()==allocation&&retained.pages.capacity()==capacity);
  assert(retained.read(ActorA+0x6C,&hp,sizeof hp)&&hp==55);
  memory.put(ActorA+0x6C,100);
 }

 // One-frame movement DURING capture is fine for non-command discovery.
 // The order gate still rejects an old result that arrives after a new one.
 bool switched=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(!switched&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){
   switched=true;frame.fetch_add(1,std::memory_order_release);
  }
 };
 std::thread changed([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 changed.join();memory.beforeRead={};
 assert(switched&&result.status==BackgroundTrialStatus::CandidateOnly);
 assert(result.before.frame==50&&result.after.frame==51);
 assert(order.admit(result,boundary()));
 auto prior=result;prior.after.frame=50;
 assert(!order.admit(prior,boundary()));
 assert(!result.usableForCommands());
 // Same-frame updates must have a strictly newer observation time.
 auto sameFrame=result;sameFrame.before.tick=101;sameFrame.after.tick=101;
 tick.store(105);
 assert(order.admit(sameFrame,boundary()));
 assert(!order.admit(sameFrame,boundary()));
 // A delivery 161 ms late does not pass despite the game/world still matching.
 auto stale=result;
 tick.store(300);
 assert(!order.admit(stale,boundary()));
 tick.store(105);

 // World/match/generation changes remain hard boundaries even across one
 // frame. A different game's roster must never become a candidate.
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
 assert(!order.admit(result,generationBoundary()));

 // Excessive capture age or a frame rewind is not an approximate live graph.
 bool delayed=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(!delayed&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){
   delayed=true;tick.fetch_add(121,std::memory_order_release);
  }
 };
 std::thread slow([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 slow.join();memory.beforeRead={};
 assert(delayed&&result.status==BackgroundTrialStatus::TooOld);
 tick.store(105);
 bool rewound=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(!rewound&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){
   rewound=true;frame.fetch_sub(1,std::memory_order_release);
  }
 };
 std::thread backward([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 backward.join();memory.beforeRead={};
 assert(rewound&&result.status==BackgroundTrialStatus::OutOfOrder);
 frame.store(51);

 // Too many frames during capture are stale even if 120 ms has not elapsed.
 bool raced=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(!raced&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){
   raced=true;frame.fetch_add(7,std::memory_order_release);
  }
 };
 std::thread skipped([&]{result=worldframe::backgroundTrial(boundary,workerRead,worldframe::Base);});
 skipped.join();memory.beforeRead={};
 assert(raced&&result.status==BackgroundTrialStatus::TooOld);
 frame.store(51);

 // Blocking the background graph must NOT block the game-thread clock/world
 // checks. Only the worker waits; no engine lock is acquired.
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
 memory.beforeRead=[&](uint32_t p,size_t n){
  if(p<=0xA80208u&&uint64_t(p)+n>0xA80208u){
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

 // A fresh candidate from a NEW game can pass only if a current stamp
 // corroborates that same new game. Older outstanding jobs stay rejected.
 auto newWorld=result;
 newWorld.before.generationLow=2;newWorld.after.generationLow=2;
 auto newCurrent=boundary();newCurrent.generationLow=2;
 assert(order.admit(newWorld,newCurrent));
 assert(!order.admit(result,newCurrent));
 std::puts("PASS approximate Graph: cross-frame allowed, same-game/age/order enforced, worker isolation, no command adoption");
}
