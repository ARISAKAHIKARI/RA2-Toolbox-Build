// Exact servicePlanner extraction, with explicit UI/worker scheduling seams.
// These portable checks do not run the Windows host or a game process.
#include "planner_task.h"
#include "planner_world_pool.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <string>
using namespace assist;
using LONG=int32_t;
static bool countAllocations=false;
static uint64_t worldAllocations=0,worldAllocationBytes=0;
void*operator new(size_t size){
 if(void*p=std::malloc(size)){if(countAllocations&&size>=sizeof(planner::World)&&size<=sizeof(planner::World)+128){++worldAllocations;worldAllocationBytes+=size;}return p;}throw std::bad_alloc();
}
void operator delete(void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}
static void check(bool value,const char*why){if(!value){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
static Shared*shared;static Snapshot snap;static bool enabled=true,commandsFresh=true,diagEnabled=false;
static uint32_t clockNow=10000,plannerAck=0,plannerMicros=0;static uint64_t planningGeneration=1,planningSerial=0;static bool planningBusy=false;
static long worldRevision=-1;static int submittedPlanFrame=0;static planner::Statistics plannerState;static planner::Parameters parameters;
static std::vector<planner::Order> submittedPlan;static std::vector<planner::Receipt> plannerReceipts;
static unsigned wakes=0,staleLogs=0,barriers=0;static std::function<void(unsigned)> barrierMutation;
static uint32_t GetTickCount(){return clockNow;}static bool commandFresh(uint32_t){return commandsFresh;}
static void bindCalculations(){}static void plannerStatistics(){}static planner::Parameters plannerParameters(){return parameters;}
static void diag(const std::wstring&){}static void wakeGame(){++wakes;}static void logPlanning(const planner::World&,const wchar_t*){++staleLogs;}
static void MemoryBarrier(){std::atomic_thread_fence(std::memory_order_seq_cst);++barriers;if(barrierMutation)barrierMutation(barriers);}
static int32_t InterlockedIncrement(volatile int32_t*p){return ++*p;}
struct WorkerSeam {
 planner::TaskInput job;std::shared_ptr<const planner::TaskOutput> result;unsigned submissions=0;
 void submit(planner::TaskInput input){job=std::move(input);result.reset();++submissions;}
 std::shared_ptr<const planner::TaskOutput> get(){return result;}
 void finish(){check(bool(job.world),"finishing absent planner job");auto next=std::make_shared<planner::TaskOutput>();next->generation=job.generation;next->serial=job.serial;next->match=job.world->match;next->frame=job.world->frame;next->tick=job.world->tick;next->parameters=job.parameters;result=std::move(next);}
} plannerWorker;
#include "planner_service_production.h"
static uint64_t digestBytes(uint64_t h,const void*data,size_t n){for(auto*p=static_cast<const uint8_t*>(data);n;--n)h=(h^*p++)*1099511628211ull;return h;}
static uint64_t digestWorld(const planner::World&w){
 const uint32_t header[]={w.match,w.tick,uint32_t(w.frame),uint32_t(w.count),uint32_t(w.intentCount),uint32_t(w.refineryCount),w.inputSerial,uint32_t(w.combatCount)};
 uint64_t h=digestBytes(1469598103934665603ull,header,sizeof header);h=digestBytes(h,w.actors,size_t(w.count)*sizeof(planner::Actor));h=digestBytes(h,w.intents,size_t(w.intentCount)*sizeof(planner::Intent));h=digestBytes(h,w.refineries,size_t(w.refineryCount)*sizeof(planner::Refinery));return digestBytes(h,w.combat,size_t(w.combatCount)*sizeof(planner::CombatProbe));
}
static void reset(){
 plannerWorker={};::new(static_cast<void*>(shared)) Shared{};snap={};snap.status=1;snap.match=7;snap.frame=100;clockNow=10000;
 enabled=commandsFresh=true;planningBusy=false;planningGeneration=1;planningSerial=plannerAck=plannerMicros=0;worldRevision=-1;plannerState={};parameters={};parameters.flags=AutoOreReturn;
 submittedPlan.clear();plannerReceipts.clear();wakes=staleLogs=barriers=0;barrierMutation={};
 shared->computeFrameSeq=2;shared->worldSeq=2;shared->computeFrame=snap.frame;shared->computeFrameMatch=snap.match;
 auto&w=shared->world;w.match=snap.match;w.frame=snap.frame;w.tick=clockNow;w.count=2;w.refineryCount=1;
 auto&a=w.actors[0];a.id=10;a.kind=2;a.type=200;a.flags=planner::Own|planner::Harvester;a.storageLimit=40;a.storage[0]=40;a.mission=10;a.dockTypes[0]=900;a.x=a.y=5000;
 auto&b=w.actors[1];b.id=20;b.kind=3;b.type=900;b.flags=planner::Own;b.x=b.y=8000;w.refineries[0].id=20;w.refineries[0].count=1;w.refineries[0].waiting[0]={9009,0,0};
}
static void nextSource(){++snap.frame;++clockNow;shared->world.frame=snap.frame;shared->world.tick=clockNow;++shared->world.inputSerial;shared->computeFrame=snap.frame;shared->worldSeq+=2;shared->computeFrameSeq+=2;}
static void poolOwnership(){
 planner::WorldPool pool;std::shared_ptr<const planner::World> pins[3];const planner::World*addresses[3]{};
 for(int n=0;n<3;++n){auto writable=pool.acquire();check(bool(writable),"pool did not lazily create bounded slot");writable->match=5;writable->frame=n;writable->count=1;writable->actors[0].id=100+n;addresses[n]=writable.get();pins[n]=std::move(writable);}
 check(addresses[0]!=addresses[1]&&addresses[1]!=addresses[2]&&addresses[0]!=addresses[2]&&!pool.acquire(),"pool reused a pinned slot or exceeded capacity");
 pins[1].reset();auto reused=pool.acquire();check(reused.get()==addresses[1]&&reused->actors[0].id==101,"released slot not reused without clearing");reused->actors[0].id=999;
 check(pins[0]->actors[0].id==100&&pins[2]->actors[0].id==102&&!pool.acquire(),"writing released slot changed an immutable observation");
 // The actual stateful Task retains its World after the caller drops the job.
 planner::WorldPool taskPool;planner::Task task;planner::TaskInput input;input.generation=1;input.serial=1;input.parameters.flags=AntiTheft;
 auto first=taskPool.acquire();first->match=1;first->frame=1;first->tick=1000;first->count=2;
 auto&a=first->actors[0];a.id=10;a.kind=3;a.flags=planner::Own|planner::PointDefense;a.x=a.y=5000;a.range=1024;a.weapons[0]={1024,0,planner::GroundWeapon,0x7ff};auto&b=first->actors[1];b.id=20;b.kind=1;b.flags=planner::Enemy|planner::Saboteur;b.x=5400;b.y=5000;
 const auto firstAddress=first.get();input.world=first;auto result=task.run(input);check(result->orders.size()==1,"Task pin fixture emitted no order");input.world.reset();first.reset();
 auto second=taskPool.acquire();check(second&&second.get()!=firstAddress,"pool overwrote Task's retained World");second->match=1;second->frame=2;second->tick=2000;second->count=0;input.world=second;++input.serial;result=task.run(input);input.world.reset();second.reset();
 auto released=taskPool.acquire();check(released.get()==firstAddress,"previous Task World did not become reusable after replacement");check(result->orders.empty(),"empty Task input consumed retained actor tail");
 std::puts("PASS lazy three-slot bound, immutable concurrent owners, saturation/release and actual Task retained-index ownership");
}
static void serviceGuards(){
 reset();servicePlanner();check(planningBusy&&plannerWorker.submissions==1&&worldRevision==2&&digestWorld(*plannerWorker.job.world)==digestWorld(shared->world),"valid service capture changed active source");
 auto first=plannerWorker.job.world;const auto firstDigest=digestWorld(*first);plannerWorker.finish();nextSource();shared->world.count=1;shared->world.refineryCount=0;servicePlanner();
 check(plannerWorker.submissions==2&&plannerWorker.job.world.get()!=first.get()&&plannerWorker.job.world->count==1&&digestWorld(*first)==firstDigest,"next service capture overwrote pinned input or consumed a stale tail");
 plannerWorker.finish();shared->worldSeq+=2;++shared->world.inputSerial;shared->world.count=2;servicePlanner();check(plannerWorker.submissions==3&&plannerWorker.job.world->inputSerial==shared->world.inputSerial,"paused-world input generation was skipped");first.reset();
 auto rejected=[&](const char*why){servicePlanner();check(!plannerWorker.submissions&&!planningBusy&&worldRevision==-1,why);};
 reset();shared->computeFrameSeq=3;rejected("odd frame batch admitted");reset();shared->worldSeq=3;rejected("odd World sequence admitted");
 reset();shared->world.count=planner::MaxActors+1;rejected("invalid actor count admitted");reset();shared->world.combatCount=-1;rejected("invalid combat count admitted");
 reset();barrierMutation=[](unsigned n){if(n==2)shared->worldSeq+=2;};rejected("World changed during copy admitted");
 reset();barrierMutation=[](unsigned n){if(n==2)shared->computeFrameSeq+=2;};rejected("frame batch changed during copy admitted");
 reset();++shared->computeFrame;rejected("cross-frame source admitted");reset();++shared->computeFrameMatch;rejected("cross-match batch admitted");
 reset();++shared->world.frame;++shared->computeFrame;rejected("future World admitted");check(wakes==1,"future World did not request control refresh");
 reset();shared->world.tick=1;servicePlanner();check(!plannerWorker.submissions&&worldRevision==2&&staleLogs==1&&wakes==1,"expired source freshness behavior changed");
 reset();snap.chat=1;rejected("chat did not block service");reset();commandsFresh=false;rejected("expired command lease did not block service");
 // Pool saturation must leave this exact revision and receipts retryable.
 reset();std::vector<std::shared_ptr<const planner::World>> pins;
 for(int n=0;n<3;++n){if(n){plannerWorker.finish();nextSource();}servicePlanner();check(plannerWorker.submissions==unsigned(n+1),"saturation setup failed");pins.push_back(plannerWorker.job.world);}
 const auto pinned0=digestWorld(*pins[0]),pinned1=digestWorld(*pins[1]),pinned2=digestWorld(*pins[2]);
 plannerWorker.finish();nextSource();plannerReceipts.push_back({{},snap.frame,planner::Receipt::Expired});const auto serial=planningSerial;const auto revision=worldRevision;servicePlanner();
 if(!PLANNER_POOL_BASELINE){
  check(plannerWorker.submissions==3&&!planningBusy&&planningSerial==serial&&worldRevision==revision&&plannerReceipts.size()==1,"saturation lost a revision, receipt or submitted work");
  pins[0].reset();servicePlanner();check(plannerWorker.submissions==4&&worldRevision==shared->worldSeq&&plannerWorker.job.receipts.size()==1&&plannerReceipts.empty(),"saturated revision did not retry after release");
 }else check(plannerWorker.submissions==4,"baseline allocation path unexpectedly deferred");
 check((!pins[0]||digestWorld(*pins[0])==pinned0)&&digestWorld(*pins[1])==pinned1&&digestWorld(*pins[2])==pinned2,"service overwrote pinned older observations");
 std::puts("PASS exact servicePlanner active headers/prefixes, paused input, seqlocks, source/frame/match freshness and saturation receipt retry");
}
static void measurements(){
 reset();servicePlanner();for(int n=0;n<20;++n){plannerWorker.finish();nextSource();servicePlanner();}
 constexpr int iterations=500,batches=5;worldAllocations=worldAllocationBytes=0;countAllocations=true;std::vector<long long> times;
 for(int b=0;b<batches;++b){const auto start=std::chrono::steady_clock::now();for(int n=0;n<iterations;++n){plannerWorker.finish();nextSource();servicePlanner();check(planningBusy&&worldRevision==shared->worldSeq,"measured capture deferred unexpectedly");}times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());}
 countAllocations=false;std::sort(times.begin(),times.end());check(worldAllocations==(PLANNER_POOL_BASELINE?uint64_t(iterations*batches):0),"steady-state large-World allocations differ from expectation");
 check(digestWorld(*plannerWorker.job.world)==digestWorld(shared->world),"measured service changed final active source");
 std::printf("METRIC {\"baseline\":%s,\"world_bytes\":%zu,\"benchmark_iterations_per_batch\":%d,\"benchmark_batches\":%d,\"large_world_allocations\":%llu,\"large_world_allocation_bytes\":%llu,\"median_batch_ns\":%lld,\"min_batch_ns\":%lld,\"max_batch_ns\":%lld,\"final_active_digest\":\"%016llx\"}\n",PLANNER_POOL_BASELINE?"true":"false",sizeof(planner::World),iterations,batches,(unsigned long long)worldAllocations,(unsigned long long)worldAllocationBytes,times[times.size()/2],times.front(),times.back(),(unsigned long long)digestWorld(*plannerWorker.job.world));
}
int main(){shared=new Shared{};poolOwnership();serviceGuards();measurements();plannerWorker={};delete shared;std::puts("PASS Host World pool production-path regression and allocation measurements (portable synthetic fixture)");}
