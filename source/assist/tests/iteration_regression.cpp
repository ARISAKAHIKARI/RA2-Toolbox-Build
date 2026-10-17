#include "../source/external_planner.h"
#include <cassert>
#include <limits>
#include <memory>
#include <iostream>
using namespace assist;using namespace assist::planner;
int main(){
 auto memory=std::make_unique<World>();auto&w=*memory;w.match=1;w.frame=1;w.count=2;
 auto&a=w.actors[0];a.id=1;a.kind=2;a.flags=Own|Harvester;a.x=a.y=3000;a.storageLimit=28;a.storage[0]=27.5f;a.storage[1]=.5f;a.dockTypes[0]=900;a.mission=10;
 auto&r=w.actors[1];r.id=2;r.kind=3;r.flags=Own;r.type=900;r.x=r.y=5000;
 Planner p;auto out=p.step(w,AutoOreReturn,200,300,90);assert(out.size()==1&&out[0].mission==7);
 p.reset();a.storage[0]=27.25f;++w.frame;assert(p.step(w,AutoOreReturn,200,300,90).empty());
 a.storage[0]=28;a.storage[1]=std::numeric_limits<float>::quiet_NaN();++w.frame;assert(p.step(w,AutoOreReturn,200,300,90).empty());
 // A declined release remains tracked. A confirmed release immediately ends
 // tracking and is not counted as a new automatic docking assignment.
 p.reset();a.storage[0]=28;a.storage[1]=0;r.radioIDs[0]=999;w.refineryCount=1;w.refineries[0].id=2;w.refineries[0].count=1;w.refineries[0].waiting[0]={20020,0,0};
 ++w.frame;out=p.step(w,AutoOreReturn,200,300,90);assert(out.size()==1&&out[0].mission==2);p.accepted(out[0],w.frame);
 ++w.frame;out=p.step(w,0,200,300,90);assert(out.size()==1&&out[0].purpose==2&&p.hasJobs());
 p.rejected(out[0],w.frame);++w.frame;out=p.step(w,0,200,300,90);assert(out.size()==1&&p.hasJobs());
 p.accepted(out[0],w.frame);assert(!p.hasJobs()&&p.issued==0);
 // A full armed miner may be explicitly commanded to advance/attack. Its
 // combat intent takes precedence even on the planner's very first snapshot.
 p.reset();w.count=3;w.intentCount=1;w.intents[0]={1,30030,0};a.range=1024;a.weapons[0]={1024,0,GroundWeapon,0x7ff};
 auto&e=w.actors[2];e.id=3;e.kind=2;e.flags=Enemy;e.x=e.y=3100;e.cost=500;
 ++w.frame;out=p.step(w,AutoOreReturn|AdvancedInput,200,300,90);
 assert(out.size()==1&&out[0].unit==1&&out[0].mission==1&&out[0].purpose==1&&p.queued==0&&p.docking==0);
 p.accepted(out[0],w.frame);a.target=3;
 ++w.frame;assert(p.step(w,AutoOreReturn|AdvancedInput,200,300,90).empty());
 std::cout<<"PASS: mixed fractional ore, incomplete load, invalid storage, release retries and acknowledgement-only release completion\n";
}
