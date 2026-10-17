#include "../source/external_planner.h"
#include <cassert>
#include <memory>
#include <iostream>
using namespace assist;using namespace assist::planner;
static void setup(World&w,int trucks){
 w={};w.match=1;w.frame=1;w.count=trucks+2;w.refineryCount=2;
 for(int i=0;i<trucks;++i){auto&a=w.actors[i];a.id=i+1;a.kind=2;a.type=200;a.flags=Own|Harvester;a.storage[0]=28;a.storageLimit=28;a.dockTypes[0]=900;a.mission=10;a.x=a.y=3000;}
 for(int i=0;i<2;++i){auto&r=w.actors[trucks+i];r.id=500+i;r.type=900;r.kind=3;r.flags=Own;r.x=r.y=i?30000:5000;r.radioIDs[0]=999;
  auto&area=w.refineries[i];area.id=r.id;area.count=40;for(int j=0;j<40;++j)area.waiting[j]={uint32_t(20020+i*60000+j),0,0};}
}
int main(){
 auto data=std::make_unique<World>();auto&w=*data;Planner p;setup(w,2);
 auto v=p.step(w,AutoOreReturn,200,300,90);assert(v.size()==2);for(auto&o:v)p.accepted(o,w.frame);
 // A proposed Move may be rejected at the DLL boundary. Its old waiting cell
 // must also be revalidated before it has ever received an accepted receipt.
 uint32_t rejected=v[0].destination;p.reset();w.frame=2;v=p.step(w,AutoOreReturn,200,300,90);
 for(auto&c:w.refineries[0].waiting)if(c.cell==rejected)c.occupation=1;
 ++w.frame;v=p.step(w,AutoOreReturn,200,300,90);assert(v.size()==2&&v[0].destination!=rejected);
 for(auto&o:v)p.accepted(o,w.frame);
 // A newly occupied waiting cell is replaced, without repeatedly issuing Move.
 uint32_t old=v[0].destination;for(auto&c:w.refineries[0].waiting)if(c.cell==old)c.occupation=1;
 w.frame=34;v=p.step(w,AutoOreReturn,200,300,90);assert(v.size()==1&&v[0].unit==1&&v[0].destination!=old);p.accepted(v[0],w.frame);
 w.frame=35;assert(p.step(w,AutoOreReturn,200,300,90).empty());
 // Refinery lost while parked: preserve the queue job and reassign to a live,
 // compatible placed refinery, even though its native current mission is Move.
 w.actors[0].mission=2;w.actors[2].flags|=OffMap;w.actors[3].radioIDs[0]=0;w.frame=36;
 v=p.step(w,AutoOreReturn,200,300,90);assert(v.size()==2&&v[0].mission==7&&v[0].refinery==501);for(auto&o:v)p.accepted(o,w.frame);
 w.actors[0].flags|=Unloading;w.actors[0].destination=501;w.frame=37;assert(p.step(w,AutoOreReturn,200,300,90).empty());
 // Turning off releases pending queued miners once; a manual token change
 // always wins over release and waiting-position corrections.
 w.actors[1].manualVersion=1;w.actors[1].flags|=OrePaused;++w.frame;v=p.step(w,0,200,300,90);assert(v.empty()&&!p.hasJobs());
 // Cap waiting-position retries even on persistently blocked native paths.
 p.reset();setup(w,1);int moves=0;
 for(int f=1;f<500;++f){w.frame=f;v=p.step(w,AutoOreReturn,200,300,90);for(auto&o:v){if(o.mission==2)++moves;p.accepted(o,f);}}
 assert(moves==4); // initial position + at most 3 bounded reposition attempts
 // A waiting miner that has already begun native unloading must never receive
 // a stale waiting-cell Move, including when automation is switched off.
 p.reset();setup(w,1);v=p.step(w,AutoOreReturn,200,300,90);assert(v.size()==1&&v[0].mission==2);p.accepted(v[0],w.frame);
 w.actors[0].flags|=Unloading;w.actors[0].destination=500;
 for(auto&c:w.refineries[0].waiting)c.occupation=1;
 w.frame=40;assert(p.step(w,AutoOreReturn,200,300,90).empty());
 ++w.frame;assert(p.step(w,0,200,300,90).empty()&&!p.hasJobs());
 // Unmodified YR storage: War Miner 40 / Chrono Miner 20. The first visible
 // five-pip loads (36 / 18) are insufficient; actual full storage triggers.
 for(int capacity:{40,20}){
  p.reset();setup(w,1);w.actors[0].storageLimit=capacity;
  w.actors[0].storage[0]=capacity*9/10;assert(p.step(w,AutoOreReturn,200,300,90).empty());
  w.actors[0].storage[0]=float(capacity)-.25f;++w.frame;assert(p.step(w,AutoOreReturn,200,300,90).empty());
  w.actors[0].storage[0]=capacity;++w.frame;v=p.step(w,AutoOreReturn,200,300,90);assert(v.size()==1&&v[0].mission==2);
 }
 // Mixed 16 attacks + 24 full miners, DLL accepts only 12-prefix per frame.
 // Neither the attack list nor the FIFO tail can starve the other family.
 p.reset();setup(w,24);int base=w.count;w.count+=17;w.intentCount=16;
 for(int i=0;i<16;++i){auto&a=w.actors[base+i];a.id=100+i;a.kind=2;a.flags=Own;a.x=a.y=50000;a.range=1024;a.weapons[0]={1024,0,GroundWeapon,0x7FF};w.intents[i]={a.id,30030,0};}
 auto&e=w.actors[base+16];e.id=400;e.kind=2;e.flags=Enemy;e.x=e.y=50100;e.cost=200;
 int oreOrders=0,attacks=0;
 for(int f=1;f<=8;++f){w.frame=f;v=p.step(w,AutoOreReturn|AdvancedInput,200,300,90);
  if(f==1){assert(v.size()==24);for(int j=0;j<12;++j)assert(v[j].purpose==(j%2?1:0));}
  for(int j=0;j<int(v.size())&&j<12;++j){auto&o=v[j];p.accepted(o,f);if(o.purpose==0){++oreOrders;auto&a=w.actors[o.unit-1];a.x=int(o.destination%1000)*256+128;a.y=int(o.destination/1000)*256+128;}else ++attacks;}
 }
 assert(oreOrders==24&&attacks==16);
 std::cout<<"PASS: unloading takes precedence over stale waiting moves and switch-off release; actual War Miner / Chrono Miner capacities and first-five-pip boundary\n";
 std::cout<<"PASS: blocked waiting-cell replacement, lost-refinery reassign, manual takeover, bounded movement retries, and 24 miners + 16 attacks all delivered under a 12-event prefix budget\n";
}
