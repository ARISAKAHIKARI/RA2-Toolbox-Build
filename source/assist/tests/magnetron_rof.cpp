#include "../source/external_planner.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace assist;using namespace assist::planner;
int main(){
 auto wp=std::make_unique<World>();auto&w=*wp;Planner p;
 w.match=1;w.frame=1;w.count=3;w.intentCount=1;
 auto&a=w.actors[0];a.id=10;a.kind=2;a.flags=Own;a.x=a.y=5000;a.range=2048;a.rearmStart=0;a.mission=1;
 a.weapons[0]={2048,0,GroundWeapon|MagneticWeapon|LocomotorWeapon,0x7ff};
 w.intents[0]={10,30030,0,MagnetronMove};
 for(int i=1;i<3;i++){auto&t=w.actors[i];t.id=20+i;t.kind=2;t.flags=Enemy;t.x=5500;t.y=5050;t.cost=i==1?200:100;}
 auto step=[&](){return p.step(w,MagnetronInput,200,300,90);};
 auto order=step();assert(order.size()==1&&order[0].target==21);p.accepted(order[0],w.frame);a.target=21;
 // An old timer alone is not proof that the requested shot fired.
 ++w.frame;a.rearmDuration=40;assert(step().empty());
 // Retarget through ordinary events while the native game still owns ROF.
 ++w.frame;a.rearmStart=3;order=step();assert(order.size()==1&&order[0].target==22);
 p.accepted(order[0],w.frame);a.target=22;
 ++w.frame;a.rearmStart=-1;w.actors[2].flags|=Manipulated;
 order=step();assert(order.size()==1&&order[0].target==21);
 p.accepted(order[0],w.frame);a.target=21;
 // Same admission-frame fire is visible in the following snapshot.
 w.actors[2].flags=Enemy;a.rearmStart=w.frame;++w.frame;
 order=step();assert(order.size()==1&&order[0].target==22);
 // One remaining eligible target must not cause a Move or a blacklist stall.
 p.reset();w.frame=100;w.count=2;a.rearmStart=0;a.target=0;
 order=step();assert(order.size()==1&&order[0].target==21);p.accepted(order[0],w.frame);
 a.rearmStart=100;++w.frame;assert(step().empty());
 w.frame=112;order=step();assert(order.size()==1&&order[0].mission==1&&order[0].target==21);
 assert(a.rearmStart==100&&a.rearmDuration==40);
 std::cout<<"PASS: pending shot, active/paused rearm rotation, same-frame fire and single-target fallback\n";

}
