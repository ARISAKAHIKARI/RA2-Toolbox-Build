// YRpp BuildingTypeClass::ClickRepairable is +0x157A (next: CanBeOccupied +0x157B).
// Config only controls eligibility; never changes game type fields or Dock lists.
static oreconfig::Config oreConfig{};static long oreConfigRevision=-1;
static bool refreshOreConfig(){
 long seq=sh->oreConfigSeq;if(seq&1)return false;if(seq==oreConfigRevision)return true;
 oreconfig::Config next;MemoryBarrier();memcpy(&next,&sh->oreConfig,sizeof next);MemoryBarrier();
 if(seq!=sh->oreConfigSeq||!oreconfig::valid(next))return false;
 oreConfig=next;oreConfigRevision=seq;return true;
}
static bool oreTypeAllowed(uint32_t type,bool miner){char id[24]{};if(!read(type+0x24,id,23))return false;
 return oreconfig::allowed(oreConfig,miner,id);
}
static bool oreMiner(uint32_t type){return type&&oreTypeAllowed(type,true)&&
 (oreConfig.customMiners||(get<bool>(type+0xE0E)||get<bool>(type+0xE0F)))&&get<int>(type+0x800)>0;}
#include "ore_queue.h"
// The injected side reads raw fields and serializes already planned orders.
// Target ranking, weapon range, ore totals and dock arbitration run in the EXE.
static planner::World planningWorld;static int planningFrame=-1;static uint32_t planningSerial,inputSerial;
static int pausedOreFrame=-1;
static void clearOreOrders(){oreConfigRevision=-1;oreConfig={};manualVersions.clear();planningFrame=pausedOreFrame=-1;inputSerial=planningSerial=0;snap.oreQueued=snap.oreDocking=snap.oreIssued=snap.oreManual=0;}
static void cancelOreJob(uint32_t pointer){if(worldObjects.contains(pointer)){int k=kind(pointer);uint32_t type=typeOf(pointer,k);bool miner=k==2&&type&&refreshOreConfig()&&oreMiner(type);uint32_t id=get<uint32_t>(pointer+0x10);
  manualVersions.change(id,miner?pointer:0,miner);releaseAutoOwner(id);++inputSerial;}}
