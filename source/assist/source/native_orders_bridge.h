#include "native_orders.h"
#include "unload_policy.h"
// All simulation commands go through EventClass constructors and the game's
// outgoing queue. This file NEVER writes a unit's mission, target, or locomotor.
struct NativeOrder {uint32_t p,id,cell,nearest,token,mode;int gx,gy;nativeorders::Progress progress;uint32_t target;int sentFrame;bool attacking,stopped;};
struct UnloadPassenger : unloadcompute::State {uint32_t p,id,carrier,carrierID,carrierType,followTarget,followTargetPointer,job;int frame,mode,landedFrame;uint32_t followTicket,followQueuedAt,manualToken;bool cancelled,followObserved;};
static uint32_t unloadJobSerial;
static void resetUnloadCalculation();
static NativeOrder nativeOrders[nativeorders::MaxOrders];static int nativeCount,nativeCursor,nativeFrame=-1;
static UnloadPassenger unloadPassengers[nativeorders::MaxPassengers];static int unloadCount,unloadCursor;
// Read-only roster history bridges a native D that executes before Ctrl+D's
// WM_HOTKEY reaches the host. It never starts a job without an explicit command.
struct RecentPassenger {uint32_t p,id,carrier,carrierID,carrierType,token,tick;int frame;};
static RecentPassenger recentPassengers[nativeorders::MaxPassengers];static int recentPassengerCount;
static ObjectIndex watchedPassengers;
static void restoreNativeHooks();
static void clearNativeOrders(){nativeCount=nativeCursor=unloadCount=unloadCursor=recentPassengerCount=0;nativeFrame=-1;advancedObjects.clear();snap.advancedUnits=snap.unloadPending=0;restoreNativeHooks();resetUnloadCalculation();}
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
static void cancelNative(uint32_t p){cancelBurst(get<uint32_t>(p+0x10));cancelDodge(get<uint32_t>(p+0x10));cancelOreJob(p);cancelLoadingJobs(p);int i=orderIndex(p);if(i>=0)removeOrder(i);for(i=0;i<unloadCount;)
 if(unloadPassengers[i].p==p||unloadPassengers[i].carrier==p){finishUnloadFollowup(unloadPassengers[i],get<int>(0xA8ED84));unloadPassengers[i]=unloadPassengers[--unloadCount];}
 else ++i;}
