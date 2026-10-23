#include "../source/external_planner.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace assist;using namespace assist::planner;
int main(){
 auto wp=std::make_unique<World>();auto&w=*wp;w.match=1;w.frame=100;w.tick=1000;w.count=3;w.intentCount=1;
 auto&a=w.actors[0];a.id=10;a.kind=2;a.flags=Own;a.x=a.y=10000;a.mission=2;a.queued=-1;a.destinationCell=30030;a.combatFirst=0;a.combatCount=2;
 auto&near=w.actors[1];near.id=20;near.flags=Enemy;near.cost=100;near.x=11792;near.y=10000;
 auto&far=w.actors[2];far.id=21;far.flags=Enemy;far.cost=9000;far.x=12500;far.y=10000;
 w.intents[0]={10,30030,0};w.combatCount=2;w.combat[0]={20,3,4,0.5f};w.combat[1]={21,2,0,1.0f};
 Planner p;auto step=[&](int priority=0){return p.step(w,AdvancedInput,200,300,90,10,priority,2);};
 auto o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20&&!o[0].destination);p.accepted(o[0],w.frame);
 ++w.frame;++w.tick;o=step();assert(o.empty()); // awaiting the real Attack, never Move
 a.target=20;a.mission=1;a.destinationCell=0;w.frame+=2;w.tick+=150;o=step(1);assert(o.empty()); // damage priority also cannot steal us out of range
 a.rearmStart=w.frame;a.rearmDuration=30;w.frame+=2;w.tick+=150;o=step();assert(o.empty());
 // Existing native pursuit of the same target is replaced with an empty-destination Attack.
 a.destinationCell=39046;w.frame+=2;w.tick+=150;o=step();assert(o.size()==1&&o[0].mission==1&&!o[0].destination);p.accepted(o[0],w.frame);a.destinationCell=0;
 // Out-of-range candidates get Move, never a premature Attack.
 w.combat[0].flags=2;w.frame+=15;w.tick+=150;o=step();assert(o.size()==1&&o[0].mission==2&&o[0].pursuit==21&&!o[0].target);p.accepted(o[0],w.frame);a.mission=2;a.target=0;a.destinationCell=o[0].destination;
 // As soon as a true range fact arrives, switch without waiting for the scan period.
 w.combat[0].flags=3;w.frame+=2;w.tick+=1;o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20);p.accepted(o[0],w.frame);a.mission=1;a.target=20;a.destinationCell=0;
 // No eligible enemy -> resume original waypoint, retaining the persistent intent.
 w.combatCount=0;a.combatCount=0;w.frame+=2;w.tick+=1;o=step();assert(o.size()==1&&o[0].mission==2&&o[0].destination==30030&&!o[0].pursuit);
 // Incomplete/torn pair sets never mean "safe to move".
 a.combatFirst=-1;w.frame+=1;o=step();assert(o.empty());a.combatFirst=8192;a.combatCount=1;w.frame+=1;o=step();assert(o.empty());
 // Manual command version ends all automatic orders.
 a.combatFirst=0;a.combatCount=0;a.manualVersion=1;w.frame+=1;o=step();assert(o.empty()&&!p.hasJobs());
 // A superseded host Move and a stale Attack do not blacklist a fresh target.
 p.reset();a.manualVersion=0;a.combatCount=2;w.combatCount=2;w.combat[0].flags=2;w.combat[1].flags=2;a.mission=5;a.destinationCell=0;w.frame+=2;
 o=step();assert(o.size()==1&&o[0].mission==2);p.rejected(o[0],w.frame);
 w.combat[0].flags=3;++w.frame;++w.tick;o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20);p.rejected(o[0],w.frame);
 ++w.frame;++w.tick;o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20);
 // Reaching the waypoint used to leave a sticky hold that blocked pursuit.
 p.reset();a.manualVersion=0;a.mission=5;a.target=0;a.combatCount=0;w.combatCount=0;a.x=30*256+128;a.y=30*256+128;a.destinationCell=0;w.frame+=10;
 o=step();assert(o.size()==1&&o[0].mission==2);p.accepted(o[0],w.frame);w.frame+=14;o=step();assert(o.size()==1&&o[0].mission==6);p.accepted(o[0],w.frame);
 a.combatCount=1;w.combatCount=1;w.combat[0]={20,2,0,1.0f};near.x=a.x+7*256;near.y=a.y;w.frame+=2;
 o=step();assert(o.size()==1&&o[0].mission==2&&o[0].pursuit==20);p.accepted(o[0],w.frame);
 w.combat[0].flags=3;w.frame+=2;o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20&&!o[0].destination);p.accepted(o[0],w.frame);
 a.mission=1;a.target=20;w.combat[0].flags=2;near.x+=256;w.frame+=2;o=step();assert(o.size()==1&&o[0].mission==2&&o[0].pursuit==20);
 // Minimum-range acquisition delegates repositioning to a native Attack;
 // its observed movement must not cause repeated Attack/Move resets.
 p.reset();a.mission=5;a.target=0;a.destinationCell=0;w.frame+=20;w.combat[0].flags=InAcquisitionRange|NeedsReposition;
 o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20&&!o[0].destination);p.accepted(o[0],w.frame);
 a.mission=1;a.target=20;a.destinationCell=40040;w.frame+=20;o=step();assert(o.empty());
 // A destination pointer alone must not keep minimum-range Attack waiting.
 w.frame+=4;o=step();assert(o.size()==1&&o[0].mission==2&&o[0].pursuit==20);p.accepted(o[0],w.frame);a.mission=2;a.destinationCell=o[0].destination;
 ++w.frame;assert(step().empty());w.combat[0].flags=InAcquisitionRange|InWeaponRange;w.frame+=2;o=step();assert(o.size()==1&&o[0].mission==1&&o[0].target==20);
 std::cout<<"PASS in-range priority, delayed Attack suppression, no-chase reset, approach-only Move, immediate range entry, waypoint resume, incomplete samples and manual cancellation\n";
}
