// The driver extracts each revision's actual publishPlanningWorld body. This
// fixture reuses the sealed Snapshot and explicit native seams of the tactical
// tests; it does not run Windows or a game and cannot measure gameplay FPS.
#define TACTICAL_FRAME_NO_MAIN
#include "tactical_frame_sampling.cpp"
#include "planner_world_copy.h"
#include "external_planner.h"
#include <chrono>
#include <algorithm>

static bool measureCopies=false;
static uint64_t retainedBytes=0,rollbackBytes=0;
extern "C" void* __real_memcpy(void*,const void*,size_t);
extern "C" void* __wrap_memcpy(void*destination,const void*source,size_t size){
 if(measureCopies){
  const auto d=reinterpret_cast<uintptr_t>(destination),s=reinterpret_cast<uintptr_t>(source);
  const auto wd=reinterpret_cast<uintptr_t>(&planningWorld),ws=reinterpret_cast<uintptr_t>(&planningCapture);
  if(d>=wd&&d-wd<sizeof(planningWorld)&&s>=ws&&s-ws<sizeof(planningCapture))retainedBytes+=size;
  if(source==weaponCache&&size==sizeof weaponCache)rollbackBytes+=size;
 }
 return __real_memcpy(destination,source,size);
}
static uint64_t digestBytes(uint64_t h,const void*data,size_t size){
 for(const auto*p=static_cast<const uint8_t*>(data);size;--size)h=(h^*p++)*1099511628211ull;return h;
}
static uint64_t digestWorld(const planner::World&w){
 const uint32_t header[]={w.match,w.tick,uint32_t(w.frame),uint32_t(w.count),uint32_t(w.intentCount),uint32_t(w.refineryCount),w.inputSerial,uint32_t(w.combatCount)};
 uint64_t h=digestBytes(1469598103934665603ull,header,sizeof header);
 h=digestBytes(h,w.actors,size_t(w.count)*sizeof(planner::Actor));h=digestBytes(h,w.intents,size_t(w.intentCount)*sizeof(planner::Intent));
 h=digestBytes(h,w.refineries,size_t(w.refineryCount)*sizeof(planner::Refinery));return digestBytes(h,w.combat,size_t(w.combatCount)*sizeof(planner::CombatProbe));
}
static size_t activeArrayBytes(const planner::World&w){return size_t(w.count)*sizeof(planner::Actor)+size_t(w.intentCount)*sizeof(planner::Intent)+size_t(w.refineryCount)*sizeof(planner::Refinery)+size_t(w.combatCount)*sizeof(planner::CombatProbe);}
static bool filled(const void*data,size_t size,uint8_t value){for(const auto*p=static_cast<const uint8_t*>(data);size;--size)if(*p++!=value)return false;return true;}
static void copyBoundaries(){
 auto source=std::make_unique<planner::World>(),destination=std::make_unique<planner::World>(),before=std::make_unique<planner::World>();
 std::memset(source.get(),0xA5,sizeof *source);source->match=71;source->tick=1000;source->frame=80;source->inputSerial=11;
 const int counts[][4]={{0,0,0,0},{1,1,1,1},{17,4,2,23},{planner::MaxActors,planner::MaxIntents,planner::MaxRefineries,planner::MaxCombatProbes},{2,0,1,0},{0,0,0,0},{31,9,3,47}};
 for(const auto&n:counts){
  source->count=n[0];source->intentCount=n[1];source->refineryCount=n[2];source->combatCount=n[3];for(int i=0;i<n[2];++i)source->refineries[i].count=i%(planner::MaxWaitingCells+1);
  std::memset(destination.get(),0x3C,sizeof *destination);check(planner::copyActiveWorld(*destination,*source),"valid active World rejected");
  check(digestWorld(*destination)==digestWorld(*source),"active World bytes/header changed");
  check(filled(destination->actors+n[0],sizeof(destination->actors)-size_t(n[0])*sizeof(planner::Actor),0x3C),"inactive actor tail overwritten");
  check(filled(destination->intents+n[1],sizeof(destination->intents)-size_t(n[1])*sizeof(planner::Intent),0x3C),"inactive intent tail overwritten");
  check(filled(destination->refineries+n[2],sizeof(destination->refineries)-size_t(n[2])*sizeof(planner::Refinery),0x3C),"inactive refinery tail overwritten");
  check(filled(destination->combat+n[3],sizeof(destination->combat)-size_t(n[3])*sizeof(planner::CombatProbe),0x3C),"inactive probe tail overwritten");
  *before=*destination;check(planner::copyActiveWorld(*destination,*destination)&&!std::memcmp(destination.get(),before.get(),sizeof *destination),"self copy changed World");
 }
 *before=*destination;
 auto reject=[&](int&field,int invalid){const int old=field;field=invalid;check(!planner::copyActiveWorld(*destination,*source),"malformed World accepted");check(!std::memcmp(destination.get(),before.get(),sizeof *destination),"malformed World partly replaced retained input");field=old;};
 reject(source->count,-1);reject(source->count,planner::MaxActors+1);reject(source->intentCount,-1);reject(source->intentCount,planner::MaxIntents+1);
 reject(source->refineryCount,-1);reject(source->refineryCount,planner::MaxRefineries+1);reject(source->combatCount,-1);reject(source->combatCount,planner::MaxCombatProbes+1);
 reject(source->frame,-1);reject(source->refineries[0].count,-1);reject(source->refineries[0].count,planner::MaxWaitingCells+1);
 std::puts("PASS active prefix/header equality, zero/max counts, shrink/reexpand, self copy and transactional malformed-input rejection");
}
static void retainedConsumers(){
 auto source=std::make_unique<planner::World>(),whole=std::make_unique<planner::World>(),active=std::make_unique<planner::World>();
 planner::Planner left,right;source->match=17;source->frame=1;source->tick=1000;source->count=2;
 auto&a=source->actors[0];a.id=10;a.kind=3;a.flags=planner::Own|planner::PointDefense;a.x=a.y=5000;a.range=1024;a.weapons[0]={1024,0,planner::GroundWeapon,0x7ff};
 auto&b=source->actors[1];b.id=20;b.kind=1;b.flags=planner::Enemy|planner::Saboteur;b.x=5400;b.y=5000;
 unsigned emitted=0;
 auto step=[&](uint32_t flags){
  *whole=*source;check(planner::copyActiveWorld(*active,*source),"consumer copy failed");
  auto l=left.step(*whole,flags,200,300,90),r=right.step(*active,flags,200,300,90);
  check(l.size()==r.size()&&(l.empty()||!std::memcmp(l.data(),r.data(),l.size()*sizeof(planner::Order))),"retained consumer emitted different orders");
  check(left.trackingFlags()==right.trackingFlags()&&left.queued==right.queued&&left.docking==right.docking&&left.issued==right.issued&&left.yielded==right.yielded&&left.targetChecks==right.targetChecks,"retained consumer statistics changed");
  for(size_t i=0;i<l.size();++i){++emitted;if(i%3==0){left.accepted(l[i],source->frame);right.accepted(r[i],source->frame);}else if(i%3==1){left.rejected(l[i],source->frame);right.rejected(r[i],source->frame);}else{left.expired(l[i]);right.expired(r[i]);}}
  return l.size();
 };
 check(step(AntiTheft)==1,"defense consumer fixture emitted no order");step(AntiTheft); // Same frame retains indices.
 ++source->frame;source->tick+=1000;source->count=1;check(step(AntiTheft)==0,"inactive enemy remained visible after shrink");
 ++source->inputSerial;source->count=0;check(step(AntiTheft)==0,"inactive turret remained visible in empty same-frame input");
 ++source->inputSerial;source->count=2;b.id=21;check(step(AntiTheft)==1,"same-frame input expansion was lost");
 ++source->match;source->count=0;step(AntiTheft);++source->frame;source->count=2;b.id=22;check(step(AntiTheft)==1,"new-match indices retained old tails");
 // Exercise ore area/actor pointers across retained copies and receipt updates.
 setupOre();check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"ore publication for retained consumer failed");*source=planningWorld;step(AutoOreReturn);
 ++source->frame;source->tick+=1000;source->actors[0].flags&=~planner::Unloading;source->actors[0].mission=10;source->actors[1].radioIDs[0]=0;step(AutoOreReturn);
 ++source->inputSerial;source->count=1;source->refineryCount=0;step(AutoOreReturn);++source->frame;source->count=source->intentCount=source->refineryCount=source->combatCount=0;step(0);
 check(emitted>=3,"retained consumer sequence did not exercise native-order outputs");
 std::puts("PASS stateful production Planner: retained addresses, same-frame input, shrink/expand, match reset, ore areas and receipts");
}
static void publicationMeasurements(){
 constexpr int skipped=1000,iterations=500,batches=5;
 reset();seal();retainedBytes=rollbackBytes=0;measureCopies=true;
 const auto idleBegin=std::chrono::steady_clock::now();for(int i=0;i<skipped;++i)check(!publishPlanningWorld(lastHouse,snap.frame,clockNow),"idle observation published");
 const auto idleNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-idleBegin).count();measureCopies=false;const auto idleRollback=rollbackBytes;
 setupOre();check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"ore warmup did not publish");
 const auto digest=digestWorld(planningWorld);const auto sequence=sh->worldSeq;retainedBytes=rollbackBytes=0;measureCopies=true;
 const auto repeatedBegin=std::chrono::steady_clock::now();for(int i=0;i<skipped;++i)check(!publishPlanningWorld(lastHouse,snap.frame,clockNow),"unchanged observation published");
 const auto repeatedNs=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-repeatedBegin).count();measureCopies=false;const auto repeatedRollback=rollbackBytes;
 check(sh->worldSeq==sequence&&digestWorld(planningWorld)==digest&&!retainedBytes,"same-frame return modified publication");
 constexpr uint64_t expectedSkip=PLANNER_PUBLICATION_BASELINE?uint64_t(skipped)*sizeof weaponCache:0;
 check(idleRollback==expectedSkip&&repeatedRollback==expectedSkip,"rollback snapshot work did not match actual gate path");
 retainedBytes=rollbackBytes=0;measureCopies=true;++inputSerial;check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"same-frame new input was skipped");measureCopies=false;
 const auto oneRetained=retainedBytes,oneRollback=rollbackBytes,activeBytes=activeArrayBytes(planningWorld);
 check(oneRetained==(PLANNER_PUBLICATION_BASELINE?sizeof(planner::World):activeBytes),"actual retained memcpy bytes unexpected");
 check(oneRollback==sizeof weaponCache,"full observation lost metadata rollback snapshot");
 const auto preserved=digestWorld(planningWorld);const auto seq=sh->worldSeq;advance(0x100000); // Active miner type is absent.
 check(!publishPlanningWorld(lastHouse,snap.frame,clockNow)&&sh->worldSeq==seq&&digestWorld(planningWorld)==preserved&&!missingLiveReads,"failed observation changed retained publication or fell back live");
 setupOre();check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"benchmark ore setup failed");
 for(int i=0;i<20;++i){++inputSerial;check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"benchmark warmup failed");}
 std::vector<long long> times;retainedBytes=rollbackBytes=0;measureCopies=true;
 for(int b=0;b<batches;++b){const auto begin=std::chrono::steady_clock::now();for(int i=0;i<iterations;++i){++inputSerial;check(publishPlanningWorld(lastHouse,snap.frame,clockNow),"benchmark observation failed");}times.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-begin).count());}
 measureCopies=false;std::sort(times.begin(),times.end());
 check(retainedBytes==uint64_t(iterations*batches)*oneRetained&&rollbackBytes==uint64_t(iterations*batches)*sizeof weaponCache,"measured repeated work counts changed");
 std::printf("METRIC {\"baseline\":%s,\"world_bytes\":%zu,\"actor_bytes\":%zu,\"intent_bytes\":%zu,\"refinery_bytes\":%zu,\"combat_bytes\":%zu,\"rollback_cache_bytes\":%zu,\"actors\":%d,\"intents\":%d,\"refineries\":%d,\"combat\":%d,\"active_array_bytes\":%zu,\"header_bytes\":32,\"retained_memcpy_bytes_per_publication\":%llu,\"idle_calls\":%d,\"idle_rollback_bytes\":%llu,\"idle_elapsed_ns\":%lld,\"repeated_rollback_bytes\":%llu,\"repeated_elapsed_ns\":%lld,\"benchmark_iterations_per_batch\":%d,\"benchmark_batches\":%d,\"median_batch_ns\":%lld,\"min_batch_ns\":%lld,\"max_batch_ns\":%lld,\"final_active_digest\":\"%016llx\"}\n",
  PLANNER_PUBLICATION_BASELINE?"true":"false",sizeof(planner::World),sizeof(planner::Actor),sizeof(planner::Intent),sizeof(planner::Refinery),sizeof(planner::CombatProbe),sizeof weaponCache,planningWorld.count,planningWorld.intentCount,planningWorld.refineryCount,planningWorld.combatCount,activeBytes,(unsigned long long)oneRetained,skipped,(unsigned long long)idleRollback,(long long)idleNs,(unsigned long long)repeatedRollback,(long long)repeatedNs,iterations,batches,times[times.size()/2],times.front(),times.back(),(unsigned long long)digestWorld(planningWorld));
}
int main(){sh=new Shared{};copyBoundaries();retainedConsumers();testPlanning();publicationMeasurements();check(!sampleReadDepth,"final scope leaked");delete sh;std::puts("PASS actual publication work counts and semantic invariants (portable synthetic fixture)");}
