#include "tactical_frame_read.h"
// Included by native_orders_bridge.h, under the sole Runtime dispatcher.
static unloadcompute::Input unloadLocalInput,unloadExternalInput;
static unloadcompute::Result unloadResultCopy;
static compute::FrameHistory<unloadcompute::Input> unloadFrames;static uint32_t unloadApplied=0;
static bool unloadCalculationPending;
static uint32_t unloadCalculationSerial;
static void resetUnloadCalculation(){unloadCalculationPending=false;unloadFrames.clear();unloadApplied=sh?sh->unloadComputed:0;unloadExternalInput.count=0;}
static unloadcompute::View unloadView(const UnloadPassenger&r,int frame,bool query=true){
 unloadcompute::View v{};const uint32_t p=r.p,t=typeOf(p,1);
 v.frame=frame;v.mode=r.mode;v.landedFrame=r.landedFrame;
 v.mission=get<int>(p+0xAC);v.queued=get<int>(p+0xB4);v.sequence=get<int>(p+0x6C4);
 v.x=get<int>(p+0x9C);v.y=get<int>(p+0xA0);
 v.deployer=get<bool>(t+0xEC8);v.engineer=get<bool>(t+0xEC3);v.spy=get<bool>(t+0xEBE);v.c4=get<bool>(t+0xEC2);
 if(v.deployer&&query)v.ready=call<bool>(get<uint32_t>(get<uint32_t>(p)+0x314),p);
 return v;
}
static bool unloadSourceCurrent(const UnloadPassenger&r,const unloadcompute::Source&s,uint32_t house,int frame){
 if(r.job!=s.job||r.id!=s.id||r.p!=s.pointer||r.cancelled||
    !worldObjects.contains(r.p)||manualVersions.get(r.id)!=s.token||
    !(sh->flags&(r.mode==0?SmartUnloadOrders:EjectAssist))||
    memcmp(static_cast<const unloadcompute::State*>(&r),&s.state,sizeof(s.state)))return false;
 // A second live read may fail even after a complete sealed world capture.
 // Do not build a current mission=0 / queued=0 from unchecked get<T>().
 unloadfollowup::Observation native{};
 if(unloadfollowup::observe(r.p,r.id,house,
     [](uint32_t at,void*out,size_t n){return read(at,out,n);},native)!=unloadfollowup::Knowledge::Current||
    !native.live()||native.carrier||native.type!=s.type)return false;
 if(!refreshFeatureTypes())return false;
 const auto passengerFeature=r.mode==0?featuretypes::UnloadPassenger:featuretypes::EscapePassenger;
 const auto transportFeature=r.mode==0?featuretypes::UnloadTransport:featuretypes::EscapeTransport;
 if(featureConfig.filters[passengerFeature].custom){
  char typeName[24]{};
  if(!read(native.type+0x24,typeName,23))return false;
 }
 if(featureConfig.filters[transportFeature].custom){
  char typeName[24]{};
  if(r.carrierType<0x10000||!read(r.carrierType+0x24,typeName,23))return false;
 }
 if(!featureTypeAllowed(passengerFeature,native.type)||
    !featureTypeAllowed(transportFeature,r.carrierType))return false;
 int sequence=0;uint8_t c4=0;
 if(!read(r.p+0x6C4,&sequence,sizeof sequence)||
    !read(native.type+0xEC2,&c4,sizeof c4))return false;
 unloadcompute::View current{};
 current.frame=s.view.frame;current.mode=r.mode;current.landedFrame=r.landedFrame;
 current.mission=native.mission;current.queued=native.queued;current.sequence=sequence;
 current.x=native.x;current.y=native.y;
 current.deployer=native.deployer;current.engineer=native.engineer;current.spy=native.spy;current.c4=c4;
 if(current.deployer){
  uint32_t vt=0,method=0;
  if(!read(r.p,&vt,sizeof vt)||vt<0x10000||!read(vt+0x314,&method,sizeof method))return false;
  current.ready=call<bool>(method,r.p);
 }
 current.queryFailed=s.view.queryFailed;
 // Native Move may change a specialist's coordinates between capture and
 // admission; not mission, queued status, animation, identity or target.
 if(!current.deployer&&(current.engineer||current.spy)&&current.mission==2&&s.view.mission==2){
  current.x=s.view.x;current.y=s.view.y;
 }
 return !memcmp(&current,&s.view,sizeof current);
}
static uint8_t unloadTargetAction(uint32_t p,uint32_t t,const unloadcompute::View&v,uint32_t q,uint32_t house,planner::Actor&source,bool&queryFailed){
 if(!q||!live(q)||get<uint32_t>(q+0x11C)||get<int>(q+0x220))return 0;
 if((v.engineer||v.spy||v.c4)&&kind(q)==3&&unloadBuildingCandidate(q,house,v.engineer!=0)){
  unsigned action=0;uint32_t query=get<uint32_t>(get<uint32_t>(p)+0x74);
  FaultPhase phase(runtimefault::LoadLegality,p,q);
  if(query<0x10000||!GuardedEnterAction(query,p,q,&action)){queryFailed=true;return 0;}
  int mission=nativeorders::specialMission(v.engineer!=0,v.spy!=0,v.c4!=0,int(action));
  if(mission>=0)return uint8_t((mission+1)|64);
 }
 if(v.engineer||v.spy||!nativeEnemy(q,house))return 0;
 int k=kind(q);uint32_t targetType=typeOf(q,k);if(!targetType)return 0;
 planner::Actor target{};rawCombatInfo(q,targetType,k,target,false);
 return planner::compatibleTarget(source,target,false)?2:0;
}
static unloadcompute::Target unloadSampleTargets[unloadcompute::MaxTargets];
static int unloadSampleTargetCount,unloadSampleTargetFrame=-1;static uint32_t unloadSampleTargetMatch,unloadSampleTargetHouse;
static bool collectUnloadTargets(uint32_t house,int frame){
 if(unloadSampleTargetFrame==frame&&unloadSampleTargetMatch==match&&unloadSampleTargetHouse==house)return true;
 static unloadcompute::Target next[unloadcompute::MaxTargets];int count=0;
 const auto collect=[&]()->bool{
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;
 for(int n=0;n<all.count&&count<unloadcompute::MaxTargets;++n){uint32_t p=item(all,n);
  if(!p||!live(p)||get<uint32_t>(p+0x21C)==house)continue;
  next[count++]={get<uint32_t>(p+0x10),p,get<int>(p+0x9C),get<int>(p+0xA0)};
 }
 return true;
 };
 if(runtimeDirectActions){if(!collect())return false;}
 else {ObservationReadScope frameRead;if(!frameRead||!collect()||!frameRead.complete())return false;}
 memcpy(unloadSampleTargets,next,size_t(count)*sizeof(next[0]));unloadSampleTargetCount=count;
 unloadSampleTargetMatch=match;unloadSampleTargetHouse=house;unloadSampleTargetFrame=frame;return true;
}
static bool collectUnloadSource(unloadcompute::Input&input,const UnloadPassenger&r,uint32_t house,int frame,bool deployerOnly=false){
 if(input.count<0||input.count>=unloadcompute::MaxSources)return false;
 unloadcompute::Source next{};static uint8_t actions[unloadcompute::MaxTargets],queries[unloadcompute::MaxTargets];
 memset(actions,0,sizeof actions);memset(queries,0,sizeof queries);bool targets=false;
 const auto collect=[&]()->bool{
  if(runtimeDirectActions&&worldObjects.byId(r.id)!=r.p)return false;
  if(get<uint32_t>(r.p+0x10)!=r.id||kind(r.p)!=1)return false;
  next.job=r.job;next.id=r.id;next.pointer=r.p;next.type=typeOf(r.p,1);next.token=manualVersions.get(r.id);
  next.state=static_cast<const unloadcompute::State&>(r);next.view=unloadView(r,frame,false);
  if(deployerOnly&&!next.view.deployer)return false;
  if(!next.view.deployer&&frame>=r.sentFrame){
   if(!collectUnloadTargets(house,frame))return false;targets=true;
   planner::Actor source{};rawCombatInfo(r.p,next.type,1,source,true);
   for(int n=0;n<unloadSampleTargetCount;++n){const uint32_t p=unloadSampleTargets[n].pointer;const auto&v=next.view;
    if(!p||(runtimeDirectActions&&worldObjects.byId(unloadSampleTargets[n].id)!=p)||!live(p)||get<uint32_t>(p+0x11C)||get<int>(p+0x220))continue;
    queries[n]=(v.engineer||v.spy||v.c4)&&kind(p)==3&&unloadBuildingCandidate(p,house,v.engineer!=0);
    if(v.engineer||v.spy||!nativeEnemy(p,house))continue;
    const int k=kind(p);const uint32_t type=typeOf(p,k);if(!type)continue;
    planner::Actor target{};rawCombatInfo(p,type,k,target,false);if(planner::compatibleTarget(source,target,false))actions[n]=2;
   }
  }
  return true;
 };
 if(runtimeDirectActions){if(!collect())return false;}
 else {ObservationReadScope frameRead;if(!frameRead||!collect()||!frameRead.complete())return false;}
 // Native queries follow candidate discovery. In particular, neither the deployer virtual
 // call nor GuardedEnterAction may execute under the sealed read adapter.
 if(worldByID(next.id)!=next.pointer||manualVersions.get(next.id)!=next.token||kind(next.pointer)!=1||typeOf(next.pointer,1)!=next.type)return false;
 if(next.view.deployer)next.view.ready=call<bool>(get<uint32_t>(get<uint32_t>(r.p)+0x314),r.p);
 if(targets)for(int n=0;n<unloadSampleTargetCount;++n)if(queries[n]){const auto&target=unloadSampleTargets[n];
  if(worldByID(target.id)!=target.pointer){actions[n]=0;continue;}
  unsigned action=0;uint32_t query=get<uint32_t>(get<uint32_t>(r.p)+0x74);FaultPhase phase(runtimefault::LoadLegality,r.p,target.pointer);
  if(query<0x10000||!GuardedEnterAction(query,r.p,target.pointer,&action)){next.view.queryFailed=1;break;}
  const int mission=nativeorders::specialMission(next.view.engineer!=0,next.view.spy!=0,next.view.c4!=0,int(action));
  if(mission>=0)actions[n]=uint8_t((mission+1)|64);
 }
 const int index=input.count++;input.sources[index]=next;memcpy(input.actions[index],actions,sizeof actions);
 if(targets){input.targetCount=unloadSampleTargetCount;memcpy(input.targets,unloadSampleTargets,size_t(input.targetCount)*sizeof(input.targets[0]));}
 return true;
}

