#include "../source/external_planner.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace assist;
using namespace assist::planner;
namespace {
struct Scene {
 std::unique_ptr<World> data=std::make_unique<World>();World&w=*data;Planner planner;std::vector<Order> orders;int miners;
 explicit Scene(int count=3):miners(count){
  w.match=1;w.frame=1;w.count=count+1;w.refineryCount=1;
  for(int i=0;i<count;++i){auto&a=w.actors[i];a.id=i+1;a.type=200;a.kind=2;a.flags=Own|Harvester;a.storage[0]=40;a.storageLimit=40;a.dockTypes[0]=900;a.mission=10;a.queued=-1;place(a,10-i,10);}
  auto&r=refinery();r.id=500;r.type=900;r.kind=3;r.flags=Own;place(r,30,30);
  auto&area=w.refineries[0];area.id=r.id;area.cellX=30;area.cellY=30;area.count=16;
  for(int i=0;i<area.count;++i)area.waiting[i]={uint32_t(33030+i),0,0};
 }
 static void place(Actor&a,int x,int y){a.x=x*256+128;a.y=y*256+128;}
 Actor&miner(int id){return w.actors[id-1];}
 Actor&refinery(){return w.actors[miners];}
 void next(uint32_t flags=AutoOreReturn){++w.frame;orders=planner.step(w,flags,200,300,90);}
 void accept(){for(const auto&o:orders)planner.accepted(o,w.frame);}
 const Order*forUnit(uint32_t id)const{for(const auto&o:orders)if(o.unit==id)return &o;return nullptr;}
 int countMission(int mission)const{int result=0;for(const auto&o:orders)result+=o.mission==mission;return result;}
 void defend(int id){auto&a=miner(id);a.flags|=HarvesterDefending;a.mission=1;a.target=9000;}
 void stopDefending(int id){miner(id).flags&=~(HarvesterDefending|OrePaused);}
 void assertNoYield()const{for(const auto&o:orders)assert(o.purpose!=OreYield);}
};
}
int main(){int cases=0;
 // A full miner not already tracked cannot be enrolled during defense, even
 // if the world still exposes a native Return/refinery destination.
 {Scene s(1);s.defend(1);s.miner(1).mission=12;s.miner(1).destination=500;
  s.next();assert(s.orders.empty()&&!s.planner.hasJobs());
  s.stopDefending(1);s.next();assert(s.forUnit(1)&&s.forUnit(1)->mission==7);++cases;}
 // A long defense preserves the original queue entry and waiting cell. Other
 // miners continue. On return the old head remains ahead of later entries,
 // even when it is still standing exactly on its old waiting cell.
 {Scene s(4);s.miner(4).storage[0]=0;s.refinery().radioIDs[0]=999;s.next();
  auto saved=*s.forUnit(1);assert(saved.mission==2);s.accept();
  Scene::place(s.miner(1),saved.destination%1000,saved.destination/1000);
  s.defend(1);s.miner(1).flags|=OrePaused;s.next();assert(!s.forUnit(1)&&s.planner.hasJobs());
  s.miner(4).storage[0]=40;s.next();assert(s.forUnit(4)&&s.forUnit(4)->destination!=saved.destination);s.accept();
  s.w.frame+=1200;s.next();assert(!s.forUnit(1)&&s.planner.oreTurnTimeouts==0&&s.planner.oreDelivered==0&&s.planner.oreUnverified==0);
  s.refinery().radioIDs[0]=0;s.next();assert(s.forUnit(2)&&s.forUnit(2)->mission==7&&!s.forUnit(1));s.accept();
  s.refinery().radioIDs[0]=2;s.miner(2).destination=500;s.miner(2).flags|=Unloading;
  s.stopDefending(1);s.next();assert(s.forUnit(1)&&s.forUnit(1)->mission==2&&s.forUnit(1)->destination==saved.destination);s.accept();
  s.miner(1).mission=2;s.miner(1).target=0;s.next();assert(!s.forUnit(1));
  s.miner(2).storage[0]=0;s.refinery().radioIDs[0]=0;s.next();assert(s.forUnit(1)&&s.forUnit(1)->mission==7&&s.countMission(7)==1);++cases;}
 // The saved native radio reservation stays authoritative: suspension never
 // makes a still-busy refinery appear free. Resume must reissue Enter even
 // when the old destination field still says refinery during Attack.
 {Scene s(2);s.next();assert(s.forUnit(1)->mission==7);s.accept();
  s.miner(1).destination=500;s.refinery().radioIDs[0]=1;s.defend(1);
  s.w.frame+=1000;s.next();assert(!s.forUnit(1)&&s.countMission(7)==0);s.assertNoYield();
  s.stopDefending(1);s.next();assert(s.forUnit(1)&&s.forUnit(1)->mission==7&&s.forUnit(1)->destination==500);s.accept();
  s.next();assert(!s.forUnit(1));++cases;}
 // Clearing AutoOreReturn while defending retains the exact entry until
 // defense ends, then releases it once through the normal native Enter path.
 {Scene s(1);s.refinery().radioIDs[0]=999;s.next();auto stale=*s.forUnit(1);s.accept();s.defend(1);s.next(0);
  assert(s.orders.empty()&&s.planner.hasJobs());s.w.frame+=1000;s.next(0);assert(s.orders.empty()&&s.planner.hasJobs());
  auto staleRelease=stale;staleRelease.purpose=OreRelease;staleRelease.mission=7;staleRelease.destination=500;
  s.planner.accepted(staleRelease,s.w.frame);assert(s.planner.hasJobs()); // old receipt cannot delete frozen entry
  s.stopDefending(1);s.next(0);assert(s.forUnit(1)&&s.forUnit(1)->purpose==OreRelease);s.accept();
  s.next(0);assert(s.orders.empty()&&!s.planner.hasJobs());++cases;}
 // New manual tokens and invalid/dead actors always revoke the continuation,
 // including when an old world flag still claims defense owns the actor.
 for(int reason=0;reason<3;++reason){Scene s(1);s.next();s.accept();s.defend(1);s.next();
  if(reason==0){++s.miner(1).manualVersion;s.miner(1).flags|=OrePaused;}
  if(reason==1)s.miner(1).flags|=Disabled;
  if(reason==2)s.miner(1).flags&=~Own;
  s.next();assert(s.orders.empty()&&!s.planner.hasJobs());++cases;}
 // A same-token paused bit may coexist with the defended snapshot; it cannot
 // erase that job. Once the defense flag is gone a real pause is respected.
 {Scene s(1);s.next();s.accept();s.defend(1);s.miner(1).flags|=OrePaused;s.next();assert(s.planner.hasJobs()&&s.orders.empty());
  s.miner(1).flags&=~HarvesterDefending;s.next();assert(s.orders.empty()&&!s.planner.hasJobs());++cases;}
 // Either participant entering defense cancels a proximity-bypass turn.
 // Neither a pending nor an accepted release may steal the defended actor
 // for a Yield, consume a timeout, or survive as a delayed turn after resume.
 for(int defended:{1,2})for(bool acknowledged:{false,true}){Scene s(3);s.next();s.accept();
  s.miner(1).destination=500;s.miner(1).mission=12;s.refinery().radioIDs[0]=1;
  Scene::place(s.miner(2),31,33);s.next();assert(s.forUnit(1)&&s.forUnit(1)->purpose==OreYield);
  auto stale=*s.forUnit(1);if(acknowledged)s.accept();s.defend(defended);s.w.frame+=1000;s.next();
  assert(!s.forUnit(defended));s.assertNoYield();assert(s.planner.oreTurnTimeouts==0);
  if(defended==1)s.planner.accepted(stale,s.w.frame);
  s.stopDefending(defended);s.next();s.assertNoYield();assert(s.planner.oreTurnTimeouts==0);++cases;}
 // An obsolete combat intent with the same token cannot cause a defense
 // snapshot to erase an existing ore entry or emit a competing attack/move.
 {Scene s(1);s.next();s.accept();s.defend(1);s.w.intentCount=1;s.w.intents[0]={1,20020,0,OrdinaryMove};
  s.next(AutoOreReturn|AdvancedInput);assert(s.orders.empty()&&s.planner.hasJobs());
  s.w.intentCount=0;s.stopDefending(1);s.next();assert(s.forUnit(1)&&s.forUnit(1)->mission==7);++cases;}
 // Refinery disappearance during suspension does not emit an old object ID.
 // The resumed entry can be routed to a compatible currently visible refinery.
 {Scene s(1);s.refinery().radioIDs[0]=999;s.next();s.accept();s.defend(1);s.next();
  s.refinery().flags|=OffMap;auto&r=s.w.actors[2];r=s.refinery();r.flags=Own;r.id=501;Scene::place(r,40,40);s.w.count=3;
  s.w.refineries[1]=s.w.refineries[0];s.w.refineries[1].id=501;s.w.refineryCount=2;
  s.next();assert(s.orders.empty());s.stopDefending(1);r.radioIDs[0]=0;s.next();
  assert(s.forUnit(1)&&s.forUnit(1)->mission==7&&s.forUnit(1)->destination==501);++cases;}
 // A new match discards all frozen entries; matching actor IDs never revive
 // the old queue while the new world's actor is still in combat.
 {Scene s(1);s.next();s.accept();s.defend(1);s.next();++s.w.match;s.stopDefending(1);s.next();
  assert(s.orders.empty()&&!s.planner.hasJobs());++cases;}
 std::cout<<"PASS: "<<cases<<" harvester defense queue scenarios: frozen FIFO, continued peers, waiting-cell/Enter resume, manual cancellation, bypass cleanup, toggle-off and match/refinery changes\n";
}
