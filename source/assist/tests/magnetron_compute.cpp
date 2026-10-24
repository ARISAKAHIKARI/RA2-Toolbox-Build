#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <set>
#include <iostream>
using namespace assist;using namespace assist::planner;
int main(){
 auto data=std::make_unique<World>();auto&w=*data;w.match=1;w.frame=1;w.count=6;w.intentCount=2;
 for(int n=0;n<3;++n){auto&a=w.actors[n];a.id=10+n;a.kind=2;a.flags=Own;a.x=a.y=5000;a.range=2048;a.rearmStart=0;
  a.weapons[0]={2048,0,GroundWeapon|MagneticWeapon|LocomotorWeapon,0x7ff};}
 w.intents[0]={10,30030,0,MagnetronMove,0};w.intents[1]={11,30030,0,MagnetronMove,1};
 w.actors[2].liftTarget=21;w.actors[2].flags|=DropHazard;
 for(int n=3;n<6;++n){auto&a=w.actors[n];a.id=18+n;a.kind=2;a.flags=Enemy;a.x=5500;a.y=5050;a.cost=200;}
 w.actors[3].flags|=Manipulated;
 for(unsigned combo=0;combo<8;++combo){uint32_t mask=(combo&1?1u<<compute::Magnetron:0)|(combo&2?1u<<compute::MagnetronGlobal:0)|(combo&4?1u<<compute::MagnetronDrop:0);
  for(bool hazard:{false,true}){w.actors[0].liftTarget=hazard?21:0;w.actors[0].flags=Own|(hazard?DropHazard:0);std::set<unsigned> attacks,drops;
   for(bool inside:{false,true}){Planner planner;auto result=planner.step(w,compute::plannerFlags(MagnetronInput,mask,inside),200,300,90,0,0,2,compute::inside(mask,compute::MagnetronDrop)==inside,compute::magnetronIntents(mask,inside),true);
    for(const auto&o:result){if(o.purpose==MagnetronDropOrder){assert(compute::inside(mask,compute::MagnetronDrop)==inside&&drops.insert(o.unit).second);}
     else{assert(o.purpose==MagnetronOrder&&o.mission==1&&o.unit!=12);assert(compute::inside(mask,o.unit==10?compute::Magnetron:compute::MagnetronGlobal)==inside&&attacks.insert(o.unit).second);}}
   }
   assert(attacks==(hazard?std::set<unsigned>({11}):std::set<unsigned>({10,11})));
   assert(drops==(hazard?std::set<unsigned>({10,12}):std::set<unsigned>({12})));
  }
 }
 std::cout<<"PASS all eight independent magnet calculation combinations; hazard release suppresses attacks in both backends\n";
}
