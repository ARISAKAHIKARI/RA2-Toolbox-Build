#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace assist;using namespace assist::planner;
int main(){
 auto storage=std::make_unique<World>();auto&w=*storage;Planner planner;
 w.match=1;w.frame=1;w.count=5;w.refineryCount=1;
 for(int i=0;i<2;++i){auto&a=w.actors[i];a.id=uint32_t(i+1);a.type=200;a.kind=2;a.flags=Own|Harvester;a.mission=10;a.x=3000+i*100;a.y=3000;a.storageLimit=28;a.storage[0]=28;a.dockTypes[0]=900;}
 auto&ref=w.actors[2];ref.id=50;ref.type=900;ref.kind=3;ref.flags=Own;ref.x=4000;ref.y=4000;ref.radioIDs[0]=80;
 auto&far=w.actors[3];far.id=60;far.type=900;far.kind=3;far.flags=Own;far.x=12000;far.y=12000;
 auto&unplaced=w.actors[4];unplaced.id=70;unplaced.type=900;unplaced.kind=3;unplaced.flags=Own|OffMap;unplaced.x=3000;unplaced.y=3000;
 auto&area=w.refineries[0];area.id=50;area.count=3;for(int i=0;i<3;++i)area.waiting[i]={uint32_t(16016+i),0,0};
 auto orders=planner.step(w,AutoOreReturn,200,300,90);assert(orders.size()==2);
 assert(orders[0].unit==1&&orders[1].unit==2&&orders[0].mission==2&&orders[1].mission==2);
 assert(orders[0].destination!=orders[1].destination);assert(planner.queued==2);
 for(auto&o:orders)planner.accepted(o,w.frame);
 ref.radioIDs[0]=0;++w.frame;orders=planner.step(w,AutoOreReturn,200,300,90);
 assert(orders.size()==1&&orders[0].unit==1&&orders[0].mission==7&&orders[0].destination==50);planner.accepted(orders[0],w.frame);
 w.actors[0].storage[0]=0;++w.frame;orders=planner.step(w,AutoOreReturn,200,300,90);
 assert(orders.size()==1&&orders[0].unit==2&&orders[0].destination==50);planner.accepted(orders[0],w.frame);
 w.actors[1].manualVersion=1;w.actors[1].flags|=OrePaused;++w.frame;orders=planner.step(w,AutoOreReturn,200,300,90);assert(orders.empty());
 w.actors[1].storage[0]=0;++w.frame;planner.step(w,AutoOreReturn,200,300,90);
 w.actors[1].storage[0]=28;++w.frame;orders=planner.step(w,AutoOreReturn,200,300,90);assert(orders.empty());
 w.actors[1].flags&=~OrePaused;++w.frame;orders=planner.step(w,AutoOreReturn,200,300,90);assert(orders.size()==1&&orders[0].token==1);
 std::cout<<"PASS: nearest refinery, occupied dock, FIFO and manual pause persists across empty/full changes until mining resumes\n";
 // Disable while queued: parked trucks receive one native refinery release,
 // and stop tracking only after the outgoing event has been accepted.
 planner.reset();w.frame=30;ref.radioIDs[0]=80;w.actors[0].storage[0]=28;w.actors[1].storage[0]=0;
 orders=planner.step(w,AutoOreReturn,200,300,90);assert(orders.size()==1&&orders[0].mission==2);planner.accepted(orders[0],w.frame);
 ++w.frame;orders=planner.step(w,0,200,300,90);assert(orders.size()==1&&orders[0].purpose==2&&orders[0].mission==7&&planner.hasJobs());planner.accepted(orders[0],w.frame);
 ++w.frame;orders=planner.step(w,0,200,300,90);assert(orders.empty()&&!planner.hasJobs());
 planner.reset();w={};w.match=2;w.frame=1;w.count=4;w.intentCount=1;
 auto&a=w.actors[0];a.id=10;a.kind=2;a.flags=Own;a.range=1024;a.weapons[0]={1024,0,GroundWeapon,0x7FF};a.x=a.y=5000;a.mission=2;
 for(int i=1;i<4;++i){auto&e=w.actors[i];e.id=uint32_t(20+i);e.flags=Enemy;e.cost=500;e.x=e.y=5100;}
 w.actors[1].veterancy=2;w.actors[3].flags=0;w.actors[3].cost=999999;
 w.intents[0]={10,30030,0};orders=planner.step(w,AdvancedInput,200,300,90);
 assert(orders.size()==1&&orders[0].mission==1&&orders[0].target==21);planner.accepted(orders[0],w.frame);
 w.frame=3;w.actors[1].flags|=Cloaked;orders=planner.step(w,AdvancedInput,200,300,90);
 assert(orders.size()==1&&orders[0].target==22);planner.accepted(orders[0],w.frame);
 w.count=1;w.frame=4;orders=planner.step(w,AdvancedInput,200,300,90);
 assert(orders.size()==1&&orders[0].mission==2&&orders[0].destination==30030);planner.accepted(orders[0],w.frame);
 w.frame=96;orders=planner.step(w,AdvancedInput,200,300,90);assert(orders.size()==1&&orders[0].mission==6);planner.accepted(orders[0],w.frame);
 w.frame=97;w.intentCount=0;orders=planner.step(w,AdvancedInput,200,300,90);assert(orders.empty());
 // Local clicks can change while the simulation frame is paused / unchanged.
 w.intentCount=1;w.inputSerial=1;w.intents[0]={10,31031,1};a.manualVersion=1;
 planner.step(w,AdvancedInput,200,300,90);assert(planner.trackingFlags()&AdvancedInput);
 w.intentCount=0;++w.inputSerial;planner.step(w,AdvancedInput,200,300,90);assert(!planner.hasJobs());
 // A target still allocated but off-map must never be ranked as an enemy.
 ++w.frame;w.count=2;w.intentCount=1;w.intents[0]={10,31031,1};w.actors[1].flags=Enemy|OffMap;
 orders=planner.step(w,AdvancedInput,200,300,90);assert(orders.empty());
 std::cout<<"PASS: rank priority, neutral/cloaked exclusion, destination resume, stationary timeout and intent cancellation using snapshots only\n";
}
