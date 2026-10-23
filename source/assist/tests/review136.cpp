#define main prior_enemy_test
#include "enemy_markers.cpp"
#undef main
#include "../../external_reader/source/passenger_bubbles.h"
#include "../../external_reader/source/radar_map.h"
#include "../source/external_planner.h"
#include "../source/magnetron_drop.h"
#include <memory>
int main(){using F=EnemyFixture;
 {F f;auto&mem=f.mem;mem.put<uint8_t>(F::foe+0x431,1);mem.put<int>(F::foe+0x114,2);mem.put(F::foe+0x118,F::infantry);mem.put(F::infantry+0x11C,F::foe);
  constexpr uint32_t second=0x110000;f.object(second,80,F::enemy,1,F::infType);mem.put(second+0x11C,F::foe);mem.put(F::infantry+0x30,second);
  external::Projection p;p.width=1920;p.height=1080;p.cameraX=-320;p.cameraY=800;
  auto sample=[&](bool a,bool b){return external::passengers::sample(f.reader,F::local,p,a,b);};
  assert(sample(false,false).empty());auto b=sample(true,false);assert(b.size()==1&&b[0].groups.size()==1&&b[0].groups[0].count==2&&!b[0].dangerous);
  mem.put<uint8_t>(F::foe+0x431,0);assert(sample(true,false).empty()&&sample(false,true).empty());
  for(int off:{0xEAE,0xEBE,0xEC2,0xEC3}){mem.put<uint8_t>(F::infType+off,1);b=sample(false,true);assert(b.size()==1&&b[0].dangerous);mem.put<uint8_t>(F::infType+off,0);}
  mem.put<uint8_t>(F::foe+0x83,1);assert(sample(true,false).size()==1);mem.put<int>(F::foe+0x220,1);assert(sample(true,false).empty());mem.put<int>(F::foe+0x220,0);
  mem.put(second+0x30,F::infantry);assert(sample(true,false).empty());mem.put<uint32_t>(second+0x30,0);mem.put<uint32_t>(second+0x11C,0);assert(sample(true,false).empty());mem.put(second+0x11C,F::foe);
  mem.put<int>(F::foe+0x114,0);assert(sample(true,false).empty());
 }
 {F f;constexpr uint32_t factory=0x740000;const uint32_t offsets[]={0x53BC,0x53CC,0x53B0,0x53B4};
  for(int tab=0;tab<4;++tab){uint32_t at=factory+tab*0x100;f.mem.put(F::local+offsets[tab],at);f.mem.put(at+0x6C,F::local);}
  auto sample=[&](){f.now+=101;f.next();f.sampler.sample(f.now,assist::Production);};sample();assert(f.sampler.idleProductionMask==15&&!f.sampler.pausedProductionMask);
  for(int tab=0;tab<4;++tab){uint32_t at=factory+tab*0x100;f.mem.put(at+0x58,F::foe);}
  sample();assert(!f.sampler.idleProductionMask&&!f.sampler.pausedProductionMask);
  f.mem.put<uint8_t>(factory+0x5C,1);f.mem.put<uint8_t>(factory+0x100+0x71,1);sample();assert(!f.sampler.idleProductionMask&&f.sampler.pausedProductionMask==3);
  f.mem.put<int>(factory+0x24,54);sample();assert(f.sampler.pausedProductionMask==2);
 }
 {using namespace assist;using namespace planner;assert(magnetdrop::lethal({true,true,false,true,1,0}));assert(magnetdrop::lethal({true,true,false,false,200,7}));
  assert(!magnetdrop::lethal({true,true,false,false,200,0}));assert(!magnetdrop::lethal({true,true,true,true,200,7}));assert(!magnetdrop::lethal({true,false,false,true,200,7}));
  auto wp=std::make_unique<World>();auto&w=*wp;w.match=1;w.frame=100;w.count=1;auto&a=w.actors[0];a.id=1;a.kind=2;a.flags=Own|DropHazard;a.liftTarget=2;a.manualVersion=4;a.x=a.y=5000;a.range=2048;a.weapons[0]={2048,0,GroundWeapon|MagneticWeapon|LocomotorWeapon,0x7ff};
  Planner p;auto step=[&](bool enabled){return p.step(w,0,200,300,90,10,0,2,enabled);};assert(step(false).empty());auto orders=step(true);assert(orders.size()==1&&orders[0].purpose==MagnetronDropOrder&&orders[0].refinery==2&&orders[0].mission==13);p.accepted(orders[0],w.frame);
  for(int i=0;i<29;++i){++w.frame;assert(step(true).empty());}++w.frame;assert(step(true).size()==1);
  ++w.frame;a.flags=Own;a.liftTarget=0;assert(step(true).empty());++w.frame;a.liftTarget=3;a.flags|=DropHazard;orders=step(true);assert(orders.size()==1&&orders[0].refinery==3);p.accepted(orders[0],w.frame);
  ++w.frame;assert(step(false).empty());++w.frame;assert(step(true).size()==1);
 }
 std::cout<<"PASS passenger hover/selection/danger capabilities/list corruption/unload; all queue states; independent drop toggle/stop dedup/release/retry\n";
}
