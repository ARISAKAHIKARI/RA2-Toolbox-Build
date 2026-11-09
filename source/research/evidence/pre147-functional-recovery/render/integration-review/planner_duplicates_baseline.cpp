#define main existingPlannerDeliveryChecks
#include "/workspace/scratch/5a310fe852f8/functional147pre2/render/integration-review/baseline/planner_task.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <set>
#include <string>
using namespace assist;using namespace assist::planner;

namespace {
int failures=0,checks=0;
void check(bool condition,const std::string& message){
 ++checks;if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
int count(const std::vector<Order>& orders,int purpose){
 return int(std::count_if(orders.begin(),orders.end(),[&](const Order&o){return o.purpose==purpose;}));
}
struct Scene {
 std::unique_ptr<World> world=std::make_unique<World>();Planner planner;bool drop;
 explicit Scene(int turns,int combat=0):drop(combat!=0){
  auto&w=*world;w.match=1;w.frame=500;w.tick=8000;w.inputSerial=1;
  w.refineryCount=turns;w.count=turns*3+combat;
  for(int i=0;i<turns;++i){
   auto&far=w.actors[i*3];auto&near=w.actors[i*3+1];auto&ref=w.actors[i*3+2];
   ref.id=10000+i;ref.type=20000+i;ref.kind=3;ref.flags=Own;place(ref,30+i*5,30);
   far.id=2*i+1;far.type=200;far.kind=2;far.flags=Own|Harvester;
   far.storage[0]=40;far.storageLimit=40;far.dockTypes[0]=ref.type;
   far.manualVersion=7;far.mission=12;far.destination=ref.id;place(far,20+i*5,30);
   near=far;near.id=2*i+2;place(near,31+i*5,32);
   auto&area=w.refineries[i];area.id=ref.id;area.cellX=30+i*5;area.cellY=30;
   area.count=1;area.waiting[0]={uint32_t(40020+i*5),0,0};
  }
  for(int i=0;i<combat;++i){auto&a=w.actors[turns*3+i];
   a.id=30000+i;a.kind=2;a.flags=Own|DropHazard;a.liftTarget=40000+i;a.manualVersion=7;
  }
 }
 static void place(Actor&a,int x,int y){a.x=x*256+128;a.y=y*256+128;}
 std::vector<Order> step(int frame=-1){
  if(frame>=0){world->frame=frame;world->tick=uint32_t(frame)*16;}
  return planner.step(*world,AutoOreReturn,200,300,90,10,0,2,drop);
 }
};
void batchCase(int turns,int combat){
 Scene s(turns,combat);auto first=s.step();const int firstYields=count(first,OreYield);
 const int expectedFirst=std::min(turns,combat?MaxOrders/2:MaxOrders);
 check(first.size()<=MaxOrders,"first batch is bounded");
 check(firstYields==expectedFirst,"first batch retains its ore share");
 check(s.planner.oreYieldAttempts==uint32_t(firstYields),"only returned yield orders consume attempts");
 std::set<uint32_t> served;for(const auto&o:first)if(o.purpose==OreYield)served.insert(o.unit);
 auto next=s.step(501);const int nextYields=count(next,OreYield);
 const int expectedNext=std::min(turns-expectedFirst,combat?MaxOrders/2:MaxOrders);
 check(nextYields==expectedNext,"omitted yield orders are eligible in the next frame");
 for(const auto&o:next)if(o.purpose==OreYield)check(served.insert(o.unit).second,"emitted yields retain their retry delay");
 check(s.planner.oreYieldAttempts==uint32_t(firstYields+nextYields),"attempt statistics match both returned batches");
 std::cout<<"batch turns="<<turns<<" combat="<<combat<<" first_yields="<<firstYields
          <<" next_yields="<<nextYields<<" attempts="<<s.planner.oreYieldAttempts<<'\n';
}
void receiptsCase(Receipt::Kind kind){
 Scene s(1);int emitted=0;
 for(int frame:{500,501,529,530,559,560,589,590}){
  auto orders=s.step(frame);const int yields=count(orders,OreYield);emitted+=yields;
  check(yields==int(frame==500||frame==530||frame==560),"receipt lifecycle retains the three bounded retries");
  for(const auto&o:orders)if(o.purpose==OreYield){
   if(kind==Receipt::Accepted)s.planner.accepted(o,frame);
   else if(kind==Receipt::Rejected)s.planner.rejected(o,frame);else s.planner.expired(o);
  }
 }
 check(emitted==ore::MaxDockAttempts&&s.planner.oreYieldAttempts==uint32_t(emitted),"actual emitted attempts remain bounded");
 std::cout<<"receipt kind="<<int(kind)<<" emitted="<<emitted<<" attempts="<<s.planner.oreYieldAttempts<<'\n';
}
void taskCase(){
 Scene scene(20,MaxOrders);Task task;TaskInput input;
 input.generation=8;input.serial=100;input.world=std::shared_ptr<const World>(std::move(scene.world));
 input.parameters.flags=AutoOreReturn;input.parameters.drop=true;
 auto first=task.run(input);
 check(first&&first->generation==8&&first->serial==100,"task result preserves its delivery identity");
 check(first->statistics.oreYieldAttempts==uint32_t(count(first->orders,OreYield)),"task statistics count only deliverable yields");
 auto nextWorld=std::make_shared<World>(*input.world);nextWorld->frame=501;nextWorld->tick=8016;
 input.world=nextWorld;++input.serial;auto next=task.run(input);
 check(count(next->orders,OreYield)==4,"stateful task recovers omitted yields on the next snapshot");
 check(next->statistics.oreYieldAttempts==20,"stateful task has one emitted attempt per refinery");
 ++input.generation;++input.serial;nextWorld=std::make_shared<World>(*input.world);++nextWorld->frame;input.world=nextWorld;
 auto reset=task.run(input);
 check(count(reset->orders,OreYield)==MaxOrders/2&&reset->statistics.oreYieldAttempts==MaxOrders/2,"generation change clears old retry state");
 std::cout<<"task first_yields="<<count(first->orders,OreYield)<<" next_yields="<<count(next->orders,OreYield)
          <<" reset_yields="<<count(reset->orders,OreYield)<<'\n';
}
}
int main(){
 batchCase(20,0);batchCase(20,MaxOrders);batchCase(40,0);batchCase(32,MaxOrders);
 for(auto kind:{Receipt::Accepted,Receipt::Rejected,Receipt::Expired})receiptsCase(kind);
 taskCase();
 std::cout<<(failures?"FAIL: ":"PASS: ")<<checks<<" checks, "<<failures<<" failures; bounded final-batch delivery and retry lifecycle\n";
 return failures?1:0;
}

#undef main
int main(){
 Scene scene(1);auto&w=*scene.world;
 w.actors[w.count++]=w.actors[0];
 w.refineries[0].count=2;w.refineries[0].waiting[1]={40021,0,0};
 std::cout<<"validWorld="<<validWorld(w)<<'\n';
 int yields=0;
 for(int frame:{500,530,560}){
  auto batch=scene.step(frame);int n=count(batch,OreYield);yields+=n;
  std::cout<<"frame="<<frame<<" yields="<<n<<" attempts="<<scene.planner.oreYieldAttempts;
  for(const auto&o:batch)if(o.purpose==OreYield)std::cout<<" unit="<<o.unit<<" token="<<o.token<<" destination="<<o.destination;
  std::cout<<'\n';
 }
 std::cout<<"total_yields="<<yields<<" retry_limit="<<ore::MaxDockAttempts<<'\n';
 return 0;
}
