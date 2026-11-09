#include "../source/command_delivery.h"
#include "../source/cooperative_plan.h"
#include "../source/burst_compute.h"
#include "../source/dodge_compute.h"
#include "../source/mo_orders.h"
#include "../source/outgoing_batch.h"
#include "../source/repair_delivery.h"
#include "../source/unit_task_arbiter.h"
#include <cassert>
#include <iostream>
#include <memory>
#include <utility>

int main(){
 using namespace assist;
 delivery::Queue queue;
 for(int i=0;i<delivery::Capacity;++i)assert(queue.add({Produce,i,0,1,1,100}));
 assert(!queue.add({Produce,delivery::Capacity,0,1,1,100}));
 for(int i=0;i<delivery::Capacity;++i){delivery::Item out;assert(queue.peek(101,1,1,true,out)&&out.arg==i);queue.consume();}
 assert(!queue.size);
 outgoing::Batch batch;batch.begin(1);
 for(int i=0;i<32;++i)batch.accept();
 assert(batch.done(32)&&batch.acceptedMask==0xffffffffu&&!batch.rejectedMask);
 batch.begin(2);for(int i=0;i<32;++i)batch.reject();
 assert(batch.rejectedMask==0xffffffffu&&!batch.acceptedMask);

 burstcompute::Input burst;burst.count=256;
 for(auto&s:burst.sources){s.target=s.currentTarget=1;s.mission=1;s.burst=2;s.index=1;s.eligible=s.inRange=1;}
 auto shots=burstcompute::calculate(burst);assert(shots.count==256);
 for(const auto&d:shots.decisions)assert(d.mission==13);
 dodgecompute::Input dodge;dodge.count=128;
 for(auto&s:dodge.sources){s.count=128;s.freshThreat=1;s.shots[127]=123;}
 auto scatters=dodgecompute::calculate(dodge);assert(scatters.count==128);
 for(const auto&d:scatters.decisions)assert(d.scatter);

 // Exercise allocation and retry identities above the old 512-source limit.
 cooperative::Input input;input.mode=cooperative::AirFirst;input.clicked=2000;
 input.targets.push_back({2000,2,0,0,3,0,{0,0,0}});
 cooperative::Request request{};request.count=1024;
 for(int i=0;i<1024;++i){input.units.push_back({{uint32_t(i+1),7},1,0,0,1});request.sources[i]={uint32_t(i+1),7};}
 input.compatible.assign(1024,1);
 auto orders=cooperative::allocate(input);assert(orders.size()==1024&&orders.back().id==1024);
 auto original=request;
 std::vector<cooperative::Assignment> processed(orders.begin(),orders.begin()+700);
 assert(cooperative::retrySourcePool(request,original,processed)&&request.count==324);
 assert(request.sources[0].id==701&&request.sources[323].id==1024);

 auto frame=std::make_unique<taskarbiter::FrameArbiter<>>();frame->begin(1,10);
 auto leases=std::make_unique<taskarbiter::LeaseArbiter<>>();
 for(uint32_t i=1;i<=8192;++i){
  assert(frame->record(i,0,taskarbiter::Owner::Ore));
  assert(leases->record(1,10,i,0x10000+i,0,taskarbiter::Owner::Ore,i));
 }
 assert(!frame->record(8193,0,taskarbiter::Owner::Ore));
 assert(!leases->record(1,10,8193,0x12000,0,taskarbiter::Owner::Ore,8193));
 leases->manual(4097);assert(leases->record(1,10,8193,0x12000,0,taskarbiter::Owner::Ore,8193));
 frame->begin(1,11);assert(frame->record(8193,0,taskarbiter::Owner::Ore));

 auto pending=std::make_unique<repairdelivery::Pending<>>();
 for(uint32_t i=1;i<=16384;++i)assert(pending->queued(i,0x10000+i));
 assert(!pending->queued(16385,0x20000));
 pending->observe([](uint32_t id,uint32_t){return repairdelivery::Observation{true,id%2==0,false};});
 assert(pending->count==8192&&pending->contains(16383,0x10000+16383));
 for(uint32_t i=16385;i<=24576;++i)assert(pending->queued(i,0x20000+i));
 assert(pending->count==16384&&!pending->room(24577));

 moorders::Point nodes[moorders::Nodes]{};int count=0;
 for(int i=0;i<400;++i)moorders::sample(nodes,count,{i*2,0},true);
 assert(count==256&&nodes[255].x==798&&nodes[0].x==0);
 std::cout<<"PASS: fixed request queue capacity, cooperative1024/retry, ownership8192/reuse, repair16384/tombstones, stroke256/tail\n";
}
