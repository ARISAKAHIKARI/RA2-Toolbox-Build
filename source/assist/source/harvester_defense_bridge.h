#pragma once
#include "harvester_defense.h"
#include <unordered_map>
// One game-thread worker, using the existing world snapshot, spatial index,
// manual revisions and EventClass gateway. No new game hook or mission writes.
namespace {
constexpr size_t MinerDefenseCapacity=1024;
struct MinerDefenseWork {OreHarvestJob identity{};uint32_t waitCell=0;};
struct MinerDefenseJob {
 OreHarvestJob identity{};uint32_t resourceCell=0,waitCell=0,target=0,targetPointer=0,targetType=0,targetTypeID=0;
 uint32_t receipt=0,originalDestination=0,originalTarget=0,previousTargetPointer=0;int originalMission=-1,originalQueued=-1,sentFrame=-1,restoreMission=-1;
 uint32_t restoreDestination=0,previousRestoreDestination=0;int restoreTag=0,previousRestoreMission=-1,previousRestoreTag=0,restoreStartedFrame=-1;DWORD sentAt=0,uncertainAt=0,restoreStartedAt=0;
 bool mining=false,managedQueue=false,attacked=false,observedAttack=false,restoring=false,returningToWait=false;
 unsigned retries=0;
};
static std::unordered_map<uint32_t,MinerDefenseJob> minerDefenseJobs;
static std::unordered_map<uint32_t,MinerDefenseWork> minerDefenseWork;
static std::unordered_map<uint32_t,uint32_t> minerDefenseLinkedRefinery;
static uint32_t minerDefenseRefineries[planner::MaxRefineries];
static int minerDefenseRefineryCount=0;
static bool minerDefenseRefineryIncomplete=false;
static OreHarvestWorld minerDefenseWorld;
static planner::ActorLookup minerDefenseActors;
static planner::EnemyGrid minerDefenseEnemies;
static int minerDefenseFrame=-1,minerDefenseCursor=0;
static uint32_t minerDefenseInput=0,minerDefenseActiveCursor=0;
}
static bool minerDefensePending(){return !minerDefenseJobs.empty();}
static void minerDefensePublish(){if(sh)sh->minerDefenseActive=uint32_t(minerDefenseJobs.size());}
static bool minerDefenseOwns(uint32_t id){
 auto it=minerDefenseJobs.find(id);return it!=minerDefenseJobs.end()&&it->second.identity.token==manualVersions.get(id)&&!manualVersions.orePaused(id);
}
static void minerDefenseDrop(uint32_t id){
 minerDefenseJobs.erase(id);const int at=actorLeaseArbiter.find(id);
 if(at>=0&&actorLeaseArbiter.leases[at].owner==taskarbiter::Owner::HarvesterDefense)releaseAutoOwner(id);
 minerDefensePublish();
}
static void minerDefenseForget(uint32_t id){minerDefenseDrop(id);minerDefenseWork.erase(id);}
static void minerDefenseReset(){
 minerDefenseJobs.clear();minerDefenseWork.clear();minerDefenseWorld={};minerDefenseFrame=-1;minerDefenseCursor=0;minerDefenseInput=minerDefenseActiveCursor=0;
 minerDefenseLinkedRefinery.clear();minerDefenseRefineryCount=0;minerDefenseRefineryIncomplete=false;
 minerDefenseActors.clear();minerDefenseEnemies.clear();if(sh)sh->minerDefenseActive=sh->minerDefenseAttacks=sh->minerDefenseResumes=0;
}
static bool minerDefenseIdentity(uint32_t id,uint32_t house,OreHarvestJob&out){
 out={};out.id=id;out.pointer=worldByID(id);out.token=manualVersions.get(id);
 if(!out.pointer||kind(out.pointer)!=2||!oreHarvestRead(out.pointer+0x6C4,out.type)||out.type<0x10000||
    !oreHarvestRead(out.type+0x10,out.typeID)||!out.typeID||!refreshOreConfig()||!oreMiner(out.type)||
    !oreHarvestRead(out.type+0x67C,out.speed)||out.speed<0||out.speed>=8)return false;
 OreHarvestState state{};return oreHarvestState(out,house,state)==1;
}
static bool minerDefenseRefinery(OreHarvestJob&j,uint32_t refinery,uint32_t house){
 OreHarvestJob next=j;next.refinery=refinery;next.refineryPointer=worldByID(refinery);
 if(!next.refineryPointer||!rawOreCompatible(j.pointer,next.refineryPointer)||
    !oreHarvestRead(next.refineryPointer+0x520,next.refineryType)||!oreHarvestRead(next.refineryType+0x10,next.refineryTypeID)||!next.refineryTypeID||
    oreHarvestRefinery(next,house)<0)return false;
 j=next;return true;
}
// Collect radio links once per world snapshot. Harvest miners with partial
// cargo need this to distinguish mining from a native linked return. Never
// scan Building::Array separately for every miner or assume a miner-side link.
static void minerDefenseIndexRefineries(){
 minerDefenseLinkedRefinery.clear();minerDefenseRefineryCount=0;
 minerDefenseRefineryIncomplete=planningWorld.count>=planner::MaxActors;int linksBudget=4096;
 for(int n=0;n<planningWorld.count;++n){const auto&a=planningWorld.actors[n];
  if(a.kind!=3||!(a.flags&planner::Own)||(a.flags&(planner::Limbo|planner::OffMap)))continue;
  if(minerDefenseRefineryCount>=planner::MaxRefineries){minerDefenseRefineryIncomplete=true;continue;}
  const uint32_t p=worldByID(a.id);if(!p||typeOf(p,3)!=a.type){minerDefenseRefineryIncomplete=true;continue;}
  minerDefenseRefineries[minerDefenseRefineryCount++]=a.id;
  uint32_t links=0;int count=0;
  if(!oreHarvestRead(p+0xE4,links)||!oreHarvestRead(p+0xE8,count)||count<0||count>256||(count&&links<0x10000)){
   minerDefenseRefineryIncomplete=true;continue;
  }
  for(int k=0;k<count;++k){
   if(!linksBudget){minerDefenseRefineryIncomplete=true;break;}--linksBudget;
   uint32_t miner=0;if(!oreHarvestRead(links+uint32_t(k)*4,miner)){minerDefenseRefineryIncomplete=true;continue;}
   const uint32_t id=rawObjectID(miner);if(!id||kind(miner)!=2||!rawOreCompatible(miner,p))continue;
   auto old=minerDefenseLinkedRefinery.find(id);
   if(old==minerDefenseLinkedRefinery.end()||a.id<old->second)minerDefenseLinkedRefinery[id]=a.id;
  }
 }
}
static bool minerDefenseFindRefinery(OreHarvestJob&j,uint32_t house,bool nearest){
 if(!nearest){auto linked=minerDefenseLinkedRefinery.find(j.id);if(linked==minerDefenseLinkedRefinery.end())return false;
  OreHarvestJob candidate=j;if(!minerDefenseRefinery(candidate,linked->second,house)||oreHarvestRefinery(candidate,house)!=1)return false;j=candidate;return true;
 }
 int64_t best=INT64_MAX;OreHarvestJob chosen{};
 int x=0,y=0;if(!oreHarvestRead(j.pointer+0x9C,x)||!oreHarvestRead(j.pointer+0xA0,y))return false;
 for(int n=0;n<minerDefenseRefineryCount;++n){const uint32_t id=minerDefenseRefineries[n],p=worldByID(id);OreHarvestJob candidate=j;
  if(!id||!minerDefenseRefinery(candidate,id,house))continue;
  const int linked=oreHarvestRefinery(candidate,house);if(linked<0||(!nearest&&!linked))continue;
  int rx=0,ry=0;if(!oreHarvestRead(p+0x9C,rx)||!oreHarvestRead(p+0xA0,ry))continue;
  const int64_t dx=int64_t(x)-rx,dy=int64_t(y)-ry,d=linked?-1:dx*dx+dy*dy;
  if(d<best||(d==best&&id<chosen.refinery)){chosen=candidate;best=d;}
 }if(!chosen.refinery)return false;j=chosen;return true;
}
// Keep only ordinary accepted ore intentions. This is tool metadata, never a
// copy/write of native radio or queue storage. It also covers EXE planning.
static void minerDefenseRememberOre(const planner::Order&o,uint32_t house){
 if(o.purpose==planner::OreHarvestOrder){minerDefenseWork.erase(o.unit);return;}
 if(o.purpose!=planner::OreOrder&&o.purpose!=planner::OreYield&&o.purpose!=planner::OreRelease)return;
 OreHarvestJob identity{};if(!minerDefenseIdentity(o.unit,house,identity)||identity.token!=o.token||!minerDefenseRefinery(identity,o.refinery,house))return;
 if(!minerDefenseWork.count(o.unit)&&minerDefenseWork.size()>=MinerDefenseCapacity)return;
 minerDefenseWork[o.unit]={identity,o.mission==2?o.destination:0};
}
static bool minerDefenseSource(const OreHarvestJob&j,uint32_t house,planner::Actor&a,OreHarvestState&s,bool*unknown=nullptr){
 if(unknown)*unknown=false;const int valid=oreHarvestState(j,house,s);
 if(valid!=1){if(unknown)*unknown=valid==0;return false;}
 if(!s.onMap||s.limbo)return false;
 if(!refreshOreConfig()){if(unknown)*unknown=true;return false;}if(!oreMiner(j.type))return false;
 a={};a.id=j.id;a.type=j.type;a.kind=2;a.flags=planner::Own|planner::Harvester;a.manualVersion=j.token;a.mission=s.mission;a.queued=s.queued;
 if(!read(j.pointer+0x9C,&a.x,3*sizeof(int))){if(unknown)*unknown=true;return false;}
 rawCombatInfo(j.pointer,j.type,2,a,true,false);
 return minerdefense::miner(a)&&!stickyOwns(j.pointer)&&!formationOwns(j.id)&&!burstOwns(j.id)&&!dodgeOwns(j.id)&&orderIndex(j.pointer)<0;
}
static bool minerDefenseVictim(uint32_t id,uint32_t house,planner::Actor&a,uint32_t&pointer,uint32_t&type,uint32_t&typeID){
 pointer=worldByID(id);type=typeID=0;
 if(!pointer||!minerDefenseThreat(pointer,house,false)||!oreHarvestRead(pointer+0x6C4,type)||type<0x10000||
    !oreHarvestRead(type+0x10,typeID)||!typeID)return false;
 a={};a.id=id;a.type=type;a.kind=2;a.flags=planner::Enemy|planner::ParasiteThreat;
 if(!read(pointer+0x9C,&a.x,3*sizeof(int))||!oreHarvestRead(type+0x9C,a.armor))return false;
 rawCombatInfo(pointer,type,2,a,false,false);return minerdefense::threat(a);
}
static bool minerDefenseCapture(const planner::Actor&a,uint32_t house,MinerDefenseJob&j){
 j={};if(!minerDefenseIdentity(a.id,house,j.identity))return false;
 planner::Actor source{};OreHarvestState state{};
 if(!minerDefenseSource(j.identity,house,source,state)||!minerdefense::queuedWork(state.mission,state.queued))return false;
 j.originalMission=state.mission;j.originalQueued=state.queued;j.originalDestination=state.destination;
 if(!oreHarvestRead(j.identity.pointer+0x2B4,j.originalTarget))return false;
 j.identity.originX=source.x/256;j.identity.originY=source.y/256;
 if(j.identity.originX<=0||j.identity.originX>=512||j.identity.originY<=0||j.identity.originY>=512)return false;
 // A proven toolbox waiting Move is distinguishable from a manual Move by
 // its revision, recorded cell and refinery identity. Never adopt arbitrary
 // idle/moving miners just because they carry some ore.
 auto saved=minerDefenseWork.find(a.id);
 if(saved!=minerDefenseWork.end()){
  const auto&r=saved->second;const auto&old=r.identity;
  if(old.pointer==j.identity.pointer&&old.type==j.identity.type&&old.typeID==j.identity.typeID&&old.token==j.identity.token&&state.cargo>0&&r.waitCell){
   uint32_t cell=readMapCell(int(r.waitCell%1000),int(r.waitCell/1000));
   const int64_t dx=int64_t(source.x)-(int(r.waitCell%1000)*256+128),dy=int64_t(source.y)-(int(r.waitCell/1000)*256+128);
   if((state.destination==cell||((state.mission==5||state.mission==2)&&dx*dx+dy*dy<=256*256))&&
      minerDefenseRefinery(j.identity,old.refinery,house)&&j.identity.refineryPointer==old.refineryPointer&&j.identity.refineryTypeID==old.refineryTypeID){
    j.waitCell=r.waitCell;j.managedQueue=true;
   }
  }
 }
 if(!minerdefense::workingMission(state.mission)&&!j.managedQueue)return false;
 if(state.cargo>0&&!j.identity.refinery){
  const uint32_t destination=rawObjectID(state.destination);
  if(destination)minerDefenseRefinery(j.identity,destination,house);
  if(!j.identity.refinery)minerDefenseFindRefinery(j.identity,house,false);
  if(!j.identity.refinery&&state.mission==10&&minerDefenseRefineryIncomplete)return false;
  if(!j.identity.refinery&&(state.mission==12||state.mission==16||state.mission==7))minerDefenseFindRefinery(j.identity,house,true);
 }
 j.mining=state.mission==10&&!j.identity.refinery&&!j.managedQueue;
 if(!j.mining&&!j.identity.refinery)return false;
 if(state.destination>=0x10000&&!rawObjectID(state.destination)){
  int16_t xy[2]{};if(read(state.destination+0x24,xy,sizeof xy)&&readMapCell(xy[0],xy[1])==state.destination)j.resourceCell=uint32_t(xy[1]*1000+xy[0]);
 }
 if(!j.resourceCell)j.resourceCell=uint32_t(j.identity.originY*1000+j.identity.originX);
 return true;
}
static bool minerDefenseSend(MinerDefenseJob&j,uint32_t house,int frame,DWORD now,int mission,uint32_t target,uint32_t destination,int tag){
 OreHarvestWorld world{};planner::Actor source{};OreHarvestState state{};
 if(!oreHarvestContext(house,frame,world)||!oreHarvestSameWorld(world,minerDefenseWorld)||!minerDefenseSource(j.identity,house,source,state)||
    snap.chat||frame==j.sentFrame||!planningRoom(0)||!outgoingRoom())return false;
 // nativeMission repeats ownership, token, live-object and same-frame checks.
 // Harvest uses the verified original click tuple with no nearest override.
 const uint32_t nearest=mission==2&&(!sh||sh->megaQueueMode==0)?destination:0;
 if(!nativeMission(house,j.identity.id,mission,target,destination,tag,nearest,nearest?11:0,taskarbiter::Owner::HarvesterDefense,&j.receipt))return false;
 consumePlanning(0);j.sentFrame=frame;j.sentAt=now;
 if(mission==1){j.attacked=true;++sh->minerDefenseAttacks;}else{
  if(j.restoreMission>=0){++j.retries;if(j.restoreMission!=mission||j.restoreDestination!=destination||j.restoreTag!=tag){
   j.previousRestoreMission=j.restoreMission;j.previousRestoreDestination=j.restoreDestination;j.previousRestoreTag=j.restoreTag;
  }}
  j.restoreMission=mission;j.restoreDestination=destination;j.restoreTag=tag;++sh->minerDefenseResumes;
 }
 return true;
}
static bool minerDefenseAttack(MinerDefenseJob&j,uint32_t target,uint32_t house,int frame,DWORD now,bool retaining){
 if(!sh->minerDefenseEnabled)return false;
 planner::Actor source{},victim{};OreHarvestState state{};uint32_t pointer=0,type=0,typeID=0;
 if(!minerDefenseSource(j.identity,house,source,state)||!minerDefenseVictim(target,house,victim,pointer,type,typeID)||
    !minerdefense::inRange(source,victim,int(sh->minerDefenseRangeCells),retaining))return false;
 uint32_t predecessor=0;if(j.target&&target!=j.target){uint32_t current=0;if(!oreHarvestRead(j.identity.pointer+0x2B4,current))return false;
  if(current==j.targetPointer||current==j.previousTargetPointer)predecessor=current;
 }
 if(!minerDefenseSend(j,house,frame,now,1,target,0,0))return false;
 if(j.target&&target!=j.target)j.previousTargetPointer=predecessor;
 j.target=target;j.targetPointer=pointer;j.targetType=type;j.targetTypeID=typeID;j.restoreMission=-1;return true;
}
static const planner::Actor* minerDefenseChoose(const planner::Actor&source){
 const planner::Actor*best=nullptr;int64_t score=INT64_MAX;int range=0;
 for(const auto&w:source.weapons)range=std::max(range,w.range);
 range+=std::clamp(int(sh->minerDefenseRangeCells),0,minerdefense::MaxExtraCells)*256;
 minerDefenseEnemies.nearby(source.x,source.y,range,[&](int n){const auto&t=planningWorld.actors[n];
  if(!minerdefense::inRange(source,t,int(sh->minerDefenseRangeCells)))return;const int64_t d=minerdefense::distance(source,t);
  if(d<score||(d==score&&(!best||t.id<best->id))){best=&t;score=d;}
 });return best;
}
static bool minerDefenseRestored(const MinerDefenseJob&j,const OreHarvestState&s,uint32_t house){
 if(j.restoreMission==7)return (s.unloading&&oreHarvestRefinery(j.identity,house)==1)||
  ((s.mission==7||s.queued==7)&&s.destination==j.identity.refineryPointer);
 if(j.restoreMission==10)return (s.mission==10||s.queued==10)&&s.destination==readMapCell(int(j.restoreDestination%1000),int(j.restoreDestination/1000));
 if(j.restoreMission==5)return s.mission==5&&s.queued==-1;
 return false;
}
static void minerDefenseBeginRestore(MinerDefenseJob&j,int frame,DWORD now){
 if(j.restoring)return;j.restoring=true;j.retries=0;j.restoreStartedFrame=frame;j.restoreStartedAt=now;
}
// Manual revision is the primary signal. If a native input was not observed,
// admit only the captured work, our own Attack, and the exact restore tuple.
// In particular a new Move/Stop/Attack must win even before the first Attack
// becomes visible, and while a submitted restoration is still pending.
static bool minerDefenseTransition(const MinerDefenseJob&j,const OreHarvestState&s,uint32_t target,uint32_t house){
 // A recycled victim pointer is still the old native target reference; fresh
 // UID/type validation below forbids attacking its replacement accidentally.
 const bool ownTarget=target==j.targetPointer||(j.previousTargetPointer&&target==j.previousTargetPointer);
 const bool noTarget=target==0;
 const uint32_t restore=j.restoreTag==11?readMapCell(int(j.restoreDestination%1000),int(j.restoreDestination/1000)):
  j.restoreTag==0x34?j.identity.refineryPointer:0;
 const bool restoreDestination=j.restoreMission>=0&&s.destination==restore;
 const bool restoreQueued=j.restoring&&restoreDestination&&s.queued==j.restoreMission;
 const uint32_t previous=j.previousRestoreTag==11?readMapCell(int(j.previousRestoreDestination%1000),int(j.previousRestoreDestination/1000)):
  j.previousRestoreTag==0x34?j.identity.refineryPointer:0;
 if(j.restoring&&j.previousRestoreMission>=0&&s.mission==j.previousRestoreMission&&s.destination==previous&&
    (ownTarget||noTarget)&&(s.queued==-1||s.queued==j.previousRestoreMission||s.queued==j.restoreMission))return true;
 if(j.restoring&&j.restoreMission==7&&s.unloading&&(s.mission==7||s.mission==16)&&
    (s.queued==-1||s.queued==7||s.queued==10||s.queued==16)&&(ownTarget||noTarget)&&oreHarvestRefinery(j.identity,house)==1)return true;
 if(j.restoring&&restoreDestination&&(s.mission==j.restoreMission||restoreQueued)&&(ownTarget||noTarget)&&
    (s.queued==-1||s.queued==j.restoreMission))return true;
 if(s.mission==1&&(ownTarget||noTarget)&&(s.queued==-1||s.queued==1||restoreQueued))return true;
 if(s.mission==j.originalMission&&s.destination==j.originalDestination&&(target==j.originalTarget||ownTarget)&&
    (s.queued==j.originalQueued||minerdefense::queuedWork(s.mission,s.queued)||s.queued==1||restoreQueued))return true;
 // The engine can finish an Attack or a waiting Move in Guard. Only an empty
 // target and the expected/cleared destination count as that continuation.
 if(s.mission==5&&noTarget&&(j.observedAttack||j.restoring)&&(s.queued==-1||restoreQueued)&&
    (!s.destination||(j.returningToWait&&restoreDestination)))return true;
 return false;
}
// Returns true only when this local job may release ownership. Queued events
// increment the UI's "issued" counters, never an invented kill/completion count.
static bool minerDefenseResume(MinerDefenseJob&j,uint32_t house,int frame,DWORD now,int&searchBudget){
 planner::Actor source{};OreHarvestState state{};if(!minerDefenseSource(j.identity,house,source,state))return true;
 if(j.restoreMission>=0&&minerDefenseRestored(j,state,house))return true;
 if(j.returningToWait){
  const int64_t dx=int64_t(source.x)-(int(j.waitCell%1000)*256+128),dy=int64_t(source.y)-(int(j.waitCell/1000)*256+128);
  if(dx*dx+dy*dy<=256*256&&state.mission!=1&&state.queued!=1)return true;
  if(frame-j.sentFrame<90||DWORD(now-j.sentAt)<1500)return false;
  // A blocked old wait cell must not hold a miner forever. Let the preserved
  // ore job choose a fresh waiting cell after an interruptible native Guard.
  if(minerDefenseSend(j,house,frame,now,5,0,0,0)){j.returningToWait=false;return false;}return false;
 }
 if(j.restoreMission>=0&&(frame-j.sentFrame<12||DWORD(now-j.sentAt)<350))return false;
 if(j.restoreMission>=0&&j.retries>=3)return true;
 if(j.managedQueue&&(sh->flags&AutoOreReturn)&&j.waitCell&&state.cargo>0&&oreHarvestRefinery(j.identity,house)>=0){
  const uint32_t cell=readMapCell(int(j.waitCell%1000),int(j.waitCell/1000));uint32_t occupation=~0u;int land=-1;
  if(cell&&oreHarvestRead(cell+0x124,occupation)&&oreHarvestRead(cell+0xEC,land)&&!(occupation&0x9Fu)&&land!=2&&land!=7){
   if(minerDefenseSend(j,house,frame,now,2,0,j.waitCell,11))j.returningToWait=true;return false;
  }
  // Preserve the toolbox's existing queue ticket; do not bypass it with Enter.
  minerDefenseSend(j,house,frame,now,5,0,0,0);return false;
 }
 if(!j.mining&&state.cargo>0){
  if(oreHarvestRefinery(j.identity,house)<0&&!minerDefenseFindRefinery(j.identity,house,true)){j.identity.refinery=0;j.identity.refineryPointer=0;}
  if(j.identity.refinery){minerDefenseSend(j,house,frame,now,7,0,j.identity.refinery,0x34);return false;}
 }
 if(!oreHarvestMapCurrent()&&!oreHarvestMapCapture())return false;
 uint32_t cell=j.resourceCell;
 if(!cell||oreHarvestCell(int(cell%1000),int(cell/1000),j.identity.speed)!=1){
  auto&search=j.identity.search;if(!search.radius&&!search.at&&!search.done)search.begin(j.identity.originX,j.identity.originY);
  int local=std::min(searchBudget,256),before=local;search.advance(local,[&](int x,int y){return oreHarvestCell(x,y,j.identity.speed);});searchBudget-=before-local;
  if(!search.done)return false;cell=search.best;
 }
 if(cell&&oreHarvestCell(int(cell%1000),int(cell/1000),j.identity.speed)==1)minerDefenseSend(j,house,frame,now,10,0,cell,11);
 else minerDefenseSend(j,house,frame,now,5,0,0,0);
 return false;
}
static bool minerDefenseTick(uint32_t house,int frame,DWORD now){
 if(!sh||(!sh->minerDefenseEnabled&&!minerDefensePending())){minerDefensePublish();return false;}
 OreHarvestWorld world{};if(!oreHarvestContext(house,frame,world))return false;
 if(minerDefenseWorld.match&&(!oreHarvestSameWorld(world,minerDefenseWorld)||frame<minerDefenseWorld.frame))minerDefenseReset();
 minerDefenseWorld=world;
 for(auto it=minerDefenseWork.begin();it!=minerDefenseWork.end();){const auto&old=it->second.identity;
  if(worldByID(it->first)!=old.pointer||manualVersions.get(it->first)!=old.token)it=minerDefenseWork.erase(it);else ++it;
 }
 if(minerDefenseFrame==frame&&minerDefenseInput==inputSerial)return false;minerDefenseFrame=frame;minerDefenseInput=inputSerial;
 if(!freshness::valid(planningWorld.match,match,planningWorld.tick,planningWorld.frame,now,frame,freshness::Tactical))return false;
 minerDefenseActors.clear();minerDefenseEnemies.clear();
 for(int n=0;n<planningWorld.count;++n){const auto&a=planningWorld.actors[n];minerDefenseActors.add(a);if(minerdefense::threat(a))minerDefenseEnemies.add(n,a);}
 minerDefenseIndexRefineries();
 const uint32_t before=sh->minerDefenseAttacks+sh->minerDefenseResumes;int searchBudget=2048,issued=0;
 // Rotate existing jobs as well as new miners. Six event slots and the bounded
 // resource search budget must not be monopolized by the same first jobs.
 static uint32_t activeIDs[MinerDefenseCapacity];size_t activeCount=0;
 for(const auto&entry:minerDefenseJobs){if(activeCount>=MinerDefenseCapacity)break;activeIDs[activeCount++]=entry.first;}
 const size_t activeStart=activeCount?minerDefenseActiveCursor%activeCount:0;
 if(activeCount)minerDefenseActiveCursor=uint32_t((activeStart+1)%activeCount);
 for(size_t n=0;n<activeCount;++n){const uint32_t id=activeIDs[(activeStart+n)%activeCount];auto found=minerDefenseJobs.find(id);
  if(found==minerDefenseJobs.end())continue;auto&j=found->second;
  OreHarvestState state{};const int valid=oreHarvestState(j.identity,house,state);
  if(valid<0||!manualVersions.trackable(id)){minerDefenseForget(id);continue;}
  if(!valid){if(!j.uncertainAt)j.uncertainAt=now;if(DWORD(now-j.uncertainAt)>2000)minerDefenseDrop(id);continue;}
  const int lease=actorLeaseArbiter.find(id);
  if(lease>=0&&actorLeaseArbiter.leases[lease].owner!=taskarbiter::Owner::HarvesterDefense&&
     taskarbiter::priority(actorLeaseArbiter.leases[lease].owner)>taskarbiter::priority(taskarbiter::Owner::HarvesterDefense)){minerDefenseDrop(id);continue;}
  planner::Actor source{};bool unknown=false;if(!minerDefenseSource(j.identity,house,source,state,&unknown)){
   if(unknown){if(!j.uncertainAt)j.uncertainAt=now;if(DWORD(now-j.uncertainAt)>2000)minerDefenseDrop(id);}else minerDefenseDrop(id);continue;
  }
  uint32_t targetPointer=0;if(!oreHarvestRead(j.identity.pointer+0x2B4,targetPointer)){
   if(!j.uncertainAt)j.uncertainAt=now;if(DWORD(now-j.uncertainAt)>2000)minerDefenseDrop(id);continue;
  }j.uncertainAt=0;
  const uint32_t currentTarget=rawObjectID(targetPointer);
  if(!minerDefenseTransition(j,state,targetPointer,house)){minerDefenseForget(id);continue;}
  if(state.mission==1&&currentTarget==j.target){j.observedAttack=true;j.previousTargetPointer=0;}
  if(!sh->minerDefenseEnabled)minerDefenseBeginRestore(j,frame,now);
  // Both clocks must advance: a paused match is not a failed restoration.
  // Failed admission/read attempts never consume the successful-send retries.
  if(j.restoring&&frame-j.restoreStartedFrame>=600&&DWORD(now-j.restoreStartedAt)>=15000){minerDefenseDrop(id);continue;}
  if(!j.restoring){
   planner::Actor victim{};uint32_t p=0,t=0,tid=0;
   const bool retain=minerDefenseVictim(j.target,house,victim,p,t,tid)&&p==j.targetPointer&&t==j.targetType&&tid==j.targetTypeID&&
    minerdefense::inRange(source,victim,int(sh->minerDefenseRangeCells),true);
   if(retain){
    if((state.mission==1||state.queued==1)&&currentTarget==j.target)continue;
    if(frame-j.sentFrame<12||DWORD(now-j.sentAt)<350)continue;
    if(j.retries<3&&issued<6&&minerDefenseAttack(j,j.target,house,frame,now,true)){++j.retries;++issued;continue;}
    if(j.retries<3)continue;minerDefenseBeginRestore(j,frame,now);
   }else if(const auto*next=minerDefenseChoose(source)){
    if(issued<6&&minerDefenseAttack(j,next->id,house,frame,now,false)){j.retries=0;++issued;}continue;
   }else minerDefenseBeginRestore(j,frame,now);
  }
  if(j.restoring){
   if(!j.attacked){minerDefenseDrop(id);continue;}
   if(issued<6){const uint32_t sent=sh->minerDefenseResumes;if(minerDefenseResume(j,house,frame,now,searchBudget))minerDefenseDrop(id);if(sh->minerDefenseResumes!=sent)++issued;}
  }
 }
 // Round-robin miners plus a shared enemy grid: no per-miner full-map scan.
 if(sh->minerDefenseEnabled&&!snap.chat&&minerDefenseJobs.size()<MinerDefenseCapacity){int visited=0,checked=0;
  for(;visited<planningWorld.count&&checked<128&&issued<6&&minerDefenseJobs.size()<MinerDefenseCapacity;++visited){const int n=(minerDefenseCursor+visited)%planningWorld.count;auto&a=planningWorld.actors[n];
   if(!minerdefense::miner(a)||minerDefenseJobs.count(a.id))continue;++checked;
   const auto*target=minerDefenseChoose(a);if(!target)continue;MinerDefenseJob candidate{};
   if(!minerDefenseCapture(a,house,candidate))continue;
   auto inserted=minerDefenseJobs.emplace(a.id,candidate);minerDefensePublish();auto&job=inserted.first->second;
   if(minerDefenseAttack(job,target->id,house,frame,now,false)){a.flags|=planner::HarvesterDefending;++issued;}
   else minerDefenseDrop(a.id);
  }if(planningWorld.count)minerDefenseCursor=(minerDefenseCursor+visited)%planningWorld.count;
 }
 minerDefensePublish();return before!=sh->minerDefenseAttacks+sh->minerDefenseResumes;
}
