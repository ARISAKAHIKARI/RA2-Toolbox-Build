#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace assist;using namespace assist::planner;
int main(){
 auto data=std::make_unique<World>();auto&w=*data;Planner p;w.match=1;w.frame=1;w.count=8;w.intentCount=1;
 auto&a=w.actors[0];a.id=10;a.kind=2;a.flags=Own;a.x=a.y=5000;a.range=2048;a.mission=2;a.rearmStart=-1;
 a.weapons[0]={2048,256,GroundWeapon|MagneticWeapon|LocomotorWeapon,0x7FF};w.intents[0]={10,30030,0,MagnetronMove};
 for(int i=1;i<8;++i){auto&e=w.actors[i];e.id=20+i;e.kind=2;e.flags=Enemy;e.cost=100*i;e.x=5500;e.y=5100;}
 w.actors[7].flags|=Airborne;w.actors[6].flags|=Manipulated;w.actors[5].kind=3;w.actors[4].flags|=Organic;
 w.actors[3].flags|=Protected;w.actors[2].armor=7;a.weapons[0].armorMask&=~(1u<<7);
 assert(p.step(w,AdvancedInput,200,300,90).empty());++w.frame;
 auto orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].purpose==3&&orders[0].target==21);p.accepted(orders[0],w.frame);
 // A current attack has not necessarily lifted anything. Don't change/reset it
 // while its native shot is pending, even if a more valuable unit appears.
 a.target=21;w.actors[2].armor=0;++w.frame;assert(p.step(w,MagnetronInput,200,300,90).empty());
 // A confirmed lift can rotate even while native rearm runs or is paused.
 a.rearmStart=w.frame;a.rearmDuration=10;w.actors[1].flags|=Manipulated;
 ++w.frame;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].target==22);
 a.rearmStart=-1;w.frame=100;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].target==22);
 a.rearmStart=3;w.frame=101;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].target==22);p.accepted(orders[0],w.frame);
 // Two magnets reserve distinct not-yet-lifted victims within a submitted batch.
 p.reset();w.frame=200;w.intentCount=2;w.count=9;w.actors[1].flags=Enemy;
 auto&second=w.actors[8];second=a;second.id=11;second.target=0;w.intents[1]={11,31031,0,MagnetronMove};
 orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==2&&orders[0].target!=orders[1].target);
 for(auto&o:orders)p.accepted(o,w.frame);
 ++w.frame;assert(p.step(w,MagnetronInput,200,300,90).empty());
 // An actual lift finishes a pending shot even when a very short ROF was missed.
 w.actors[1].flags|=Manipulated;w.actors[2].flags|=Manipulated;w.frame=203;
 orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==2&&orders[0].mission==2&&orders[1].mission==2);
 for(auto&o:orders)p.accepted(o,w.frame);
 w.frame=205;w.actors[1].flags=Enemy;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].target==21);
 a.manualVersion=1;++w.frame;p.step(w,MagnetronInput,200,300,90);assert(p.hasJobs());
 w.intentCount=0;++w.frame;assert(p.step(w,MagnetronInput,200,300,90).empty()&&!p.hasJobs());
 // Native Attack is allowed beyond firing range and inside minimum range.
 for(int offset:{128,2304}){
  p.reset();w.frame=300;w.count=2;w.intentCount=1;a.manualVersion=0;a.target=0;a.mission=5;a.x=a.y=5000;a.rearmStart=-1;a.rearmDuration=0;
  auto&e=w.actors[1];e.flags=Enemy;e.x=a.x+offset;e.y=a.y;
  orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].mission==1&&orders[0].target==e.id&&!orders[0].destination);p.accepted(orders[0],w.frame);
  a.target=e.id;a.mission=1;
  // Continuous motion past the old 45-frame and configured 90-frame deadlines
  // must not generate replacement orders or count an unobserved shot.
  for(int n=0;n<120;++n){++w.frame;a.x+=20;e.x+=20;assert(p.step(w,MagnetronInput,200,300,90).empty());}
  assert(!p.magnetUnverified&&!p.magnetShots);
 }
 p.reset();++w.frame;w.actors[1].x=a.x+3000;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.empty());
 // A stationary Attack outside either boundary must leave the wait state.
 // Global mode uses the same pursuit moves, and returns to Attack on entry.
 for(int offset:{128,2304}){p.reset();w.frame=600;w.count=2;w.intentCount=1;w.intents[0].global=1;
  a.x=a.y=5000;a.target=0;a.mission=5;a.destinationCell=0;a.rearmStart=-1;a.rearmDuration=0;
  auto&e=w.actors[1];e.x=a.x+offset;e.y=a.y;e.flags=Enemy;
  orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].mission==1);p.accepted(orders[0],w.frame);a.target=e.id;a.mission=1;
  for(int n=0;n<23;++n){++w.frame;assert(p.step(w,MagnetronInput,200,300,90).empty());}
  ++w.frame;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].mission==2&&orders[0].pursuit==e.id&&!orders[0].target);
  if(offset==128)assert(int(orders[0].destination%1000)*256+128<a.x);p.accepted(orders[0],w.frame);a.mission=2;a.destinationCell=orders[0].destination;
  for(int n=0;n<12;++n){++w.frame;assert(p.step(w,MagnetronInput,200,300,90).empty());}
  e.x=a.x+1024;++w.frame;orders=p.step(w,MagnetronInput,200,300,90);assert(orders.size()==1&&orders[0].mission==1&&orders[0].target==e.id);
 }
 // Basic armor / AA / AG / minimum range checks use the selected elite profile.
 Actor src{},t{};src.weapons[0]={2048,256,GroundWeapon,1u<<3};t.armor=3;t.kind=2;
 assert(canTarget(src,t,512*512,false));assert(!canTarget(src,t,128*128,false));
 t.flags=Airborne;assert(!canTarget(src,t,512*512,false));src.weapons[1]={1024,0,AirWeapon,1u<<3};assert(canTarget(src,t,512*512,false));
 t.armor=4;assert(!canTarget(src,t,512*512,false));t.armor=11;assert(!canTarget(src,t,512*512,false));
 std::cout<<"PASS: independent magnetron intents, actual cooldown and lift, pending-shot preservation, ground/organic/armor filtering, distinct victims, movement resume, manual cancellation and basic weapon compatibility\n";
}
