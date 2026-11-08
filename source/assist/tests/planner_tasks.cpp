#include "../source/planner_task.h"
#include "../source/value_task.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
using namespace assist;
int main(){
 auto state=std::make_unique<planner::Task>();auto reference=std::make_unique<planner::Planner>();
 std::vector<planner::Receipt> receipts;size_t total=0;
 for(int frame=1;frame<=240;++frame){
  auto world=std::make_shared<planner::World>();world->match=8;world->frame=frame;world->tick=frame*16;world->count=2;world->refineryCount=1;
  auto&miner=world->actors[0];miner.id=7;miner.kind=2;miner.type=200;miner.flags=planner::Own|planner::Harvester;miner.storageLimit=40;miner.storage[0]=frame%10<5?40:0;miner.mission=10;miner.dockTypes[0]=900;miner.x=miner.y=5000;
  if(frame%10==5)miner.flags|=planner::Unloading;
  auto&ref=world->actors[1];ref.id=8;ref.kind=3;ref.type=900;ref.flags=planner::Own;ref.x=ref.y=8000;
  auto&area=world->refineries[0];area.id=8;area.count=1;area.waiting[0]={9009,0,0};
  planner::TaskInput input;input.generation=1;input.serial=frame;input.world=world;input.parameters.flags=AutoOreReturn;input.receipts=std::move(receipts);
  for(const auto&r:input.receipts){if(r.kind==planner::Receipt::Accepted)reference->accepted(r.order,r.frame);else if(r.kind==planner::Receipt::Rejected)reference->rejected(r.order,r.frame);else reference->expired(r.order);}
  const auto expected=reference->step(*world,AutoOreReturn,200,300,90,10);auto result=state->run(input);
  assert(result->orders.size()==expected.size());assert(result->statistics.tracking==reference->trackingFlags());assert(result->statistics.oreDelivered==reference->oreDelivered);
  assert(result->generation==1&&result->serial==uint64_t(frame)&&result->frame==frame&&result->tick==world->tick);
  receipts.clear();for(size_t n=0;n<expected.size();++n){assert(!std::memcmp(&expected[n],&result->orders[n],sizeof(planner::Order)));receipts.push_back({expected[n],frame,frame%3?planner::Receipt::Accepted:planner::Receipt::Expired});++total;}
 }
 assert(total>0);
 planner::TaskInput reset;reset.generation=2;reset.serial=999;reset.world=std::make_shared<planner::World>();auto empty=state->run(reset);assert(!empty->statistics.tracking&&!empty->statistics.oreDelivered&&empty->orders.empty());
 // Hold a worker inside calculation: submission/get/invalidation must finish
 // on the caller without waiting for it, and old bridge results cannot escape.
 struct Input{int value;};struct Output{int value=0;};std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
 compute::ValueTask<Input,Output> worker([&](Input&i){if(i.value==1){std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_one();cv.wait(lock,[&]{return release;});}return Output{i.value};});
 auto first=worker.acquire();first->value=1;worker.submit(1,1,2,std::move(first));
 {std::unique_lock<std::mutex> lock(mutex);assert(cv.wait_for(lock,std::chrono::seconds(5),[&]{return entered;}));}
 assert(!worker.get());worker.invalidate();auto next=worker.acquire();next->value=2;worker.submit(2,2,4,std::move(next));assert(!worker.get());
 {std::lock_guard<std::mutex> lock(mutex);release=true;cv.notify_one();}
 const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);std::shared_ptr<const decltype(worker)::Result> result;
 while(!(result=worker.get())&&std::chrono::steady_clock::now()<limit)std::this_thread::yield();
 assert(result&&result->generation==2&&result->task==2&&result->sequence==4&&result->value.value==2);
 assert(!worker.needs(2,2,4)&&worker.needs(2,2,6)&&worker.needs(3,2,4));
 std::cout<<"PASS 240 stateful planner frames and receipts match synchronous decisions; blocked computation leaves caller responsive, invalidated bridge result rejected\n";
}
