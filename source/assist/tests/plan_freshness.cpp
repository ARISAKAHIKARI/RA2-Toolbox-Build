#include "../source/plan_freshness.h"
#include "../source/cooperative_types.h"
#include "../source/cooperative_plan.h"
#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace assist;
int main(){
 using namespace freshness;
 assert(valid(1,1,1000,100,1150,106,Tactical));
 assert(!valid(1,1,1000,100,1151,106,Tactical));
 assert(!valid(1,1,1000,100,1010,107,Tactical));
 assert(!valid(1,2,1000,100,1010,100,Tactical));
 assert(!valid(0,0,1000,100,1010,100,Tactical));
 assert(!valid(1,1,1000,100,999,100,Tactical));
 assert(!valid(1,1,1000,100,1010,99,Tactical));
 assert(!valid(1,1,1000,-1,1010,100,Tactical));
 assert(valid(1,1,0xfffffff0u,100,20,101,Tactical));
 assert(valid(1,1,1000,100,1250,112,Cooperative));
 assert(!valid(1,1,1000,100,1251,112,Cooperative));
 assert(!valid(1,1,1000,100,1010,113,Cooperative));
 assert(intentAlive(1000,6000)&&!intentAlive(1000,6001));
 cooperative::Request request{};request.serial=9;request.tick=1000;request.match=3;
 cooperative::Assignment assignments[512]{};for(int i=0;i<512;++i)assignments[i]={uint32_t(i+1),17,99,10};
 assert(cooperative::retryRemaining(request,assignments,512,24));
 assert(request.count==488&&request.sources[0].id==25&&request.sources[487].id==512);
 assert(request.tick==1000&&request.serial==9&&request.match==3);
 assert(!cooperative::retryRemaining(request,assignments,512,513));
 assert(!cooperative::retryRemaining(request,assignments,512,512));
 // Expiration must replan in the same frame, without treating stale data as a
 // failed move/illegal enemy or erasing an already accepted task.
 auto storage=std::make_unique<planner::World>();auto&w=*storage;w.match=1;w.frame=10;w.count=2;
 auto&a=w.actors[0];a.id=1;a.type=20;a.kind=2;a.flags=planner::Own|planner::Harvester;a.mission=10;a.x=a.y=3000;a.storageLimit=28;a.storage[0]=28;a.dockTypes[0]=900;
 auto&b=w.actors[1];b.id=2;b.type=900;b.kind=3;b.flags=planner::Own;b.x=4000;b.y=4000;
 planner::Planner p;auto first=p.step(w,AutoOreReturn,200,300,90);assert(first.size()==1);
 assert(p.step(w,AutoOreReturn,200,300,90).empty());p.expired(first[0]);
 auto retry=p.step(w,AutoOreReturn,200,300,90);assert(retry.size()==1&&retry[0].unit==1&&retry[0].destination==first[0].destination);
 p.accepted(retry[0],10);assert(p.hasJobs());
 cooperative::Input air;air.mode=cooperative::AirFirst;air.clicked=99;
 air.units.push_back({{1,0},1,0,0,1});air.targets.push_back({99,10,0,0,0,0,{1,2,1},1});air.compatible={1};
 assert(cooperative::allocate(air).empty());air.targets[0].quota.maximum=2;
 assert(cooperative::allocate(air).size()==1);
 std::cout<<"PASS source-age/frame/epoch/clock-wrap boundaries; 512-source retry excludes handled prefix; same-frame planner expiration\n";
}
