#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace assist;using namespace assist::planner;
static void init(World&w){
 w={};w.match=19;w.frame=1;w.count=2;w.refineryCount=1;
 auto&miner=w.actors[0];miner.id=7;miner.kind=2;miner.type=200;miner.flags=Own|Harvester;miner.storageLimit=40;miner.storage[0]=40;miner.mission=10;miner.dockTypes[0]=900;miner.x=miner.y=5000;
 auto&ref=w.actors[1];ref.id=8;ref.kind=3;ref.type=900;ref.flags=Own;ref.x=8000;ref.y=8000;
 auto&area=w.refineries[0];area.id=8;area.count=1;area.waiting[0]={9009,0,0};
}
int main(){
 auto wp=std::make_unique<World>();auto&w=*wp;Planner p;init(w);
 p.step(w,AutoOreReturn,200,300,90);
 w.frame=2;w.actors[0].flags|=Unloading;w.actors[0].storage[0]=10;
 p.step(w,AutoOreReturn,200,300,90);assert(!p.oreDelivered);
 w.frame=3;w.actors[0].storage[0]=0;
 p.step(w,AutoOreReturn,200,300,90);assert(p.oreDelivered==1&&p.oreUnverified==0);
 p.reset();init(w);p.step(w,AutoOreReturn,200,300,90);
 w.frame=2;w.actors[0].storage[0]=0;
 p.step(w,AutoOreReturn,200,300,90);assert(p.oreDelivered==0&&p.oreUnverified==1);
 std::cout<<"PASS: ore unload success requires observed unloading then empty hold; no docking evidence => unverified\n";
}
