#include "tactical_frame_read.h"
#include "reverse_dodge.h"
#include "../../shared/source/debug_entry_checks.h"
// Implemented by cooperative_bridge.h later in this translation unit.
static int cooperativePendingSlots(uint32_t targetID,uint32_t targetPointer);
static bool cooperativePendingActor(uint32_t actorID);
static bool candidateLoadAdmission(uint32_t actor,uint32_t carrier,uint32_t id,uint32_t house,int slots);
static uint32_t readMapCell(int x,int y);
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
static bool enterEvent(uint32_t house,uint32_t infantry,uint32_t vehicle,
 const executionproof::Ticket&proof,uint32_t* receiptID=nullptr){
 // The original capture-time passenger and carrier pointers are not trusted
 // until the native dispatcher confirms both UIDs and the manual revision.
 return submitProvenMission(house,infantry,7,0,vehicle,0x34,
   taskarbiter::Owner::AutoLoad,proof,receiptID);
}
static loading::Observation loadingObservation(uint32_t p){return {get<int>(p+0xAC),get<int>(p+0xB4),get<uint32_t>(p+0x2B4),get<uint32_t>(p+0x5A4)};}
// Explicitly distinguish a configured exclusion from an unreadable type name
// or an in-flight Host configuration publication.
static int loadingFilterStatus(featuretypes::Feature feature,uint32_t p,int kind){
 if(!refreshFeatureTypes())return -1;
 if(!featureConfig.filters[feature].custom)return 1;
 uint32_t type=0;char id[24]{};
 if(!read(p+(kind==1?0x6C0u:0x6C4u),&type,sizeof type))return -1;
 if(type<0x10000)return 0;
 if(!read(type+0x24,id,23))return -1;
 return featuretypes::allowed(featureConfig,feature,id)?1:0;
}
static loading::ReservationView loadingReservationView(const loading::Reservation&r,uint32_t house,int frame){
 return loading::inspectReservation(r,house,frame,
  [](uint32_t at,void*out,size_t n){return read(at,out,n);},
  [](uint32_t p){return worldObjects.contains(p);},
  [](uint32_t p,int kind,bool transport){return loadingFilterStatus(transport?featuretypes::LoadTransport:featuretypes::LoadPassenger,p,kind);});
}
extern "C" int GuardedEnterAction(unsigned,unsigned,unsigned,unsigned*);
static bool loadQueryFailed;
// Called only inside an already validated, non-pumping Runtime query batch.
// Native failures still abort the batch; final submission uses canEnter below.
static bool queryEnterAction(uint32_t p,uint32_t target){
 if(p==target||loadQueryFailed)return false;
 FaultPhase phase(runtimefault::LoadLegality,p,target);unsigned action=0;
 if(!GuardedEnterAction(get<uint32_t>(get<uint32_t>(p)+0x74),p,target,&action)){loadQueryFailed=true;return false;}
 return action==3;
}
static bool canEnter(uint32_t p,uint32_t target){return gameentry::active(gameentry::Client::Assist)&&queryEnterAction(p,target);}
static int passengerSlots(uint32_t p){int k=kind(p);uint32_t type=typeOf(p,k);return type?loading::slots(get<double>(type+0x380)):0;}
static void finishLoadingReservation(const loading::Reservation&r,uint32_t session,int frame,bool boarded){
 (void)frame; // Age alone is not a terminal reservation state.
 if(!r.ticket)return;gateway::Receipt outcome{};
 gateway::Intent intent{session,0,r.id,r.vehicleID,0};
 gateway::planned(outcome,r.ticket,intent);outcome.owner=taskarbiter::Owner::AutoLoad;outcome.queuedAt=r.queuedAt;
 if(boarded){outcome.phase=gateway::Phase::Completed;outcome.observedAt=sampleClock(sh);}
 else{outcome.phase=gateway::Phase::Cancelled;outcome.failure=gateway::Failure::Superseded;}
 // Business terminal supersedes the generic task-change observer for this
 // specific send; otherwise a late generic Expired would contradict Completed.
 missionObserver.dismissTicket(session,r.ticket);
 recordReceipt(outcome);actorLeaseArbiter.release(session,r.id,r.ticket);
}
static bool loadCalculationPending;static uint32_t loadTaskSeen;
static bool loadCaptureWaiting;
static uint32_t loadPickupOptions,loadPickupAt;
static bool loadVehicleOnly;
struct LoadSelectedIdentity {uint32_t pointer,id,token;};
static LoadSelectedIdentity loadSelection[4096],loadCarrierSelection[loading::MaxVehicles];
static int loadSelectionCount,loadCarrierSelectionCount=-1;
static uint32_t loadPickupTokens[loading::MaxVehicles];
static int loadPickupCount;
struct LoadSelectionFact {loading::Infantry unit;uint32_t token=0;bool acceptable=false,candidate=false,waiting=false;};
static LoadSelectionFact loadSelectionFacts[4096];
static int loadFactCount;
static compute::LoadInput loadSample;
static int loadAcceptRoom[loading::MaxVehicles];
static compute::FrameHistory<compute::LoadObservation> loadFrames;static compute::LoadObservation loadObservation;static compute::LoadResult loadResultCopy;static compute::LoadInput loadCaptureCohort;
static int autoLoad(uint32_t house,const compute::LoadInput*cohort=nullptr,bool candidateOnly=false,uint32_t pickup=0);
struct PickupWalk {
 uint32_t actor,method=0;int fromX,fromY,speed=0;
 PickupWalk(uint32_t p,int x,int y):actor(p),fromX(x),fromY(y){
  if(!loading::coordinate(x)||!loading::coordinate(y)||!gameentry::active(gameentry::Client::Assist))return;
  const uint32_t query=get<uint32_t>(get<uint32_t>(actor)+0x1AC);
  if(!nativeExecutableRange(query,1)||!nativeExecutableRange(0x578460,1)||!nativeExecutableRange(0x5F5F00,1))return;
  method=query;speed=call<int>(0x5F5F00,actor);
 }
 uint32_t operator()(int x,int y)const{
  const uint32_t cell=method?readMapCell(x,y):0;
  if(!cell)return 0;
  vanilla::Cell xy{short(x),short(y)};
  if(!call<bool>(0x578460,0x87F7E8,&xy,1))return 0;
  const int direction=reversedodge::direction(x*256+128-fromX,y*256+128-fromY);
  return call<int>(method,actor,cell,direction,speed,0,1)==0?cell:0;
 }
};
static bool pickupMove(uint32_t house,uint32_t id,uint32_t actor,uint32_t token,uint32_t cell){
 if(!outgoingRoom()||!manualVersions.trackable(id))return false;
 const executionproof::Ticket proof{match,house,sampleClock(sh),{id,actor,token},{},{}};
 return submitProvenMission(house,id,2,0,cell,11,taskarbiter::Owner::AutoLoad,proof);
}
static void planPickupRallies(uint32_t house,const loading::Vehicle*vehicles,int prior){
 for(int j=0;j<loadPickupCount;++j){
  const auto&v=vehicles[j];int x[loading::MaxInfantry],y[loading::MaxInfantry],count=0;
  const uint32_t token=loadPickupTokens[j];
  if(worldByID(v.id)!=v.object||!loadingObject(v.object,house,2)||manualVersions.get(v.id)!=token)continue;
  for(int n=prior;n<loadLedger.count;++n){const auto&r=loadLedger.records[n];
   if(r.pickup&&r.vehicle==v.object){x[count]=r.x;y[count++]=r.y;}
  }
  if(!count)continue;
  const int vx=get<int>(v.object+0x9C),vy=get<int>(v.object+0xA0);
  const PickupWalk walk(v.object,vx,vy);
  const uint32_t meeting=loading::meetingCell(vx,vy,x,y,count,loading::moving(loadingObservation(v.object)),[&](int tx,int ty){
   const uint32_t key=uint32_t(ty*1000+tx);
   for(int n=0;n<loadLedger.count;++n)if(loadLedger.records[n].meeting==key)return false;
   return walk(tx,ty)!=0;
  },loadVehicleOnly);
  if(!meeting)continue;
  for(int n=prior;n<loadLedger.count;++n)if(loadLedger.records[n].pickup&&loadLedger.records[n].vehicle==v.object)
   loadLedger.records[n].meeting=meeting;
 }
}
static bool startPickupRally(uint32_t house,loading::Reservation&r,const loading::ReservationView&view,DWORD now){
 const uint32_t key=r.meeting;
 const uint32_t cell=PickupWalk(r.vehicle,view.vehicleX,view.vehicleY)(int(key%1000),int(key/1000));
 const bool sent=cell&&pickupMove(house,r.vehicleID,r.vehicle,r.vehicleManual,key);
 // The budget and start time apply to the carrier and its whole group,
 // including passengers visited in later frames or later in the ledger.
 for(int n=0;n<loadLedger.count;++n){auto&job=loadLedger.records[n];
  if(job.meeting!=key||job.vehicleID!=r.vehicleID)continue;
  if(sent){job.meetingAddress=cell;job.meetingAt=now;}
  else if(!cell)job.meeting=0;
  else job.retryAfterAt=now+loading::RetryBackoffMs;
 }
 return sent;
}
static bool pickupApproach(uint32_t house,loading::Reservation&r,DWORD now){
 if(!loading::coordinate(r.x)||!loading::coordinate(r.y))return false;
 const int cx=int(r.meeting%1000),cy=int(r.meeting/1000);
 const int dx=cx-r.x/256,dy=cy-r.y/256;
 if(std::max(std::abs(dx),std::abs(dy))<=2){r.approachAttempted=true;return false;}
 PickupWalk walk(r.object,r.x,r.y);
 struct Cell {int x,y,distance;};Cell cells[24];int count=0;
 for(int y=cy-2;y<=cy+2;++y)for(int x=cx-2;x<=cx+2;++x){
  if((x==cx&&y==cy)||x<=0||y<=0||x>=512||y>=512)continue;
  const int sx=x-r.x/256,sy=y-r.y/256;cells[count++]={x,y,sx*sx+sy*sy};
 }
 std::sort(cells,cells+count,[](const Cell&a,const Cell&b){
  if(a.distance!=b.distance)return a.distance<b.distance;
  return a.y!=b.y?a.y<b.y:a.x<b.x;
 });
 for(int i=0;i<count;++i){
  const auto&point=cells[i];const uint32_t key=uint32_t(point.y*1000+point.x),cell=readMapCell(point.x,point.y);
  if(!cell)continue;
  bool occupied=false;
  for(int n=0;n<loadLedger.count;++n)if(loadLedger.records[n].meeting==key||loadLedger.records[n].approachAddress==cell){occupied=true;break;}
  if(occupied||!walk(point.x,point.y))continue;
  if(!pickupMove(house,r.id,r.object,r.manualToken,key)){r.retryAfterAt=now+loading::RetryBackoffMs;return false;}
  r.approachAttempted=true;
  r.approachAddress=cell;return true;
 }
 r.approachAttempted=true;return false;
}
static bool pickupVehicleApproaching(const loading::Reservation&r,const loading::ReservationView&view,DWORD now){
 const int x=int(r.meeting%1000)*256+128,y=int(r.meeting/1000)*256+128;
 if(std::max(std::abs(int64_t(view.vehicleX)-x),std::abs(int64_t(view.vehicleY)-y))<=256)return false;
 if(!loading::elapsed(now,r.meetingAt,loading::PickupStartMs)||view.transport.queued==2)return true;
 return view.transport.mission==2&&get<uint32_t>(r.vehicle+0x5A4)==r.meetingAddress;
}
static int submitLoading(uint32_t house,const loading::Infantry*soldiers,int ns,const loading::Vehicle*vehicles,int nv,const int*assignment,const uint32_t*tokens,bool candidateOnly=false){
 using namespace loading;int frame=get<int>(0xA8ED84);loadQueryFailed=false;
 faultContext.phase=runtimefault::LoadSubmit;
 int room[MaxVehicles]{};bool pickupBusy[MaxVehicles]{};
 for(int j=0;j<nv;++j){const auto&v=vehicles[j];if(worldByID(v.id)!=v.object||!loadingObject(v.object,house,2))continue;
  uint32_t type=typeOf(v.object,2),q=get<uint32_t>(v.object+0x118);room[j]=type?get<int>(type+0x5E0):0;int steps=0;
  while(q>=0x10000&&steps++<256){int size=passengerSlots(q);if(!size){room[j]=0;break;}room[j]-=size;uint32_t next=get<uint32_t>(q+0x30);if(next==q){room[j]=0;break;}q=next;}if(q>=0x10000)room[j]=0;
 }
 const auto reserveIncoming=[&]()->bool{
  Vec all{};if(!sampleRoster(0xA8EC78,all))return false;
  for(int n=0;n<all.count;++n){uint32_t other=item(all,n);int k=other?kind(other):-1;if((k!=1&&k!=2)||!own(other,house))continue;uint32_t target=incomingVehicle(other);if(!target)continue;
   for(int j=0;j<nv;++j)if(vehicles[j].object==target){int size=passengerSlots(other);room[j]-=size?size:1024;pickupBusy[j]=true;break;}
  }
  return true;
 };
 if(runtimeDirectActions&&!candidateOnly){if(!reserveIncoming())return -30;}
 else {SampleReadScope frameRead(candidateOnly);if(!frameRead||!reserveIncoming()||!frameRead.complete())return -30;}
 for(int n=0;n<loadLedger.count;++n){auto&r=loadLedger.records[n];if(!worldObjects.contains(r.object)||incomingVehicle(r.object))continue;for(int j=0;j<nv;++j)if(r.vehicleID==vehicles[j].id){room[j]-=r.slots;pickupBusy[j]=true;break;}}
 // Outstanding cooperative Enter packets reserve seats before native fields
 // reflect their destination. Do not let AutoLoad overbook the same carrier.
 for(int j=0;j<nv;++j)if(room[j]>0){
  const int pending=cooperativePendingSlots(vehicles[j].id,vehicles[j].object);
  room[j]-=pending;pickupBusy[j]=pickupBusy[j]||pending>0;
 }
 const int prior=loadLedger.count;
 for(int i=0;i<ns;++i){const auto&p=soldiers[i];int j=assignment[i];
  if(j<0||j>=nv){if(p.slots)++snap.loadUnassigned;continue;}auto&v=vehicles[j];
  if(room[j]<p.slots||loadLedger.count>=MaxInfantry||worldByID(p.id)!=p.object||worldByID(v.id)!=v.object||manualVersions.get(p.id)!=tokens[i]||
     !loadingObject(p.object,house,p.kind)||!loadingObject(v.object,house,2)||incomingVehicle(p.object)||loadLedger.contains(p.id)||cooperativePendingActor(p.id)){
   ++snap.loadUnassigned;continue;
  }
  const bool pickup=!candidateOnly&&j<loadPickupCount&&!pickupBusy[j]&&p.kind==1&&
   manualVersions.get(v.id)==loadPickupTokens[j]&&manualVersions.trackable(v.id)&&manualVersions.trackable(p.id);
  const auto initial=loadingObservation(p.object);
  if(loadVehicleOnly&&(!pickup||!idleForPickup(initial))){++snap.loadUnassigned;continue;}
  const bool canBoard=canEnter(p.object,v.object);
  if(loadQueryFailed)return -15;
  if(!canBoard&&!(pickup&&moving(loadingObservation(v.object)))){++snap.loadUnassigned;continue;}
  room[j]-=p.slots;
  // Saved source/target identities come from the frozen LoadInput cohort.
  // Never upgrade a delayed worker assignment to permission to Enter.
  const executionproof::Ticket proof{match,house,sampleClock(sh),
   {p.id,p.object,tokens[i]}, {},{v.id,v.object}};
  cancelUnloadPassenger(p.object);uint32_t ticket=0;
  // A delayed observation can plan and reserve an assignment. The retry
  // service proves up-to-date shared seat capacity before emitting Enter.
  bool sent=!pickup&&!candidateOnly&&outgoingRoom()>0&&enterEvent(house,p.id,v.id,proof,&ticket);
  const DWORD submittedAt=sampleClock(sh);
  Reservation job{p.object,p.id,v.object,v.id,p.slots,frame};job.lastSent=sent?frame:-1;job.lastAttempt=frame;job.attempts=sent?0:1;
  job.lastSentAt=sent?submittedAt:0;job.progressAt=submittedAt;job.retryAfterAt=submittedAt+(sent?loading::RetryIntervalMs:loading::FirstRetryMs);
  job.x=p.x;job.y=p.y;job.progress=frame;job.initial=initial;job.kind=p.kind;job.manualToken=tokens[i];job.ticket=ticket;job.queuedAt=sent?submittedAt:0;
  if(pickup){job.pickup=true;job.stationaryPickup=loadVehicleOnly;job.pickupAt=submittedAt;job.vehicleManual=loadPickupTokens[j];}
  loadLedger.add(job);
  if(sent)++snap.loadIssued;else ++snap.loadWaiting;
 }
 if(loadPickupCount&&!candidateOnly)planPickupRallies(house,vehicles,prior);
 return snap.loadWaiting?2:1;
}
static void loadingCalculationTick(uint32_t house,DWORD now,bool candidateOnly=false){
 if(!loadCalculationPending)return;
 auto finish=[&](){loadCalculationPending=false;loadCaptureWaiting=false;};
 // If no full sample ever arrives, the saved click can finish through
 // ordinary candidate boarding without authorizing a carrier Move.
 if(loadPickupOptions&&loading::elapsed(now,loadPickupAt,3500))loadPickupOptions=0;
 const uint32_t completed=sh->loadComputed;
 if(compute::newer(completed,loadTaskSeen)){
  long seq=sh->loadResultSeq;if(!(seq&1)){MemoryBarrier();loadResultCopy=sh->loadResult;MemoryBarrier();
   if(seq==sh->loadResultSeq&&completed==sh->loadComputed){const auto*source=loadFrames.find(completed);const auto&r=loadResultCopy;
    if(source&&compute::valid(*source)&&source->mode==int(sh->loadMode==1)&&source->radius==std::clamp(sh->loadRadius,1,512)&&r.count==source->ns&&r.match==source->match&&r.tick==source->tick&&r.frame==source->frame&&
       freshness::valid(source->match,match,source->tick,source->frame,now,get<int>(0xA8ED84),freshness::Cooperative)&&!get<bool>(0xA8BC60+25)){
     const int result=submitLoading(house,source->soldiers,source->ns,source->vehicles,source->nv,r.assignment,source->tokens,candidateOnly);
     // A partial capacity sample leaves the same assignment and click pending.
     if(result==-30)return;
     loadTaskSeen=completed;finish();return;
    }
    loadTaskSeen=completed;
   }
  }
 }
 if((externalInputFrame||candidateOnly)&&!get<bool>(0xA8BC60+25)){compute::copyInput(loadCaptureCohort,sh->loadInput);int result=autoLoad(house,&loadCaptureCohort,candidateOnly);if(result!=-30&&result!=-6)finish();}
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
 const auto collect=[&]()->bool{
 // Keep the original click cohort,
 // match exact UIDs in the frozen pages, and revalidate live UID/manual
 // versions before any native Enter action. No sampled command authority.
 for(int n=0;n<loadSelectionCount;++n)if(selected[n]&&get<uint32_t>(selected[n]+0x10)!=loadSelection[n].id)return false;
 for(int n=0;n<loadCarrierSelectionCount;++n)if(carriers[n]&&get<uint32_t>(carriers[n]+0x10)!=loadCarrierSelection[n].id)return false;
 auto&in=loadSample;in.match=match;in.tick=sampleClock(sh);in.frame=frame;in.ns=in.nv=0;in.mode=sh->loadMode==1;in.radius=std::clamp(sh->loadRadius,1,512);in.composition.clear();
 loadPickupCount=loadFactCount=0;loadVehicleOnly=false;
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;
 bool selectedCarrier=loadCarrierSelectionCount>0;
 for(int n=0;n<loadSelectionCount;++n){uint32_t p=selected[n];
  if(p&&own(p,house)&&kind(p)==2&&featureObjectAllowed(featuretypes::LoadTransport,p)){uint32_t t=typeOf(p,2);if(t&&get<int>(t+0x5E0)>0){selectedCarrier=true;break;}}
 }
 const auto addCarrier=[&](uint32_t p,uint32_t token){
  if(!p||in.nv>=MaxVehicles||!loadingObject(p,house,2)||incomingVehicle(p))return;uint32_t t=typeOf(p,2);if(!t||!featureTypeAllowed(featuretypes::LoadTransport,t))return;
  int capacity=get<int>(t+0x5E0),count=get<int>(p+0x114);double limit=get<double>(t+0x388);if(capacity<=0||capacity>1024||count<0||count>256||!(limit>0&&limit<=128))return;
  for(auto&number:in.composition.numbers[in.nv])number=0;
  int used=0;uint32_t q=get<uint32_t>(p+0x118),seen[256]{};int steps=0;
  while(q>=0x10000&&steps<256){bool cycle=false;for(int j=0;j<steps;++j)if(seen[j]==q)cycle=true;if(cycle){used=capacity;break;}seen[steps++]=q;
   int size=passengerSlots(q);if(!size){used=capacity;break;}used+=size;in.composition.add(in.nv,typeOf(q,kind(q)));q=get<uint32_t>(q+0x30);
  }
  if(used>capacity||steps!=count)return;
  uint32_t id=get<uint32_t>(p+0x10);if(!id)return;
  for(int j=0;j<in.nv;++j)if(in.vehicles[j].id==id)return;
  const int index=in.nv++;in.vehicles[index]={p,id,get<int>(p+0x9C),get<int>(p+0xA0),capacity,used,0,limit,count};loadAcceptRoom[index]=freeSlots(in.vehicles[index]);
  loadPickupTokens[index]=token;
 };
 if(loadCarrierSelectionCount>=0){for(int n=0;n<loadCarrierSelectionCount&&in.nv<MaxVehicles;++n)addCarrier(carriers[n],0);}
 else{
  if(selectedCarrier)for(int n=0;n<loadSelectionCount&&in.nv<MaxVehicles;++n)addCarrier(selected[n],loadSelection[n].token);
  bool selectedInfantry=false,onlyVehicles=loadSelectionCount!=0;
  if(loadPickupOptions&&in.nv)for(int n=0;n<loadSelectionCount;++n){
   const int k=selected[n]?kind(selected[n]):-1;
   selectedInfantry|=k==1;onlyVehicles&=k==2;
  }
  loadVehicleOnly=(loadPickupOptions&AutoBoardPickupVehiclesOnly)&&onlyVehicles&&in.nv>0;
  const bool paired=(loadPickupOptions&AutoBoardPickupSelected)&&selectedInfantry;
  if(paired||loadVehicleOnly)loadPickupCount=in.nv;
  if(!selectedCarrier||paired)for(int n=0;n<all.count&&in.nv<MaxVehicles;++n)addCarrier(item(all,n),0);
 }
 const auto passengerFact=[&](uint32_t p,int k,uint32_t token,LoadSelectionFact&fact){
  fact={};uint32_t t=typeOf(p,k);if(!t||!featureTypeAllowed(featuretypes::LoadPassenger,t))return;
  fact.unit={p,get<uint32_t>(p+0x10),get<int>(p+0x9C),get<int>(p+0xA0),slots(get<double>(t+0x380)),t,k};fact.acceptable=fact.unit.slots>0;fact.token=token;
  if(get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D))return;
  fact.candidate=true;fact.waiting=incomingVehicle(p)||loadLedger.contains(fact.unit.id)||cooperativePendingActor(fact.unit.id);
 };
 for(int n=0;!loadVehicleOnly&&n<loadSelectionCount;++n){
  const uint32_t p=selected[n];const int k=p?kind(p):-1;
  if((k==1||k==2)&&loadingObject(p,house,k))passengerFact(p,k,loadSelection[n].token,loadSelectionFacts[loadFactCount++]);
 }
 // The original incoming roster is shared with every other collector; this
 // capacity observation is independent of the later live submit revalidation.
 for(int n=0;n<all.count;++n){uint32_t p=item(all,n);int k=p?kind(p):-1;if((k!=1&&k!=2)||!loadingObject(p,house,k))continue;uint32_t target=incomingVehicle(p);
  if(loadVehicleOnly&&k==1&&!target&&loadFactCount<MaxInfantry&&idleForPickup(loadingObservation(p))){
   LoadSelectionFact fact{};const uint32_t uid=get<uint32_t>(p+0x10);
   passengerFact(p,1,manualVersions.get(uid),fact);
   if(uid&&fact.candidate&&fact.acceptable&&!fact.waiting){
    const int j=nearest(fact.unit,in.vehicles,in.nv,[&](const Infantry&actor,const Vehicle&v){
     const int64_t dx=int64_t(actor.x)-v.x,dy=int64_t(actor.y)-v.y,limit=int64_t(in.radius)*256;
     return dx*dx+dy*dy<=limit*limit;
    });
    if(j>=0)loadSelectionFacts[loadFactCount++]=fact;
   }
  }
  if(!target)continue;
  for(int j=0;j<in.nv;++j)if(in.vehicles[j].object==target){int size=passengerSlots(p);in.vehicles[j].reserved+=size?size:in.vehicles[j].capacity;++in.vehicles[j].passengers;in.composition.add(j,typeOf(p,k));break;}
 }
 for(int n=0;n<loadLedger.count;++n){const auto&r=loadLedger.records[n];
  // An invalid retained pointer cannot be dereferenced on the native lane.
  if(runtimeDirectActions&&worldObjects.byId(r.id)!=r.object)continue;
  if(incomingVehicle(r.object))continue;
  for(int j=0;j<in.nv;++j)if(in.vehicles[j].id==r.vehicleID){in.vehicles[j].reserved+=r.slots;++in.vehicles[j].passengers;in.composition.add(j,typeOf(r.object,r.kind));break;}
 }
 for(int j=0;j<in.nv;++j){int pending=cooperativePendingSlots(in.vehicles[j].id,in.vehicles[j].object);
  if(pending>0){in.vehicles[j].reserved+=pending;++in.vehicles[j].passengers;}
 }
 return true;
 };
 if(runtimeDirectActions)return collect();
 ObservationReadScope frameRead;return frameRead&&collect()&&frameRead.complete();
}
static int autoLoad(uint32_t house,const compute::LoadInput*cohort,bool candidateOnly,uint32_t pickup){
 using namespace loading;
 FaultPhase phase(runtimefault::LoadCollect);loadQueryFailed=false;
 if(!loadInterfaces())return -12;const int frame=get<int>(0xA8ED84);
 // The native input cohort can be sampled again after an incomplete world.
 // Only a complete live reservation observation is allowed to retire a ticket.
 for(int n=0;!candidateOnly&&n<loadLedger.count;){auto&r=loadLedger.records[n];
  if(r.pickup&&manualVersions.get(r.id)!=r.manualToken){finishLoadingReservation(r,match,frame,false);loadLedger.erase(n);continue;}
  const auto view=loadingReservationView(r,house,frame);
  if(view.state==loading::ReservationState::Unknown){++n;continue;}
  const bool boarded=view.state==loading::ReservationState::Boarded;
  const bool remove=view.state==loading::ReservationState::Invalid||boarded||
    (r.pickup&&loading::pickupCancelled(r,manualVersions.get(r.vehicleID),sampleClock(sh)))||
    (view.state==loading::ReservationState::Active&&takenOver(r,view.mission,view.incoming));
  if(remove){finishLoadingReservation(r,match,frame,boarded);loadLedger.erase(n);}else ++n;
 }
 // A new physical press can replace an unresolved selection. Already queued
 // actors keep their own original seat reservation and manual version.
 if(!cohort){loadFrames.clear();loadTaskSeen=sh->loadComputed;loadCalculationPending=loadCaptureWaiting=false;
  loadPickupOptions=pickup&(AutoBoardPickupSelected|AutoBoardPickupVehiclesOnly);loadPickupAt=sampleClock(sh);
  loadPickupCount=0;loadVehicleOnly=false;
  snap.loadUnits=snap.loadVehicles=snap.loadIssued=snap.loadWaiting=snap.loadUnassigned=0;
 }
 if(!loadCaptureWaiting&&!captureLoadSelection(cohort))return -4;
 if(!loadSelectionCount)return 1;
 // Freeze the actual press even when neither strict nor observation pages
 // have arrived. The load job outlives its receipt in the command mailbox.
 if(candidateOnly&&!cohort){loadCaptureWaiting=true;loadCalculationPending=true;return -30;}
 if(!collectLoadFacts(house,frame)){loadCaptureWaiting=true;loadCalculationPending=true;return -30;}
 if(candidateOnly&&loadPickupCount){loadCaptureWaiting=true;loadCalculationPending=true;return -30;}
 loadCaptureWaiting=false;auto&in=loadSample;
 snap.loadUnits=snap.loadIssued=snap.loadWaiting=snap.loadUnassigned=0;snap.loadVehicles=in.nv;
 // Engine legality is its own phase. No SampleReadScope survives into a
 // GuardedEnterAction call, assignment, reservation, or outgoing event.
 if(!gameentry::active(gameentry::Client::Assist))return -12;
 bool accepts[MaxVehicles]{};
 for(int j=0;!loadPickupCount&&j<in.nv;++j)if(loadAcceptRoom[j]>0&&worldByID(in.vehicles[j].id)==in.vehicles[j].object)
  for(int n=0;n<loadFactCount;++n){const auto&fact=loadSelectionFacts[n];const auto&p=fact.unit;
   if(fact.acceptable&&p.object!=in.vehicles[j].object&&p.slots<=loadAcceptRoom[j]&&worldByID(p.id)==p.object&&queryEnterAction(p.object,in.vehicles[j].object)){accepts[j]=true;break;}
  }
 if(loadQueryFailed)return -15;
 for(int n=0;n<loadFactCount;++n){const auto&fact=loadSelectionFacts[n];const auto&p=fact.unit;if(!fact.candidate)continue;
  bool carrier=false;for(int j=0;j<in.nv;++j)if((accepts[j]||j<loadPickupCount)&&in.vehicles[j].object==p.object)carrier=true;if(carrier)continue;
  ++snap.loadUnits;if(!p.id)continue;if(fact.waiting){++snap.loadWaiting;continue;}
  if(!p.slots||in.ns>=MaxInfantry){++snap.loadUnassigned;continue;}
  bool duplicate=false;for(int j=0;j<in.ns;++j)if(in.soldiers[j].id==p.id)duplicate=true;if(duplicate)continue;
  in.tokens[in.ns]=fact.token;in.soldiers[in.ns++]=p;
 }
 if(!in.nv){snap.loadUnassigned+=in.ns;return 1;}
 faultContext.phase=runtimefault::LoadAssign;
 bool pickupMoving[MaxVehicles]{};
 for(int j=0;j<loadPickupCount;++j){const auto&v=in.vehicles[j];
  pickupMoving[j]=!v.reserved&&worldByID(v.id)==v.object&&manualVersions.trackable(v.id)&&moving(loadingObservation(v.object));
 }
 const auto compatible=[&](const Infantry&p,const Vehicle&v){
  const int j=int(&v-in.vehicles);
  int64_t dx=int64_t(p.x)-v.x,dy=int64_t(p.y)-v.y,limit=int64_t(in.radius)*256;
  const bool withinRange=coordinate(p.x)&&coordinate(p.y)&&coordinate(v.x)&&coordinate(v.y)&&dx*dx+dy*dy<=limit*limit;
  return p.slots<=freeSlots(v)&&p.slots<=v.sizeLimit&&withinRange&&worldByID(p.id)==p.object&&worldByID(v.id)==v.object&&
   (queryEnterAction(p.object,v.object)||(pickupMoving[j]&&p.kind==1&&manualVersions.trackable(p.id)));
 };
 static int assignment[MaxInfantry];static Workspace workspace;
 if(loadPickupCount){
  // Probe the selected carriers first. Overflow alone needs to query
  // unselected vehicles for legality.
  assign(in.soldiers,in.ns,in.vehicles,in.nv,in.mode,in.radius,in.composition,workspace,assignment,compatible,loadPickupCount,loadVehicleOnly);
  if(loadQueryFailed)return -15;
 }else{
  for(int n=0;n<in.ns;++n)for(int j=0;j<in.nv;++j){
   in.compatible[n][j]=compatible(in.soldiers[n],in.vehicles[j]);if(loadQueryFailed)return -15;
  }
  if(!compute::inside(sh->computeApplied,compute::Load)){
   compute::copyInput(sh->loadInput,in);loadCalculationPending=true;
   const uint32_t serial=compute::nextTask(sh->loadTask);compute::observe(loadObservation,in);loadFrames.retain(serial,loadObservation);MemoryBarrier();sh->loadTask=serial;notifyHost();return -30;
  }
  assign(in.soldiers,in.ns,in.vehicles,in.nv,in.mode,in.radius,in.composition,workspace,assignment,[&](const Infantry&p,const Vehicle&v){return in.compatible[&p-in.soldiers][&v-in.vehicles]!=0;});
 }
 const int result=submitLoading(house,in.soldiers,in.ns,in.vehicles,in.nv,assignment,in.tokens,candidateOnly);
 if(result==-30){loadCaptureWaiting=true;loadCalculationPending=true;}
 return result;
}

