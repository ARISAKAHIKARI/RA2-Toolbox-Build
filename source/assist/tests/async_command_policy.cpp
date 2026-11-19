#include "../../shared/source/world_frame_command_policy.h"
#include "../source/object_index.h"
#include <cassert>
#include <cstdio>
#include <cstring>
int main(){
 using namespace worldframe;
 Key a{};a.pid=100;a.owner=250;a.match=7;a.scenario=0x110000;
 a.cells=0x140000;a.house=0x200000;a.cellCount=16;
 a.frame=55;a.session=0;a.tick=100;a.generationLow=19;a.connectionLow=27;
 constexpr uint64_t revision=(uint64_t(55)<<32)|17u;
 const uint32_t demand=Base|Combat|Passengers;
 assert(acceptCommandCapture(a,a,demand,Base|Combat,revision,revision,110));
 assert(acceptCommandCapture(a,a,demand,demand,revision,revision,180));
 assert(!acceptCommandCapture(a,a,demand,demand,revision,revision,181));
 // A real full graph can finish after the game's frame boundary. It is
 // observational data only, never a permit to submit an old EventClass.
 Key candidateNext=a;++candidateNext.frame;
 assert(acceptObservationCandidate(a,candidateNext,demand,Base,160));
 assert(!acceptCommandCapture(a,candidateNext,demand,Base,revision,revision,160));
 assert(!acceptObservationCandidate(a,candidateNext,demand,Base|Ore,160));
 assert(!acceptObservationCandidate(a,candidateNext,demand,Base,601));
 Key candidateOld=a;--candidateOld.frame;
 assert(!acceptObservationCandidate(a,candidateOld,demand,Base,160));
 Key candidateOther=a;++candidateOther.connectionHigh;
 assert(!acceptObservationCandidate(a,candidateOther,demand,Base,160));
 // A candidate UID hash needs an independent live identity check at lookup.
 auto owner=std::make_unique<assist::ObjectIndex>();
 owner->add(0x200000,77);
 bool candidateReady=true;
 uint32_t currentUID=77;
 // Stateless C function pointers emulate the production native UID validators.
 // A stale candidate is never allowed to authorize the wrong in-place actor.
 struct Check {
  static inline bool*ready=nullptr;
  static inline uint32_t*uid=nullptr;
  static bool available(){return *ready;}
  static bool byId(uint32_t id,uint32_t pointer){return pointer==0x200000&&id==*uid;}
  static bool byPtr(uint32_t pointer){return pointer==0x200000&&*uid==77;}
 };
 Check::ready=&candidateReady;Check::uid=&currentUID;
 assist::ObjectIndexView view;
 view.bindCandidate(&owner,Check::available,Check::byId,Check::byPtr);
 assert(view.byId(77)==0x200000&&view.contains(0x200000));
 currentUID=88;assert(!view.byId(77)&&!view.contains(0x200000));
 candidateReady=false;currentUID=77;assert(!view.byId(77)&&!view.contains(0x200000));
 view.unbind();
 
 // A cold Host demand read can move its revision from 54 to 55 while
 // assembling the requested pages. The worker ticket must carry 55, and
 // subsequent exact-frame admission rejects the obsolete pre-read 54.
 const uint64_t priorHostRevision=(uint64_t(54)<<32)|17u;
 CommandRequestGate assembledGate;
 assembledGate.submitted(a,demand,revision,false);
 assert(assembledGate.defer(a,demand,revision,false,110));
 assert(!assembledGate.defer(a,demand,priorHostRevision,false,110));
 assert(!acceptCommandCapture(a,a,demand,demand,priorHostRevision,revision,110));
 assert(acceptCommandCapture(a,a,demand,demand,revision,revision,110));
 // Runtime request admission: no repeated 16K-page merge for the same
 // worker job, but never throttle a new frame, requirement or config.
 CommandRequestGate gate;
 assert(!gate.defer(a,demand,revision,false,110));
 gate.submitted(a,demand,revision,false);
 assert(gate.defer(a,demand,revision,false,115));
 assert(!gate.defer(a,demand,revision,false,116));
 assert(!gate.defer(a,demand,revision+1,false,110));
 assert(!gate.defer(a,demand|Ore,revision,false,110));
 assert(!gate.defer(a,demand,revision,true,110));
 Key nextFrame=a;++nextFrame.frame;
 assert(!gate.defer(nextFrame,demand,revision,false,110));
 // A new simulation frame can coalesce the same *running request*
 // without ever making the old frame's snapshot native command authority.
 assert(coalesceInFlightWorld(a,nextFrame,demand,demand,revision,revision,false,false,25,true));
 assert(!acceptCommandCapture(a,nextFrame,demand,demand,revision,revision,125));
 assert(!coalesceInFlightWorld(a,nextFrame,demand,demand,revision,revision,false,false,80,true));
 assert(!coalesceInFlightWorld(a,nextFrame,demand,demand,revision,revision,false,false,20,false));
 assert(!coalesceInFlightWorld(a,nextFrame,demand,demand|Ore,revision,revision,false,false,20,true));
 assert(!coalesceInFlightWorld(a,nextFrame,demand,demand,revision,revision+1,false,false,20,true));
 assert(!coalesceInFlightWorld(a,nextFrame,demand,demand,revision,revision,false,true,20,true));
 Key foreignWorld=nextFrame;++foreignWorld.connectionLow;
 assert(!coalesceInFlightWorld(a,foreignWorld,demand,demand,revision,revision,false,false,20,true));
 Key nextHouse=a;++nextHouse.house;
 assert(!gate.defer(nextHouse,demand,revision,false,110));
 Key wrapRequest=a;wrapRequest.tick=0xfffffff8u;
 gate.submitted(wrapRequest,demand,revision,false);
 assert(gate.defer(wrapRequest,demand,revision,false,0x02u));
 assert(!gate.defer(wrapRequest,demand,revision,false,0x08u));
 gate.reset();
 assert(!gate.defer(a,demand,revision,false,110));
 assert(!acceptCommandCapture(a,a,Base,Base|Passengers,revision,revision,110));
 assert(!acceptCommandCapture(a,a,demand,demand,revision,revision+1,110));
 for(int change=0;change<11;++change){
  Key b=a;switch(change){
   case 0:++b.pid;break;case 1:++b.owner;break;case 2:++b.match;break;
   case 3:++b.scenario;break;case 4:++b.cells;break;case 5:++b.house;break;
   case 6:++b.cellCount;break;case 7:++b.frame;break;
   case 8:++b.session;break;case 9:++b.generationLow;break;
   case 10:++b.connectionLow;break;
  }
  assert(!acceptCommandCapture(a,b,demand,demand,revision,revision,110));
 }
 // Unsigned elapsed time remains defined across GetTickCount wrap.
 Key wrap=a;wrap.tick=0xfffffff0u;
 assert(acceptCommandCapture(wrap,wrap,demand,demand,revision,revision,0x20u));
 assert(!acceptCommandCapture(wrap,wrap,demand,demand,revision,revision,0x50u));
 // Only the owner of each Snapshot accesses its bytes. Handoff swaps
 // ownership, including the 32K lookup slots, without copying 4KB pages.
 Snapshot main,worker;
 auto live=[](uint32_t at,void*out,size_t n){std::memset(out,at==0x21000?0xA7:0x3D,n);return true;};
 auto first=a;first.serial=1;main.reset(first);
 assert(main.prefetch(live,0x21000,4096));assert(main.seal());
 auto next=a;next.serial=2;worker.reset(next);
 assert(worker.prefetch(live,0x33000,4096));assert(worker.seal());
 const Page*mainData=main.pages.data(),*workerData=worker.pages.data();
 main.swap(worker);
 assert(main.pages.data()==workerData&&worker.pages.data()==mainData);
 assert(main.key.serial==2&&worker.key.serial==1);
 uint8_t out=0;
 assert(main.read(0x33000,&out,1)&&out==0x3D);
 assert(!main.read(0x21000,&out,1));
 assert(worker.read(0x21000,&out,1)&&out==0xA7);
 assert(!worker.read(0x33000,&out,1));
 main.reset(a);assert(!main.read(0x33000,&out,1));
 assert(worker.read(0x21000,&out,1)&&out==0xA7);
 std::puts("PASS exact-frame event authority, cross-frame busy work coalescing only, demand/revision/age isolation, exact page-buffer ownership swap");
}
