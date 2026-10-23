#include "../source/external_planner.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace assist;using namespace assist::planner;
struct Scene {
 std::unique_ptr<World> data=std::make_unique<World>();World&w=*data;Planner p;std::vector<Order> orders;
 Scene(int trucks=3){w.match=1;w.frame=1;w.count=trucks+1;w.refineryCount=1;
  for(int i=0;i<trucks;++i){auto&a=w.actors[i];a.id=i+1;a.kind=2;a.type=200;a.flags=Own|Harvester;a.storage[0]=40;a.storageLimit=40;a.dockTypes[0]=900;a.mission=10;place(a,10-i,10);}
  auto&r=ref();r.id=500;r.type=900;r.kind=3;r.flags=Own;place(r,30,30);
  auto&area=w.refineries[0];area.id=500;area.cellX=30;area.cellY=30;area.count=40;
  for(int i=0;i<40;++i)area.waiting[i]={uint32_t(33030+i),0,0};
 }
 Actor& ref(){return w.actors[w.count-1];}
 static void place(Actor&a,int x,int y){a.x=x*256+128;a.y=y*256+128;}
 Actor& miner(int id){return w.actors[id-1];}
 void next(){++w.frame;orders=p.step(w,AutoOreReturn,200,300,90);}
 void accept(){for(auto&o:orders)p.accepted(o,w.frame);}
 int count(int mission,int purpose=-1)const{int n=0;for(auto&o:orders)if(o.mission==mission&&(purpose<0||o.purpose==purpose))++n;return n;}
 Order one(int mission,int purpose=-1)const{assert(count(mission,purpose)==1);for(auto&o:orders)if(o.mission==mission&&(purpose<0||o.purpose==purpose))return o;return {};}
 void start(bool radio=true){next();assert(one(7).unit==1);accept();miner(1).destination=500;miner(1).mission=12;if(radio)ref().radioIDs[0]=1;}
 void arrive(int id){place(miner(id),31,33);}
 void release(){miner(1).destination=0;miner(1).mission=2;ref().radioIDs[0]=0;}
 void empty(int id){miner(id).flags|=Unloading;miner(id).storage[0]=0;if(ref().radioIDs[0]==uint32_t(id))ref().radioIDs[0]=0;}
};
int main(){int cases=0;
 // Native reservation and logical-only reservation both require an observed
 // release; acceptance alone or radio clear alone never grants a second Enter.
 for(bool radio:{false,true}){Scene s;s.start(radio);s.arrive(2);s.next();assert(s.one(2,OreYield).unit==1&&!s.count(7));s.accept();
  for(int i=0;i<8;++i){s.next();assert(!s.count(7));}
  s.ref().radioIDs[0]=0;s.next();assert(!s.count(7));s.release();s.next();assert(s.one(7).unit==2);s.accept();++cases;
 }
 // Several arrived miners keep one winner; even a closer/new arrival does
 // not replace it; the displaced miner wins the next turn, still far away.
 {Scene s(4);s.start();s.arrive(2);s.arrive(3);s.next();assert(s.one(2,OreYield).unit==1);s.accept();s.release();s.next();assert(s.one(7).unit==2);s.accept();s.miner(2).destination=500;s.miner(2).mission=7;
  s.arrive(4);s.ref().radioIDs[0]=2;
  for(int i=0;i<100;++i){s.next();assert(!s.count(7)&&!s.count(2,OreYield));s.accept();}
  s.empty(2);s.next();assert(s.one(7).unit==1&&!s.count(2,OreYield));s.accept();s.miner(1).destination=500;s.ref().radioIDs[0]=1;
  for(int i=0;i<20;++i){s.next();assert(!s.count(2,OreYield));}
  ++cases;
 }
 // FIFO by observed arrival, not nearest distance or lowest object ID.
 {Scene s;s.start();s.arrive(3);s.next();assert(s.one(2,OreYield).unit==1);s.accept();s.arrive(2);s.release();s.next();assert(s.one(7).unit==3);++cases;}
 // Never withdraw a native unloading/arrived/unknown/manual reservation.
 for(int condition=0;condition<5;++condition){Scene s;s.start();s.arrive(2);
  if(condition==0)s.miner(1).flags|=Unloading;
  if(condition==1)s.arrive(1);
  if(condition==2)s.ref().radioIDs[0]=999;
  if(condition==3){s.miner(1).flags|=OrePaused;++s.miner(1).manualVersion;}
  if(condition==4)for(auto&c:s.w.refineries[0].waiting)c.occupation=1;
  s.next();assert(!s.count(2,OreYield));++cases;
 }
 // Rejected, lost and admitted-but-never-executed releases have a fixed
 // retry budget, then fall back without passing Enter through a busy dock.
 for(int ack=0;ack<3;++ack){Scene s;s.start();s.arrive(2);int yields=0;
  for(int f=0;f<600;++f){s.next();yields+=s.count(2,OreYield);assert(!s.count(7));
   for(auto&o:s.orders)if(o.purpose==OreYield){if(ack==0)s.p.rejected(o,s.w.frame);if(ack==1)s.p.expired(o);if(ack==2)s.p.accepted(o,s.w.frame);}
  }assert(yields==3);++cases;
 }
 // A stalled selected truck cannot own the local turn forever. After the
 // timeout the displaced miner proceeds, and is never preempted again.
 {Scene s;s.start();s.arrive(2);s.next();s.accept();s.release();s.next();assert(s.one(7).unit==2);s.accept();s.miner(2).destination=500;
  bool served=false;for(int i=0;i<500;++i){s.next();for(auto&o:s.orders)if(o.mission==7&&o.unit==1){served=true;s.p.accepted(o,s.w.frame);s.miner(1).destination=500;}assert(!s.count(2,OreYield));}
  assert(served);++cases;
 }
 // Winner disappearance/manual control, changed refinery and new match
 // invalidate the turn without retaining a dead reservation or old token.
 for(int event=0;event<4;++event){Scene s;s.start();s.arrive(2);s.next();s.accept();s.release();s.next();s.accept();
  if(event==0)s.miner(2).flags|=Disabled;
  if(event==1){s.miner(2).flags|=OrePaused;++s.miner(2).manualVersion;}
  if(event==2)s.ref().flags|=OffMap;
  if(event==3){++s.w.match;s.miner(1).mission=10;s.miner(2).flags|=OrePaused;}
  s.next();if(event<2)assert(s.one(7).unit==1);if(event==2)assert(s.orders.empty());if(event==3)assert(s.one(7).unit==1);++cases;
 }
 // Saturated order batches still carry the single release first, not buried
 // behind parked-truck moves; a competing combat family retains its share.
 {Scene s(80);s.start();s.miner(2).storage[0]=0;s.arrive(80);s.next();assert(!s.orders.empty()&&s.orders[0].purpose==OreYield&&s.count(2,OreYield)==1);++cases;}
 // Switch-off releases queued work through native orders, clears turn state.
 {Scene s;s.start();s.arrive(2);s.next();s.accept();s.release();++s.w.frame;s.orders=s.p.step(s.w,0,200,300,90);
  assert(!s.count(2,OreYield));s.accept();++s.w.frame;assert(s.p.step(s.w,0,200,300,90).empty()&&!s.p.hasJobs());++cases;}
 // Complete finite-load frame model: no ping-pong, no starvation, each miner
 // unloads exactly once. This models movement/receipts, not the full game.
 {Scene s(20);s.start();for(int id=2;id<=20;++id)s.arrive(id);int done=0,yields=0;std::vector<int> service;
  for(int f=0;f<500&&done<20;++f){s.next();s.accept();
   for(auto&o:s.orders){if(o.purpose==OreYield){++yields;s.release();}
    else if(o.mission==7){service.push_back(o.unit);s.empty(o.unit);++done;}}
  }assert(done==20&&yields==1&&service[0]==2&&service[1]==1);++cases;}
 // Each refinery has its own turn; two independent releases and winners do
 // not share a global reservation or steal each other's protected service.
 {Scene s(4);auto&r1=s.w.actors[4];auto&r2=s.w.actors[5];r2=r1;r2.id=501;Scene::place(r2,100,100);s.w.count=6;s.w.refineryCount=2;
  s.w.refineries[1]=s.w.refineries[0];s.w.refineries[1].id=501;for(auto&c:s.w.refineries[1].waiting)c.cell+=70070;
  Scene::place(s.miner(3),80,80);Scene::place(s.miner(4),81,80);s.next();assert(s.count(7)==2);s.accept();r1.radioIDs[0]=1;r2.radioIDs[0]=3;
  s.miner(1).destination=500;s.miner(3).destination=501;s.miner(1).mission=s.miner(3).mission=12;
  s.arrive(2);Scene::place(s.miner(4),101,103);s.next();assert(s.count(2,OreYield)==2);s.accept();r1.radioIDs[0]=r2.radioIDs[0]=0;
  s.miner(1).destination=s.miner(3).destination=0;s.miner(1).mission=s.miner(3).mission=2;s.next();assert(s.count(7)==2);
  for(auto&o:s.orders)if(o.mission==7)assert((o.unit==2&&o.refinery==500)||(o.unit==4&&o.refinery==501));++cases;
 }
 // A removed refinery pointer cannot be dereferenced during turn cleanup.
 {Scene s;s.start();s.arrive(2);s.next();s.accept();--s.w.count;s.w.refineryCount=0;s.next();assert(s.orders.empty());++cases;}
 // An old receipt after a new manual token cannot reactivate the old turn.
 {Scene s;s.start();s.arrive(2);s.next();auto old=s.one(2,OreYield);++s.miner(1).manualVersion;s.miner(1).flags|=OrePaused;s.next();s.p.accepted(old,s.w.frame);s.release();s.next();assert(!s.count(2,OreYield));++cases;}
 // Native partial returns must get a protected turn without prematurely
 // recalling a partially loaded miner that is still harvesting.
 for(int mode=0;mode<4;++mode){Scene s(4);s.miner(1).storage[0]=7;
  s.miner(1).mission=mode==2?7:12;
  if(mode!=1)s.miner(1).destination=500;
  if(mode==1||mode==3)s.ref().radioIDs[0]=1;
  s.next();s.accept();s.arrive(2);s.next();assert(s.one(2,OreYield).unit==1);s.accept();s.release();s.next();assert(s.one(7).unit==2);s.accept();
  s.miner(2).flags|=Unloading;s.empty(2);s.next();assert(s.one(7).unit==1);s.accept();s.miner(1).destination=500;s.ref().radioIDs[0]=1;
  s.arrive(3);s.arrive(4);for(int f=0;f<60;++f){s.next();assert(!s.count(2,OreYield));}++cases;
 }
 {Scene s(1);s.miner(1).storage[0]=7;s.next();assert(s.orders.empty());
  s.miner(1).mission=12;s.next();assert(s.one(7).unit==1); // native Return without a link still joins
  s.miner(1).destination=500;s.miner(1).flags|=OrePaused;s.next();assert(s.orders.empty());++cases;}
 std::cout<<"PASS: "<<cases<<" ore priority scenarios: observed release, one winner, bounded retries, protected next turn, cancellation, starvation and finite-load progress\n";
}
