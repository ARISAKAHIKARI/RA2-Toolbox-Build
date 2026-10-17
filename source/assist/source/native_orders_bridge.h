#include "native_orders.h"
#include "unload_policy.h"
// All simulation commands go through EventClass constructors and the game's
// outgoing queue. This file NEVER writes a unit's mission, target, or locomotor.
struct NativeOrder {uint32_t p,id,cell,nearest,token,mode;int gx,gy;nativeorders::Progress progress;uint32_t target;int sentFrame;bool attacking,stopped;};
struct UnloadPassenger {uint32_t p,id,carrier,carrierID,carrierType,followTarget,followTargetPointer;int frame,mode,stage,sentFrame,seenUnload,retries,x,y,stableSince,landedFrame;uint32_t followTicket,followQueuedAt;bool sleepObserved,cancelled,followObserved;};
static NativeOrder nativeOrders[nativeorders::MaxOrders];static int nativeCount,nativeCursor,nativeFrame=-1;
static UnloadPassenger unloadPassengers[nativeorders::MaxPassengers];static int unloadCount,unloadCursor;
static ObjectIndex watchedPassengers;
static void restoreNativeHooks();
static void clearNativeOrders(){nativeCount=nativeCursor=unloadCount=unloadCursor=0;nativeFrame=-1;advancedObjects.clear();snap.advancedUnits=snap.unloadPending=0;restoreNativeHooks();}
static bool nativeMember(Vec,uint32_t p){return worldObjects.contains(p);}
static bool nativeReady(){return simulationReady()&&sh->foreground&&!get<bool>(0xA8BC60+25);}

static int orderIndex(uint32_t p){uint32_t slot=advancedObjects.byId(get<uint32_t>(p+0x10));if(!slot||slot>uint32_t(nativeCount))return -1;int i=int(slot-1);return nativeOrders[i].p==p&&nativeOrders[i].id==get<uint32_t>(p+0x10)?i:-1;}
static void removeOrder(int i){nativeOrders[i]=nativeOrders[--nativeCount];if(i<nativeCount)advancedObjects.add(nativeOrders[i].p,nativeOrders[i].id,uint32_t(i+1));}
static void finishUnloadFollowup(const UnloadPassenger&r,int frame){
 if(!r.followTicket)return;
 gateway::Receipt rec{};gateway::Intent intent{match,r.followQueuedAt,r.id,r.followTarget,0};
 gateway::planned(rec,r.followTicket,intent);rec.queuedAt=r.followQueuedAt;rec.owner=taskarbiter::Owner::Unload;
 if(r.followObserved){rec.phase=gateway::Phase::Observed;rec.observedAt=sampleClock(sh);}
 else if(frame>=r.frame&&frame-r.frame>900){rec.phase=gateway::Phase::Expired;rec.failure=gateway::Failure::Stale;++snap.receiptExpired;}
 else {rec.phase=gateway::Phase::Cancelled;rec.failure=gateway::Failure::Superseded;++snap.receiptCancelled;}
 missionObserver.dismissTicket(match,r.followTicket);recordReceipt(rec);
 actorLeaseArbiter.release(match,r.id,r.followTicket);
}
static void cancelUnloadPassenger(uint32_t p){for(int i=0;i<unloadCount;)
 if(unloadPassengers[i].p==p){finishUnloadFollowup(unloadPassengers[i],get<int>(0xA8ED84));unloadPassengers[i]=unloadPassengers[--unloadCount];}
 else ++i;}
static void cancelNative(uint32_t p){cancelOreJob(p);cancelLoadingJobs(p);int i=orderIndex(p);if(i>=0)removeOrder(i);for(i=0;i<unloadCount;)
 if(unloadPassengers[i].p==p||unloadPassengers[i].carrier==p){finishUnloadFollowup(unloadPassengers[i],get<int>(0xA8ED84));unloadPassengers[i]=unloadPassengers[--unloadCount];}
 else ++i;}
