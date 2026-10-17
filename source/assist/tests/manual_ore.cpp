#include "../source/external_planner.h"
#include "../source/object_index.h"
#include <cassert>
#include <iostream>
#include <memory>
using namespace assist;using namespace assist::planner;
int main(){
 auto data=std::make_unique<World>();auto&w=*data;w.match=1;w.count=2;w.frame=1;
 auto&a=w.actors[0];a.id=1;a.kind=2;a.flags=Own|Harvester;a.mission=10;a.queued=-1;a.storageLimit=28;a.storage[0]=28;a.dockTypes[0]=900;
 auto&r=w.actors[1];r.id=2;r.kind=3;r.flags=Own;r.type=900;r.x=5000;r.y=5000;
 ManualVersions manual;Planner p;
 auto step=[&](uint32_t flags=AutoOreReturn){
  a.manualVersion=manual.get(a.id);a.flags&=~OrePaused;if(manual.observeOre(a.id,a.mission,a.queued))a.flags|=OrePaused;
  ++w.frame;++w.inputSerial;return p.step(w,flags,200,300,90);
 };
 auto out=step();assert(out.size()==1);p.accepted(out[0],w.frame);
 // Click Move while the engine still reports the previous Harvest. No old
 // snapshot, empty tank, Guard, toggle or planner reset may resume assistance.
 manual.change(a.id);manual.expectOreMission(a.id,2);for(int i=0;i<5;++i)assert(step().empty()&&!p.hasJobs());
 a.mission=2;assert(step().empty());a.storage[0]=0;assert(step().empty());
 a.mission=5;assert(step().empty());a.storage[0]=28;assert(step().empty());
 assert(step(0).empty());p.reset();assert(step().empty());
 // A newly observed Harvest after leaving mining resumes even when still full.
 a.mission=10;out=step();assert(out.size()==1&&out[0].token==1);p.accepted(out[0],w.frame);
 // Attack from a partial load is also a pause, independent of fullness.
 a.storage[0]=7;manual.change(a.id);manual.expectOreMission(a.id,1);a.mission=1;assert(step().empty());
 a.storage[0]=28;a.mission=5;assert(step().empty());
 // An explicit mining / refinery order immediately hands control back and
 // invalidates previously computed actions with its new token.
 manual.change(a.id);manual.resumeOre(a.id);a.mission=12;out=step();assert(out.size()==1&&out[0].token==3);
 // Tool-generated waiting-cell Move never counts as a manual interruption.
 p.reset();r.radioIDs[0]=99;w.refineryCount=1;w.refineries[0].id=2;w.refineries[0].count=1;w.refineries[0].waiting[0]={20020,0,0};
 a.mission=10;out=step();assert(out.size()==1&&out[0].mission==2);p.accepted(out[0],w.frame);
 a.mission=2;assert(step().empty()&&p.hasJobs());r.radioIDs[0]=0;out=step();assert(out.size()==1&&out[0].mission==7);
 // Stop before enabling is not treated as an automatic mining mission.
 p.reset();a.mission=13;assert(step().empty());
 // A still-queued non-mining command prevents an old Harvest from resuming.
 manual.change(a.id);manual.expectOreMission(a.id,1);a.mission=1;step();a.mission=10;a.queued=2;assert(step().empty());a.queued=-1;assert(step().size()==1);
 // An old Return/Guard -> Harvest transition before Move executes cannot
 // acknowledge the new Move. Only its observed mission can open that gate.
 manual.change(a.id);manual.expectOreMission(a.id,2);a.mission=12;assert(step().empty());a.mission=10;assert(step().empty());
 a.mission=2;assert(step().empty());a.mission=10;assert(step().size()==1);
 // Collision, clear, and a new match do not leak the pause to another ID.
 manual.change(1+8192);assert(manual.orePaused(1+8192)&&!manual.orePaused(1));
 manual.clear();assert(!manual.orePaused(1+8192)&&manual.get(1)==0);
 for(unsigned id=1;id<=ManualVersions::Size;++id)manual.change(id);
 assert(!manual.trackable(9000)&&manual.orePaused(9000)&&manual.observeOre(9000,10,-1));
 manual.resumeOre(9000);assert(manual.orePaused(9000));manual.clear();assert(manual.trackable(9000)&&!manual.orePaused(9000));
 // The off-switch observer follows only known paused miners, and its dense
 // list must survive arbitrary removals, pointer updates and live-ID pruning.
 manual.change(11,101);manual.expectOreMission(11,2);
 manual.change(12,102);manual.change(13,103);manual.change(14,0,false);
 assert(manual.used==4&&manual.pausedCount==3&&!manual.orePaused(14));
 manual.resumeOre(12);assert(manual.pausedCount==2&&manual.watchingOre(11)&&manual.watchingOre(13));
 manual.change(13,203);assert(manual.pausedCount==2&&manual.get(13)==2);
 manual.observeOre(11,2,-1);ManualVersions live;
 live.copyLive(manual,[](uint32_t id){return id!=13;});
 assert(live.used==3&&live.pausedCount==1&&live.get(11)==1&&live.get(14)==1&&!live.get(13));
 assert(live.watchingOre(11)&&!live.observeOre(11,10,-1)&&live.pausedCount==0);
 assert(manual.pausedCount==2); // Compaction never mutates the source mid-read.
 live.change(15,105);live.change(16,106);live.change(17,107);
 live.resumeOre(17);live.resumeOre(15);live.resumeOre(16);assert(live.pausedCount==0);
 live.change(18,108);live.change(18,0,false);assert(live.pausedCount==0&&!live.watchingOre(18));
 for(unsigned id=100;id<100+ManualVersions::Size;++id)manual.change(id,10000+id);
 live.copyLive(manual,[](uint32_t id){return id==11;});assert(live.used==1&&live.trackable(999999));
 std::cout<<"PASS: manual Move/Attack/Stop priority, no empty/full or toggle resume, fresh mining hand-back, in-flight input lag and tool waiting Move distinction\n";
 std::cout<<"PASS: paused-miner dense list, swap removal, repeated pointer replacement, non-miner exclusion and live-ID compaction preserve tokens and hand-back stage\n";
}
