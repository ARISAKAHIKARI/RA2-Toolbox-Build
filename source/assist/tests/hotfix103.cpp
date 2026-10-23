#include "../source/external_planner.h"
#include "../source/pointer_intent.h"
#include "../source/idle_aircraft_policy.h"
#include <memory>
#include <cassert>
#include <iostream>
using namespace assist;using namespace assist::planner;
int main(){
 auto w=std::make_unique<World>();auto p=std::make_unique<Planner>();w->match=1;w->frame=1;w->tick=100;w->count=3;w->intentCount=1;
 auto&a=w->actors[0];a.id=10;a.kind=2;a.flags=Own;a.x=a.y=5000;a.range=1024;a.weapons[0]={1024,0,GroundWeapon,0x7ff};
 w->intents[0]={10,20020,0,OrdinaryMove};
 auto&b=w->actors[1];b.id=20;b.kind=2;b.flags=Enemy;b.x=5000+1024+3*256;b.y=5000;b.cost=100;
 auto&c=w->actors[2];c=b;c.id=30;c.x++;c.cost=1000;
 // Since the native-range probe interface, the planner cannot synthesize
 // these facts from a primary weapon. Supply the sampled candidate/range set.
 w->combatCount=2;a.combatFirst=0;a.combatCount=2;
 w->combat[0].target=20;w->combat[0].flags=InAcquisitionRange;
 w->combat[1].target=30;w->combat[1].flags=0;
 auto orders=p->step(*w,AdvancedInput,200,300,90,10);assert(orders.size()==1&&orders[0].mission==2&&orders[0].pursuit==20);p->accepted(orders[0],1);a.mission=2;a.destinationCell=orders[0].destination;
 // Fire range remains native; the extra three cells authorize acquisition/chase.
 assert(!canTarget(a,b,int64_t(1792)*1792,false));assert(canTarget(a,b,int64_t(1792)*1792,false,768));assert(!canTarget(a,c,int64_t(1793)*1793,false,768));
 c.x=b.x-256;w->combat[1].flags=InAcquisitionRange;w->frame=14;w->tick=300;
 orders=p->step(*w,AdvancedInput,200,300,90,10);assert(orders.size()==1&&orders[0].mission==2&&orders[0].pursuit==30);p->accepted(orders[0],14);
 // Destination arrival and a long spell with no enemies do not disarm the task.
 w->count=1;a.x=a.y=20*256+128;w->frame=500;w->tick=10000;orders=p->step(*w,AdvancedInput,200,300,90,10);for(auto&o:orders)p->accepted(o,w->frame);
 w->frame=1000;w->tick=20000;p->step(*w,AdvancedInput,200,300,90,10);assert(p->hasJobs());
 w->count=2;b.x=a.x+100;b.y=a.y;w->combat[0].flags=InAcquisitionRange|InWeaponRange;w->frame=1002;w->tick=20100;orders=p->step(*w,AdvancedInput,200,300,90,10);assert(orders.size()==1&&orders[0].mission==1&&orders[0].target==20);
 w->intentCount=0;++w->frame;w->tick+=100;p->step(*w,AdvancedInput,200,300,90,10);assert(!p->hasJobs());
 idleaircraft::View plane{4,4,5,-1,true,false,false,true,false};assert(idleaircraft::idle(plane));
 plane.ammo=3;assert(!idleaircraft::idle(plane));plane.ammo=4;plane.airborne=true;plane.docked=false;assert(!idleaircraft::idle(plane));
 plane.mission=7;plane.returning=true;plane.target=true;assert(idleaircraft::idle(plane));plane.queued=1;assert(!idleaircraft::idle(plane));
 plane.queued=-1;plane.mission=1;assert(!idleaircraft::idle(plane));plane.maximum=0;assert(!idleaircraft::idle(plane));
 uint8_t keys[256]{};uint32_t buttons[pointerinput::MouseCount]{};buttons[0]=6;
 auto route=pointerinput::resolveMouse(keys,6,buttons,{0,0,0,0,0,0,0,0,pointerinput::Advanced},0);
 assert(route.operation==pointerinput::Enhanced&&(route.bindings&pointerinput::Advanced)&&(route.bindings&pointerinput::LeftClick));
 assert(pointerinput::resolveMouse(keys,5,buttons,{},0).operation==pointerinput::Cancel);
 std::cout<<"PASS: range+3 boundary, native fire range, periodic retarget, persistent idle, cancellation, left mouse route\n";
}
