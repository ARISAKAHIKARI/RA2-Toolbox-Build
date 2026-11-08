#include "tactical_frame_read.h"
#include "../../shared/source/debug_entry_checks.h"
// Runs on the game's message thread. Only the normal outgoing event queue
// receives writes: no direct passenger insertion or mission mutation.
static bool loadInterfaces(){
 static uint32_t checkedMatch,checkedPolicy;static DWORD checkedAt;static bool valid;
 DWORD now=sampleClock(sh);if(checkedMatch==match&&checkedPolicy==debugentries::policyRevision&&DWORD(now-checkedAt)<500)return valid;
 checkedMatch=match;checkedPolicy=debugentries::policyRevision;checkedAt=now;valid=false;
 static constexpr Signature required[]={
  {0x4C6860,8,{0x8B,0xC1,0x8B,0x4C,0x24,0x04,0x85,0xC9}}
  // MouseOverObject is a YRpp virtual call. Do not demand the unused vanilla
  // Infantry implementation at 0x51E3B0 remain unhooked by Ares/MO.
 };
 for(auto&s:required)if(!debugentries::pattern(debugentries::Kind::Call,s.address,s.bytes,s.length,read))return false;
 valid=true;return true;
}
static bool loadingObject(uint32_t p,uint32_t house,int expected){
 return worldObjects.contains(p)&&own(p,house)&&kind(p)==expected&&!get<bool>(p+0x1C8)&&!get<uint32_t>(p+0x11C);
}
static uint32_t incomingVehicle(uint32_t p){
 if(get<uint32_t>(p+0x11C))return 0;
 if(get<int>(p+0xAC)!=7&&get<int>(p+0xB4)!=7)return 0;
 uint32_t queued=get<uint32_t>(p+0x500);if(queued>=0x10000)return queued;
 uint32_t destination=get<uint32_t>(p+0x5A4);return destination?destination:get<uint32_t>(p+0x2B4);
}
static bool enterEvent(uint32_t house,uint32_t infantry,uint32_t vehicle,uint32_t* receiptID=nullptr){
 // Ordinary Enter is just a vanilla MegaMission with a destination object.
 // Use exactly the same constructor and enqueue entry as ore/attack/unload.
 return nativeMission(house,infantry,7,0,vehicle,0x34,0,0,taskarbiter::Owner::AutoLoad,receiptID);
}
static loading::Observation loadingObservation(uint32_t p){return {get<int>(p+0xAC),get<int>(p+0xB4),get<uint32_t>(p+0x2B4),get<uint32_t>(p+0x5A4)};}
extern "C" int GuardedEnterAction(unsigned,unsigned,unsigned,unsigned*);
static bool loadQueryFailed;
static bool canEnter(uint32_t p,uint32_t target){
 if(p==target||loadQueryFailed||!gameentry::active(gameentry::Client::Assist))return false;
 FaultPhase phase(runtimefault::LoadLegality,p,target);unsigned action=0;
 if(!GuardedEnterAction(get<uint32_t>(get<uint32_t>(p)+0x74),p,target,&action)){loadQueryFailed=true;return false;}
 return action==3;
}
static int passengerSlots(uint32_t p){int k=kind(p);uint32_t type=typeOf(p,k);return type?loading::slots(get<double>(type+0x380)):0;}
static void finishLoadingReservation(const loading::Reservation&r,uint32_t session,int frame,bool boarded){
 if(!r.ticket)return;gateway::Receipt outcome{};
 gateway::Intent intent{session,0,r.id,r.vehicleID,0};
 gateway::planned(outcome,r.ticket,intent);outcome.owner=taskarbiter::Owner::AutoLoad;outcome.queuedAt=r.queuedAt;
 if(boarded){outcome.phase=gateway::Phase::Completed;outcome.observedAt=sampleClock(sh);}
 else{outcome.phase=frame-r.frame>900?gateway::Phase::Expired:gateway::Phase::Cancelled;
      outcome.failure=frame-r.frame>900?gateway::Failure::Stale:gateway::Failure::Superseded;}
 // Business terminal supersedes the generic task-change observer for this
 // specific send; otherwise a late generic Expired would contradict Completed.
 missionObserver.dismissTicket(session,r.ticket);
 recordReceipt(outcome);actorLeaseArbiter.release(session,r.id,r.ticket);
}
static bool loadCalculationPending,loadAckPending;static uint32_t loadAckRequest,loadTaskSeen;
static DWORD loadCalculationStarted;
static bool loadCaptureWaiting;
struct LoadSelectedIdentity {uint32_t pointer,id,token;};
static LoadSelectedIdentity loadSelection[4096],loadCarrierSelection[loading::MaxVehicles];
static int loadSelectionCount,loadCarrierSelectionCount=-1;
struct LoadSelectionFact {loading::Infantry unit;bool acceptable=false,candidate=false,waiting=false;};
static LoadSelectionFact loadSelectionFacts[4096];
static compute::LoadInput loadSample;
static int loadAcceptRoom[loading::MaxVehicles];
static compute::FrameHistory<compute::LoadInput> loadFrames;static compute::LoadResult loadResultCopy;static compute::LoadInput loadCaptureCohort;
static int autoLoad(uint32_t house,const compute::LoadInput*cohort=nullptr);
static int submitLoading(uint32_t house,const loading::Infantry*soldiers,int ns,const loading::Vehicle*vehicles,int nv,const int*assignment,const uint32_t*tokens){
 using namespace loading;int frame=get<int>(0xA8ED84);loadQueryFailed=false;
 faultContext.phase=runtimefault::LoadSubmit;
 int room[MaxVehicles]{};for(int j=0;j<nv;++j){const auto&v=vehicles[j];if(worldByID(v.id)!=v.object||!loadingObject(v.object,house,2))continue;
  uint32_t type=typeOf(v.object,2),q=get<uint32_t>(v.object+0x118);room[j]=type?get<int>(type+0x5E0):0;int steps=0;
  while(q>=0x10000&&steps++<256){int size=passengerSlots(q);if(!size){room[j]=0;break;}room[j]-=size;uint32_t next=get<uint32_t>(q+0x30);if(next==q){room[j]=0;break;}q=next;}if(q>=0x10000)room[j]=0;
 }
 Vec all=vec(0xA8EC78);for(int n=0;n<all.count;++n){uint32_t other=item(all,n);int k=other?kind(other):-1;if((k!=1&&k!=2)||!own(other,house))continue;uint32_t target=incomingVehicle(other);if(!target)continue;
  for(int j=0;j<nv;++j)if(vehicles[j].object==target){int size=passengerSlots(other);room[j]-=size?size:1024;break;}
 }
 for(int n=0;n<loadLedger.count;++n){auto&r=loadLedger.records[n];if(!worldObjects.contains(r.object)||incomingVehicle(r.object))continue;for(int j=0;j<nv;++j)if(r.vehicleID==vehicles[j].id){room[j]-=r.slots;break;}}
 for(int i=0;i<ns;++i){const auto&p=soldiers[i];int j=assignment[i];
  if(j<0||j>=nv){if(p.slots)++snap.loadUnassigned;continue;}auto&v=vehicles[j];
  if(room[j]<p.slots||loadLedger.count>=MaxInfantry||worldByID(p.id)!=p.object||worldByID(v.id)!=v.object||manualVersions.get(p.id)!=tokens[i]||
     !loadingObject(p.object,house,p.kind)||!loadingObject(v.object,house,2)||incomingVehicle(p.object)||loadLedger.contains(p.id)||!canEnter(p.object,v.object)){
   ++snap.loadUnassigned;if(loadQueryFailed)return -15;continue;
  }
  room[j]-=p.slots;
  cancelUnloadPassenger(p.object);uint32_t ticket=0;bool sent=outgoingRoom()>0&&enterEvent(house,p.id,v.id,&ticket);
  Reservation job{p.object,p.id,v.object,v.id,p.slots,frame};job.lastSent=sent?frame:-1;job.lastAttempt=frame;job.attempts=sent?0:1;job.retryAfter=sent?frame+30:frame+8;job.x=p.x;job.y=p.y;job.progress=frame;job.initial=loadingObservation(p.object);job.kind=p.kind;job.ticket=ticket;job.queuedAt=sent?sampleClock(sh):0;loadLedger.add(job);
  if(sent)++snap.loadIssued;else ++snap.loadWaiting;
 }
 return snap.loadWaiting?2:1;
}
static void loadingCalculationTick(uint32_t house,DWORD now){
 if(!loadCalculationPending)return;
 auto finish=[&](int result){loadCalculationPending=false;loadCaptureWaiting=false;if(loadAckPending){sh->result=result;sh->count=snap.loadIssued;pendingAck=loadAckRequest;pendingAckReady=true;loadAckPending=false;}};
 if(DWORD(now-loadCalculationStarted)>1000){finish(-7);return;}
 const uint32_t completed=sh->loadComputed;
 if(compute::newer(completed,loadTaskSeen)){
  long seq=sh->loadResultSeq;if(!(seq&1)){MemoryBarrier();loadResultCopy=sh->loadResult;MemoryBarrier();
   if(seq==sh->loadResultSeq&&completed==sh->loadComputed){loadTaskSeen=completed;const auto*source=loadFrames.find(completed);const auto&r=loadResultCopy;
    if(source&&compute::valid(*source)&&source->mode==int(sh->loadMode==1)&&source->radius==std::clamp(sh->loadRadius,1,512)&&r.count==source->ns&&r.match==source->match&&r.tick==source->tick&&r.frame==source->frame&&
       freshness::valid(source->match,match,source->tick,source->frame,now,get<int>(0xA8ED84),freshness::Cooperative)&&!get<bool>(0xA8BC60+25)){
     finish(submitLoading(house,source->soldiers,source->ns,source->vehicles,source->nv,r.assignment,source->tokens));return;
    }
   }
  }
 }
 if(externalInputFrame&&!get<bool>(0xA8BC60+25)){compute::copyInput(loadCaptureCohort,sh->loadInput);int result=autoLoad(house,&loadCaptureCohort);if(result!=-30&&result!=-6)finish(result);}
}
// Capture the user's selection as a separate input generation. Repeated world
// captures keep this exact cohort and its manual tokens even if selection moves.
static bool captureLoadSelection(const compute::LoadInput*cohort){
 loadSelectionCount=0;loadCarrierSelectionCount=cohort?cohort->nv:-1;
 if(cohort){
  if(!compute::valid(*cohort))return false;
  for(int n=0;n<cohort->ns;++n){const auto&p=cohort->soldiers[n];loadSelection[loadSelectionCount++]={p.object,p.id,cohort->tokens[n]};}
  for(int n=0;n<cohort->nv;++n){const auto&v=cohort->vehicles[n];loadCarrierSelection[n]={v.object,v.id,0};}
 }else{
  Vec selected{};if(!read(0xA8ECB8,&selected,sizeof selected)||selected.count<0||selected.count>4096||selected.capacity<selected.count||(selected.count&&selected.items<0x10000))return false;
  for(int n=0;n<selected.count;++n){uint32_t p=0,id=0;if(!read(selected.items+uint32_t(n)*4,&p,sizeof p))return false;if(!p)continue;
   if(!read(p+0x10,&id,sizeof id))return false;if(id)loadSelection[loadSelectionCount++]={p,id,manualVersions.get(id)};
  }
 }
 return true;
}
static bool collectLoadFacts(uint32_t house,int frame){
 using namespace loading;
 // Identity and permission belong to this live input cohort, not the frozen
 // raw world. Validate once before entering the pure decoder below.
 static uint32_t selected[4096],carriers[MaxVehicles];
 for(int n=0;n<loadSelectionCount;++n){const auto&p=loadSelection[n];uint32_t id=0;if(!read(p.pointer+0x10,&id,sizeof id))return false;
  selected[n]=id==p.id&&manualVersions.get(p.id)==p.token?p.pointer:0;
  if(selected[n]&&worldObjects.byId(p.id)!=p.pointer)return false;
 }
 for(int n=0;n<loadCarrierSelectionCount;++n){const auto&v=loadCarrierSelection[n];uint32_t id=0;if(!read(v.pointer+0x10,&id,sizeof id))return false;
  carriers[n]=id==v.id?v.pointer:0;if(carriers[n]&&worldObjects.byId(v.id)!=v.pointer)return false;
 }
 SampleReadScope frameRead;if(!frameRead)return false;
 // A click can follow this frame's capture. Wait for its exact identities;
 // never pair a new cohort/token with a previous allocation's raw fields.
 for(int n=0;n<loadSelectionCount;++n)if(selected[n]&&get<uint32_t>(selected[n]+0x10)!=loadSelection[n].id)return false;
 for(int n=0;n<loadCarrierSelectionCount;++n)if(carriers[n]&&get<uint32_t>(carriers[n]+0x10)!=loadCarrierSelection[n].id)return false;
 auto&in=loadSample;in.match=match;in.tick=sampleClock(sh);in.frame=frame;in.ns=in.nv=0;in.mode=sh->loadMode==1;in.radius=std::clamp(sh->loadRadius,1,512);in.composition.clear();
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;
 bool selectedCarrier=loadCarrierSelectionCount>0;
 for(int n=0;n<loadSelectionCount;++n){uint32_t p=selected[n];
  if(p&&own(p,house)&&kind(p)==2&&featureObjectAllowed(featuretypes::LoadTransport,p)){uint32_t t=typeOf(p,2);if(t&&get<int>(t+0x5E0)>0){selectedCarrier=true;break;}}
 }
 const int carrierCount=loadCarrierSelectionCount>=0?loadCarrierSelectionCount:selectedCarrier?loadSelectionCount:all.count;
 for(int n=0;n<carrierCount&&in.nv<MaxVehicles;++n){uint32_t p=loadCarrierSelectionCount>=0?carriers[n]:selectedCarrier?selected[n]:item(all,n);
  if(!p||!loadingObject(p,house,2)||incomingVehicle(p))continue;uint32_t t=typeOf(p,2);if(!t||!featureTypeAllowed(featuretypes::LoadTransport,t))continue;
  int capacity=get<int>(t+0x5E0),count=get<int>(p+0x114);double limit=get<double>(t+0x388);if(capacity<=0||capacity>1024||count<0||count>256||!(limit>0&&limit<=128))continue;
  for(auto&number:in.composition.numbers[in.nv])number=0;
  int used=0;uint32_t q=get<uint32_t>(p+0x118),seen[256]{};int steps=0;
  while(q>=0x10000&&steps<256){bool cycle=false;for(int j=0;j<steps;++j)if(seen[j]==q)cycle=true;if(cycle){used=capacity;break;}seen[steps++]=q;
   int size=passengerSlots(q);if(!size){used=capacity;break;}used+=size;in.composition.add(in.nv,typeOf(q,kind(q)));q=get<uint32_t>(q+0x30);
  }
  if(used>capacity||steps!=count)continue;
  uint32_t id=get<uint32_t>(p+0x10);bool duplicate=false;for(int j=0;j<in.nv;++j)if(in.vehicles[j].id==id)duplicate=true;if(!id||duplicate)continue;
  const int index=in.nv++;in.vehicles[index]={p,id,get<int>(p+0x9C),get<int>(p+0xA0),capacity,used,0,limit,count};loadAcceptRoom[index]=freeSlots(in.vehicles[index]);
 }
 for(int n=0;n<loadSelectionCount;++n){auto&fact=loadSelectionFacts[n];fact={};uint32_t p=selected[n];int k=p?kind(p):-1;
  if((k!=1&&k!=2)||!loadingObject(p,house,k))continue;uint32_t t=typeOf(p,k);if(!t||!featureTypeAllowed(featuretypes::LoadPassenger,t))continue;
  fact.unit={p,get<uint32_t>(p+0x10),get<int>(p+0x9C),get<int>(p+0xA0),slots(get<double>(t+0x380)),t,k};fact.acceptable=fact.unit.slots>0;
  if(get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D))continue;
  fact.candidate=true;fact.waiting=incomingVehicle(p)||loadLedger.contains(fact.unit.id);
 }
 // The original incoming roster is shared with every other collector; this
 // capacity observation is independent of the later live submit revalidation.
 for(int n=0;n<all.count;++n){uint32_t p=item(all,n);int k=p?kind(p):-1;if((k!=1&&k!=2)||!loadingObject(p,house,k))continue;uint32_t target=incomingVehicle(p);if(!target)continue;
  for(int j=0;j<in.nv;++j)if(in.vehicles[j].object==target){int size=passengerSlots(p);in.vehicles[j].reserved+=size?size:in.vehicles[j].capacity;++in.vehicles[j].passengers;in.composition.add(j,typeOf(p,k));break;}
 }
 for(int n=0;n<loadLedger.count;++n){const auto&r=loadLedger.records[n];if(incomingVehicle(r.object))continue;
  for(int j=0;j<in.nv;++j)if(in.vehicles[j].id==r.vehicleID){in.vehicles[j].reserved+=r.slots;++in.vehicles[j].passengers;in.composition.add(j,typeOf(r.object,r.kind));break;}
 }
 return frameRead.complete();
}
static int autoLoad(uint32_t house,const compute::LoadInput*cohort){
 if(!cohort&&loadCalculationPending)return -6;
 using namespace loading;
 FaultPhase phase(runtimefault::LoadCollect);loadQueryFailed=false;
 if(!loadInterfaces())return -12;const int frame=get<int>(0xA8ED84);
 // Reservation completion/cancellation remains an immediate live observation.
 for(int n=0;n<loadLedger.count;){auto&r=loadLedger.records[n];
  bool remove=!worldObjects.contains(r.object)||!worldObjects.contains(r.vehicle)||!own(r.object,house)||!own(r.vehicle,house)||get<uint32_t>(r.object+0x10)!=r.id||get<uint32_t>(r.vehicle+0x10)!=r.vehicleID||get<uint32_t>(r.object+0x11C)!=0||int64_t(frame)-r.frame>900;
  if(!remove)remove=takenOver(r,loadingObservation(r.object),incomingVehicle(r.object));
  if(remove){bool boarded=worldObjects.contains(r.object)&&get<uint32_t>(r.object+0x10)==r.id&&get<uint32_t>(r.object+0x11C)==r.vehicle;finishLoadingReservation(r,match,frame,boarded);loadLedger.erase(n);}else ++n;
 }
 if(!cohort){loadFrames.clear();loadTaskSeen=sh->loadComputed;loadCalculationStarted=sampleClock(sh);loadCaptureWaiting=false;snap.loadIssued=0;}
 if(!loadCaptureWaiting&&!captureLoadSelection(cohort))return -4;
 if(!collectLoadFacts(house,frame)){loadCaptureWaiting=true;loadCalculationPending=true;return -30;}
 loadCaptureWaiting=false;auto&in=loadSample;
 snap.loadUnits=snap.loadIssued=snap.loadWaiting=snap.loadUnassigned=0;snap.loadVehicles=in.nv;
 // Engine legality is its own phase. No SampleReadScope survives into a
 // GuardedEnterAction call, assignment, reservation, or outgoing event.
 bool accepts[MaxVehicles]{};
 for(int j=0;j<in.nv;++j)if(loadAcceptRoom[j]>0&&worldByID(in.vehicles[j].id)==in.vehicles[j].object)
  for(int n=0;n<loadSelectionCount;++n){const auto&fact=loadSelectionFacts[n];const auto&p=fact.unit;
   if(fact.acceptable&&p.object!=in.vehicles[j].object&&p.slots<=loadAcceptRoom[j]&&worldByID(p.id)==p.object&&canEnter(p.object,in.vehicles[j].object)){accepts[j]=true;break;}
  }
 if(loadQueryFailed)return -15;
 for(int n=0;n<loadSelectionCount;++n){const auto&fact=loadSelectionFacts[n];const auto&p=fact.unit;if(!fact.candidate)continue;
  bool carrier=false;for(int j=0;j<in.nv;++j)if(accepts[j]&&in.vehicles[j].object==p.object)carrier=true;if(carrier)continue;
  ++snap.loadUnits;if(!p.id)continue;if(fact.waiting){++snap.loadWaiting;continue;}
  if(!p.slots||in.ns>=MaxInfantry){++snap.loadUnassigned;continue;}
  bool duplicate=false;for(int j=0;j<in.ns;++j)if(in.soldiers[j].id==p.id)duplicate=true;if(duplicate)continue;
  in.tokens[in.ns]=loadSelection[n].token;in.soldiers[in.ns++]=p;
 }
 if(!in.nv){snap.loadUnassigned+=in.ns;return 1;}
 faultContext.phase=runtimefault::LoadAssign;
 for(int n=0;n<in.ns;++n)for(int j=0;j<in.nv;++j){const auto&p=in.soldiers[n];const auto&v=in.vehicles[j];int64_t dx=int64_t(p.x)-v.x,dy=int64_t(p.y)-v.y,limit=int64_t(in.radius)*256;
  in.compatible[n][j]=p.slots<=freeSlots(v)&&p.slots<=v.sizeLimit&&dx*dx+dy*dy<=limit*limit&&worldByID(p.id)==p.object&&worldByID(v.id)==v.object&&canEnter(p.object,v.object);
  if(loadQueryFailed)return -15;
 }
 if(!compute::inside(sh->computeApplied,compute::Load)){
  compute::copyInput(sh->loadInput,in);loadCalculationPending=true;
  const uint32_t serial=compute::nextTask(sh->loadTask);loadFrames.retain(serial,in);MemoryBarrier();sh->loadTask=serial;notifyHost();return -30;
 }
 static int assignment[MaxInfantry];static Workspace workspace;
 assign(in.soldiers,in.ns,in.vehicles,in.nv,in.mode,in.radius,in.composition,workspace,assignment,[&](const Infantry&p,const Vehicle&v){return in.compatible[&p-in.soldiers][&v-in.vehicles]!=0;});
 return submitLoading(house,in.soldiers,in.ns,in.vehicles,in.nv,assignment,in.tokens);
}