static void cancelLoadingJobs(uint32_t p){
 for(int i=0;i<loadLedger.count;){auto &r=loadLedger.records[i];
  if(r.object==p||r.vehicle==p){finishLoadingReservation(r,match,get<int>(0xA8ED84),false);loadLedger.erase(i);}
  else ++i;
 }
}
static void autoLoadTick(uint32_t house,int frame,bool candidateOnly=false){
 FaultPhase phase(runtimefault::LoadRetry);loadQueryFailed=false;const DWORD now=sampleClock(sh);
 static int processed[2]={-1,-1};static uint32_t processedMatch[2];
 const int lane=candidateOnly?1:0;
 if(frame==processed[lane]&&match==processedMatch[lane])return;
 processed[lane]=frame;processedMatch[lane]=match;
 if(!loadLedger.count)return;
 if(!loadInterfaces()){
  // Missing gateway evidence is UNKNOWN. Keep same selected UID/manual token.
  snap.loadWaiting=loadLedger.count;return;
 }
 int budget=outgoingRoom()?recurringBudget(EventLane::Boarding):0;
 bool candidateProbed=false;
 static int candidateCursor=0;
 int i=candidateOnly?candidateCursor:0;
 int budgeted=candidateOnly?std::min(loadLedger.count,4):loadLedger.count;
 while(budgeted--&&loadLedger.count){
  if(i>=loadLedger.count)i=0;auto&r=loadLedger.records[i];
  // A player order cancels the original AutoLoad permission.
  if(manualVersions.get(r.id)!=r.manualToken){finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);continue;}
  if(candidateOnly&&(!worldObjects.contains(r.object)||!worldObjects.contains(r.vehicle))){++i;continue;}
  const auto view=loadingReservationView(r,house,frame);
  if(view.state==loading::ReservationState::Unknown){++i;continue;}
  const bool boarded=view.state==loading::ReservationState::Boarded;
  if(boarded||view.state==loading::ReservationState::Invalid||
     (r.pickup&&loading::pickupCancelled(r,manualVersions.get(r.vehicleID),now))||
     (r.stationaryPickup&&r.lastSent<0&&!view.incoming&&!loading::idleForPickup(view.mission))||
     loading::takenOver(r,view.mission,view.incoming)){
   finishLoadingReservation(r,match,frame,boarded);loadLedger.erase(i);continue;
  }
  const uint32_t incoming=view.incoming;
  if(view.x!=r.x||view.y!=r.y){r.x=view.x;r.y=view.y;r.progress=frame;r.progressAt=now;}
  bool due=loading::retryDue(now,r.lastSent<0,r.lastSentAt,r.progressAt,r.retryAfterAt,incoming!=0);
  if(due&&budget>0&&outgoingRoom()>0&&loadingObject(r.object,house,r.kind)&&loadingObject(r.vehicle,house,2)){
   if(r.meeting&&r.lastSent<0&&!incoming){
    if(candidateOnly){++i;continue;}
    if(!r.meetingAddress){
     if(startPickupRally(house,r,view,now)){--budget;++i;continue;}
     if(r.meeting){++i;continue;}
    }
    if(r.meeting&&pickupVehicleApproaching(r,view,now)){
     if(!r.stationaryPickup&&!r.approachAttempted&&pickupApproach(house,r,now)){--budget;r.retryAfterAt=now+loading::FirstRetryMs;}
     ++i;continue;
    }
   }
   if(r.stationaryPickup&&r.lastSent<0&&!incoming&&!loading::pickupNearby(r.x,r.y,view.vehicleX,view.vehicleY)){
    if(!r.meeting){++snap.loadUnassigned;loadLedger.erase(i);continue;}
    ++i;continue;
   }
   if(candidateOnly){
    candidateProbed=true;
    if(!candidateLoadAdmission(r.object,r.vehicle,r.id,house,r.slots)){++i;break;}
   }
   if(canEnter(r.object,r.vehicle)){
    r.lastAttempt=frame;
    uint32_t ticket=0;
    // Reissue the same job under its original manual token.
    const executionproof::Ticket proof{match,house,0,
       {r.id,r.object,r.manualToken}, {},{r.vehicleID,r.vehicle}};
    if(enterEvent(house,r.id,r.vehicleID,proof,&ticket)){
     if(r.lastSent<0)++snap.loadIssued;
     finishLoadingReservation(r,match,frame,false);
     r.ticket=ticket;r.queuedAt=now;r.lastSent=frame;r.lastSentAt=now;r.progress=frame;r.progressAt=now;r.retryAfterAt=now+loading::RetryIntervalMs;r.attempts=0;--budget;
    }else{
     if(r.attempts<10)++r.attempts;
     r.retryAfterAt=now+loading::RetryBackoffMs*uint32_t(r.attempts);
    }
   }else if(r.pickup)r.retryAfterAt=now+loading::RetryBackoffMs;
  }
  if(loadQueryFailed){
   // No retry loop on a throwing predicate. Retire the affected reservation;
   // any previously accepted original event remains owned by the game.
   finishLoadingReservation(r,match,frame,false);loadLedger.erase(i);loadQueryFailed=false;
   if(candidateProbed)break;
   continue;
  }
  ++i;
  if(candidateProbed)break;
 }
 if(candidateOnly)candidateCursor=i;
 snap.loadWaiting=loadLedger.count;
}

static void resetLoadCalculation(){
 loadCalculationPending=loadCaptureWaiting=loadVehicleOnly=false;loadPickupOptions=0;
 loadSelectionCount=loadFactCount=loadPickupCount=0;loadCarrierSelectionCount=-1;
}
