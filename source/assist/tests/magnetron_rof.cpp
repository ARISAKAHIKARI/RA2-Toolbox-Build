#include "../source/external_planner.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace assist;using namespace assist::planner;
int main(){
 auto wp=std::make_unique<World>();auto& w=*wp;Planner p;
 w.match=1;w.frame=1;w.count=3;w.intentCount=1;
 auto& a=w.actors[0];a.id=10;a.kind=2;a.flags=Own;a.x=5000;a.y=5000;a.range=2048;a.rearmStart=0;
 a.weapons[0]={2048,0,GroundWeapon|MagneticWeapon|LocomotorWeapon,0x7ff};
 w.intents[0]={10,30030,0,MagnetronMove};
 for(int i=1;i<3;i++){auto& t=w.actors[i];t.id=20+i;t.kind=2;t.flags=Enemy;t.armor=0;t.x=5500;t.y=5050;t.cost=i==1?200:100;}
 auto first=p.step(w,MagnetronInput,200,300,90);
 assert(first.size()==1&&first[0].target==21);
 p.accepted(first[0],w.frame);a.target=21;
 ++w.frame;a.rearmStart=2;a.rearmDuration=4;
 auto waiting=p.step(w,MagnetronInput,200,300,90);
 assert(waiting.empty());
 // A fired shot, but no lifted target and the previous target still viable.
 // As soon as ROF recovers, issue the next standard Attack for target 22.
 w.frame=6;
 auto next=p.step(w,MagnetronInput,200,300,90);
 assert(next.size()==1&&next[0].target==22&&next[0].mission==1);
 p.accepted(next[0],w.frame);
 w.frame=7;
 assert(p.step(w,MagnetronInput,200,300,90).empty());
 std::cout<<"PASS: observed rearm -> first-ready-frame target rotation without editing game weapons\n";
}