static void cancelLoadingJobs(uint32_t p){
 for(int i=0;i<loadLedger.count;){auto &r=loadLedger.records[i];
  if(r.object==p||r.vehicle==p){finishLoadingReservation(r,match,get<int>(0xA8ED84),false);loadLedger.erase(i);}
  else ++i;
 }
}
static void autoLoadTick(uint32_t house,int frame){
 FaultPhase phase(runtimefault::LoadRetry);loadQueryFailed=false;
 static int processed=-1;static uint32_t processedMatch;if(frame==processed&&match==processedMatch)return;processed=frame;processedMatch=match;
 if(!loadLedger.count)return;
 if(!loadInterfaces()){
  for(int i=0;i<loadLedger.count;++i)finishLoadingReservation(loadLedger.records[i],match,frame,false);
  loadLedger.clear();snap.loadWaiting=0;return;
 }
 auto present=[&](uint32_t p){return worldObjects.contains(p);};int budget=outgoingRoom()?recurringBudget(EventLane::Boarding):0;
 for(int i=0;i<loadLedger.count;){auto&r=loadLedger.records[i];bool remove=!present(r.object)||!present(r.vehicle);
  if(!remove)remove=!featureObjectAllowed(featuretypes::LoadPassenger,r.object)||!featureObjectAllowed(featuretypes::LoadTransport,r.vehicle)||get<uint32_t>(r.object+0x10)!=r.id||get<uint32_t>(r.vehicle+0x10)!=r.vehicleID||get<uint32_t>(r.object+0x11C)!=0||!own(r.object,house)||!own(r.vehicle,house)||frame-r.frame>900;
  if(remove){
   bool boarded=present(r.object)&&get<uint32_t>(r.object+0x10)==r.id&&get<uint32_t>(r.object+0x11C)==r.vehicle;
   finishLoadingReservation(r,match,frame,boarded);loadLedger.erase(i);continue;
  }
  uint32_t incoming=incomingVehicle(r.object);if(loading::takenOver(r,loadingObservation(r.object),incoming)){
   finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);continue;
  }
  int x=get<int>(r.object+0x9C),y=get<int>(r.object+0xA0);if(x!=r.x||y!=r.y){r.x=x;r.y=y;r.progress=frame;}
  bool due=r.lastSent<0||(frame-r.lastSent>=30&&(!incoming||frame-r.progress>=120));
  if(due&&frame>=r.retryAfter&&budget>0&&outgoingRoom()>0&&loadingObject(r.object,house,r.kind)&&loadingObject(r.vehicle,house,2)&&canEnter(r.object,r.vehicle)){
   r.lastAttempt=frame;
   uint32_t ticket=0;
   if(enterEvent(house,r.id,r.vehicleID,&ticket)){
    // End the superseded Enter receipt before recording the replacement.
    finishLoadingReservation(r,match,frame,false);
    r.ticket=ticket;r.queuedAt=sampleClock(sh);r.lastSent=frame;r.progress=frame;r.retryAfter=frame+30;r.attempts=0;--budget;
   }else{
    if(r.attempts<10)++r.attempts;
    r.retryAfter=frame+6*r.attempts;
   }
  }
  if(loadQueryFailed){
   // No retry loop on a throwing predicate. Retire the affected reservation;
   // any previously accepted original event remains owned by the game.
   finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);loadQueryFailed=false;continue;
  }
  ++i;
 }
 snap.loadWaiting=loadLedger.count;
}

static void resetLoadCalculation(){loadCalculationPending=loadAckPending=loadCaptureWaiting=false;loadSelectionCount=0;loadCarrierSelectionCount=-1;}
