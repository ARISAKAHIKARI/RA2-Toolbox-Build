#include "native_orders.h"
#include "unload_policy.h"
#include "unload_admission.h"
static unloadadmission::Queue queuedUnload;
// All simulation commands go through EventClass constructors and the game's
// outgoing queue. This file NEVER writes a unit's mission, target, or locomotor.
struct NativeOrder {uint32_t p,id,cell,nearest,token,mode;int gx,gy;nativeorders::Progress progress;uint32_t target;int sentFrame;bool attacking,stopped,global;};
struct UnloadPassenger : unloadcompute::State {uint32_t p,id,carrier,carrierID,carrierType,followTarget,followTargetPointer,job;int frame,mode,landedFrame;uint32_t followTicket,followQueuedAt,manualToken;bool cancelled,followObserved;};
static uint32_t unloadJobSerial;
static void resetUnloadCalculation();
static NativeOrder nativeOrders[nativeorders::MaxOrders];static int nativeCount,nativeCursor,nativeFrame=-1;
// A manual click can precede its native event by several simulation frames.
// Cancellation alone is insufficient: the still-idle unit would immediately
// re-enrol with the NEW manual token and then overwrite that arriving Attack.
struct GlobalMagnetronPause {uint32_t pointer,id;int frame;uint32_t victim;};
static GlobalMagnetronPause globalMagnetronPauses[nativeorders::MaxOrders];static int globalMagnetronPauseCount=0,globalMagnetronPauseOverflow=-1;
static ObjectIndex globalMagnetronPauseIndex;
static void pauseGlobalMagnetron(uint32_t p){
 if(!sh||!sh->magnetronGlobal||!rawMagnetron(p))return;
 int frame=get<int>(0xA8ED84);uint32_t id=get<uint32_t>(p+0x10),victim=get<uint32_t>(p+0x2AC);
 for(int i=0;i<globalMagnetronPauseCount;++i)if(globalMagnetronPauses[i].id==id){globalMagnetronPauses[i]={p,id,frame,victim};globalMagnetronPauseIndex.add(p,id,uint32_t(i+1));return;}
 if(globalMagnetronPauseCount<nativeorders::MaxOrders){globalMagnetronPauses[globalMagnetronPauseCount++]={p,id,frame,victim};globalMagnetronPauseIndex.add(p,id,uint32_t(globalMagnetronPauseCount));}
 else globalMagnetronPauseOverflow=frame; // bounded capacity: defer enrolment, never evict another manual command
}
static void pruneMagnetronPauses(int frame){
 globalMagnetronPauseIndex.clear();
 for(int i=0;i<globalMagnetronPauseCount;){const auto&p=globalMagnetronPauses[i];
  if(!worldObjects.contains(p.pointer)||get<uint32_t>(p.pointer+0x10)!=p.id||frame<p.frame||frame-p.frame>=90)globalMagnetronPauses[i]=globalMagnetronPauses[--globalMagnetronPauseCount];
  else {globalMagnetronPauseIndex.add(p.pointer,p.id,uint32_t(i+1));++i;}
 }
 if(globalMagnetronPauseOverflow>=0&&(frame<globalMagnetronPauseOverflow||frame-globalMagnetronPauseOverflow>=90))globalMagnetronPauseOverflow=-1;
}
static UnloadPassenger unloadPassengers[nativeorders::MaxPassengers];static int unloadCount,unloadCursor;
// Read-only roster history bridges a native D that executes before Ctrl+D's
// WM_HOTKEY reaches the host. It never starts a job without an explicit command.
struct RecentPassenger {uint32_t p,id,carrier,carrierID,carrierType,token,tick;int frame;};
static RecentPassenger recentPassengers[nativeorders::MaxPassengers];static int recentPassengerCount;
static ObjectIndex watchedPassengers;
static void restoreNativeHooks();
static void clearNativeOrders(bool retireHooks){queuedUnload.clear();globalMagnetronPauseCount=0;globalMagnetronPauseOverflow=-1;globalMagnetronPauseIndex.clear();nativeCount=nativeCursor=unloadCount=unloadCursor=recentPassengerCount=0;nativeFrame=-1;advancedObjects.clear();snap.advancedUnits=snap.unloadPending=0;if(retireHooks)restoreNativeHooks();resetUnloadCalculation();}
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
static void cancelNative(uint32_t p,bool manual=true){
 // If the original Ctrl+D has already unloaded the transporter, its
 // passenger roster may still await the next admission tick. Retain that
 // pending handoff while moving this now-empty transport. An interruption
 // with passengers still aboard cancels only the unstarted unload.
 const uint32_t carrierID=get<uint32_t>(p+0x10);
 if(queuedUnload.find(p,carrierID)<0||get<int>(p+0x114)>0)
  queuedUnload.cancel(p,carrierID);
 if(manual)releaseSticky(p);
 pauseGlobalMagnetron(p);
 cancelBurst(get<uint32_t>(p+0x10));cancelDodge(get<uint32_t>(p+0x10));cancelOreJob(p);cancelLoadingJobs(p);int i=orderIndex(p);if(i>=0)removeOrder(i);for(i=0;i<unloadCount;)
 // Transport commands must not cancel the passengers' independent post-ejection jobs.
 if(unloadPassengers[i].p==p){finishUnloadFollowup(unloadPassengers[i],get<int>(0xA8ED84));unloadPassengers[i]=unloadPassengers[--unloadCount];}
 else ++i;}
