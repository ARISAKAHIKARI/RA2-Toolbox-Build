#include "../source/planner_task.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>

using namespace assist;
using namespace assist::planner;
namespace {
int checks=0,failures=0;
void check(bool condition,const std::string&message){
 ++checks;if(!condition){++failures;std::cerr<<"FAIL: "<<message<<'\n';}
}
int count(const std::vector<Order>&orders,int purpose){
 return int(std::count_if(orders.begin(),orders.end(),[&](const Order&o){return o.purpose==purpose;}));
}
struct Scene {
 std::unique_ptr<World> world=std::make_unique<World>();Planner planner;
 explicit Scene(int drops,int miners=20,bool duplicateDefense=false){
  auto&w=*world;w.match=7;w.frame=500;w.tick=8000;w.inputSerial=1;
  for(int i=0;i<miners;++i){
   auto&miner=w.actors[w.count++];miner.id=1000+i;miner.type=10;miner.kind=2;
   miner.flags=Own|Harvester;miner.manualVersion=3;miner.mission=12;
   miner.storage[0]=40;miner.storageLimit=40;miner.dockTypes[0]=20;
   miner.x=(20+i)*256;miner.y=20*256;
  }
  if(miners){
   auto&ref=w.actors[w.count++];ref.id=2000;ref.type=20;ref.kind=3;ref.flags=Own;
   ref.x=40*256;ref.y=40*256;auto&area=w.refineries[w.refineryCount++];
   area.id=ref.id;area.cellX=40;area.cellY=40;area.count=32;
   for(int i=0;i<area.count;++i)area.waiting[i]={uint32_t(43020+i),0,0};
  }
  for(int i=0;i<drops;++i){
   auto&drop=w.actors[w.count++];drop.id=3000+i;drop.kind=2;
   drop.flags=Own|DropHazard;drop.liftTarget=4000+i;drop.manualVersion=5;
  }
  auto&defense=w.actors[w.count++];defense.id=5000;defense.kind=3;
  defense.flags=Own|PointDefense;defense.manualVersion=9;
  defense.x=50*256;defense.y=50*256;defense.range=10*256;
  defense.weapons[0].flags=GroundWeapon;defense.weapons[0].range=defense.range;
  defense.weapons[0].armorMask=1;
  if(duplicateDefense)w.actors[w.count++]=defense;
  auto&enemy=w.actors[w.count++];enemy.id=6000;enemy.kind=1;
  enemy.flags=Enemy|Saboteur;enemy.x=51*256;enemy.y=50*256;enemy.armor=0;
 }
 std::vector<Order> step(uint32_t flags=AutoOreReturn|AntiTheft){
  return planner.step(*world,flags,200,300,90,10,0,2,true);
 }
 void advance(int frames=1,uint32_t ticks=16){world->frame+=frames;world->tick+=ticks;}
 void accept(const std::vector<Order>&orders){for(const auto&o:orders)planner.accepted(o,world->frame);}
};
void omittedCase(){
 Scene scene(16);auto first=scene.step();
 check(first.size()==MaxOrders,"mixed batch reaches its exact capacity");
 check(count(first,MagnetronDropOrder)==16,"first batch returns sixteen drops");
 check(count(first,DefenseOrder)==0,"the defense candidate is omitted by final interleaving");
 scene.accept(first);scene.advance();auto next=scene.step();
 check(count(next,DefenseOrder)==1,"never-delivered defense is eligible on the next frame");
 auto found=std::find_if(next.begin(),next.end(),[](const Order&o){return o.purpose==DefenseOrder;});
 check(found!=next.end()&&found->unit==5000&&found->target==6000&&found->token==9,
       "recovered defense preserves actor, target and manual token");
 std::cout<<"omitted first_defense="<<count(first,DefenseOrder)<<" next_defense="<<count(next,DefenseOrder)<<'\n';
}
void returnedCooldownCase(int drops,int miners,bool duplicateDefense){
 Scene scene(drops,miners,duplicateDefense);auto first=scene.step();
 check(count(first,DefenseOrder)==1,"returned defense is unique even with duplicate raw actors");
 scene.accept(first);scene.advance(1,349);auto early=scene.step();
 check(count(early,DefenseOrder)==0,"actually returned defense retains its 350ms cooldown");
 scene.advance(1,1);auto due=scene.step();
 check(count(due,DefenseOrder)==1,"returned defense becomes eligible exactly at 350ms");
 std::cout<<"returned drops="<<drops<<" miners="<<miners<<" duplicate="<<duplicateDefense
          <<" first="<<count(first,DefenseOrder)<<" early="<<count(early,DefenseOrder)
          <<" due="<<count(due,DefenseOrder)<<'\n';
}
void lifecycleCase(){
 Scene scene(0,0);auto first=scene.step();
 for(const auto&o:first)scene.planner.expired(o);
 auto expired=scene.step();check(count(expired,DefenseOrder)==1,"expired submission permits same-frame retry");
 scene.advance();check(scene.step(0).empty(),"disabled defense produces no new order");
 scene.advance();check(count(scene.step(),DefenseOrder)==1,"enable after disable clears old cooldown");
 ++scene.world->match;scene.world->frame=1;scene.world->tick=10;
 check(count(scene.step(),DefenseOrder)==1,"match change cannot inherit defense cooldown");
}
void hostTaskCase(){
 Scene scene(16);Task task;TaskInput input;
 input.generation=4;input.serial=1;input.parameters.flags=AutoOreReturn|AntiTheft;
 input.parameters.drop=true;input.world=std::shared_ptr<const World>(std::move(scene.world));
 auto first=task.run(input);
 check(first&&count(first->orders,DefenseOrder)==0,"host task retains bounded final merge");
 for(const auto&o:first->orders)input.receipts.push_back({o,first->frame,Receipt::Accepted});
 auto nextWorld=std::make_shared<World>(*input.world);++nextWorld->frame;nextWorld->tick+=16;
 input.world=nextWorld;++input.serial;auto next=task.run(input);
 check(next&&count(next->orders,DefenseOrder)==1,"host worker retries an omitted defense without phantom cooldown");
 check(next&&next->generation==4&&next->serial==2&&next->match==7,"host output retains request provenance");
 std::cout<<"host_task first="<<count(first->orders,DefenseOrder)<<" next="<<count(next->orders,DefenseOrder)<<'\n';
}
}
int main(){
 omittedCase();returnedCooldownCase(15,20,false);returnedCooldownCase(0,0,false);
 returnedCooldownCase(0,20,true);lifecycleCase();hostTaskCase();
 std::cout<<(failures?"FAIL: ":"PASS: ")<<checks<<" checks, "<<failures<<" failures; real Planner final-batch defense lifecycle\n";
 return failures?1:0;
}
