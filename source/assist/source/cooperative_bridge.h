// Local mouse/UI entry points only. Received network events, simulation AI,
// weapon selectors, ownership, rules and missions are never patched.
#include "../../external_reader/source/game_view.h"
#include "business_effects.h"
#include "../../external_reader/source/cooperative_reader.h"
#include "aircraft_selection_bridge.h"
#include "entry_cohort.h"
struct LocalRawRead {bool operator()(uint32_t a,void*out,size_t n)const{if(a<0x10000||uint64_t(a)+n>=0x80000000ull)return false;memcpy(out,(const void*)(uintptr_t)a,n);return true;}};
static external::GameView<LocalRawRead> clickView(LocalRawRead{});
// UI click capture keeps its independent live input generation. Tactical
// conversion consumes only the manager's already sealed world sample.
struct CooperativeFrameRead {bool operator()(uint32_t a,void*out,size_t n)const{return read(a,out,n);}};
static bool collectCooperativeInput(const cooperative::Request&r,const compute::QuotaConfig& quotas,cooperative::Input&out,worldframe::Key*source=nullptr,cooperative::CapturedIdentities*identities=nullptr){
 ObservationReadScope frameRead;if(!frameRead)return false;
 auto input=external::cooperativeInput(CooperativeFrameRead{},r,[&](uint32_t type){
  char id[24]{};if(!read(type+0x24,id,23))return quotas.defaults;
  for(int i=0;i<quotas.count;++i)if(sameid(id,quotas.entries[i].id))return quotas.entries[i].quota;return quotas.defaults;
 },identities);
 const auto feature=r.mode==cooperative::Enter?featuretypes::EnterSource:featuretypes::Aircraft;
 for(size_t i=0;i<input.units.size();++i)for(size_t j=0;j<input.targets.size();++j)
  if(!featureTypeAllowed(feature,input.units[i].type)||(r.mode==cooperative::Enter&&!featureTypeAllowed(featuretypes::EnterTarget,input.targets[j].type)))input.compatible[i*input.targets.size()+j]=0;
 if(!frameRead.complete())return false;
 if(source)*source=sampleWorldSource->key;
 out=std::move(input);return true;
}
static uint32_t clickSerial,cooperativePlanSeen;static int cooperativeAt;
static cooperative::CaptureHistory cooperativeCaptures;
static uint64_t cooperativeCandidateAt=0;
static constexpr uint64_t CooperativeCandidateIntervalUs=4000;
static uint32_t singleDamageSerial=0,singleDamageMatch=0;static double singleDamageCommitted=0;
struct EnterReservation {uint32_t id,target,type,token;DWORD queuedAt;int slots;};
static EnterReservation entryReservations[cooperative::MaxSources];static int entryReservationCount;
struct CooperativeEffect {uint32_t actor=0,target=0,ticket=0,match=0,actorPointer=0,targetPointer=0,beforeTarget=0,queuedAt=0;int frame=0,mission=0,rearmStart=-1;bool acquired=false;};
static CooperativeEffect cooperativeEffects[cooperative::MaxSources];static int cooperativeEffectCount;
static void observeCooperativeEffects(uint32_t house,int frame,DWORD now){
 for(int i=0;i<cooperativeEffectCount;){const auto e=cooperativeEffects[i];
  uint32_t actor=worldByID(e.actor),target=worldByID(e.target);
  bool identity=e.match==match&&frame>=e.frame&&actor&&actor==e.actorPointer&&get<uint32_t>(actor+0x21C)==house;
  bool active=identity&&live(actor); // aircraft can temporarily leave the map
  bool targetSame=target&&target==e.targetPointer; // no target-ID reuse false positive
  bool boarded=identity&&(e.mission==7||e.mission==2)&&targetSame&&clickView.entered(actor,target);
  bool targeting=active&&e.mission==1&&frame>e.frame&&targetSame&&get<uint32_t>(actor+0x2B4)==target;
  // An already-selected target is not fresh acquisition, but a NEW rearm can
  // still demonstrate that the repeated Attack caused another firing cycle.
  if(targeting&&e.beforeTarget!=target)cooperativeEffects[i].acquired=true;
  int rearm=active?get<int>(actor+0x2EC):-1; // verified raw combat field, read-only
  auto air=effects::aircraft(active,targeting,e.rearmStart,rearm,e.frame,frame);
  bool shot=e.mission==1&&air==effects::Aircraft::ShotObserved;
  // Temporarily absent aircraft are not immediate failure. A reused pointer is.
  bool expired=e.match!=match||(actor&&actor!=e.actorPointer)||(target&&target!=e.targetPointer)||frame<e.frame||DWORD(now-e.queuedAt)>3000;
  if(boarded||shot||expired){
   if(e.ticket){gateway::Receipt receipt{};gateway::Intent intent{e.match,e.queuedAt,e.actor,e.target,0};
    gateway::planned(receipt,e.ticket,intent);receipt.owner=taskarbiter::Owner::Cooperative;
    receipt.queuedAt=e.queuedAt;
    // Shot evidence = changed rearm after target acquisition, not proven damage.
    receipt.phase=(boarded||shot)?gateway::Phase::Completed:
       cooperativeEffects[i].acquired?gateway::Phase::Observed:gateway::Phase::Expired;
    receipt.failure=(expired&&!boarded&&!shot&&!cooperativeEffects[i].acquired)?gateway::Failure::Stale:gateway::Failure::None;
    receipt.observedAt=receipt.phase!=gateway::Phase::Expired?sampleClock(sh):0;
    missionObserver.dismissTicket(e.match,e.ticket);recordReceipt(receipt);
    actorLeaseArbiter.release(e.match,e.actor,e.ticket);
   }
   cooperativeEffects[i]=cooperativeEffects[--cooperativeEffectCount];
  }else ++i;
 }
}
// Only cancel older watch records AFTER the replacement event was enqueued.
// Never cancel on a failed queue attempt; that would lose a valid task.
static void supersedeCooperativeEffect(uint32_t actor,uint32_t session){
 for(int i=0;i<cooperativeEffectCount;){auto e=cooperativeEffects[i];
  if(e.actor==actor&&e.match==session){
   gateway::Receipt rec{};gateway::Intent intent{e.match,e.queuedAt,e.actor,e.target,0};
   gateway::planned(rec,e.ticket,intent);rec.owner=taskarbiter::Owner::Cooperative;
   rec.queuedAt=e.queuedAt;gateway::cancelled(rec,gateway::Failure::Superseded);
   missionObserver.dismissTicket(e.match,e.ticket);recordReceipt(rec);++snap.receiptCancelled;
   actorLeaseArbiter.release(e.match,e.actor,e.ticket);
   cooperativeEffects[i]=cooperativeEffects[--cooperativeEffectCount];
  }else ++i;
 }
}
static bool airUnit(uint32_t p){int k=kind(p);return featureObjectAllowed(featuretypes::Aircraft,p)&&(k==0||(k==2&&(get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D))));}
static bool cooperativeSingleInput(uint32_t bindings){return sh&&(sh->flags&AircraftSpread)&&sh->aircraftAllocationPolicy==2&&(bindings&(pointerinput::AirFirst|pointerinput::AirSecond));}
// Called only AFTER an accepted single-target Attack. Keep the just-created
// Cooperative token/lease/receipt; cancelNative/cancelOreJob would invalidate
// that new ownership or locally release Sticky. Unsent aircraft never enter.
static void finishSingleCooperative(uint32_t p){
 uint32_t id=get<uint32_t>(p+0x10);pauseGlobalMagnetron(p);pauseMissileForManual(id,p);cancelBurst(id);cancelDodge(id);cancelLoadingJobs(p);
 int i=orderIndex(p);if(i>=0)removeOrder(i);
 for(i=0;i<unloadCount;)if(unloadPassengers[i].p==p||unloadPassengers[i].carrier==p){finishUnloadFollowup(unloadPassengers[i],get<int>(0xA8ED84));unloadPassengers[i]=unloadPassengers[--unloadCount];}else ++i;
}
static bool vehicleSellCell(uint32_t p){
 uint32_t cell=readMapCell(get<int>(p+0x9C)/256,get<int>(p+0xA0)/256);if(!cell)return false;
 for(uint32_t q=get<uint32_t>(cell+0xE4),n=0;q>=0x10000&&n<256;++n){if(!worldObjects.contains(q))return false;if(kind(q)==3)return true;uint32_t next=get<uint32_t>(q+0x30);if(next==q)return false;q=next;}return false;
}
static bool sellAllowed(uint32_t p){
 if(!nativeReady()||!p||!live(p))return false;
 int k=kind(p);uint32_t owner=get<uint32_t>(p+0x21C);bool drained=get<uint32_t>(p+0x1D0)!=0;
 if(owner!=lastHouse)return enemyBuildingSellAllowed(p);
 if(drained&&!(sh->flags&SellDrained))return false;
 if(k==2)return featureObjectAllowed(featuretypes::SellVehicle,p)&&owner>=0x10000&&(sh->flags&SellVehicles)&&vehicleSellCell(p);
 if(k!=3)return false;
 return drained&&(sh->flags&SellDrained);
}
static bool newCooperative(uint32_t p,uint32_t target,int mode){
 auto&r=sh->cooperativeRequest;int frame=get<int>(0xA8ED84);DWORD now=sampleClock(sh);uint32_t id=get<uint32_t>(p+0x10),targetID=get<uint32_t>(target+0x10);
 const uint32_t inputRequest=sh->pointer.request;
 if(!id||!targetID||!inputRequest||inputRequest==sh->pointer.ack)return false;
 const bool samePointer=r.serial&&r.pointerRequest==inputRequest;
 const bool same=samePointer&&cooperative::sameInputRequest(r,inputRequest,match,targetID,mode,sh->aircraftDamageRadiusCells,sh->aircraftDamageCoveragePercent)&&r.count>=0&&r.count<=cooperative::MaxSources;
 // One physical input cannot silently change target/mode while its selected
 // sources are retried on later simulation frames.
 if(samePointer&&!same)return false;
 if(get<int>(0xA8ECB8+16)>cooperative::MaxSources){
  if(!samePointer){InterlockedIncrement(&sh->cooperativeSeq);memset(&r,0,sizeof r);r.serial=++clickSerial;if(!r.serial)r.serial=++clickSerial;r.match=match;r.target=targetID;r.mode=mode;r.pointerRequest=inputRequest;r.tick=now;r.count=cooperative::MaxSources+1;MemoryBarrier();InterlockedIncrement(&sh->cooperativeSeq);notifyHost();}return false;
 }
 if(same)for(int i=0;i<r.count;++i)if(r.sources[i].id==id)return r.sources[i].token==manualVersions.get(id);
 if(!manualVersions.trackable(id))return false;
 if(mode!=cooperative::AirSingle)cancelNative(p);InterlockedIncrement(&sh->cooperativeSeq);
 if(!same){memset(&r,0,sizeof r);r.serial=++clickSerial;if(!r.serial)r.serial=++clickSerial;r.match=match;r.target=targetID;r.type=typeOf(target,kind(target));r.mode=mode;r.pointerRequest=inputRequest;r.tick=now;
  r.centerObject=target;
  if(mode==cooperative::AirDamage){r.airRadiusCells=sh->aircraftDamageRadiusCells;r.airDamagePercent=sh->aircraftDamageCoveragePercent;}
  else {r.airRadiusCells=0;r.airDamagePercent=0;}
  if(cooperative::damageMode(mode)){r.cameraX=get<int>(target+0x9C);r.cameraY=get<int>(target+0xA0);}
  else {uint32_t tactical=get<uint32_t>(0x887324);if(tactical>=0x10000){r.cameraX=get<int>(tactical+0xB0);r.cameraY=get<int>(tactical+0xB4);}}
  int width=get<int>(0xB0CE30),height=get<int>(0xB0CE34);r.viewWidth=width>0&&width<=32768?width:get<int>(0x8A00A4);r.viewHeight=height>0&&height<=32768?height:get<int>(0x8A00A8);
 }
 r.sources[r.count++]={id,manualVersions.get(id)};MemoryBarrier();InterlockedIncrement(&sh->cooperativeSeq);notifyHost();return true;
}
static int cooperativeInput(uint32_t p,uint32_t target,uint32_t bindings,bool candidateEntry=false){
 if(!nativeReady()||!p||!target||!own(p,lastHouse)||!get<bool>(p+0x83)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C))return 0;
 if(((bindings&pointerinput::Shift)&&!(bindings&(pointerinput::Enter|pointerinput::AirFirst|pointerinput::AirSecond)))||get<bool>(0x87F7E8+0x11B3)||get<bool>(0x87F7E8+0x11B1))return 0;
 if(indexedFrame!=get<int>(0xA8ED84)||(!commandRosterReady()&&(!candidateEntry||!candidateRosterReady())))return 0;
 if(!worldObjects.contains(p)||!worldObjects.contains(target)||!live(target))return 0;
 if((sh->flags&AdvancedEnter)&&(bindings&pointerinput::Enter)&&featureObjectAllowed(featuretypes::EnterSource,p)&&featureObjectAllowed(featuretypes::EnterTarget,target)&&clickView.entryCompatible(p,target,lastHouse))return newCooperative(p,target,cooperative::Enter)?1:0;

 if(candidateEntry)return 0;
 if((sh->flags&DiskPrimary)&&kind(p)==2&&kind(target)==3&&(bindings&pointerinput::Control)){
  uint32_t t=typeOf(p,2),secondary=t?get<uint32_t>(t+0x898+28):0;
  if(t&&featureTypeAllowed(featuretypes::Disk,t)&&(sameid((const char*)(t+0x24),"DISK")||(secondary>=0x10000&&get<bool>(secondary+0x142)))){
   int x=get<int>(target+0x9C)/256,y=get<int>(target+0xA0)/256;if(!readMapCell(x,y)||!loadInterfaces())return 0;
   uint32_t diskID=get<uint32_t>(p+0x10);if(!diskID)return 0;
   // Explicit input cancellation already released previous tool ownership.
   return nativeMission(lastHouse,diskID,1,uint32_t(y*1000+x),0,0,0,0,
                        taskarbiter::Owner::Cooperative,nullptr,11)?1:-1;
  }
 } if((sh->flags&AircraftSpread)&&airUnit(p)&&nativeEnemy(target,lastHouse)){
  int mode=(bindings&pointerinput::AirSecond)?cooperative::AirSecond:(bindings&pointerinput::AirFirst)?cooperative::AirFirst:0;
  if(mode){if(sh->aircraftAllocationPolicy==1)mode=cooperative::AirDamage;else if(sh->aircraftAllocationPolicy==2)mode=cooperative::AirSingle;return newCooperative(p,target,mode)?1:0;}
 }
 return 0;
}
static void cancelCooperativePlan(int result){
 if(!sh||sh->cooperativePlan==sh->cooperativeAck)return;
 // Counters belong to planSeen, never to a newer unconsumed mailbox serial.
 if(sh->cooperativePlan!=cooperativePlanSeen){sh->cooperativeAccepted=0;sh->cooperativeProcessed=0;memset(sh->cooperativeAcceptedBits,0,sizeof sh->cooperativeAcceptedBits);}
 int count=sh->cooperativeCount;if(count<0)count=0;if(count>cooperative::MaxSources)count=cooperative::MaxSources;
 sh->cooperativeRejected=count-sh->cooperativeAccepted;if(sh->cooperativeRejected<0)sh->cooperativeRejected=0;
 sh->cooperativeResult=result;MemoryBarrier();sh->cooperativeAck=sh->cooperativePlan;
}
static void resetRuntimeCooperative();
static void resetCooperativeState(){resetRuntimeCooperative();cooperativeCaptures={};cooperativeCandidateAt=0;singleDamageSerial=singleDamageMatch=0;singleDamageCommitted=0;entryReservationCount=cooperativeEffectCount=0;cooperativeAt=0;cooperativePlanSeen=0;}
static void restoreClickHooks(){}
static void clickHooksTick(uint32_t flags){sh->clickHookStatus=(flags&SpecialClickFlags)?1:0;}
// Cross-feature seat reservations are read-only and never grant permission
// to issue a native order: live UID, token and capacity are checked again.
static bool cooperativePendingActor(uint32_t id){
 for(int i=0;i<entryReservationCount;++i)
  if(entryReservations[i].id==id&&manualVersions.get(id)==entryReservations[i].token)return true;
 return false;
}
static int cooperativePendingSlots(uint32_t targetID,uint32_t targetPointer){
 if(!targetID||!targetPointer||worldByID(targetID)!=targetPointer)return 1024;
 int reserved=0;
 for(int i=0;i<entryReservationCount;++i){const auto&r=entryReservations[i];
  if(r.target!=targetID||r.slots<=0||manualVersions.get(r.id)!=r.token)continue;
  uint32_t actor=worldByID(r.id);
  if(actor&&(get<uint32_t>(actor+0x11C)==targetPointer||clickView.entryDestination(actor)==targetPointer))continue;
  reserved+=r.slots;if(reserved>=1024)return 1024;
 }
 return reserved;
}
static int entryReserved(uint32_t target,uint32_t ignore,uint32_t house,bool* evidenceReady=nullptr){
 // An incomplete frame is UNKNOWN, not a permanent capacity denial.
 if(evidenceReady)*evidenceReady=false;
 struct Candidate {uint32_t pointer,id;};static Candidate candidates[16384];
 int count=0;SampleReadScope frameRead;if(!frameRead||!commandRosterReady())return 1024;
 // Filter the canonical worker-owned Techno membership using the sealed
 // sample. Only relevant owned infantry/vehicles receive later live checks.
 for(const auto&member:worldFrameCommandRoster.members){
  const uint32_t p=member.pointer;
  const int k=kind(p);
  if((k!=1&&k!=2)||member.id==ignore||get<uint32_t>(p+0x21C)!=house)continue;
  if(count>=16384)return 1024;
  candidates[count++]={p,member.id};
 }
 if(!frameRead.complete())return 1024;
 frameRead.finish();
 if(evidenceReady)*evidenceReady=true;
 int reserved=0;const bool facility=clickView.capacity(target).singleVehicle;
 for(int i=0;i<count;++i){
  const auto&candidate=candidates[i];const uint32_t p=candidate.pointer;
  if(worldByID(candidate.id)!=p||!own(p,house)||candidate.id==ignore||
     clickView.entryDestination(p)!=target)continue;
  if(facility&&clickView.radioLinked(target,p))continue;
  int slots=clickView.entrySlots(p,target);if(!slots)return 1024;
  reserved+=slots;if(reserved>=1024)return 1024;
 }
 const uint32_t targetID=get<uint32_t>(target+0x10);
 for(int i=0;i<entryReservationCount;++i){auto&r=entryReservations[i];uint32_t p=worldByID(r.id);if(r.target==targetID&&r.id!=ignore&&p&&get<uint32_t>(p+0x11C)!=target&&clickView.entryDestination(p)!=target&&!(facility&&clickView.radioLinked(target,p)))reserved+=r.slots;}
 // AutoLoad's pending EventClass may not yet be represented by a native
 // destination. Avoid re-counting actors already in a cooperative reservation.
 for(int i=0;i<loadLedger.count;++i){const auto&r=loadLedger.records[i];
  if(r.id==ignore||r.vehicleID!=targetID||r.vehicle!=target||r.slots<=0||
     manualVersions.get(r.id)!=r.manualToken)continue;
  uint32_t p=worldByID(r.id);
  // A missing candidate index is UNKNOWN, not proof that the original
  // AutoLoad ticket vanished. Hold its seat until a valid identity returns.
  // A positively observed different pointer is a recycled UID and retires it.
  if(p&&p!=r.object)continue;
  if(p&&(get<uint32_t>(p+0x11C)==target||clickView.entryDestination(p)==target))continue;
  bool counted=false;
  for(int k=0;k<entryReservationCount;++k)if(entryReservations[k].id==r.id){counted=true;break;}
  if(!counted)reserved+=r.slots;
 }
 return std::min(reserved,1024);
}
// Cross-frame lookup can only supply identities. A small local boarding
// cohort proves current membership and incoming seats explicitly.
static bool candidateEntryCohortReady(){
 return eventdelivery::isSingleplayer(snap.session)&&candidateRosterReady()&&
  worldFrameCandidateRoster.count>0&&worldFrameCandidateRoster.count<=entrycohort::MaxMembers;
}
static bool candidateEntryCapacity(uint32_t target,uint32_t actor,uint32_t ignore,uint32_t house,
 external::EntryCapacity&capacity,int&slots,int&reserved){
 if(!candidateEntryCohortReady())return false;
 const auto&roster=worldFrameCandidateRoster;
 const uint64_t started=preciseclock::micros();
 unsigned reads=0;bool complete=true;
 auto withinBudget=[&](){return preciseclock::micros()-started<=750;};
 auto read=[&](uint32_t at,void*out,size_t n){
  if(!complete)return false;
  if(++reads>768||(reads%16==0&&!withinBudget())){complete=false;return false;}
  return complete=readLiveWorld(at,out,n);
 };
 entrycohort::Members members{};
 if(!entrycohort::verify(read,[&](uint32_t at,void*out,size_t n){return worldFrameCandidate.read(at,out,n);},
  [&](uint32_t id){return roster.objects->byId(id);},roster.items,roster.count,members))return false;
 external::GameView<decltype(read)> current(read);
 capacity=current.capacity(target);slots=current.entrySlots(actor,target);reserved=0;
 const uint32_t targetID=current.get<uint32_t>(target+0x10);
 auto represented=[&](uint32_t pointer){
  return pointer&&(current.get<uint32_t>(pointer+0x11C)==target||
                  current.entryDestination(pointer)==target||
                  (capacity.singleVehicle&&current.radioLinked(target,pointer)));
 };
 for(uint32_t pointer:members){
  if(!pointer||pointer==actor)continue;
  const int k=current.kind(pointer);
  if((k!=1&&k!=2)||current.get<uint32_t>(pointer+0x21C)!=house||
     current.entryDestination(pointer)!=target)continue;
  if(capacity.singleVehicle&&current.radioLinked(target,pointer))continue;
  const int seats=current.entrySlots(pointer,target);
  if(!seats)return false;
  reserved+=seats;
 }
 for(int i=0;i<entryReservationCount;++i){
  if((i&15)==0&&!withinBudget())return false;
  const auto&r=entryReservations[i];
  if(r.target==targetID&&r.id!=ignore&&manualVersions.get(r.id)==r.token&&
     !represented(roster.objects->byId(r.id)))reserved+=r.slots;
 }
 for(int i=0;i<loadLedger.count;++i){
  if((i&15)==0&&!withinBudget())return false;
  const auto&r=loadLedger.records[i];
  if(r.id==ignore||r.vehicleID!=targetID||r.vehicle!=target||r.slots<=0||
     manualVersions.get(r.id)!=r.manualToken)continue;
  const uint32_t pointer=roster.objects->byId(r.id);
  if((pointer&&pointer!=r.object)||represented(pointer))continue;
  bool shared=false;
  for(int j=0;j<entryReservationCount;++j){
   const auto&entry=entryReservations[j];
   if(entry.id==r.id&&entry.target==targetID&&entry.token==r.manualToken){shared=true;break;}
  }
  if(!shared)reserved+=r.slots;
 }
 return complete&&withinBudget()&&targetID&&current.get<int>(0xA8ED84,-1)==snap.frame;
}
static bool candidateLoadAdmission(uint32_t actor,uint32_t carrier,uint32_t id,uint32_t house,int slots){
 external::EntryCapacity capacity{};int currentSlots=0,reserved=0;
 return candidateEntryCapacity(carrier,actor,id,house,capacity,currentSlots,reserved)&&
  !capacity.singleVehicle&&slots>0&&slots==currentSlots&&capacity.maximum-capacity.used-reserved>=slots;
}
static int admitCooperative(const cooperative::Assignment&o,int mode,uint32_t house,int frame,DWORD now,
 const executionproof::Ticket*proof=nullptr,bool candidateEntry=false){
 const bool enter=mode==cooperative::Enter;
 if(!nativeReady()||get<bool>(0xA8BC60+25)||!(sh->flags&(enter?AdvancedEnter:AircraftSpread)))return -1;
 uint32_t p=worldByID(o.id),t=worldByID(o.target);
 if(proof){
  const auto&target=enter?proof->destination:proof->target;
  if(proof->match!=match||proof->house!=house||proof->actor.id!=o.id||
     proof->actor.manualVersion!=o.token||target.id!=o.target||
     manualVersions.get(o.id)!=o.token)return -1;
  uint32_t actorID=0,targetID=0;
  if(!readObjectIdentity(proof->actor.pointer,actorID)||!readObjectIdentity(target.pointer,targetID))return -2;
  if(actorID!=o.id||targetID!=o.target||(p&&p!=proof->actor.pointer)||(t&&t!=target.pointer))return -1;
  if(!p||!t)return -2; // Missing candidate membership is unknown.
 }
 bool valid=p&&t&&own(p,house)&&live(t)&&typeOf(t,kind(t))==o.type&&manualVersions.get(o.id)==o.token&&!get<bool>(p+0x1C8)&&!get<uint32_t>(p+0x11C);
  int slots=0,mission=enter?7:1;
  if(valid&&enter){
   external::EntryCapacity c{};int reserved=0;
   if(candidateEntry){
    if(!proof||!candidateEntryCapacity(t,p,o.id,house,c,slots,reserved))return -2;
   }else{
    bool capacityEvidence=false;c=clickView.capacity(t);slots=clickView.entrySlots(p,t);
    reserved=entryReserved(t,o.id,house,&capacityEvidence);
    if(!capacityEvidence)return -2;
   }
   valid=featureObjectAllowed(featuretypes::EnterSource,p)&&featureObjectAllowed(featuretypes::EnterTarget,t)&&clickView.entryCompatible(p,t,house);
   bool ownContact=c.singleVehicle&&clickView.entryDestination(p)==t&&clickView.radioLinked(t,p);
   valid=valid&&entryReservationCount<cooperative::MaxSources&&slots>0&&int64_t(c.used)-(ownContact?1:0)+reserved+slots<=c.maximum;
   if(valid){int action=call<int>(get<uint32_t>(get<uint32_t>(p)+0x74),p,t,true);bool bunker=kind(t)==3&&get<bool>(o.type+0x16AB);valid=action==3||action==11||(bunker&&(action==1||ownContact));
    // TankBunker hover returns Action::Move, but that is not the completed
    // boarding mission. Native EnterTankBunker (4E0045..4E0067) sets the
    // building destination, establishes radio and queues Mission::Enter=7.
    // SetDestination in the received Enter event owns that same handshake.
    if(!bunker&&action==11)mission=9;}
  }else if(valid){valid=airUnit(p)&&nativeEnemy(t,house);if(valid){planner::Actor source{},target{};rawCombatInfo(p,typeOf(p,kind(p)),kind(p),source,true);rawCombatInfo(t,o.type,kind(t),target,false);valid=planner::compatibleTarget(source,target,false);
    if(valid&&cooperative::damageMode(mode)){const auto&r=sh->cooperativeRequest;uint32_t center=worldByID(r.target);
     valid=cooperative::validDamage(o.expectedDamage)&&(mode!=cooperative::AirDamage||cooperative::validRange(r.airRadiusCells,r.airDamagePercent))&&center&&center==r.centerObject&&typeOf(center,kind(center))==r.type;
     if(valid&&mode==cooperative::AirDamage)valid=kind(t)==2&&!(target.flags&(planner::Airborne|planner::Manipulated))&&cooperative::distance(r.cameraX,r.cameraY,get<int>(t+0x9C),get<int>(t+0xA0))<=int64_t(cooperative::radiusCells(r.airRadiusCells)*256)*(cooperative::radiusCells(r.airRadiusCells)*256);
     if(valid&&mode==cooperative::AirSingle){
      if(singleDamageSerial!=r.serial||singleDamageMatch!=r.match){singleDamageSerial=r.serial;singleDamageMatch=r.match;singleDamageCommitted=0;}
      valid=o.target==r.target&&!stickyOwns(p)&&singleDamageCommitted*5<double(get<int>(t+0x6C))*6&&dispatchedAircraftSelectionReady(p);
     }
     if(valid){auto s=clickView.combat(p,frame,true),v=clickView.combat(t,frame,false);auto estimated=external::cooperativedamage::estimate(clickView,p,t,s,v);
      uint32_t selectedWeapon=0;int slot=-1,close=0,air=0;
      // The external selector is the existing supported policy. Confirm its
      // current raw slot/weapon immediately before issuing. The optional high-risk
      // mode queries the native selector instead. A changed weapon/ammo/modifier
      // fails closed; never budget a different live weapon as the old estimate.
      valid=cooperative::validDamage(estimated.damage)&&estimated.damage==o.expectedDamage&&combatWeaponQuery(p,t,&selectedWeapon,&slot,&close,&air)>0&&selectedWeapon==estimated.weapon&&slot==estimated.slot;
     }
    }
   }}
  if(!valid)return -1;

  uint32_t ticket=0;uint32_t beforeTarget=get<uint32_t>(p+0x2B4);
  int beforeRearm=get<int>(p+0x2EC);
  if(!nativeMission(house,o.id,mission,enter?0:o.target,enter?o.target:0,enter?0x34:0,0,0,taskarbiter::Owner::Cooperative,&ticket,-1,OrderAuthority::Local,false,mode==cooperative::AirSingle,false,proof))return 0;
  if(mode==cooperative::AirSingle){singleDamageCommitted+=o.expectedDamage;finishSingleCooperative(p);if(worldByID(o.id)==p&&get<uint32_t>(p+0x10)==o.id)deselectDispatchedAircraft(p);}
  // Preserve raw native event encoding, add only read-only effect observations.
  supersedeCooperativeEffect(o.id,match);
  if(ticket&&cooperativeEffectCount<cooperative::MaxSources&&(mission==1||mission==7||mission==2)){
   // One observer owns this ticket: avoid generic timeout/observed duplicates.
   missionObserver.dismissTicket(match,ticket);
   cooperativeEffects[cooperativeEffectCount++]={o.id,o.target,ticket,match,p,t,beforeTarget,now,frame,mission,beforeRearm,false};
  }
  if(enter)entryReservations[entryReservationCount++]={o.id,o.target,o.type,o.token,now,slots};
 consumePlanning(1);return 1;
}
static std::vector<cooperative::Assignment> runtimeCoopOrders,runtimeCoopAccepted,runtimeCoopProcessed;
static cooperative::Request runtimeCoopRequest;static size_t runtimeCoopAt;static DWORD runtimeCoopTick;static int runtimeCoopFrame;static bool runtimeCoopPending;
static void resetRuntimeCooperative(){runtimeCoopOrders.clear();runtimeCoopAccepted.clear();runtimeCoopProcessed.clear();runtimeCoopRequest={};runtimeCoopAt=0;runtimeCoopPending=false;}
static void runtimeCooperativeTick(uint32_t house,int frame,DWORD now){
 const auto&request=sh->cooperativeRequest;
 bool internal=compute::inside(sh->computeApplied,request.mode==cooperative::Enter?compute::Enter:compute::Aircraft);
 const bool newRequest=request.serial!=runtimeCoopRequest.serial;
 if(!internal||!cooperative::valid(request)||request.match!=match||(newRequest&&!freshness::intentAlive(request.tick,now))||get<bool>(0xA8BC60+25)){
  if(runtimeCoopPending)sh->runtimeCoopDone=runtimeCoopRequest.serial;resetRuntimeCooperative();return;
 }
 if(newRequest){resetRuntimeCooperative();runtimeCoopRequest=request;sh->runtimeCoopAccepted=sh->runtimeCoopRejected=0;runtimeCoopPending=true;}
 if(!runtimeCoopPending)return;
 bool recalc=runtimeCoopOrders.empty()||(cooperative::damageMode(request.mode)&&frame!=runtimeCoopFrame)||!freshness::valid(runtimeCoopRequest.match,match,runtimeCoopTick,runtimeCoopFrame,now,frame,freshness::Cooperative);
 if(recalc){
  auto remaining=runtimeCoopRequest;
  bool hasRemaining=cooperative::damageMode(request.mode)?cooperative::retrySourcePool(remaining,runtimeCoopRequest,runtimeCoopProcessed):runtimeCoopOrders.empty()||cooperative::retryRemaining(remaining,runtimeCoopOrders.data(),int(runtimeCoopOrders.size()),int(runtimeCoopAt));
  if(!hasRemaining){runtimeCoopPending=false;sh->runtimeCoopDone=request.serial;return;}
  static compute::QuotaConfig quotas;long seq=sh->quotaConfigSeq;if(seq&1)return;MemoryBarrier();memcpy(&quotas,&sh->quotaConfig,sizeof quotas);MemoryBarrier();if(seq!=sh->quotaConfigSeq||quotas.count<0||quotas.count>512)return;
  cooperative::Input input;if(!collectCooperativeInput(remaining,quotas,input))return;
  cooperative::reserveAccepted(input,runtimeCoopAccepted);
  runtimeCoopOrders=cooperative::allocate(input);runtimeCoopAt=0;runtimeCoopTick=now;runtimeCoopFrame=frame;
 }
 while(runtimeCoopAt<runtimeCoopOrders.size()&&planningRoom(1)>0&&outgoingRoom()>0){
  if(!freshness::valid(request.match,match,runtimeCoopTick,runtimeCoopFrame,sampleClock(sh),get<int>(0xA8ED84),freshness::Cooperative))return;
  const auto&o=runtimeCoopOrders[runtimeCoopAt];int accepted=admitCooperative(o,request.mode,house,frame,now);if(!accepted||accepted==-2)return;
  runtimeCoopProcessed.push_back(o);if(accepted>0){runtimeCoopAccepted.push_back(o);++sh->runtimeCoopAccepted;}else ++sh->runtimeCoopRejected;++runtimeCoopAt;
 }
 if(runtimeCoopAt>=runtimeCoopOrders.size()){
  cooperative::Request remaining;
  if(cooperative::damageMode(request.mode)&&!runtimeCoopOrders.empty()&&cooperative::retrySourcePool(remaining,runtimeCoopRequest,runtimeCoopProcessed)){runtimeCoopOrders.clear();runtimeCoopAt=0;return;}
  runtimeCoopPending=false;MemoryBarrier();sh->runtimeCoopDone=request.serial;notifyHost();
 }
}
// Only the common game-thread capture epoch produces host cooperative input.
static void captureCooperativeFrame(DWORD now,bool indexedWorkReady){
 // Never publish a partial prefix while the physical pointer request is still
 // being admitted/retried across simulation frames.
 if(sh->pointer.request!=sh->pointer.ack)return;
 const auto&r=sh->cooperativeRequest;auto&out=sh->cooperativeInputFrame;
 const bool captured=out.frame>=0&&out.request.serial==r.serial&&out.match==r.match;
 if(!cooperative::valid(r)||r.match!=match||r.pointerRequest!=sh->pointer.request||(!captured&&!freshness::intentAlive(r.tick,now))||snap.chat||compute::inside(sh->computeApplied,r.mode==cooperative::Enter?compute::Enter:compute::Aircraft)){out.frame=-1;return;}
 if(out.request.serial==r.serial&&sh->cooperativeFrameWanted!=r.serial)return;
 // A worker may finish between two callbacks in the same game frame. Use
 // its original stamp, and avoid rebuilding the graph when several game
 // frames reuse the same observation.
 ObservationReadScope discovery;if(!discovery)return;
 const auto key=sampleWorldSource->key;
 discovery.finish();
 if(!indexedWorkReady&&key.frame==snap.frame)return;
 static long lastQuotaRevision=-1;
 const long seq=sh->quotaConfigSeq;if(seq&1)return;
 if(out.request.serial==r.serial&&(key.frame<out.frame||
    (key.frame==out.frame&&key.tick==out.tick&&seq==lastQuotaRevision)))return;
 static compute::QuotaConfig quotas;
 MemoryBarrier();memcpy(&quotas,&sh->quotaConfig,sizeof quotas);MemoryBarrier();if(seq!=sh->quotaConfigSeq||quotas.count<0||quotas.count>512)return;
 cooperative::Input input;worldframe::Key source{};cooperative::CapturedIdentities identities;
 if(!collectCooperativeInput(r,quotas,input,&source,&identities)||
    !freshness::valid(source.match,match,source.tick,source.frame,now,snap.frame,freshness::Cooperative))return;
 // The Host distinguishes an observation-only plan by its earlier frame.
 // A candidate stamped with this frame must not look like exact authority.
 if(!indexedWorkReady&&source.frame==snap.frame)return;
 if(!cooperative::pack(out,input,r,source.tick,source.frame))return;
 lastQuotaRevision=seq;
 identities.stamp={r.serial,r.match,source.tick,source.frame};
 cooperativeCaptures.keep(std::move(identities));
}
static void serviceCooperativePlan(uint32_t house,int frame,DWORD now,bool candidateOnly,uint64_t preciseNow=0){
 // More than one small pass can run in a long simulation frame. Both the
 // shared event budget and native legality still use the actual game frame.
 if(candidateOnly&&((cooperativeCandidateAt&&preciseNow-cooperativeCandidateAt<CooperativeCandidateIntervalUs)||
                    sampleReadDepth||!candidateRosterReady()))return;
 uint32_t plan=sh->cooperativePlan;if(plan==sh->cooperativeAck)return;MemoryBarrier();
 if(candidateOnly){
  if(sh->cooperativeMode==cooperative::Enter&&!candidateEntryCohortReady())return;
  cooperativeCandidateAt=preciseNow;
 }
 if(plan!=cooperativePlanSeen){cooperativePlanSeen=plan;cooperativeAt=0;sh->cooperativeAccepted=sh->cooperativeRejected=0;sh->cooperativeProcessed=0;sh->cooperativeResult=1;memset(sh->cooperativeAcceptedBits,0,sizeof sh->cooperativeAcceptedBits);}
 if(sh->cooperativeCount<0||sh->cooperativeCount>cooperative::MaxSources){sh->cooperativeRejected=sh->cooperativeCount>0?cooperative::MaxSources:0;sh->cooperativeResult=-4;MemoryBarrier();sh->cooperativeAck=plan;notifyHost();return;}
 bool enter=sh->cooperativeMode==cooperative::Enter;
 auto fresh=[&](){return freshness::valid(sh->cooperativeMatch,match,sh->cooperativeTick,sh->cooperativeFrame,sampleClock(sh),get<int>(0xA8ED84),freshness::Cooperative);};
 auto expire=[&](){sh->cooperativeResult=-7;sh->cooperativeRejected+=sh->cooperativeCount-cooperativeAt;cooperativeAt=sh->cooperativeCount;};
 if(compute::inside(sh->computeApplied,enter?compute::Enter:compute::Aircraft)||!nativeReady()||sh->cooperativeSerial!=sh->cooperativeRequest.serial||sh->cooperativeMode!=sh->cooperativeRequest.mode||sh->cooperativeMode<cooperative::Enter||sh->cooperativeMode>cooperative::AirSingle||sh->cooperativeRequest.match!=match||sh->cooperativeRequest.pointerRequest!=sh->pointer.request||sh->pointer.request!=sh->pointer.ack||sh->cooperativeCount<0||sh->cooperativeCount>cooperative::MaxSources||!(sh->flags&(enter?AdvancedEnter:AircraftSpread))){sh->cooperativeResult=-4;sh->cooperativeRejected+=sh->cooperativeCount-cooperativeAt;cooperativeAt=sh->cooperativeCount;}
 if(cooperativeAt<sh->cooperativeCount&&!fresh())expire();
 const auto*identities=enter&&!candidateOnly?nullptr:cooperativeCaptures.find({sh->cooperativeSerial,sh->cooperativeMatch,sh->cooperativeTick,sh->cooperativeFrame});
 if(cooperativeAt<sh->cooperativeCount&&(!enter||candidateOnly)&&!identities)expire();
 int budget=candidateOnly?(enter?1:4):cooperative::MaxSources;
 while(cooperativeAt<sh->cooperativeCount&&budget>0&&planningRoom(1)>0&&outgoingRoom()>0){
  if(!fresh()){expire();break;}
  --budget;
  const auto&order=sh->cooperativeOrders[cooperativeAt];
  const auto proof=identities?(enter?identities->boarding(order.id,order.target):
    identities->attack(order.id,order.target)):executionproof::Ticket{};
  const int admitted=admitCooperative(order,sh->cooperativeMode,house,frame,now,identities?&proof:nullptr,enter&&candidateOnly);
  if(admitted==-2){
   // Preserve the original source and serial until fresh evidence arrives.
   // A game-frame lag is not proof that the entry target became illegal.
   break;
  }
  if(!admitted)break;
  if(admitted<0){++sh->cooperativeRejected;++cooperativeAt;sh->cooperativeProcessed=cooperativeAt;continue;}
  sh->cooperativeAcceptedBits[cooperativeAt/32]|=uint32_t(1)<<(cooperativeAt%32);++sh->cooperativeAccepted;++cooperativeAt;sh->cooperativeProcessed=cooperativeAt;
 }
 if(cooperativeAt>=sh->cooperativeCount){MemoryBarrier();sh->cooperativeAck=plan;notifyHost();}
}
static void cooperativeTick(uint32_t house,int frame,DWORD now){
 observeCooperativeEffects(house,frame,now);
 runtimeCooperativeTick(house,frame,now);
 for(int i=0;i<entryReservationCount;){auto&r=entryReservations[i];uint32_t p=worldByID(r.id),t=worldByID(r.target);
  if(!p||!t||!own(p,house)||manualVersions.get(r.id)!=r.token||
     get<uint32_t>(p+0x11C)||get<uint32_t>(p+0x2E4)||!live(p)){
   entryReservations[i]=entryReservations[--entryReservationCount];continue;
  }
  // A reservation remains in flight while native Enter/Radio is observable.
  // Missing evidence is not a cancellation. Revalidate both UIDs before
  // discarding it; do not rely solely on wall time or game-frame age.
  uint32_t uid=0,targetUID=0,destination=0,incoming=0;int mission=0,queued=0;
  bool observed=read(p+0x10,&uid,4)&&read(t+0x10,&targetUID,4)&&
   read(p+0x5A4,&destination,4)&&read(p+0x500,&incoming,4)&&
   read(p+0xAC,&mission,4)&&read(p+0xB4,&queued,4);
  if(!observed){++i;continue;}
  bool stillEntering=destination==t||incoming==t||mission==7||queued==7||mission==9||queued==9;
  if(uid!=r.id||targetUID!=r.target||(DWORD(now-r.queuedAt)>3000&&!stillEntering))
   entryReservations[i]=entryReservations[--entryReservationCount];
  else ++i;
 }
 serviceCooperativePlan(house,frame,now,false);
}

