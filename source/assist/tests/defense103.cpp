#include "../source/external_planner.h"
#include "../source/construction_policy.h"
#include <memory>
#include <cassert>
#include <iostream>
using namespace assist;using namespace assist::planner;
int main(){
 auto w=std::make_unique<World>();auto p=std::make_unique<Planner>();w->match=1;w->frame=1;w->tick=100;w->count=4;
 auto&a=w->actors[0];a.id=10;a.kind=3;a.flags=Own|PointDefense;a.x=a.y=5000;a.range=1024;a.weapons[0]={1024,0,GroundWeapon,0x7ff};
 auto&b=w->actors[1];b.id=20;b.kind=1;b.flags=Enemy|Saboteur;b.x=6024;b.y=5000;
 auto&c=w->actors[2];c=b;c.id=30;c.x=6025;
 auto&d=w->actors[3];d=b;d.id=40;d.flags=Enemy;d.x=5001;a.target=40;
 auto out=p->step(*w,AntiTheft,200,300,90);assert(out.size()==1&&out[0].target==20&&out[0].purpose==DefenseOrder&&out[0].mission==1);
 ++w->frame;w->tick+=16;assert(p->step(*w,AntiTheft,200,300,90).empty());
 a.target=20;w->tick=1000;++w->frame;assert(p->step(*w,AntiTheft,200,300,90).empty()); // retain current high priority
 a.target=40;b.flags|=Cloaked;w->tick+=1000;++w->frame;assert(p->step(*w,AntiTheft,200,300,90).empty()); // no reveal & no range+3
 b.flags=Own|Saboteur;c.flags=Own|Saboteur;w->tick+=1000;++w->frame;assert(p->step(*w,AntiTheft,200,300,90).empty());
 b.flags=Enemy|Saboteur;w->tick+=1000;++w->frame;assert(p->step(*w,0,200,300,90).empty());
 // Known warhead effectiveness excludes exactly two percent.
 a.weapons[0].flags|=EffectivenessKnown;a.weapons[0].effectiveness[0]=.02f;assert(!compatibleTarget(a,b,false));a.weapons[0].effectiveness[0]=.02001f;assert(compatibleTarget(a,b,false));
 p->reset();w->count=3;w->intentCount=1;w->intents[0]={10,20020,0,OrdinaryMove};a.kind=2;a.flags=Own;a.target=0;
 b.flags=c.flags=Enemy;b.x=5100;c.x=5150;b.armor=0;c.armor=1;b.cost=1000;c.cost=10;a.weapons[0].effectiveness[0]=.1f;a.weapons[0].effectiveness[1]=1.f;
 ++w->frame;w->tick+=1000;out=p->step(*w,AdvancedInput,200,300,90,10,1);assert(out.size()==1&&out[0].target==30);
 p->reset();out=p->step(*w,AdvancedInput,200,300,90,10,0);assert(out.size()==1&&out[0].target==20);
 std::vector<int> owned(4);owned[0]=1;std::vector<int> groups[6];groups[0]={1,2};
 assert(!construction::available({0,-1},{},owned,groups));assert(construction::available({0,-1},{},owned,groups,2));assert(!construction::available({0,-7},{},owned,groups,2));assert(construction::available({3},{0},owned,groups));
 std::cout<<"PASS: defense threat retention, range boundary, enemy eligibility, hidden targets, debounce, opt-in, 2% exclusion, both targeting priorities, prerequisite preview\n";
}