static void observePausedOre(uint32_t house,int frame){
 if(frame==pausedOreFrame)return;pausedOreFrame=frame;
 // Follow only the miners that were manually suspended. This remains a small
 // read-only observation while automation is off; no world scan or order send.
 for(unsigned n=0;n<manualVersions.pausedCount;){unsigned slot=manualVersions.pausedSlots[n];uint32_t id=manualVersions.ids[slot],p=manualVersions.orePointers[slot];uint32_t raw[0x220/4];
  if(!read(p,raw,sizeof raw)||raw[0x10/4]!=id||int(raw[0x6C/4])<=0||raw[0x21C/4]!=house)manualVersions.resumeOre(id);
  else manualVersions.observeOre(id,int(raw[0xAC/4]),int(raw[0xB4/4]));
  if(n<manualVersions.pausedCount&&manualVersions.pausedSlots[n]==slot)++n;
 }
 snap.oreManual=int(manualVersions.pausedCount);
}
static uint32_t worldByID(uint32_t id){uint32_t p=worldObjects.byId(id),actual=0;return p&&read(p+0x10,&actual,sizeof actual)&&actual==id?p:0;}
static uint32_t rawObjectID(uint32_t pointer){uint32_t id=0;return worldObjects.contains(pointer)&&read(pointer+0x10,&id,sizeof id)?id:0;}
static uint32_t readMapCell(int x,int y){
 if(x<=0||y<=0||x>=512||y>=512)return 0;int index=y*512+x,count=get<int>(0x87F7E8+320);uint32_t cells=get<uint32_t>(0x87F7E8+316);
 return cells>=0x10000&&count>index&&count<=512*512?get<uint32_t>(cells+index*4):0;
}
static bool publishPlanningWorld(uint32_t house,int frame,DWORD now){
 bool advanced=(sh->flags&(AdvancedInput|MagnetronInput))&&nativeCount>0;
 bool ore=((sh->flags|sh->plannerTracking|sh->runtimeTracking)&AutoOreReturn)!=0;
 bool defense=(sh->flags&AntiTheft)!=0;
 if(defense){defense=false;Vec buildings=vec(0xA8EB40);for(int i=0;i<buildings.count;++i){uint32_t p=item(buildings,i);if(p&&own(p,house)&&get<int>(p+0x6C)>0&&!get<bool>(p+0x81)&&antiTheftBuilding(p)){defense=true;break;}}}
 if(ore&&!refreshOreConfig())return false;
 static long configSeen=-1;bool configChanged=configSeen!=oreConfigRevision;if(configChanged){planningFrame=-1;configSeen=oreConfigRevision;}
 if(!advanced&&!ore&&!defense&&!sh->plannerTracking&&!sh->runtimeTracking&&!planningWorld.count&&!planningWorld.intentCount)return false;
 static uint32_t lastMode;uint32_t mode=(advanced?(sh->flags&(AdvancedInput|MagnetronInput)):0)|(ore?AutoOreReturn:0)|(defense?AntiTheft:0);
 if(planningFrame==frame&&planningSerial==inputSerial&&lastMode==mode)return false;planningFrame=frame;planningSerial=inputSerial;if(lastMode!=mode)memset(weaponCache,0,sizeof weaponCache);lastMode=mode;
 auto&w=planningWorld;w.match=match;w.tick=now;w.frame=frame;w.count=w.intentCount=w.refineryCount=0;w.inputSerial=inputSerial;
 Vec all=vec(0xA8EC78);static uint32_t refineryTypes[128],dockMatch,dockItems;static int refineryTypesCount,dockFrame=-100000,dockCount=-1;
 // Dock compatibility is type metadata. Cache it instead of rescanning all
 // combat units just to discover the same refinery types on every frame.
 if(ore){Vec types=vec(0xA83CE0,4096);
  if(configChanged||dockMatch!=match||dockItems!=types.items||dockCount!=types.count||frame-dockFrame>=60){
   dockMatch=match;dockItems=types.items;dockCount=types.count;dockFrame=frame;refineryTypesCount=0;
   for(int i=0;i<types.count&&refineryTypesCount<128;++i){uint32_t type=item(types,i);if(type<0x10000||!oreMiner(type))continue;
    Vec docks=vec(type+0x3E8,256);for(int j=0;j<docks.count&&refineryTypesCount<128;++j){uint32_t dock=item(docks,j);bool known=false;for(int n=0;n<refineryTypesCount;++n)if(refineryTypes[n]==dock)known=true;if(dock&&!known&&oreTypeAllowed(dock,false))refineryTypes[refineryTypesCount++]=dock;}
   }
  }
 }
 if(advanced||ore||defense)for(int pass=0;pass<2;++pass)for(int i=0;i<all.count&&w.count<planner::MaxActors;++i){uint32_t p=item(all,i);int k=p?kind(p):-1;
  if(k<0||get<int>(p+0x6C)<=0||!get<bool>(p+0x74))continue;
  bool ownActor=get<uint32_t>(p+0x21C)==house;if(ownActor!=(pass==0))continue;if(!advanced&&!defense&&(!ownActor||(k!=2&&k!=3)))continue;
  uint32_t type=typeOf(p,k);if(!type)continue;
  bool tracked=advanced&&ownActor&&orderIndex(p)>=0;
  bool harvester=ore&&ownActor&&k==2&&oreMiner(type);
  bool oreDock=false;if(ore&&ownActor&&k==3)for(int j=0;j<refineryTypesCount;++j)if(refineryTypes[j]==type){oreDock=true;break;}
  bool turret=defense&&ownActor&&antiTheftBuilding(p);
  bool threat=defense&&!ownActor&&antiTheftThreat(p,house);
  bool enemy=!ownActor&&(advanced||threat)&&nativeEnemy(p,house);
  if(!tracked&&!harvester&&!oreDock&&!enemy&&!turret)continue;
  auto&a=w.actors[w.count++];memset(&a,0,sizeof a);
  a.id=get<uint32_t>(p+0x10);a.type=type;a.kind=k;a.x=get<int>(p+0x9C);a.y=get<int>(p+0xA0);a.z=get<int>(p+0xA4);
  a.mission=get<int>(p+0xAC);a.queued=get<int>(p+0xB4);a.cost=get<int>(type+0x610);a.veterancy=get<float>(p+0x150);a.manualVersion=manualVersions.get(a.id);
  if(ownActor)a.flags|=planner::Own;else if(enemy)a.flags|=planner::Enemy;
  if(get<bool>(p+0x83))a.flags|=planner::Selected;if(get<bool>(p+0x1C8))a.flags|=planner::Disabled;
  if(get<bool>(p+0x81))a.flags|=planner::Limbo;if(!get<bool>(p+0x90))a.flags|=planner::OffMap;
  if(get<uint32_t>(p+0x11C))a.flags|=planner::Transported;if(get<int>(p+0x220)!=0)a.flags|=planner::Cloaked;
  a.target=rawObjectID(get<uint32_t>(p+0x2B4));uint32_t destination=k!=3?get<uint32_t>(p+0x5A4):0;a.destination=rawObjectID(destination);
  if(destination>=0x10000&&!a.destination){int x=get<short>(destination+0x24),y=get<short>(destination+0x26);if(readMapCell(x,y)==destination)a.destinationCell=uint32_t(y*1000+x);}
  if(turret)a.flags|=planner::PointDefense;if(threat)a.flags|=planner::Saboteur;
  if(advanced||defense)rawCombatInfo(p,type,k,a,tracked||turret,true);
  if(harvester){a.flags|=planner::Harvester;a.storageLimit=get<int>(type+0x800);memcpy(a.storage,(void*)(p+0x33C),sizeof a.storage);
   if(manualVersions.observeOre(a.id,a.mission,a.queued))a.flags|=planner::OrePaused;
   Vec docks=vec(type+0x3E8,256);for(int j=0;j<docks.count&&j<8;++j)a.dockTypes[j]=item(docks,j);
   if(get<bool>(p+0x6D1))a.flags|=planner::Unloading;
  }
  if(oreDock&&!(a.flags&(planner::Limbo|planner::OffMap))){
   int count=get<int>(p+0xE8);uint32_t links=get<uint32_t>(p+0xE4);
   if(count>=0&&count<=256&&links>=0x10000)for(int j=0;j<count&&j<4;++j)a.radioIDs[j]=rawObjectID(get<uint32_t>(links+j*4));
   if(count<0||count>4||(count&&links<0x10000))a.radioIDs[3]=0xFFFFFFFFu;
   if(w.refineryCount>=planner::MaxRefineries)continue;
   auto&refinery=w.refineries[w.refineryCount++];memset(&refinery,0,sizeof refinery);refinery.id=a.id;refinery.cellX=a.x/256;refinery.cellY=a.y/256;
   refinery.dockStyleA=get<bool>(type+0x16E4);refinery.dockStyleB=get<bool>(type+0x16E5);
   // Read the neighbourhood around both original refinery dock styles. No cell
   // entry, pathfinder, radio query or game-side docking helper is called.
   int cx=refinery.cellX+(refinery.dockStyleB?2:1),cy=refinery.cellY+2;
   for(int radius=1;radius<=4&&refinery.count<planner::MaxWaitingCells;++radius)for(int dy=-radius;dy<=radius&&refinery.count<planner::MaxWaitingCells;++dy)for(int dx=-radius;dx<=radius&&refinery.count<planner::MaxWaitingCells;++dx){
    if(dx!=-radius&&dx!=radius&&dy!=-radius&&dy!=radius)continue;int x=cx+dx,y=cy+dy;uint32_t cell=readMapCell(x,y);if(!cell)continue;
    auto&sample=refinery.waiting[refinery.count++];sample.cell=uint32_t(y*1000+x);sample.occupation=get<uint32_t>(cell+0x124);sample.land=get<int>(cell+0xEC);
   }
  }
 }
 for(int i=0;i<nativeCount&&w.intentCount<planner::MaxIntents;++i){const auto&o=nativeOrders[i];w.intents[w.intentCount++]={o.id,o.cell,o.token,o.mode};}
 InterlockedIncrement(&sh->worldSeq);sh->world.match=w.match;sh->world.tick=w.tick;sh->world.frame=w.frame;sh->world.count=w.count;sh->world.intentCount=w.intentCount;sh->world.refineryCount=w.refineryCount;sh->world.inputSerial=w.inputSerial;
 memcpy(sh->world.actors,w.actors,size_t(w.count)*sizeof(planner::Actor));memcpy(sh->world.intents,w.intents,size_t(w.intentCount)*sizeof(planner::Intent));memcpy(sh->world.refineries,w.refineries,size_t(w.refineryCount)*sizeof(planner::Refinery));
 // The dispatcher publishes the matching control frame before notifying the
 // host. A world update must also prevent the display-throttle early return.
 MemoryBarrier();InterlockedIncrement(&sh->worldSeq);return true;
}
static bool rawOreCompatible(uint32_t unit,uint32_t refinery){
 if(kind(unit)!=2||kind(refinery)!=3)return false;uint32_t t=typeOf(unit,2),r=typeOf(refinery,3);
 if(!refreshOreConfig()||!t||!r||!oreMiner(t)||!oreTypeAllowed(r,false)||get<bool>(refinery+0x1C8))return false;
 Vec docks=vec(t+0x3E8,256);for(int i=0;i<docks.count&&i<8;++i)if(item(docks,i)==r)return true;return false;
}
static bool rawDockBusy(uint32_t refinery,uint32_t unit){
 int n=get<int>(refinery+0xE8);uint32_t links=get<uint32_t>(refinery+0xE4);if(n<0||n>256||(n&&links<0x10000))return true;
 for(int i=0;i<n;++i){uint32_t p=get<uint32_t>(links+i*4);if(p&&p!=unit)return true;}return false;
}
static bool rawDockLinked(uint32_t refinery,uint32_t unit){
 int n=get<int>(refinery+0xE8);uint32_t links=get<uint32_t>(refinery+0xE4);if(n<0||n>256||(n&&links<0x10000))return true;
 for(int i=0;i<n;++i)if(get<uint32_t>(links+i*4)==unit)return true;return false;
}
static bool externalOrderValid(const planner::Order&o,uint32_t house){
 uint32_t unit=worldByID(o.unit);if(!unit||!own(unit,house)||manualVersions.get(o.unit)!=o.token)return false;
 if(get<int>(unit+0x6C)<=0||!get<bool>(unit+0x74)||get<bool>(unit+0x81)||!get<bool>(unit+0x90)||get<bool>(unit+0x1C8)||get<uint32_t>(unit+0x11C))return false;
 bool defense=o.purpose==planner::DefenseOrder,combat=o.purpose==1||o.purpose==3,mag=o.purpose==3;
 if(defense){if(!(sh->flags&AntiTheft)||!antiTheftBuilding(unit)||o.mission!=1||!o.target||o.destination||o.destinationTag||!antiTheftThreat(worldByID(o.target),house)||antiTheftThreat(get<uint32_t>(unit+0x2B4),house))return false;}
 if(combat){if(mag?!rawMagnetron(unit):!featureObjectAllowed(featuretypes::AdvancedAttack,unit))return false;int index=orderIndex(unit);if(!(sh->flags&(mag?MagnetronInput:AdvancedInput))||snap.nativeHookStatus!=1||index<0||nativeOrders[index].token!=o.token||nativeOrders[index].mode!=(mag?planner::MagnetronMove:planner::OrdinaryMove))return false;}
 else if(o.purpose==0||o.purpose==2||o.purpose==planner::OreYield){
  if(manualVersions.orePaused(o.unit))return false;
  if(((o.purpose==0||o.purpose==planner::OreYield)&&!(sh->flags&AutoOreReturn))||(o.purpose==2&&o.mission!=7))return false;
  uint32_t refinery=worldByID(o.refinery);
  if(!refinery||!own(refinery,house)||!rawOreCompatible(unit,refinery))return false;
  if(get<bool>(unit+0x6D1))return false;
  if(o.purpose==planner::OreYield){
   // Only the explicit managed yield may replace an en-route radio reservation.
   // Recheck distance at execution; never interrupt an arrival or unloading.
   uint32_t rt=typeOf(refinery,3);
   if(o.mission!=2||o.target||o.destinationTag!=11||rawDockBusy(refinery,unit)||
      ore::dockDistance(get<int>(unit+0x9C),get<int>(unit+0xA0),get<int>(refinery+0x9C),get<int>(refinery+0xA0),get<bool>(rt+0x16E5))<=ore::FarDockCells)return false;
  }else if(o.mission==2&&rawDockLinked(refinery,unit))return false;
  float storage[4];memcpy(storage,(void*)(unit+0x33C),sizeof storage);if(ore::storageAmount(storage)<=0)return false;
  // The EXE chooses the dock turn/nearest/cells. Recheck volatile raw preconditions here
  // to reject a dock occupied or foundation placed since its snapshot.
  if(o.mission==7&&(o.destination!=o.refinery||(o.purpose==0&&rawDockBusy(refinery,unit))))return false;
  if(o.mission==2){uint32_t cell=readMapCell(int(o.destination%1000),int(o.destination/1000));if(!cell||get<uint32_t>(cell+0x124)||get<int>(cell+0xEC)==2||get<int>(cell+0xEC)==7)return false;}
 }else if(!defense)return false;
 if(o.mission!=1&&o.mission!=2&&o.mission!=6&&o.mission!=7)return false;
 if(o.mission==1&&!o.target)return false;
 if(o.target){uint32_t target=worldByID(o.target);if((!combat&&!defense)||o.mission!=1||!target||!nativeEnemy(target,house)||get<int>(target+0x6C)<=0||!get<bool>(target+0x74)||get<bool>(target+0x81)||!get<bool>(target+0x90)||get<uint32_t>(target+0x11C)||get<int>(target+0x220)!=0)return false;
  planner::Actor source{},victim{};int sk=kind(unit),tk=kind(target);uint32_t st=typeOf(unit,sk),tt=typeOf(target,tk);if(!st||!tt)return false;
  victim.kind=tk;rawCombatInfo(unit,st,sk,source,true);rawCombatInfo(target,tt,tk,victim,false);
  int64_t dx=int64_t(get<int>(unit+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(unit+0xA0))-get<int>(target+0xA0);
  if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)return false;
  if(!planner::canTarget(source,victim,dx*dx+dy*dy,mag,(mag||defense)?0:int(std::min(uint32_t(sh->advancedRangeCells),30u))*256)||(mag&&planner::cooldown(source,get<int>(0xA8ED84))>0))return false;
 }
 if(o.destinationTag==0x34){uint32_t destination=worldByID(o.destination);if(combat||o.mission!=7||!destination||!own(destination,house)||kind(destination)!=3)return false;}
 else if(o.destinationTag==11){if(o.mission!=2||!readMapCell(int(o.destination%1000),int(o.destination/1000)))return false;}
 else if(o.destinationTag||o.destination)return false;
 return true;
}
static outgoing::Batch planBatch;
// Shared admission for host and Runtime decisions. Neither planner owns a
// constructor, RNG, mission write, private hook or alternative event channel.
static int admitPlannerOrder(const planner::Order&o,uint32_t house){
 if(!externalOrderValid(o,house))return -1;
 actorFrameArbiter.begin(match,get<int>(0xA8ED84));
 if(!actorFrameArbiter.canIssue(o.unit,o.token)){++snap.planDeferred;return 0;}
 if(!planningRoom(0)||!outgoingRoom()){++snap.planDeferred;return 0;}
 const bool nearest=o.destinationTag==11&&(!sh||sh->megaQueueMode==0);
 if(!nativeMission(house,o.unit,o.mission,o.target,o.destination,o.destinationTag,nearest?o.destination:0,nearest?11:0,
    o.purpose==3?taskarbiter::Owner::Magnetron:(o.purpose==1||o.purpose==planner::DefenseOrder)?taskarbiter::Owner::Combat:taskarbiter::Owner::Ore))return -1;
 consumePlanning(0);
 if(o.purpose==1||o.purpose==3){int index=orderIndex(worldByID(o.unit));if(index>=0)nativeOrders[index].stopped=o.mission==6;}
 return 1;
}
static void externalOrdersTick(uint32_t house,int frame,DWORD now){
 uint32_t request=sh->planRequest;
 if(request==sh->planAck)return;
 MemoryBarrier();
 if(request!=planBatch.request)planBatch.begin(request);
 const int count=sh->planCount;
 int result=1;
 // Every unsent order retains the original source deadline, including a
 // partially admitted batch. Admission never grants a longer drain window.
 auto fresh=[&](){return freshness::valid(sh->planMatch,match,sh->planTick,sh->planFrame,sampleClock(sh),get<int>(0xA8ED84),freshness::Tactical);};
 if(count<0||count>planner::MaxOrders||!fresh())result=-7;
 else if(get<bool>(0xA8BC60+25))result=-8;
 else {
  while(!planBatch.done(count)){
   if(!fresh()){result=-7;break;}
   const int i=planBatch.next;
   auto o=sh->plan[i];
   compute::Feature family=o.purpose==planner::CombatOrder?compute::Advanced:o.purpose==planner::MagnetronOrder?compute::Magnetron:o.purpose==planner::DefenseOrder?compute::Defense:compute::Ore;
   if(compute::inside(sh->computeApplied,family)){planBatch.reject();continue;}
   if(!fresh()){result=-7;break;}
   int admitted=admitPlannerOrder(o,house);
   if(!admitted)return;
   if(admitted<0)planBatch.reject();else planBatch.accept();
  }
 }
 // On invalidation/restart, all remaining items are explicitly rejected.
 if(result!=1)while(!planBatch.done(count)&&count>=0&&count<=planner::MaxOrders)planBatch.reject();
 sh->planAccepted=planBatch.accepted;
 sh->planAcceptedMask=planBatch.acceptedMask;
 sh->planRejectedMask=planBatch.rejectedMask;
 sh->planResult=result;
 MemoryBarrier();sh->planAck=request;notifyHost();
}

