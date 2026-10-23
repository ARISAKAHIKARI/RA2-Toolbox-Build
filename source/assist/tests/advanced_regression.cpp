#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace assist;using namespace assist::planner;
static void setup(World&w){
 w={};w.match=1;w.frame=1;w.count=3;w.intentCount=1;
 auto&a=w.actors[0];a.id=10;a.kind=2;a.flags=Own;a.x=a.y=5000;a.range=2048;a.rearmStart=-1;a.weapons[0]={2048,0,GroundWeapon,0x7ff};w.intents[0]={10,30030,0};
 for(int i=1;i<3;++i){auto&e=w.actors[i];e.id=20+i;e.kind=2;e.flags=Enemy;e.x=5200+i*100;e.y=5000;e.cost=500;}
}
int main(){auto memory=std::make_unique<World>();auto&w=*memory;Planner p;setup(w);auto&a=w.actors[0];
 auto v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==21);p.accepted(v[0],w.frame);a.target=21;
 for(int f=3;f<20;++f){w.frame=f;w.actors[2].x=5100+(f%2)*20;assert(p.step(w,AdvancedInput,200,300,30).empty());}
 w.frame=20;w.actors[2].cost=900;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==22);p.accepted(v[0],20);a.target=22;
 // Rearm no longer prevents periodic target evaluation; a native burst is
 // still allowed to finish before a target change.
 w.frame=21;a.rearmStart=20;a.rearmDuration=30;w.actors[1].cost=1500;
 assert(p.step(w,AdvancedInput,200,300,30).empty()&&p.targetChecks>0);
 w.frame=22;w.actors[2].flags|=Cloaked;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==21);p.accepted(v[0],22);a.target=21;
 w.frame=51;a.flags|=BurstFiring;w.actors[2].flags=Enemy;w.actors[2].cost=2000;
 assert(p.step(w,AdvancedInput,200,300,30).empty()&&p.targetChecks==0);
 w.frame=52;a.flags&=~BurstFiring;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==22);
 // A failed enqueue is reported per order: back off only the failed target,
 // immediately choose an alternative, and never mark it as an accepted shot.
 p.reset();setup(w);v=p.step(w,AdvancedInput,200,300,30);assert(v[0].target==21);p.rejected(v[0],1);
 w.frame=2;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==22);
 // An accepted native order that never takes effect gets a bounded grace,
 // rather than repeated attack resets or the old 45-frame retry wait.
 p.reset();setup(w);v=p.step(w,AdvancedInput,200,300,30);p.accepted(v[0],1);
 for(int f=2;f<13;++f){w.frame=f;assert(p.step(w,AdvancedInput,200,300,30).empty());}
 w.frame=13;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==22);
 // A target assigned forever with no actual firing/motion can no longer reset
 // its own path timeout just because it remains the most valuable target.
 p.reset();setup(w);v=p.step(w,AdvancedInput,200,300,30);p.accepted(v[0],1);a.target=21;
 w.frame=32;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].target==22);
 p.reset();setup(w);v=p.step(w,AdvancedInput,200,300,30);p.accepted(v[0],1);a.target=21;a.rearmStart=31;a.rearmDuration=1;
 w.frame=32;assert(p.step(w,AdvancedInput,200,300,30).empty()); // actual shot renewed progress
 // Repeated failed destination validation falls back to one GuardArea.
 w.count=1;w.frame=33;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].mission==2);p.rejected(v[0],33);
 w.frame=34;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].mission==2);p.rejected(v[0],34);
 w.frame=35;v=p.step(w,AdvancedInput,200,300,30);assert(v.size()==1&&v[0].mission==5);p.accepted(v[0],35);
 w.frame=36;assert(p.step(w,AdvancedInput,200,300,30).empty());
 // Slow but continuous approach counts cumulatively as movement progress;
 // a per-frame distance threshold would incorrectly abandon this target.
 p.reset();setup(w);v=p.step(w,AdvancedInput,200,300,30);p.accepted(v[0],1);a.target=21;
 for(int frame=2;frame<90;++frame){w.frame=frame;++a.x;assert(p.step(w,AdvancedInput,200,300,30).empty());}
 // Source profile selects the real weapon instead of treating a longer unused
 // secondary / another IFV mode as an alternative for every target.
 Actor source{},target{};source.weapons[0]={512,0,GroundWeapon,0x7ff};source.weapons[1]={2048,0,GroundWeapon|AirWeapon,0x7ff};target.kind=2;
 assert(!canTarget(source,target,1000*1000,false));source.flags=FixedWeapon;target.flags=Airborne;assert(!canTarget(source,target,300*300,false));
 source.flags=StagedWeapon;source.airRangeBonus=256;assert(canTarget(source,target,2200*2200,false));target.flags=0;assert(!canTarget(source,target,1000*1000,false));
 source.flags=0;source.weapons[0]={2048,0,GroundWeapon|MindControlWeapon,0x7ff};source.weapons[1]={};target.flags=PsionicImmune;assert(!canTarget(source,target,100*100,false));
 target.flags=Controlled;assert(!canTarget(source,target,100*100,false));source.weapons[0].flags=GroundWeapon|HealingWeapon;target.flags=0;assert(!canTarget(source,target,100*100,false));
 source.weapons[0].flags=GroundWeapon|ParasiteWeapon;target.flags=Infested;assert(!canTarget(source,target,100*100,false));
 source.weapons[0].flags=GroundWeapon|TemporalWeapon;target.flags=TemporalImmune;assert(!canTarget(source,target,100*100,false));target.flags=TemporalTarget;assert(canTarget(source,target,100*100,false));
 // A new intent behind a large existing selection is processed immediately.
 p.reset();w={};w.match=2;w.count=w.intentCount=256;w.frame=1;
 for(int i=0;i<256;++i){auto&x=w.actors[i];x.id=i+1;x.kind=2;x.flags=Own;x.range=1024;x.x=x.y=1000;x.weapons[0]={1024,0,GroundWeapon,0x7ff};w.intents[i]={x.id,30030,0};}
 p.step(w,AdvancedInput,200,300,90);++w.frame;p.step(w,AdvancedInput,200,300,90);
 w.frame=3;w.intentCount=257;w.count=258;auto&fresh=w.actors[256];fresh=w.actors[0];fresh.id=999;fresh.x=fresh.y=90000;w.intents[256]={999,30030,0};
 auto&e=w.actors[257];e.id=9000;e.kind=2;e.flags=Enemy;e.x=e.y=90100;e.cost=500;
 v=p.step(w,AdvancedInput,200,300,90);assert(!v.empty()&&v[0].unit==999&&v[0].target==9000);

 // Extra acquisition cells apply to every target check, including zero and
 // the default +2 boundary. No weapon range or rule memory is modified.
 for(int cells:{0,1,2,3,10,30})for(int delta:{0,1}){
  p.reset();setup(w);w.count=2;w.actors[1].x=w.actors[0].x+2048+cells*256+delta;
  auto orders=p.step(w,AdvancedInput,200,300,90,10,0,cells);
  bool attack=false;for(auto&o:orders)if(o.mission==1&&o.target==21)attack=true;
  assert(attack==(delta==0));
 }
 p.reset();setup(w);w.count=2;w.actors[1].x=w.actors[0].x+2048+2*256+1;
 v=p.step(w,AdvancedInput,200,300,90);for(auto&o:v)assert(o.mission!=1);
 std::cout<<"PASS: stable equal-value targets, higher-value priority, rearm/burst gating, dead-target prompt recovery, per-order rejection, pending/attack timeouts, accepted-only movement progress, GuardArea fallback, weapon selection, special immunities and urgent large-selection intents\n";
}