static void cancelAutomatic(uint32_t p){cancelBurst(get<uint32_t>(p+0x10));cancelDodge(get<uint32_t>(p+0x10));
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
 if(!manualVersions.trackable(get<uint32_t>(p+0x10))||(!(bindings&pointerinput::Advanced)||!(sh->flags&AdvancedInput))&&!mag||(mag?!rawMagnetron(p):!featureObjectAllowed(featuretypes::AdvancedAttack,p))||(k!=1&&k!=2)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||target||
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
 if(!engineer)return nativeEnemy(p,house);
 if(!live(p)||kind(p)!=3)return false;uint32_t owner=get<uint32_t>(p+0x21C);if(owner<0x10000||owner==house)return false;
 int index=get<int>(owner+0x30);return index>=0&&index<32&&!(get<uint32_t>(house+0x5788)&(1u<<index));
 // Neutral capturable tech structures are legal too; MouseOverObject remains
 // authoritative. Do not attempt friendly repair or bypass Capturable.
}
static bool unloadExit(const UnloadPassenger&r,int mission,int frame){
 if(mission!=7)return false;uint32_t destination=get<uint32_t>(r.p+0x5A4);
 return destination==r.carrier||(r.mode==0&&!destination&&!get<uint32_t>(r.p+0x2B4)&&frame-r.landedFrame<=unloadpolicy::ScatterFrames);
}
static void observeUnloadRoster(uint32_t house,int frame,DWORD now){
 if(!(sh->flags&SmartUnloadOrders)||snap.chat){recentPassengerCount=0;return;}
 for(int n=0;n<recentPassengerCount;){auto&r=recentPassengers[n];
  if(DWORD(now-r.tick)>1500||frame<r.frame||frame-r.frame>90||!worldObjects.contains(r.p)||
     get<uint32_t>(r.p+0x10)!=r.id||manualVersions.get(r.id)!=r.token)
   r=recentPassengers[--recentPassengerCount];else ++n;
 }
 Vec inf=vec(0xA83DE8);
 for(int n=0;n<inf.count;++n){uint32_t p=item(inf,n);if(!p)continue;uint32_t carrier=get<uint32_t>(p+0x11C);
  if(!carrier||!worldObjects.contains(carrier)||!own(carrier,house)||kind(carrier)!=2||!get<bool>(carrier+0x83)||
     !featureObjectAllowed(featuretypes::UnloadTransport,carrier)||get<uint32_t>(p+0x21C)!=house||
     !featureObjectAllowed(featuretypes::UnloadPassenger,p))continue;
  int at=0;for(;at<recentPassengerCount;++at)if(recentPassengers[at].p==p)break;
  if(at==recentPassengerCount){if(at==nativeorders::MaxPassengers)continue;++recentPassengerCount;}
  recentPassengers[at]={p,get<uint32_t>(p+0x10),carrier,get<uint32_t>(carrier+0x10),typeOf(carrier,2),manualVersions.get(get<uint32_t>(p+0x10)),now,frame};
 }
}
static int smartUnload(uint32_t house){
 // Smart unload only creates native EventClass missions. The input-hook
 // installation state is unrelated and must never prevent a manual hotkey
 // from starting an otherwise valid unload in a restricted launcher.
 if(!(sh->flags&SmartUnloadOrders)||!loadInterfaces())return -12;
 Vec selected=vec(0xA8ECB8),inf=vec(0xA83DE8);int sent=0;
 const int frame=get<int>(0xA8ED84);const DWORD now=sampleClock(sh);
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);if(!p||!own(p,house)||kind(p)!=2||!featureObjectAllowed(featuretypes::UnloadTransport,p)||get<bool>(p+0x1C8))continue;
  uint32_t roster[nativeorders::MaxPassengers];int count=0;
  for(int j=0;j<inf.count&&count<nativeorders::MaxPassengers;++j){uint32_t q=item(inf,j);if(q&&kind(q)==1&&featureObjectAllowed(featuretypes::UnloadPassenger,q)&&get<uint32_t>(q+0x21C)==house&&get<uint32_t>(q+0x11C)==p)roster[count++]=q;}
  for(int j=0;j<recentPassengerCount&&count<nativeorders::MaxPassengers;++j){const auto&old=recentPassengers[j];uint32_t q=old.p;
   if(old.carrier!=p||old.carrierID!=get<uint32_t>(p+0x10)||old.carrierType!=typeOf(p,2)||
      DWORD(now-old.tick)>1500||frame<old.frame||frame-old.frame>90||!worldObjects.contains(q)||
      get<uint32_t>(q+0x10)!=old.id||!own(q,house)||get<uint32_t>(q+0x11C)||manualVersions.get(old.id)!=old.token||
      !featureObjectAllowed(featuretypes::UnloadPassenger,q))continue;
   bool found=false;for(int k=0;k<count;++k)found|=roster[k]==q;
   if(!found)roster[count++]=q;
  }
  const bool loaded=get<int>(p+0x114)>0;if(!loaded&&!count)continue;
  int replace=0;for(int j=0;j<unloadCount;++j)if(unloadPassengers[j].p==p||unloadPassengers[j].carrier==p)++replace;
  if(count>nativeorders::MaxPassengers-unloadCount+replace)return sent?sent:-6;
  // Do not issue a second Deploy to an empty vehicle or an active unload.
  if(loaded&&get<int>(p+0xAC)!=16&&get<int>(p+0xB4)!=16&&
     !nativeMission(house,get<uint32_t>(p+0x10),16,0,0,0,0,0,taskarbiter::Owner::Unload))return sent?sent:-6;
  cancelNative(p);++sent;
  for(int j=0;j<count;++j){uint32_t q=roster[j];auto&r=unloadPassengers[unloadCount++];r={};r.job=++unloadJobSerial;r.p=q;r.id=get<uint32_t>(q+0x10);r.carrier=p;r.carrierID=get<uint32_t>(p+0x10);r.carrierType=typeOf(p,2);r.frame=get<int>(0xA8ED84);r.manualToken=manualVersions.get(r.id);}
 }return sent;
}
static void watchEjections(uint32_t house,int frame,Vec all){
 snap.ejectWatched=0;if(!(sh->flags&EjectAssist))return;
 watchedPassengers.clear();for(int i=0;i<unloadCount;++i)watchedPassengers.add(unloadPassengers[i].p,unloadPassengers[i].id);
 Vec inf=vec(0xA83DE8);
 for(int j=0;j<inf.count;++j){uint32_t p=item(inf,j);if(!p||get<uint32_t>(p+0x21C)!=house||!featureObjectAllowed(featuretypes::EscapePassenger,p))continue;uint32_t carrier=get<uint32_t>(p+0x11C);
  if(!carrier||!nativeMember(all,carrier)||!own(carrier,house)||kind(carrier)!=2||!get<bool>(carrier+0x83)||!featureObjectAllowed(featuretypes::EscapeTransport,carrier))continue;
  ++snap.ejectWatched;
  if(!watchedPassengers.contains(p)&&unloadCount<nativeorders::MaxPassengers){auto&r=unloadPassengers[unloadCount++];r={};r.job=++unloadJobSerial;r.p=p;r.id=get<uint32_t>(p+0x10);r.carrier=carrier;r.carrierID=get<uint32_t>(carrier+0x10);r.carrierType=typeOf(carrier,2);r.frame=frame;r.mode=1;r.manualToken=manualVersions.get(r.id);watchedPassengers.add(p,r.id);}
 }
}
#include "unload_compute_bridge.h"
static void smartUnloadTick(uint32_t house,int frame,int&budget){
 serviceUnloadCalculation(house,frame,budget);
 if(!unloadCalculationPending){unloadExternalInput.count=0;unloadExternalInput.targetCount=0;unloadExternalInput.match=match;unloadExternalInput.tick=sampleClock(sh);unloadExternalInput.frame=frame;}
 Vec all=vec(0xA8EC78),inf=vec(0xA83DE8);watchEjections(house,frame,all);int todo=unloadCount;if(todo>nativeorders::WorkPerFrame)todo=nativeorders::WorkPerFrame;
 while(todo--&&unloadCount){if(unloadCursor>=unloadCount)unloadCursor=0;int i=unloadCursor++;auto&r=unloadPassengers[i];uint32_t p=r.p;
  bool member=nativeMember(inf,p),carrierMember=nativeMember(all,r.carrier);
  bool carrierValid=carrierMember&&get<uint32_t>(r.carrier+0x10)==r.carrierID;
  bool carrierAlive=carrierValid&&own(r.carrier,house);
  uint32_t passengerType=member&&kind(p)==1?typeOf(p,1):0;
  if(member&&(!featureTypeAllowed(r.mode?featuretypes::EscapePassenger:featuretypes::UnloadPassenger,passengerType)||!featureTypeAllowed(r.mode?featuretypes::EscapeTransport:featuretypes::UnloadTransport,r.carrierType)))r.cancelled=true;
  bool ejectDeployer=r.mode!=1&&passengerType>=0x10000&&get<bool>(passengerType+0xEC8);
  bool done=manualVersions.get(r.id)!=r.manualToken||!member||get<uint32_t>(p+0x10)!=r.id||kind(p)!=1||get<uint32_t>(p+0x21C)!=house||get<int>(p+0x6C)<=0;
  if(!done){uint32_t carrier=get<uint32_t>(p+0x11C);
   if(carrier&&(carrier!=r.carrier||!carrierValid||r.stage!=0||r.landedFrame))done=true;
   if(!carrier&&live(p)){if(!r.landedFrame)r.landedFrame=frame;
    int m=get<int>(p+0xAC);if(r.stage==0&&!unloadpolicy::initialMayContinue(m,r.mode,frame-r.landedFrame,unloadExit(r,m,frame),ejectDeployer))done=true;
   }
  }
  if(!done&&(r.stage==1||r.stage==2)&&live(p)&&!get<uint32_t>(p+0x11C)){
   int mission=get<int>(p+0xAC);
   if(r.stage==1&&r.sleepObserved&&(mission==13||mission==7))done=true;
   else if(mission!=0&&mission!=5&&mission!=6&&mission!=11&&!(ejectDeployer&&mission==2&&frame-r.landedFrame<=unloadpolicy::ScatterFrames)&&!(r.stage==2&&mission==16)&&!(mission==7&&get<uint32_t>(p+0x5A4)==r.carrier))done=true;
   else if(r.stage==1&&mission==0)r.sleepObserved=true;
  }
  bool release=!done&&(r.cancelled||(r.mode==0&&!(sh->flags&SmartUnloadOrders))||(r.mode!=0&&!(sh->flags&EjectAssist))||(r.mode!=1&&frame-r.frame>900));
  if(!done&&!release&&r.stage==3&&live(p)&&!get<uint32_t>(p+0x11C)){
   uint32_t target=worldByID(r.followTarget);if(target!=r.followTargetPointer)target=0;
   int mission=get<int>(p+0xAC);
   const bool linked=target&&(get<uint32_t>(p+0x2B4)==target||get<uint32_t>(p+0x5A4)==target);
   const int x=get<int>(p+0x9C),y=get<int>(p+0xA0);
   if(x!=r.x||y!=r.y){r.x=x;r.y=y;r.stableSince=frame;}
   if(target&&get<uint32_t>(target+0x21C)==house){r.followObserved=true;done=true;}
   else if(mission==13||get<int>(p+0xB4)==13)done=true;
   else if(linked&&(mission==8||mission==7||mission==2)&&frame-r.stableSince<120){r.followObserved=true;continue;}
   else if(mission!=0&&mission!=5&&mission!=6&&mission!=11&&mission!=2&&mission!=7&&mission!=8)done=true;
   else if(frame-r.sentFrame<30||frame-r.stableSince<2)continue;
   else if(++r.retries>=6){
    if(mission==0){if(budget<=0||!nativeMission(house,r.id,5,0,0,0,0,0,taskarbiter::Owner::Unload))continue;--budget;}
    done=true;
   }
   else {finishUnloadFollowup(r,frame);r.followTicket=0;r.followObserved=false;r.stage=0;r.sentFrame=0;}
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
   if((r.stage==1||r.stage==2||r.stage==3)&&get<int>(p+0xAC)==0&&live(p)&&!get<uint32_t>(p+0x11C)&&!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x554),p)){
    if(budget>0&&nativeMission(house,r.id,5,0,0,0,0,0,taskarbiter::Owner::Unload)){--budget;done=true;}else continue;
   }else done=true;
  }
  if(!done&&r.mode!=1&&live(p)&&!get<uint32_t>(p+0x11C)){uint32_t t=typeOf(p,1);int currentMission=get<int>(p+0xAC);
   // Normal unloading also scatters infantry away from the exit. Keep this
   // explicit unload job while they walk out; never append Sleep/Deploy to
   // their temporary Move. Manual orders cancel via the shared input manager.
   if(r.mode==0&&currentMission==7&&unloadExit(r,currentMission,frame))continue;
   const bool earlySpecialist=!ejectDeployer&&(get<bool>(t+0xEC3)||get<bool>(t+0xEBE));
   if(r.mode==0&&!earlySpecialist&&(currentMission==2||get<int>(p+0xB4)==2)&&frame-r.frame<=900){r.x=get<int>(p+0x9C);r.y=get<int>(p+0xA0);r.stableSince=frame;continue;}
   if(!r.landedFrame)r.landedFrame=frame;
   if(!t||(!r.stage&&!unloadpolicy::initialMayContinue(currentMission,r.mode,frame-r.landedFrame,unloadExit(r,currentMission,frame),ejectDeployer)))done=true;
   else if(compute::inside(sh->computeApplied,r.mode==0?compute::Unload:compute::Escape)){
    auto&input=unloadLocalInput;input.count=0;input.targetCount=0;input.match=match;input.tick=sampleClock(sh);input.frame=frame;
    collectUnloadSource(input,r,house,frame);
    auto decision=unloadcompute::decide(input.sources[0],input.targets,input.actions[0],input.targetCount);
    done=applyUnloadDecision(r,input,0,decision,house,frame,budget,false);
   }else if(!unloadCalculationPending&&unloadExternalInput.count<unloadcompute::MaxSources){
    collectUnloadSource(unloadExternalInput,r,house,frame);
   }
  }
  if(done){finishUnloadFollowup(unloadPassengers[i],frame);unloadPassengers[i]=unloadPassengers[--unloadCount];unloadCursor=i;}
 }
 publishUnloadCalculation();
}
static void nativeOrdersTick(uint32_t house,int frame){
 if(frame==nativeFrame)return;nativeFrame=frame;
 if(!(sh->flags&NativeOrders)&&!unloadCount){if(nativeCount)advancedObjects.clear();nativeCount=0;snap.advancedUnits=snap.unloadPending=0;return;}
 // Input interception is for advanced click / CS capture ONLY.  An input
 // hook error must not cancel an independently observed disembark/ejection:
 // those workers issue vanilla EventClass orders and need no code patch.
 // Interface readiness may recover next tick; retain explicit intents.
 for(int i=0;i<nativeCount;)if(!worldObjects.contains(nativeOrders[i].p)||get<uint32_t>(nativeOrders[i].p+0x10)!=nativeOrders[i].id||!(sh->flags&(nativeOrders[i].mode==planner::MagnetronMove?MagnetronInput:AdvancedInput))||!(nativeOrders[i].mode==planner::MagnetronMove?rawMagnetron(nativeOrders[i].p):featureObjectAllowed(featuretypes::AdvancedAttack,nativeOrders[i].p)))removeOrder(i);else ++i;
 int budget=recurringBudget(EventLane::Unload);if(unloadCount||(sh->flags&EjectAssist))smartUnloadTick(house,frame,budget);
 // Advanced decisions use the selected shared Planner backend.
 // This state stores click intents and validates actor identities in either mode.
 for(int i=0;i<nativeCount;){auto&o=nativeOrders[i];if(!worldObjects.contains(o.p)||!own(o.p,house)||get<uint32_t>(o.p+0x10)!=o.id)removeOrder(i);else ++i;}
 if(nativeCount||snap.advancedUnits){advancedObjects.clear();for(int i=0;i<nativeCount;++i)advancedObjects.add(nativeOrders[i].p,nativeOrders[i].id,uint32_t(i+1));}
 snap.advancedUnits=nativeCount;snap.unloadPending=0;for(int i=0;i<unloadCount;++i)if(unloadPassengers[i].mode!=1)++snap.unloadPending;
}