static void cancelAutomatic(uint32_t p){queuedUnload.cancel(p,get<uint32_t>(p+0x10));releaseSticky(p);cancelBurst(get<uint32_t>(p+0x10));cancelDodge(get<uint32_t>(p+0x10));
 cancelOreJob(p);cancelLoadingJobs(p);int i=orderIndex(p);if(i>=0)removeOrder(i);
 // Keep a release record until a native Guard can follow a tool-issued Sleep.
 // Manual click paths still use cancelNative because their own order takes over.
 for(i=0;i<unloadCount;++i)if(unloadPassengers[i].p==p)unloadPassengers[i].cancelled=true;
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
 if(indexedFrame!=frame)return; // incomplete identity sample: retain history until retry
 for(int n=0;n<recentPassengerCount;){auto&r=recentPassengers[n];
  if(DWORD(now-r.tick)>1500||frame<r.frame||frame-r.frame>90||!worldObjects.contains(r.p)||
     get<uint32_t>(r.p+0x10)!=r.id||manualVersions.get(r.id)!=r.token)
   r=recentPassengers[--recentPassengerCount];else ++n;
 }
 static unloadpolicy::RosterLookup<nativeorders::MaxPassengers> history;history.clear();
 for(int n=0;n<recentPassengerCount;++n)history.add(recentPassengers[n].p,n);
 Vec inf=vec(0xA83DE8);
 for(int n=0;n<inf.count;++n){uint32_t p=item(inf,n);if(!p)continue;uint32_t carrier=get<uint32_t>(p+0x11C);
  // Normal loaded passengers must be remembered too. 0x83 is an
  // exceptional vehicle state used by EjectAssist; requiring it here
  // left ordinary Ctrl+D with no pre-ejection passenger history.
  if(!carrier||!worldObjects.contains(carrier)||!own(carrier,house)||kind(carrier)!=2||
     !featureObjectAllowed(featuretypes::UnloadTransport,carrier)||get<uint32_t>(p+0x21C)!=house||
     !featureObjectAllowed(featuretypes::UnloadPassenger,p))continue;
  int at=history.find(p);
  if(at<0){if(recentPassengerCount==nativeorders::MaxPassengers)continue;at=recentPassengerCount++;history.add(p,at);}
  recentPassengers[at]={p,get<uint32_t>(p+0x10),carrier,get<uint32_t>(carrier+0x10),typeOf(carrier,2),manualVersions.get(get<uint32_t>(p+0x10)),now,frame};
 }
}
// Build one carrier-keyed roster per admission tick. A large selected cohort
// must not rescan every infantry object once for each selected transport.
// Loaded passengers are alive but in limbo/off-map; own() deliberately means
// a live on-map actor. Keep their identity/owner/life checks without applying
// that map-presence gate until they have actually left the carrier.
static bool unloadPassengerEligible(uint32_t p,uint32_t house){
 return p&&worldObjects.contains(p)&&kind(p)==1&&get<uint32_t>(p+0x21C)==house&&
  get<int>(p+0x6C)>0&&
  // Loaded infantry is often Limbo/inactive until the engine ejects each one.
  // Enrollment must not be restricted to the first currently active exit.
  (get<uint32_t>(p+0x11C)||get<bool>(p+0x74)&&live(p));
}
struct UnloadRosterEntry {uint32_t carrier,pointer,id,token;int history=-1;};
static UnloadRosterEntry unloadRoster[nativeorders::MaxPassengers*2];static int unloadRosterCount;
static void buildUnloadAdmissionRoster(uint32_t house){
 unloadRosterCount=0;Vec inf=vec(0xA83DE8);
 for(int n=0;n<inf.count&&unloadRosterCount<nativeorders::MaxPassengers;++n){uint32_t p=item(inf,n),carrier=p?get<uint32_t>(p+0x11C):0;
  if(!carrier||!worldObjects.contains(carrier)||queuedUnload.find(carrier,get<uint32_t>(carrier+0x10))<0||!unloadPassengerEligible(p,house)||!featureObjectAllowed(featuretypes::UnloadPassenger,p))continue;
  unloadRoster[unloadRosterCount++]={carrier,p,get<uint32_t>(p+0x10),manualVersions.get(get<uint32_t>(p+0x10)),-1};
 }
 for(int n=0;n<recentPassengerCount&&unloadRosterCount<nativeorders::MaxPassengers*2;++n){const auto&r=recentPassengers[n];
  if(queuedUnload.find(r.carrier,r.carrierID)>=0)unloadRoster[unloadRosterCount++]={r.carrier,r.p,r.id,r.token,n};
 }
 std::sort(unloadRoster,unloadRoster+unloadRosterCount,[](const auto&a,const auto&b){if(a.carrier!=b.carrier)return a.carrier<b.carrier;if(a.pointer!=b.pointer)return a.pointer<b.pointer;return a.history<b.history;});
}
// A Ctrl+D command may arrive after the game's native D began deploying.
// Reject stale/foreign carriers, not the transport's own unload-in-progress
// state; otherwise the queued command is acknowledged then discarded next tick.
// Three-state admission for an already-accepted Ctrl+D request.
//  1: native fields prove an eligible carrier
//  0: unreadable or query unavailable; do NOT retire the player's intent
// -1: complete roster/fields prove replacement, death or manual takeover
static int unloadCarrierKnowledge(const unloadadmission::Carrier&task,uint32_t house){
 const uint32_t p=task.pointer;
 if(!p||!worldObjects.contains(p))return -1;
 uint32_t id=0,vt=0,owner=0,carried=0;int hp=0,cargo=0;
 uint8_t map=0,alive=0,limbo=1,disabled=0;
 if(!read(p+0x10,&id,sizeof id)||!read(p,&vt,sizeof vt)||
    !read(p+0x21C,&owner,sizeof owner)||!read(p+0x6C,&hp,sizeof hp)||
    !read(p+0x114,&cargo,sizeof cargo)||!read(p+0x11C,&carried,sizeof carried)||
    !read(p+0x74,&map,sizeof map)||!read(p+0x90,&alive,sizeof alive)||
    !read(p+0x81,&limbo,sizeof limbo)||!read(p+0x1C8,&disabled,sizeof disabled))return 0;
 if(id!=task.id||vt!=0x7F5C70u||owner!=house||hp<=0||!map||!alive||limbo||disabled||carried)return -1;
 if(!unloadadmission::retainAfterCarrierMove(task.token,manualVersions.get(task.id),cargo))return -1;
 // A failed optional TypeID/feature-query observation cannot establish death
 // or cancellation. The original 3.5s / 12s admission horizon still applies.
 if(!featureObjectAllowed(featuretypes::UnloadTransport,p))return 0;
 return 1;
}
static bool unloadCarrierEligible(uint32_t p,uint32_t house){return p&&worldObjects.contains(p)&&own(p,house)&&live(p)&&kind(p)==2&&featureObjectAllowed(featuretypes::UnloadTransport,p)&&!get<uint32_t>(p+0x11C);}
static int smartUnloadNow(uint32_t house,uint32_t p,bool firstAttempt=true){
 if(!unloadCarrierEligible(p,house))return 0;
 const int frame=get<int>(0xA8ED84);const DWORD now=sampleClock(sh);
 static uint32_t roster[nativeorders::MaxPassengers];int count=0;uint32_t previous=0;
 auto*begin=std::lower_bound(unloadRoster,unloadRoster+unloadRosterCount,p,[](const UnloadRosterEntry&r,uint32_t carrier){return r.carrier<carrier;});
 for(auto*at=begin;at<unloadRoster+unloadRosterCount&&at->carrier==p;++at){uint32_t q=at->pointer;
  if(q==previous)continue;
  if(!unloadPassengerEligible(q,house)||get<uint32_t>(q+0x10)!=at->id||manualVersions.get(at->id)!=at->token||!featureObjectAllowed(featuretypes::UnloadPassenger,q))continue;
  uint32_t carrier=get<uint32_t>(q+0x11C);
  if(at->history>=0){const auto&old=recentPassengers[at->history];
   if(old.carrierID!=get<uint32_t>(p+0x10)||old.carrierType!=typeOf(p,2)||DWORD(now-old.tick)>1500||frame<old.frame||frame-old.frame>90||carrier)continue;
  }else if(carrier!=p)continue;
  if(count==nativeorders::MaxPassengers)return -6;
  previous=q;roster[count++]=q;
 }
 // Native cargo chain contains the entire load even while later occupants
 // remain inactive/limbo. Supplement the infantry inventory once per carrier
 // admission; do not turn a partial active-unit sample into a one-person job.
 const int cargoCount=get<int>(p+0x114);
 if(cargoCount>0&&cargoCount<=256){
  uint32_t q=get<uint32_t>(p+0x118);
  uint32_t seen[256]{};int scanned=0;
  while(q&&scanned<cargoCount){
   if(!worldObjects.contains(q)||get<uint32_t>(q+0x11C)!=p)break;
   bool duplicate=false;for(int k=0;k<scanned;++k)if(seen[k]==q){duplicate=true;break;}
   if(duplicate)break;
   seen[scanned++]=q;
   if(unloadPassengerEligible(q,house)&&featureObjectAllowed(featuretypes::UnloadPassenger,q)){
    // An actor reused between roster snapshot and admission must never
    // inherit the prior passenger's Ctrl+D intent.
    const uint32_t id=get<uint32_t>(q+0x10),token=manualVersions.get(id);
    for(auto*at=begin;at<unloadRoster+unloadRosterCount&&at->carrier==p;++at)
     if(at->pointer==q&&(at->id!=id||at->token!=token))return -6;
    bool enrolled=false;for(int k=0;k<count;++k)if(roster[k]==q){enrolled=true;break;}
    if(!enrolled){if(count==nativeorders::MaxPassengers)return -6;roster[count++]=q;}
   }
   q=get<uint32_t>(q+0x30);
  }
  // A torn list must not silently omit later passengers. The original
  // infantry roster is still used for recovered prior ejectees.
  if(scanned!=cargoCount||q)return -6;
 }
 const bool loaded=get<int>(p+0x114)>0;
 // Missing infantry is not proof of an empty transport: it may carry tanks,
 // or the infantry list may lag native D. Issue at most one ordinary Deploy
 // on initial admission, but retain this Ctrl+D for later passenger enrolment.
 // Return 2 means "native event issued, passenger admission still pending".
 if(!count){
  if(!loaded||!firstAttempt||get<bool>(p+0x1C8)||get<int>(p+0xAC)==16||get<int>(p+0xB4)==16)return -6;
  return nativeMission(house,get<uint32_t>(p+0x10),16,0,0,0,0,0,taskarbiter::Owner::Unload)?2:-6;
 }
 int replace=0;for(int j=0;j<unloadCount;++j)if(unloadPassengers[j].p==p||unloadPassengers[j].carrier==p)++replace;
 if(count>nativeorders::MaxPassengers-unloadCount+replace)return -6;
 // Queue acceptance is separate from this native Deploy. Existing active
 // unload and empty historical carriers never receive a second Deploy.
 // Original D can already have initiated unloading. Never queue a second
 // native Deploy while its in-progress state is present.
 if(loaded&&!get<bool>(p+0x1C8)&&get<int>(p+0xAC)!=16&&get<int>(p+0xB4)!=16&&
    !nativeMission(house,get<uint32_t>(p+0x10),16,0,0,0,0,0,taskarbiter::Owner::Unload))return -6;
 // A NEW Ctrl+D supersedes this carrier's prior unload episode. Moving
 // the carrier does not; the passenger's own manual order does.
 for(int j=0;j<unloadCount;)if(unloadPassengers[j].carrier==p){
  finishUnloadFollowup(unloadPassengers[j],frame);
  unloadPassengers[j]=unloadPassengers[--unloadCount];
 }else ++j;
 cancelNative(p);
 for(int j=0;j<count;++j){uint32_t q=roster[j];auto&r=unloadPassengers[unloadCount++];r={};r.job=++unloadJobSerial;r.p=q;r.id=get<uint32_t>(q+0x10);r.carrier=p;r.carrierID=get<uint32_t>(p+0x10);r.carrierType=typeOf(p,2);r.frame=frame;r.manualToken=manualVersions.get(r.id);}
 return 1;
}
static int smartUnload(uint32_t house){
 if(!(sh->flags&SmartUnloadOrders)||!loadInterfaces())return -12;
 if(!simulationReady()||get<bool>(0xA8BC60+25))return -11;
 const DWORD now=sampleClock(sh);queuedUnload.begin(match,house,now);
 static unloadadmission::Carrier candidates[unloadadmission::Capacity];static unloadpolicy::RosterLookup<unloadadmission::Capacity> selected;selected.clear();
 static unloadpolicy::RosterLookup<nativeorders::MaxPassengers> recent;recent.clear();
 const int frame=get<int>(0xA8ED84);
 for(int n=0;n<recentPassengerCount;++n){const auto&r=recentPassengers[n];
  if(DWORD(now-r.tick)<=1500&&frame>=r.frame&&frame-r.frame<=90)recent.add(r.carrier,n);
 }
 int count=0,added=0;Vec v=vec(0xA8ECB8);
 for(int n=0;n<v.count;++n){uint32_t p=item(v,n);if(!unloadCarrierEligible(p,house)||selected.find(p)>=0)continue;
  uint32_t id=get<uint32_t>(p+0x10);int old=recent.find(p);
  if(get<int>(p+0x114)<=0&&(old<0||recentPassengers[old].carrierID!=id||recentPassengers[old].carrierType!=typeOf(p,2)))continue;
  if(count==unloadadmission::Capacity)return -42;
  selected.add(p,count);candidates[count++]={p,id,manualVersions.get(id)};
  if(queuedUnload.find(p,id)<0)++added;
 }
 // Preflight the whole request; never report a silently truncated prefix.
 if(added>unloadadmission::Capacity-queuedUnload.count)return -42;
 for(int n=0;n<count;++n)queuedUnload.add(candidates[n]);
 // Latch the passenger identities at the command boundary, not one frame
 // later: native D may unlink their carrier before admissionTick runs.
 // This only remembers members of the explicitly selected own transports;
 // all UID/manual-command/owner/filter checks still run at admission.
 if(count){
  static unloadpolicy::RosterLookup<nativeorders::MaxPassengers> tracked;tracked.clear();
  for(int n=0;n<recentPassengerCount;++n)tracked.add(recentPassengers[n].p,n);
  Vec inf=vec(0xA83DE8);
  for(int n=0;n<inf.count;++n){uint32_t q=item(inf,n);if(!q)continue;
   uint32_t carrier=get<uint32_t>(q+0x11C);
   if(selected.find(carrier)<0||!unloadPassengerEligible(q,house)||!featureObjectAllowed(featuretypes::UnloadPassenger,q))continue;
   const uint32_t id=get<uint32_t>(q+0x10),carrierID=get<uint32_t>(carrier+0x10);
   int at=tracked.find(q);
   if(at<0){if(recentPassengerCount==nativeorders::MaxPassengers)continue;at=recentPassengerCount++;tracked.add(q,at);}
   recentPassengers[at]={q,id,carrier,carrierID,typeOf(carrier,2),manualVersions.get(id),now,frame};
  }
 }
 return count; // acknowledgement means arranged, not physically unloaded
}
static void smartUnloadAdmissionTick(uint32_t house,int frame,int&budget){
 const DWORD now=sampleClock(sh);
 if(!(sh->flags&SmartUnloadOrders)||queuedUnload.epoch!=match||queuedUnload.house!=house||DWORD(now-queuedUnload.lastProgress)>12000){queuedUnload.clear();return;}
 // Failed UID/page capture means unknown, never an empty cargo roster.
 if(!unloadpolicy::completeIdentityIndex(indexedFrame,frame))return;
 if(!queuedUnload.count||queuedUnload.frame==frame||!loadInterfaces()||!simulationReady()||get<bool>(0xA8BC60+25))return;
 queuedUnload.frame=frame;queuedUnload.reindex();
 if(budget<=0||!outgoingRoom())return;
 // Reserve delivery room for passengers already on the ground. Enlarging
 // carrier storage must not starve their follow-up commands until timeout.
 int admissionBudget=std::min(budget,nativeorders::EventBudget);
 if(unloadCount)admissionBudget=std::min(admissionBudget,outgoingRoom()/2);
 if(admissionBudget<=0)return;
 buildUnloadAdmissionRoster(house);int todo=std::min(queuedUnload.count,nativeorders::WorkPerFrame);
 while(todo--&&queuedUnload.count&&budget>0&&admissionBudget>0&&outgoingRoom()){
  if(queuedUnload.cursor>=queuedUnload.count)queuedUnload.cursor=0;
  int at=queuedUnload.cursor++;auto task=queuedUnload.carriers[at];
  if(task.attempted&&DWORD(now-task.attemptedAt)>3500){queuedUnload.remove(at);continue;}
  const int knowledge=unloadCarrierKnowledge(task,house);
  if(knowledge<0){queuedUnload.remove(at);continue;}
  if(!knowledge)continue; // Missing page is not a completed Ctrl+D.
  if(!task.attempted){queuedUnload.carriers[at].attempted=true;queuedUnload.carriers[at].attemptedAt=now;}
  const int result=smartUnloadNow(house,task.pointer,!task.attempted);
  if(result==2){ // Native Deploy accepted; passenger admission remains pending.
   queuedUnload.lastProgress=now;--budget;--admissionBudget;
  }else if(result==1){ // Only an actual completed enrollment can retire intent.
   queuedUnload.cancel(task.pointer,task.id);queuedUnload.lastProgress=now;--budget;--admissionBudget;
  }
  // Zero/negative admission is deferred, never silently acknowledged.
 }
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
 // An incomplete global identity snapshot must not cancel already admitted
 // passengers. Resume them on the next complete world-index frame.
 if(!unloadpolicy::completeIdentityIndex(indexedFrame,frame))return;
 serviceUnloadCalculation(house,frame,budget);
 if(externalInputFrame){unloadExternalInput.count=0;unloadExternalInput.targetCount=0;unloadExternalInput.match=match;unloadExternalInput.tick=sampleClock(sh);unloadExternalInput.frame=frame;}
 Vec all=vec(0xA8EC78);watchEjections(house,frame,all);int todo=unloadCount;if(todo>nativeorders::WorkPerFrame)todo=nativeorders::WorkPerFrame;
 while(todo--&&unloadCount){if(unloadCursor>=unloadCount)unloadCursor=0;int i=unloadCursor++;auto&r=unloadPassengers[i];uint32_t p=r.p;
  // The global indexed Techno list is authoritative. The live infantry
  // roster can temporarily omit later passengers during sequential ejection.
  bool member=worldObjects.contains(p),carrierMember=nativeMember(all,r.carrier);
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
    if(collectUnloadSource(input,r,house,frame)){
     auto decision=unloadcompute::decide(input.sources[0],input.targets,input.actions[0],input.targetCount);
     done=applyUnloadDecision(r,input,0,decision,house,frame,budget,false);
    }
   }else if(externalInputFrame&&unloadExternalInput.count<unloadcompute::MaxSources){
    collectUnloadSource(unloadExternalInput,r,house,frame);
   }
  }
  if(done){finishUnloadFollowup(unloadPassengers[i],frame);unloadPassengers[i]=unloadPassengers[--unloadCount];unloadCursor=i;}
 }
 publishUnloadCalculation();
}
static void nativeOrdersTick(uint32_t house,int frame){
 if(frame==nativeFrame)return;nativeFrame=frame;
 pruneMagnetronPauses(frame); // drop is independently enabled, even without NativeOrders
 if(!(sh->flags&NativeOrders)&&!unloadCount&&!queuedUnload.count){if(nativeCount)advancedObjects.clear();nativeCount=0;snap.advancedUnits=snap.unloadPending=0;return;}
 // Input interception is for advanced click / CS capture ONLY.  An input
 // hook error must not cancel an independently observed disembark/ejection:
 // those workers issue vanilla EventClass orders and need no code patch.
 // Interface readiness may recover next tick; retain explicit intents.
 for(int i=0;i<nativeCount;)if(!worldObjects.contains(nativeOrders[i].p)||get<uint32_t>(nativeOrders[i].p+0x10)!=nativeOrders[i].id||!(sh->flags&(nativeOrders[i].mode==planner::MagnetronMove?MagnetronInput:AdvancedInput))||!(nativeOrders[i].mode==planner::MagnetronMove?rawMagnetron(nativeOrders[i].p):featureObjectAllowed(featuretypes::AdvancedAttack,nativeOrders[i].p)))removeOrder(i);else ++i;
 // Global mode enrolls only idle magnets; manual movement/attacks are not
 // converted into automation. Existing explicit enhanced-click tasks survive.
 // An enrolled Attack may set its own destination while approaching. Only the
 // manual-input token cancels it; destination alone is not a manual command.
 for(int i=0;i<nativeCount;)if(nativeOrders[i].global&&(!sh->magnetronGlobal||manualVersions.get(nativeOrders[i].id)!=nativeOrders[i].token))removeOrder(i);else ++i;
 advancedObjects.clear();for(int i=0;i<nativeCount;++i)advancedObjects.add(nativeOrders[i].p,nativeOrders[i].id,uint32_t(i+1));
 // Match the pending-input horizon. After it expires, normal mission, queued
 // mission, destination and target checks still protect an ongoing command.
 if(sh->magnetronGlobal&&!snap.chat&&globalMagnetronPauseOverflow<0){
  for(int n=0;n<globalVehicleCandidateCount&&nativeCount<nativeorders::MaxOrders;++n){uint32_t p=globalVehicleCandidates[n];if(!p||!own(p,house)||!rawMagnetron(p)||stickyOwns(p)||formationOwns(get<uint32_t>(p+0x10))||globalMagnetronPauseIndex.contains(p)||orderIndex(p)>=0||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x5A4)||get<uint32_t>(p+0x2B4)||get<uint32_t>(p+0x2AC))continue;
   int m=get<int>(p+0xAC),q=get<int>(p+0xB4);if((m!=5&&m!=6&&m!=11&&m!=13)||(q!=-1&&q!=m))continue;
   int x=get<int>(p+0x9C)/256,y=get<int>(p+0xA0)/256;if(x<=0||x>=512||y<=0||y>=512)continue;
   auto&o=nativeOrders[nativeCount++];o={};o.p=p;o.id=get<uint32_t>(p+0x10);o.token=manualVersions.get(o.id);o.mode=planner::MagnetronMove;o.cell=o.nearest=y*1000+x;o.gx=x*256+128;o.gy=y*256+128;o.global=true;o.progress.hold=true;
   advancedObjects.add(p,o.id,uint32_t(nativeCount));
  }
 }
 int budget=recurringBudget(EventLane::Unload);smartUnloadAdmissionTick(house,frame,budget);if(unloadCount||(sh->flags&EjectAssist))smartUnloadTick(house,frame,budget);
 // Advanced decisions use the selected shared Planner backend.
 // This state stores click intents and validates actor identities in either mode.
 for(int i=0;i<nativeCount;){auto&o=nativeOrders[i];if(!worldObjects.contains(o.p)||!own(o.p,house)||get<uint32_t>(o.p+0x10)!=o.id)removeOrder(i);else ++i;}
 if(nativeCount||snap.advancedUnits){advancedObjects.clear();for(int i=0;i<nativeCount;++i)advancedObjects.add(nativeOrders[i].p,nativeOrders[i].id,uint32_t(i+1));}
 snap.advancedUnits=nativeCount;snap.unloadPending=queuedUnload.count;for(int i=0;i<unloadCount;++i)if(unloadPassengers[i].mode!=1)++snap.unloadPending;
}

