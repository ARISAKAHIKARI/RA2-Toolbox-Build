#include "tactical_frame_read.h"
#include "planner_world_copy.h"
#include "anti_theft_sample_bridge.h"
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
// Runtime samples raw fields and original read-only weapon/range decisions.
// The same pure planner ranks those facts in the configured EXE or Runtime location.
#include "combat_probe_bridge.h"
static planner::World planningWorld,planningCapture;static int planningFrame=-1;static uint32_t planningSerial,inputSerial;
static int pausedOreFrame=-1;
static void oreHarvestReset();
static void clearOreOrders(){oreHarvestReset();oreConfigRevision=-1;oreConfig={};manualVersions.clear();planningFrame=pausedOreFrame=-1;inputSerial=planningSerial=0;snap.oreQueued=snap.oreDocking=snap.oreIssued=snap.oreManual=0;}
static void cancelOreJob(uint32_t pointer){if(worldObjects.contains(pointer)){int k=kind(pointer);uint32_t type=typeOf(pointer,k);bool miner=k==2&&type&&(minerDefenseMiner(type)||(refreshOreConfig()&&oreMiner(type)));uint32_t id=get<uint32_t>(pointer+0x10);
  manualVersions.change(id,miner?pointer:0,miner);pauseMissileForManual(id,pointer);releaseAutoOwner(id);minerDefenseForget(id);++inputSerial;}}
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
extern "C" int GuardedReadObjectIdentity(unsigned,unsigned*);
static bool readObjectIdentity(uint32_t object,uint32_t&id){
 // Raw sample consumers must stay on the sealed source, including failures.
 if(sampleReadDepth)return read(object+0x10,&id,sizeof id);
 return GuardedReadObjectIdentity(object,&id)!=0;
}
static uint32_t worldByID(uint32_t id){uint32_t p=worldObjects.byId(id),actual=0;return p&&readObjectIdentity(p,actual)&&actual==id?p:0;}
static uint32_t rawObjectID(uint32_t pointer){uint32_t id=0;return worldObjects.contains(pointer)&&readObjectIdentity(pointer,id)?id:0;}
static uint32_t readMapCell(int x,int y){
 if(x<=0||y<=0||x>=512||y>=512)return 0;int index=y*512+x,count=get<int>(0x87F7E8+320);uint32_t cells=get<uint32_t>(0x87F7E8+316);
 return cells>=0x10000&&count>index&&count<=512*512?get<uint32_t>(cells+index*4):0;
}
#include "magnetron_drop_bridge.h"
static void observeNativeOreIntent(uint32_t p,uint32_t target,uint32_t cell,uint32_t bindings){
 const uint32_t id=get<uint32_t>(p+0x10);
 if(!manualVersions.watchingOre(id))return;
 bool mining=false;
 if(!(bindings&(pointerinput::NativeForced|pointerinput::Shift))){
  if(target&&own(target,lastHouse)&&kind(target)==3){
   uint32_t type=typeOf(target,3);Vec docks=vec(typeOf(p,2)+0x3E8,256);
   for(int n=0;n<docks.count;++n)if(item(docks,n)==type){mining=true;break;}
  }else if(!target){
   // YRpp CellClass::OverlayTypeIndex +0x44, OverlayTypeClass::Tiberium +0x2A9.
   // Raw reads only: do not invoke a cell action, radio or docking routine.
   uint32_t at=readMapCell(int(cell%1000),int(cell/1000));Vec overlays=vec(0xA83D80,4096);
   int index=at?get<int>(at+0x44):-1;uint32_t overlay=index>=0&&index<overlays.count?item(overlays,index):0;
   mining=overlay>=0x10000&&get<bool>(overlay+0x2A9);
  }
 }
 // An explicit mining/refinery click hands back assistance. Ordinary movement
 // must first be observed as Move, then return to Harvest; no timeout resumes it.
 if(mining)manualVersions.resumeOre(id);
 else{manualVersions.expectOreMission(id,target?1:2);manualVersions.observeOre(id,get<int>(p+0xAC),get<int>(p+0xB4));}
}
static bool publishPlanningWorld(uint32_t house,int frame,DWORD now){
 auto&w=planningCapture;
 struct DropCandidate {uint32_t pointer,victim;int actor;};
 static DropCandidate dropCandidates[planner::MaxActors];int dropCount=0;
 bool queryCombat=false;static bool oreParticipant[planner::MaxActors];
 uint32_t nextMode=0;
 static uint32_t lastMode;
 {SampleReadScope frameRead;if(!frameRead)return false;
 bool advanced=(sh->flags&(AdvancedInput|MagnetronInput))&&nativeCount>0;
 bool ore=((sh->flags|sh->plannerTracking|sh->runtimeTracking)&AutoOreReturn)!=0;
 bool minerDefense=sh->minerDefenseEnabled||minerDefensePending();
 const bool defenseRequested=(sh->flags&AntiTheft)!=0;bool defense=defenseRequested,drop=sh->magnetronDrop!=0;
 worldframe::Key defenseSource{};long defenseRevision=-1;
 if(defenseRequested){bool changed=false;if(!collectAntiTheftPresence(house,defense,changed,defenseRevision))return false;defenseSource=worldFrame.key;
  // A changed participant filter/source may retain the same enabled mode.
  // Negative-to-negative observations do not bypass other features' frame gate.
  if(changed&&(defense||(lastMode&AntiTheft)))planningFrame=-1;
 }
 if(ore&&!refreshOreConfig())return false;if(minerDefense&&!refreshFeatureTypes())return false;
 static long configSeen=-1;bool configChanged=configSeen!=oreConfigRevision;if(configChanged){planningFrame=-1;configSeen=oreConfigRevision;}
 static long defenseConfigSeen=-1;if(minerDefense&&defenseConfigSeen!=featureConfigRevision){planningFrame=-1;defenseConfigSeen=featureConfigRevision;}
 if(!advanced&&!ore&&!minerDefense&&!defense&&!drop&&!sh->plannerTracking&&!sh->runtimeTracking&&!planningWorld.count&&!planningWorld.intentCount)return false;
 uint32_t mode=(advanced?(sh->flags&(AdvancedInput|MagnetronInput)):0)|(ore?AutoOreReturn:0)|(defense?AntiTheft:0)|(drop?1u<<31:0)|(minerDefense?1u<<29:0);
 if(planningFrame==frame&&planningSerial==inputSerial&&lastMode==mode)return false;
 // No metadata mutation precedes this point. Idle and repeated same-frame
 // observations must not copy the entire rollback cache before returning.
 static WeaponCacheEntry cacheBefore[1024]{};memcpy(cacheBefore,weaponCache,sizeof weaponCache);
 if(lastMode!=mode)memset(weaponCache,0,sizeof weaponCache);nextMode=mode;
 w.match=match;w.tick=now;w.frame=frame;w.count=w.intentCount=w.refineryCount=w.combatCount=0;w.inputSerial=inputSerial;
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;static uint32_t refineryTypes[512],dockMatch,dockItems;static int refineryTypesCount,dockFrame=-100000,dockCount=-1;static bool dockOre=false,dockDefense=false;
 // Dock compatibility is type metadata. Cache it instead of rescanning all
 // combat units just to discover the same refinery types on every frame.
 if(ore||minerDefense){Vec types=vec(0xA83CE0,4096);
  if(configChanged||dockOre!=ore||dockDefense!=minerDefense||dockMatch!=match||dockItems!=types.items||dockCount!=types.count||frame-dockFrame>=60){
   dockMatch=match;dockItems=types.items;dockCount=types.count;dockFrame=frame;dockOre=ore;dockDefense=minerDefense;refineryTypesCount=0;
   for(int i=0;i<types.count&&refineryTypesCount<512;++i){uint32_t type=item(types,i);const bool nativeMiner=minerDefense&&minerDefenseMiner(type);if(type<0x10000||(!nativeMiner&&(!ore||!oreMiner(type))))continue;
    Vec docks=vec(type+0x3E8,256);for(int j=0;j<docks.count&&refineryTypesCount<512;++j){uint32_t dock=item(docks,j);bool known=false;for(int n=0;n<refineryTypesCount;++n)if(refineryTypes[n]==dock)known=true;if(dock&&!known&&(nativeMiner||oreTypeAllowed(dock,false)))refineryTypes[refineryTypesCount++]=dock;}
   }
  }
 }
 if(advanced||ore||minerDefense||defense||drop)for(int pass=0;pass<2;++pass)for(int i=0;i<all.count&&w.count<planner::MaxActors;++i){uint32_t p=item(all,i);int k=p?kind(p):-1;
  if(k<0||get<int>(p+0x6C)<=0)continue;
  bool ownActor=get<uint32_t>(p+0x21C)==house;
  // Native refinery/chrono transitions may temporarily hide the unloading
  // miner. Keep its read-only unloading evidence; it cannot receive orders
  // until the separate live/on-map admission succeeds after completion.
  if(!get<bool>(p+0x74)&&!(ore&&ownActor&&k==2&&get<bool>(p+0x6D1)))continue;
  if(ownActor!=(pass==0))continue;if(!advanced&&!minerDefense&&!defense&&!drop&&(!ownActor||(k!=2&&k!=3)))continue;
  uint32_t type=typeOf(p,k);if(!type)continue;
  bool tracked=advanced&&ownActor&&orderIndex(p)>=0;
  bool dropSource=drop&&ownActor&&rawMagnetron(p,featuretypes::MagnetronDrop);
  bool harvester=ore&&ownActor&&k==2&&oreMiner(type);
  bool defenseMiner=minerDefense&&ownActor&&k==2&&minerDefenseMiner(type);
  bool spiderSource=minerDefense&&ownActor&&(minerDefenseType(type,k)||minerDefenseOwns(get<uint32_t>(p+0x10)));
  bool oreDock=false;if((ore||minerDefense)&&ownActor&&k==3)for(int j=0;j<refineryTypesCount;++j)if(refineryTypes[j]==type){oreDock=true;break;}
  bool turret=defense&&ownActor&&antiTheftBuilding(p);
  bool threat=defense&&!ownActor&&antiTheftThreat(p,house);
  bool spider=minerDefense&&!ownActor&&minerDefenseThreat(p,house,true);
  bool enemy=!ownActor&&(advanced||threat||spider)&&nativeEnemy(p,house);
  if(!tracked&&!harvester&&!oreDock&&!enemy&&!turret&&!dropSource&&!spiderSource)continue;
  auto&a=w.actors[w.count++];memset(&a,0,sizeof a);a.combatFirst=-1;oreParticipant[w.count-1]=harvester||defenseMiner;
  a.id=get<uint32_t>(p+0x10);a.type=type;a.kind=k;a.x=get<int>(p+0x9C);a.y=get<int>(p+0xA0);a.z=get<int>(p+0xA4);
  a.mission=get<int>(p+0xAC);a.queued=get<int>(p+0xB4);a.cost=get<int>(type+0x610);a.veterancy=get<float>(p+0x150);a.manualVersion=manualVersions.get(a.id);
  if(ownActor)a.flags|=planner::Own;else if(enemy)a.flags|=planner::Enemy;
  if(get<bool>(p+0x83))a.flags|=planner::Selected;if(get<bool>(p+0x1C8))a.flags|=planner::Disabled;
  if(get<bool>(p+0x81))a.flags|=planner::Limbo;if(!get<bool>(p+0x74)||!get<bool>(p+0x90))a.flags|=planner::OffMap;
  if(get<uint32_t>(p+0x11C))a.flags|=planner::Transported;if(get<int>(p+0x220)!=0)a.flags|=planner::Cloaked;
  a.target=rawObjectID(get<uint32_t>(p+0x2B4));uint32_t destination=k!=3?get<uint32_t>(p+0x5A4):0;a.destination=rawObjectID(destination);
  if(destination>=0x10000&&!a.destination){int x=get<short>(destination+0x24),y=get<short>(destination+0x26);if(readMapCell(x,y)==destination)a.destinationCell=uint32_t(y*1000+x);}
  if(turret)a.flags|=planner::PointDefense;if(threat)a.flags|=planner::Saboteur;
  if(spider)a.flags|=planner::ParasiteThreat;if(minerDefenseOwns(a.id))a.flags|=planner::HarvesterDefending;
  if(advanced||defense||dropSource||spider||spiderSource)rawCombatInfo(p,type,k,a,tracked||turret||dropSource||spiderSource,true);
  // Native height/cliff/air queries run after the pure conversion scope.
  if(dropSource)dropCandidates[dropCount++]={p,get<uint32_t>(p+0x2AC),w.count-1};
  if(harvester){a.flags|=planner::Harvester;a.storageLimit=get<int>(type+0x800);read(p+0x33C,a.storage,sizeof a.storage);
   // Defense publishes native docks outside the ore whitelist for recovery.
   // Keep automatic-unload compatibility restricted to its own filter.
   Vec docks=vec(type+0x3E8,256);for(int j=0;j<docks.count&&j<8;++j){uint32_t dock=item(docks,j);if(dock&&oreTypeAllowed(dock,false))a.dockTypes[j]=dock;}
   if(get<bool>(p+0x6D1))a.flags|=planner::Unloading;
  }
  if(oreDock&&!(a.flags&(planner::Limbo|planner::OffMap))){
   int count=get<int>(p+0xE8);uint32_t links=get<uint32_t>(p+0xE4);
   if(count>=0&&count<=256&&links>=0x10000)for(int j=0;j<count&&j<4;++j)a.radioIDs[j]=rawObjectID(get<uint32_t>(links+j*4));
   if(count<0||count>4||(count&&links<0x10000))a.radioIDs[3]=0xFFFFFFFFu;
   // Defense needs refinery identities and radio metadata even with automatic
   // unloading off. Only ore queue planning needs the sampled waiting rings.
   if(!ore||!oreTypeAllowed(type,false)||w.refineryCount>=planner::MaxRefineries)continue;
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
 for(int i=0;i<nativeCount&&w.intentCount<planner::MaxIntents;++i){const auto&o=nativeOrders[i];w.intents[w.intentCount++]={o.id,o.cell,o.token,o.mode,uint32_t(o.global)};}
 queryCombat=advanced&&sh->nativeCombatQueries;
 if(advanced&&!queryCombat)collectCombatProbes(w,true);
 if(!frameRead.complete()||(defenseRequested&&!antiTheftSourceCurrent(defenseSource,defenseRevision))){
  // An incomplete observation must neither replace a valid input nor poison
  // the existing metadata caches with values returned by failed reads.
  memcpy(weaponCache,cacheBefore,sizeof weaponCache);dockFrame=-100000;dockCount=-1;return false;
 }
 } // sealed facts only; native queries and publication follow outside scope
 for(int n=0;n<dropCount;++n){const auto&candidate=dropCandidates[n];auto&a=w.actors[candidate.actor];
  if(worldByID(a.id)==candidate.pointer&&rawMagnetronDrop(candidate.pointer,candidate.victim,house))a.flags|=planner::DropHazard;
 }
 if(queryCombat)collectCombatProbes(w);
 // Permission observation is advanced only by observePausedOre/input hooks
 // with live missions. A new input can share this frame with older raw bytes.
 for(int n=0;n<w.count;++n)if(oreParticipant[n]&&manualVersions.orePaused(w.actors[n].id))w.actors[n].flags|=planner::OrePaused;
 if(!planner::copyActiveWorld(planningWorld,w))return false;
 planningFrame=frame;planningSerial=w.inputSerial;lastMode=nextMode;
 InterlockedIncrement(&sh->worldSeq);sh->world.match=w.match;sh->world.tick=w.tick;sh->world.frame=w.frame;sh->world.count=w.count;sh->world.intentCount=w.intentCount;sh->world.refineryCount=w.refineryCount;sh->world.inputSerial=w.inputSerial;
 memcpy(sh->world.actors,w.actors,size_t(w.count)*sizeof(planner::Actor));memcpy(sh->world.intents,w.intents,size_t(w.intentCount)*sizeof(planner::Intent));memcpy(sh->world.refineries,w.refineries,size_t(w.refineryCount)*sizeof(planner::Refinery));
 sh->world.combatCount=w.combatCount;memcpy(sh->world.combat,w.combat,size_t(w.combatCount)*sizeof(planner::CombatProbe));
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
#include "ore_harvest_bridge.h"
// Retain only an effective current shot. A live infiltrator outside the
// weapon annulus (or no longer targetable) must not block a new nearby threat
// after the planner has already decided to switch.
static bool antiTheftCurrentTargetUsable(uint32_t unit,uint32_t house){
 const uint32_t target=get<uint32_t>(unit+0x2B4);
 if(!antiTheftThreat(target,house)||!get<bool>(target+0x90)||get<bool>(target+0x81)||
    get<uint32_t>(target+0x11C)||get<int>(target+0x220)!=0)return false;
 const int sk=kind(unit),tk=kind(target);const uint32_t st=typeOf(unit,sk),tt=typeOf(target,tk);
 if(!st||!tt)return false;
 planner::Actor source{},victim{};rawCombatInfo(unit,st,sk,source,true);rawCombatInfo(target,tt,tk,victim,false);
 const int64_t dx=int64_t(get<int>(unit+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(unit+0xA0))-get<int>(target+0xA0);
 if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)return false;
 return planner::canTarget(source,victim,dx*dx+dy*dy,false);
}

static bool externalOrderValid(const planner::Order&o,uint32_t house){
 uint32_t unit=worldByID(o.unit);if(!unit||!own(unit,house)||manualVersions.get(o.unit)!=o.token||(stickyOwns(unit)&&o.purpose!=planner::MagnetronDropOrder))return false;
 if(get<int>(unit+0x6C)<=0||!get<bool>(unit+0x74)||get<bool>(unit+0x81)||!get<bool>(unit+0x90)||get<bool>(unit+0x1C8)||get<uint32_t>(unit+0x11C))return false;
 if(burstOwns(o.unit)||dodgeOwns(o.unit)||minerDefenseOwns(o.unit))return false;
 if(o.purpose==planner::OreHarvestOrder)return oreHarvestOrderValid(o,house);
 if(o.purpose==planner::MagnetronDropOrder)return o.mission==13&&!o.target&&!o.destination&&!o.destinationTag&&!o.pursuit&&o.refinery&&rawMagnetronDrop(unit,worldByID(o.refinery),house);
 int globalIndex=orderIndex(unit);if(globalIndex>=0&&nativeOrders[globalIndex].global&&(!sh->magnetronGlobal||(o.mission==2&&!o.pursuit)))return false;
 bool defense=o.purpose==planner::DefenseOrder,combat=o.purpose==1||o.purpose==3,mag=o.purpose==3;
 if(defense){if(!(sh->flags&AntiTheft)||!antiTheftBuilding(unit)||o.mission!=1||!o.target||o.destination||o.destinationTag||!antiTheftThreat(worldByID(o.target),house)||antiTheftCurrentTargetUsable(unit,house))return false;}
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
 // Reject stale pre.6 Sticky proposals at the shared admission boundary.
 if(o.mission!=1&&o.mission!=2&&o.mission!=5&&o.mission!=7)return false;
 if(o.mission==5&&(!combat||o.target||o.destination||o.destinationTag||o.pursuit))return false;
 if(o.mission==1&&!o.target)return false;
 if(o.target){uint32_t target=worldByID(o.target);if((!combat&&!defense)||o.mission!=1||!target||!nativeEnemy(target,house)||get<int>(target+0x6C)<=0||!get<bool>(target+0x74)||get<bool>(target+0x81)||!get<bool>(target+0x90)||get<uint32_t>(target+0x11C)||get<int>(target+0x220)!=0)return false;
  planner::Actor source{},victim{};int sk=kind(unit),tk=kind(target);uint32_t st=typeOf(unit,sk),tt=typeOf(target,tk);if(!st||!tt)return false;
  victim.kind=tk;rawCombatInfo(unit,st,sk,source,true);rawCombatInfo(target,tt,tk,victim,false);
  int64_t dx=int64_t(get<int>(unit+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(unit+0xA0))-get<int>(target+0xA0);
  if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)return false;
  if(combat&&!mag){auto probe=probeCombatPair(unit,target,source,victim,0);if(!(probe.flags&(planner::InWeaponRange|planner::NeedsReposition)))return false;}
  else if(mag?!planner::magneticAcquisition(source,victim,dx*dx+dy*dy,int(std::min(uint32_t(sh->advancedRangeCells),30u))*256):!planner::canTarget(source,victim,dx*dx+dy*dy,false,0))return false;
 }
 if(o.pursuit){
  if((o.purpose!=planner::CombatOrder&&o.purpose!=planner::MagnetronOrder)||o.mission!=2||o.target||o.destinationTag!=11)return false;
  uint32_t target=worldByID(o.pursuit);if(!target||!nativeEnemy(target,house))return false;
  planner::Actor source{},victim{};rawCombatInfo(unit,typeOf(unit,kind(unit)),kind(unit),source,true);rawCombatInfo(target,typeOf(target,kind(target)),kind(target),victim,false);
  int padding=int(std::min(uint32_t(sh->advancedRangeCells),30u))*256;
  if(mag){int64_t dx=int64_t(get<int>(unit+0x9C))-get<int>(target+0x9C),dy=int64_t(get<int>(unit+0xA0))-get<int>(target+0xA0);
   if(!planner::magneticAcquisition(source,victim,dx*dx+dy*dy,padding)||planner::canTarget(source,victim,dx*dx+dy*dy,true))return false;
  }else{auto probe=probeCombatPair(unit,target,source,victim,padding);
   if(!(probe.flags&planner::InAcquisitionRange)||(probe.flags&planner::InWeaponRange))return false;
  }
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
 // A fresh in-range target takes precedence over any delayed movement proposal.
 // Reject an obsolete movement proposal immediately so an external planner can
 // receive its receipt and use the next world; do not block its whole batch
 // until the original movement deadline expires.
 if(o.purpose==planner::CombatOrder&&o.mission==2&&advancedTargetInRange(o.unit,house))return -1;
 if(!externalOrderValid(o,house))return -1;
 // Explicit and global magnets may use different calculation locations.
 // Reserve queued magnetic targets at their common admission boundary too.
 if(o.purpose==planner::MagnetronOrder&&o.mission==1){int frame=get<int>(0xA8ED84);
  for(int n=0;n<nativeCount;++n){const auto&r=nativeOrders[n];if(r.id!=o.unit&&r.mode==planner::MagnetronMove&&r.attacking&&r.target==o.target&&frame>=r.sentFrame&&frame-r.sentFrame<12&&manualVersions.get(r.id)==r.token)return -1;}
 }
 actorFrameArbiter.begin(match,get<int>(0xA8ED84));
 if(!actorFrameArbiter.canIssue(o.unit,o.token)){++snap.planDeferred;return 0;}
 if(!planningRoom(0)||!outgoingRoom()){++snap.planDeferred;return 0;}
 if(o.purpose==planner::MagnetronDropOrder){if(!nativeObjectEvent(get<int>(house+0x30),vanilla::EventType::Idle,o.unit,OrderAuthority::Local,nullptr,true))return 0;int index=orderIndex(worldByID(o.unit));if(index>=0)nativeOrders[index].attacking=false;consumePlanning(0);return 1;}
 // Native Harvest click uses (10,nullptr,clickedCell,nullptr), not Move's
 // nearest-accessible override. Preserve that exact target packing.
 const bool nearest=o.purpose!=planner::OreHarvestOrder&&o.destinationTag==11&&(!sh||sh->megaQueueMode==0);
 if(!nativeMission(house,o.unit,o.mission,o.target,o.destination,o.destinationTag,nearest?o.destination:0,nearest?11:0,
    o.purpose==3?taskarbiter::Owner::Magnetron:(o.purpose==1||o.purpose==planner::DefenseOrder)?taskarbiter::Owner::Combat:taskarbiter::Owner::Ore))return -1;
 consumePlanning(0);minerDefenseRememberOre(o,house);
 if(o.purpose==1||o.purpose==3){int index=orderIndex(worldByID(o.unit));if(index>=0){auto&r=nativeOrders[index];r.stopped=o.mission==5;r.attacking=o.mission==1;r.target=o.target;r.sentFrame=get<int>(0xA8ED84);}}
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
   int index=o.purpose==planner::MagnetronOrder?orderIndex(worldByID(o.unit)):-1;
   compute::Feature family=o.purpose==planner::CombatOrder?compute::Advanced:o.purpose==planner::MagnetronDropOrder?compute::MagnetronDrop:o.purpose==planner::MagnetronOrder?(index>=0&&nativeOrders[index].global?compute::MagnetronGlobal:compute::Magnetron):o.purpose==planner::DefenseOrder?compute::Defense:compute::Ore;
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