static void cancelAutomatic(uint32_t p){
 cancelOreJob(p);cancelLoadingJobs(p);int i=orderIndex(p);if(i>=0)removeOrder(i);
 // Keep a release record until a native Guard can follow a tool-issued Sleep.
 // Manual click paths still use cancelNative because their own order takes over.
 for(i=0;i<unloadCount;++i)if(unloadPassengers[i].p==p||unloadPassengers[i].carrier==p)unloadPassengers[i].cancelled=true;
}
// Registers a new explicit intent only after its native event is accepted.
static bool nativeOutgoing(uint32_t p,int mission,uint32_t packedCell,uint32_t target,uint32_t destination,uint32_t bindings){
 if(!nativeReady()||!own(p,lastHouse))return false;
 int k=kind(p),cx=int(packedCell%1000),cy=int(packedCell/1000);
 bool mag=(bindings&pointerinput::Magnetron)&&(sh->flags&MagnetronInput);
 if(!manualVersions.trackable(get<uint32_t>(p+0x10))||(!(bindings&pointerinput::Advanced)||!(sh->flags&AdvancedInput))&&!mag||(mag?!rawMagnetron(p):!featureObjectAllowed(featuretypes::AdvancedAttack,p))||(k!=1&&k!=2)||!get<bool>(p+0x83)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||target||
  (mission!=2&&mission!=29)||cx<=0||cy<=0||cx>=512||cy>=512||!rawArmed(p))return false;
 int previous=orderIndex(p);if(previous>=0)removeOrder(previous);if(nativeCount>=nativeorders::MaxOrders)return false;
 auto&o=nativeOrders[nativeCount++];o={};o.p=p;o.id=get<uint32_t>(p+0x10);o.mode=mag?planner::MagnetronMove:planner::OrdinaryMove;o.token=manualVersions.get(o.id);o.cell=o.nearest=packedCell;o.gx=cx*256+128;o.gy=cy*256+128;
 advancedObjects.add(p,o.id,uint32_t(nativeCount));snap.advancedUnits=nativeCount;
 o.progress.begin(get<int>(p+0x9C),get<int>(p+0xA0),get<int>(0xA8ED84));o.sentFrame=get<int>(0xA8ED84);return true;
}
static bool nativeEnemy(uint32_t p,uint32_t house){
 if(!live(p)||kind(p)<0)return false;uint32_t owner=get<uint32_t>(p+0x21C);if(owner<0x10000||owner==house)return false;uint32_t country=get<uint32_t>(owner+0x34);if(country<0x10000||get<bool>(country+0x1A6))return false;
 int idx=get<int>(owner+0x30);return idx>=0&&idx<32&&!(get<uint32_t>(house+0x5788)&(1u<<idx));
}
static bool unloadBuildingCandidate(uint32_t p,uint32_t house,bool engineer){
 return nativeEnemy(p,house); // User scope: hostile players, including engineers.
}
static int smartUnload(uint32_t house){
 // Smart unload only creates native EventClass missions. The input-hook
 // installation state is unrelated and must never prevent a manual hotkey
 // from starting an otherwise valid unload in a restricted launcher.
 if(!(sh->flags&SmartUnloadOrders)||!loadInterfaces())return -12;
 Vec selected=vec(0xA8ECB8),inf=vec(0xA83DE8);int sent=0;
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);if(!p||!own(p,house)||kind(p)!=2||!featureObjectAllowed(featuretypes::UnloadTransport,p)||get<int>(p+0x114)<=0||get<bool>(p+0x1C8))continue;
  uint32_t roster[nativeorders::MaxPassengers];int count=0;
  for(int j=0;j<inf.count&&count<nativeorders::MaxPassengers;++j){uint32_t q=item(inf,j);if(q&&kind(q)==1&&featureObjectAllowed(featuretypes::UnloadPassenger,q)&&get<uint32_t>(q+0x21C)==house&&get<uint32_t>(q+0x11C)==p)roster[count++]=q;}
  int replace=0;for(int j=0;j<unloadCount;++j)if(unloadPassengers[j].p==p||unloadPassengers[j].carrier==p)++replace;
  if(count>nativeorders::MaxPassengers-unloadCount+replace)return sent?sent:-6;
  if(!nativeMission(house,get<uint32_t>(p+0x10),16,0,0,0,0,0,taskarbiter::Owner::Unload))return sent?sent:-6;
  cancelNative(p);++sent;
  for(int j=0;j<count;++j){uint32_t q=roster[j];auto&r=unloadPassengers[unloadCount++];r={};r.p=q;r.id=get<uint32_t>(q+0x10);r.carrier=p;r.carrierID=get<uint32_t>(p+0x10);r.carrierType=typeOf(p,2);r.frame=get<int>(0xA8ED84);}
 }return sent;
}
static void watchEjections(uint32_t house,int frame,Vec all){
 snap.ejectWatched=0;if(!(sh->flags&EjectAssist))return;
 watchedPassengers.clear();for(int i=0;i<unloadCount;++i)watchedPassengers.add(unloadPassengers[i].p,unloadPassengers[i].id);
 Vec inf=vec(0xA83DE8);
 for(int j=0;j<inf.count;++j){uint32_t p=item(inf,j);if(!p||get<uint32_t>(p+0x21C)!=house||!featureObjectAllowed(featuretypes::EscapePassenger,p))continue;uint32_t carrier=get<uint32_t>(p+0x11C);
  if(!carrier||!nativeMember(all,carrier)||!own(carrier,house)||kind(carrier)!=2||!get<bool>(carrier+0x83)||!featureObjectAllowed(featuretypes::EscapeTransport,carrier))continue;
  ++snap.ejectWatched;
  if(!watchedPassengers.contains(p)&&unloadCount<nativeorders::MaxPassengers){auto&r=unloadPassengers[unloadCount++];r={};r.p=p;r.id=get<uint32_t>(p+0x10);r.carrier=carrier;r.carrierID=get<uint32_t>(carrier+0x10);r.carrierType=typeOf(carrier,2);r.frame=frame;r.mode=1;watchedPassengers.add(p,r.id);}
 }
}
struct UnloadTarget {uint32_t p,id;int x,y;};
static UnloadTarget unloadTargets[planner::MaxActors];static int unloadTargetsCount,unloadTargetsFrame=-1;static uint32_t unloadTargetsMatch;
static uint32_t unloadAttackTarget(uint32_t p,uint32_t type,uint32_t house,int frame){
 if(unloadTargetsFrame!=frame||unloadTargetsMatch!=match){
  unloadTargetsFrame=frame;unloadTargetsMatch=match;unloadTargetsCount=0;Vec all=vec(0xA8EC78);
  for(int i=0;i<all.count&&unloadTargetsCount<planner::MaxActors;++i){uint32_t q=item(all,i);if(!q||!nativeEnemy(q,house)||get<uint32_t>(q+0x11C)||get<int>(q+0x220))continue;
   unloadTargets[unloadTargetsCount++]={q,get<uint32_t>(q+0x10),get<int>(q+0x9C),get<int>(q+0xA0)};
  }
 }
 planner::Actor source{};rawCombatInfo(p,type,1,source,true);uint32_t best=0;int64_t bestDistance=INT64_MAX;
 int x=get<int>(p+0x9C),y=get<int>(p+0xA0);
 for(int i=0;i<unloadTargetsCount;++i){const auto&q=unloadTargets[i];int64_t dx=int64_t(x)-q.x,dy=int64_t(y)-q.y;
  if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)continue;int64_t distance=dx*dx+dy*dy;
  if(distance>bestDistance||(distance==bestDistance&&best&&q.id>=best))continue;
  if(!nativeEnemy(q.p,house)||get<uint32_t>(q.p+0x10)!=q.id)continue;int k=kind(q.p);uint32_t t=typeOf(q.p,k);if(!t)continue;
  planner::Actor target{};rawCombatInfo(q.p,t,k,target,false);if(!planner::compatibleTarget(source,target,false))continue;
  best=q.id;bestDistance=distance;
 }return best;
}
static void smartUnloadTick(uint32_t house,int frame,int&budget){
 Vec buildings=vec(0xA8EB40),all=vec(0xA8EC78),inf=vec(0xA83DE8);watchEjections(house,frame,all);int todo=unloadCount;if(todo>nativeorders::WorkPerFrame)todo=nativeorders::WorkPerFrame;
 while(todo--&&unloadCount){if(unloadCursor>=unloadCount)unloadCursor=0;int i=unloadCursor++;auto&r=unloadPassengers[i];uint32_t p=r.p;
  bool member=nativeMember(inf,p),carrierMember=nativeMember(all,r.carrier);
  bool carrierValid=carrierMember&&get<uint32_t>(r.carrier+0x10)==r.carrierID;
  bool carrierAlive=carrierValid&&own(r.carrier,house);
  uint32_t passengerType=member&&kind(p)==1?typeOf(p,1):0;
  if(member&&(!featureTypeAllowed(r.mode?featuretypes::EscapePassenger:featuretypes::UnloadPassenger,passengerType)||!featureTypeAllowed(r.mode?featuretypes::EscapeTransport:featuretypes::UnloadTransport,r.carrierType)))r.cancelled=true;
  bool ejectDeployer=r.mode!=0&&passengerType>=0x10000&&get<bool>(passengerType+0xEC8);
  bool done=!member||get<uint32_t>(p+0x10)!=r.id||kind(p)!=1||get<uint32_t>(p+0x21C)!=house||get<int>(p+0x6C)<=0;
  if(!done){uint32_t carrier=get<uint32_t>(p+0x11C);
   if(carrier&&(carrier!=r.carrier||!carrierValid||r.stage!=0||r.landedFrame))done=true;
   if(!carrier&&live(p)){if(!r.landedFrame)r.landedFrame=frame;
    int m=get<int>(p+0xAC);if(r.stage==0&&!unloadpolicy::initialMayContinue(m,r.mode,frame-r.landedFrame,m==7&&get<uint32_t>(p+0x5A4)==r.carrier,ejectDeployer))done=true;
   }
  }
  if(!done&&(r.stage==1||r.stage==2)&&live(p)&&!get<uint32_t>(p+0x11C)){
   int mission=get<int>(p+0xAC);
   if(r.stage==1&&r.sleepObserved&&(mission==13||mission==7))done=true;
   else if(mission!=0&&mission!=5&&mission!=11&&!(ejectDeployer&&mission==2&&frame-r.landedFrame<=unloadpolicy::ScatterFrames)&&!(r.stage==2&&mission==16)&&!(mission==7&&get<uint32_t>(p+0x5A4)==r.carrier))done=true;
   else if(r.stage==1&&mission==0)r.sleepObserved=true;
  }
  bool release=!done&&(r.cancelled||(r.mode==0&&!(sh->flags&SmartUnloadOrders))||(r.mode!=0&&!(sh->flags&EjectAssist))||(r.mode!=1&&frame-r.frame>900));
  if(!done&&!release&&r.stage==3&&live(p)&&!get<uint32_t>(p+0x11C)){
   uint32_t target=worldByID(r.followTarget);if(target!=r.followTargetPointer)target=0;
   int mission=get<int>(p+0xAC);
   if(target&&(get<uint32_t>(p+0x2B4)==target||get<uint32_t>(p+0x5A4)==target)){
    r.followObserved=true;done=true; // acquired destination; capture/kill is NOT proven
   }
   else if(mission!=0&&mission!=5&&!(mission==7&&get<uint32_t>(p+0x5A4)==r.carrier))done=true;
   else if(frame-r.sentFrame<30)continue;
   else if(++r.retries>=3){
    if(mission==0){if(budget<=0||!nativeMission(house,r.id,5,0,0,0,0,0,taskarbiter::Owner::Unload))continue;--budget;}
    done=true;
   }
   else {r.stage=0;r.sentFrame=0;}
  }
  if(!done&&r.mode==1){
   if(!(sh->flags&EjectAssist))done=true;
   else if(carrierAlive){if(!get<bool>(r.carrier+0x83)||get<uint32_t>(p+0x11C)!=r.carrier)done=true;}
   else if(carrierValid&&get<uint32_t>(r.carrier+0x21C)!=house)done=true;
   else {r.mode=2;r.frame=frame;}
  }
  release=!done&&(release||((r.mode!=0&&!(sh->flags&EjectAssist))||(r.mode!=1&&frame-r.frame>900)));
  if(release){
   // Never leave an undeployed passenger asleep after timeout / switch-off.
   if((r.stage==1||r.stage==2||(r.stage==3&&get<int>(p+0xAC)==0))&&live(p)&&!get<uint32_t>(p+0x11C)&&!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x554),p)){
    if(budget>0&&nativeMission(house,r.id,5,0,0,0,0,0,taskarbiter::Owner::Unload)){--budget;done=true;}else continue;
   }else done=true;
  }
  if(!done&&r.mode!=1&&live(p)&&!get<uint32_t>(p+0x11C)&&budget>0&&outgoingRoom()>0){uint32_t t=typeOf(p,1);int currentMission=get<int>(p+0xAC);
   if(!r.landedFrame)r.landedFrame=frame;
   bool takenOver=!unloadpolicy::initialMayContinue(currentMission,r.mode,frame-r.landedFrame,currentMission==7&&get<uint32_t>(p+0x5A4)==r.carrier,ejectDeployer);
   if(!t){done=true;}else if(!r.stage&&takenOver){done=true;}else if(get<bool>(t+0xEC8)){
    if(call<bool>(get<uint32_t>(get<uint32_t>(p)+0x554),p))done=true;
    else {
     int x=get<int>(p+0x9C),y=get<int>(p+0xA0),mission=get<int>(p+0xAC);
     if(x!=r.x||y!=r.y){r.x=x;r.y=y;r.stableSince=frame;}
     if(!r.stage){
      // Each survivor independently uses the original deployment predicate.
      // Do not wait for another passenger or impose a fixed settling delay.
      // If not ready, preserve native scatter instead of issuing Sleep.
      bool ready=call<bool>(get<uint32_t>(get<uint32_t>(p)+0x314),p);
      if(ejectDeployer&&!ready)continue;
      if(nativeMission(house,r.id,ready?16:0,0,0,0,0,0,taskarbiter::Owner::Unload)){--budget;r.stage=ready?2:1;r.sentFrame=frame;r.stableSince=frame;r.sleepObserved=false;r.seenUnload=0;if(ready)++snap.unloadIssued;}
     }else if(r.stage==1&&(mission==0||mission==5||mission==11)&&frame-r.sentFrame>=2&&frame-r.stableSince>=2&&call<bool>(get<uint32_t>(get<uint32_t>(p)+0x314),p)){
      if(nativeMission(house,r.id,16,0,0,0,0,0,taskarbiter::Owner::Unload)){--budget;r.stage=2;r.sentFrame=frame;r.seenUnload=0;++snap.unloadIssued;}
     }else if(r.stage==2){
      if(mission==16)r.seenUnload=1;
      // Do not blindly repeat a toggling Deploy while a previous event may
      // still be in flight. Retry only after observed execution and failure.
      if(r.seenUnload&&mission!=16&&frame-r.sentFrame>=12){
       if(ejectDeployer&&mission==2)continue;
       if(mission!=0&&mission!=5)done=true;
       else if(++r.retries<3)r.stage=0;
       else if(mission!=0)done=true;
       else if(nativeMission(house,r.id,5,0,0,0,0,0,taskarbiter::Owner::Unload)){--budget;done=true;}
      }
     }
    }
   }else{
    if(frame<r.sentFrame)continue;
    bool engineer=get<bool>(t+0xEC3),spy=get<bool>(t+0xEBE),c4=get<bool>(t+0xEC2);uint32_t best=0,bestID=~0u;int mission=-1;int64_t distance=INT64_MAX;
    if(engineer||spy||c4)for(int j=0;j<buildings.count;++j){uint32_t q=item(buildings,j);if(!q||kind(q)!=3||!unloadBuildingCandidate(q,house,engineer))continue;
     int action=call<int>(0x51E3B0,p,q,true),m=nativeorders::specialMission(engineer,spy,c4,action);if(m<0)continue;
     int64_t dx=int64_t(get<int>(p+0x9C))-get<int>(q+0x9C),dy=int64_t(get<int>(p+0xA0))-get<int>(q+0xA0);if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)continue;int64_t d=dx*dx+dy*dy;uint32_t id=get<uint32_t>(q+0x10);
     if(d<distance||(d==distance&&id<bestID)){best=q;bestID=id;distance=d;mission=m;}
    }
    if(!best&&!engineer&&!spy){bestID=unloadAttackTarget(p,t,house,frame);best=bestID;mission=1;}
    uint32_t ticket=0;
    if(best&&nativeMission(house,r.id,mission,mission==1?bestID:0,mission==1?0:bestID,mission==1?0:0x34,0,0,taskarbiter::Owner::Unload,&ticket)){
     --budget;++snap.unloadIssued;
     if(engineer||spy){
      finishUnloadFollowup(r,frame); // supersede old retries, never leave stale receipt
      r.stage=3;r.sentFrame=frame;r.followTarget=bestID;r.followTargetPointer=worldByID(bestID);r.followTicket=ticket;
      r.followQueuedAt=sampleClock(sh);r.followObserved=false;
      // Business watcher owns this Ticket rather than the generic observer.
      missionObserver.dismissTicket(match,ticket);
     }else done=true;
    }
    // Just-disembarked infantry may not yet pass native click validation.
    // Retry specialists briefly, at most once per six frames, then release.
    if(!best&&(engineer||spy)&&frame-r.landedFrame<90){r.sentFrame=frame+6;continue;}
    // No compatible target: restore native Guard if necessary.
    if(!best){if(get<int>(p+0xAC)!=0)done=true;else if(nativeMission(house,r.id,5,0,0,0,0,0,taskarbiter::Owner::Unload)){--budget;done=true;}}
   }
  }
  if(done){finishUnloadFollowup(unloadPassengers[i],frame);unloadPassengers[i]=unloadPassengers[--unloadCount];unloadCursor=i;}
 }
}
static void nativeOrdersTick(uint32_t house,int frame){
 if(frame==nativeFrame)return;nativeFrame=frame;
 if(!(sh->flags&NativeOrders)&&!unloadCount){if(nativeCount)advancedObjects.clear();nativeCount=0;snap.advancedUnits=snap.unloadPending=0;return;}
 // Input interception is for advanced click / CS capture ONLY.  An input
 // hook error must not cancel an independently observed disembark/ejection:
 // those workers issue vanilla EventClass orders and need no code patch.
 if(snap.nativeHookStatus!=1&&nativeCount){nativeCount=0;advancedObjects.clear();}
 for(int i=0;i<nativeCount;)if(!worldObjects.contains(nativeOrders[i].p)||get<uint32_t>(nativeOrders[i].p+0x10)!=nativeOrders[i].id||!(sh->flags&(nativeOrders[i].mode==planner::MagnetronMove?MagnetronInput:AdvancedInput))||!(nativeOrders[i].mode==planner::MagnetronMove?rawMagnetron(nativeOrders[i].p):featureObjectAllowed(featuretypes::AdvancedAttack,nativeOrders[i].p)))removeOrder(i);else ++i;
 int budget=recurringBudget(EventLane::Unload);if(unloadCount||(sh->flags&EjectAssist))smartUnloadTick(house,frame,budget);
 // Advanced tactical decisions are made in the EXE's external planner.
 // This DLL retains only local click intents and checks whether IDs remain live.
 for(int i=0;i<nativeCount;){auto&o=nativeOrders[i];if(!worldObjects.contains(o.p)||!own(o.p,house)||get<uint32_t>(o.p+0x10)!=o.id||get<uint32_t>(o.p+0x11C))removeOrder(i);else ++i;}
 if(nativeCount||snap.advancedUnits){advancedObjects.clear();for(int i=0;i<nativeCount;++i)advancedObjects.add(nativeOrders[i].p,nativeOrders[i].id,uint32_t(i+1));}
 snap.advancedUnits=nativeCount;snap.unloadPending=0;for(int i=0;i<unloadCount;++i)if(unloadPassengers[i].mode!=1)++snap.unloadPending;
}
