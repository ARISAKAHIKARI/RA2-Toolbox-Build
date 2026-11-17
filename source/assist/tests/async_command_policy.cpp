#include "../../shared/source/world_frame_command_policy.h"
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
 std::puts("PASS same-frame strict admission, request coalescing/retry/isolation, demand/revision/age rejection, exact page-buffer ownership swap");
}