static bool applyUnloadDecision(UnloadPassenger&r,const unloadcompute::Input&input,int index,const unloadcompute::Decision&decision,uint32_t house,int frame,int&budget,bool external){
 const auto&s=input.sources[index];
 // Both compute locations now consume capture-time facts; current admission
 // must check the source even when the pure planner ran inside Runtime.
 if(!unloadSourceCurrent(r,s,house,frame))return false;
 auto&state=static_cast<unloadcompute::State&>(r);state=decision.observed;
 if(decision.done)return true;
 if(decision.mission<0||budget<=0||outgoingRoom()<=0)return false;
 // Runtime alone revalidates the chosen target and creates the event.
 uint32_t capturedTargetPointer=0;
 if(decision.target){int targetIndex=-1;
  for(int i=0;i<input.targetCount;++i)if(input.targets[i].id==decision.target){targetIndex=i;break;}
  if(targetIndex<0)return false;const auto&q=input.targets[targetIndex];
  if(worldByID(q.id)!=q.pointer||get<uint32_t>(q.pointer+0x10)!=q.id)return false;
  capturedTargetPointer=q.pointer;
  bool failed=false;planner::Actor source{};rawCombatInfo(r.p,s.type,1,source,true);
  uint8_t action=unloadTargetAction(r.p,s.type,s.view,q.pointer,house,source,failed);
  if(failed||!action||int(action&63)-1!=decision.mission)return false;
 }else if(decision.mission!=0&&decision.mission!=5&&decision.mission!=16)return false;
 if(decision.mission==16&&(!s.view.deployer||!call<bool>(get<uint32_t>(get<uint32_t>(r.p)+0x314),r.p)))return false;
 uint32_t ticket=0;const bool attack=decision.mission==1;
 // Capture-time source token and chosen target UID/pointer are independently
 // rechecked at the common native game-thread event submission gateway.
 const executionproof::Ticket proof{input.match,house,input.tick,
    {s.id,s.pointer,s.token},
    attack?executionproof::Identity{decision.target,capturedTargetPointer}:executionproof::Identity{},
    !attack?executionproof::Identity{decision.target,capturedTargetPointer}:executionproof::Identity{}};
 if(!submitProvenMission(house,r.id,decision.mission,attack?decision.target:0,
    attack?0:decision.target,!attack&&decision.target?0x34:0,
    taskarbiter::Owner::Unload,proof,&ticket))return false;
 --budget;state=decision.accepted;
 if(decision.mission!=5){state.sentFrame=frame;if(decision.mission==16||decision.target)++snap.unloadIssued;}
 if(decision.follow){
  finishUnloadFollowup(r,frame);r.followTarget=decision.target;r.followTargetPointer=worldByID(decision.target);
  r.followTicket=ticket;r.followQueuedAt=sampleClock(sh);r.followObserved=false;missionObserver.dismissTicket(match,ticket);
 }
 return decision.doneAfter!=0;
}
static void serviceUnloadCalculation(uint32_t house,int frame,int&budget){
 const uint32_t completed=sh->unloadComputed;if(!compute::newer(completed,unloadApplied))return;
 long before=sh->unloadResultSeq;if(before&1)return;MemoryBarrier();memcpy(&unloadResultCopy,&sh->unloadResult,sizeof(unloadResultCopy));MemoryBarrier();
 if(before!=sh->unloadResultSeq||completed!=sh->unloadComputed)return;unloadApplied=completed;
 const auto*source=unloadFrames.find(completed);if(!source)return;const auto&input=*source;const auto&result=unloadResultCopy;
 if(!freshness::valid(input.match,match,input.tick,input.frame,sampleClock(sh),frame,freshness::Tactical)||result.match!=input.match||result.tick!=input.tick||result.frame!=input.frame||result.count!=input.count)return;
 for(int n=0;n<result.count;++n){const auto&s=input.sources[n];
  for(int i=0;i<unloadCount;++i)if(unloadPassengers[i].job==s.job){auto&r=unloadPassengers[i];
   if(!compute::inside(sh->computeApplied,r.mode==0?compute::Unload:compute::Escape)&&
      applyUnloadDecision(r,input,n,result.decisions[n],house,frame,budget,true)){
    finishUnloadFollowup(r,frame);r=unloadPassengers[--unloadCount];
   }break;
  }
 }
}
static void publishUnloadCalculation(){
 if(!externalInputFrame||!unloadExternalInput.count)return;
 InterlockedIncrement(&sh->unloadInputSeq);MemoryBarrier();
 compute::copyInput(sh->unloadInput,unloadExternalInput);
 unloadCalculationSerial=compute::nextTask(sh->unloadTask);unloadFrames.retain(unloadCalculationSerial,unloadExternalInput);sh->unloadTask=unloadCalculationSerial;
 MemoryBarrier();InterlockedIncrement(&sh->unloadInputSeq);unloadCalculationPending=true;notifyHost();
}
