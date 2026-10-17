// Runs on the game's message thread. Only the normal outgoing event queue
// receives writes: no direct passenger insertion or mission mutation.
static bool loadInterfaces(){
 static uint32_t checkedMatch;static DWORD checkedAt;static bool valid;
 DWORD now=sampleClock(sh);if(checkedMatch==match&&DWORD(now-checkedAt)<500)return valid;
 checkedMatch=match;checkedAt=now;valid=false;
 static constexpr Signature required[]={
  {0x4C6860,8,{0x8B,0xC1,0x8B,0x4C,0x24,0x04,0x85,0xC9}}
  // MouseOverObject is a YRpp virtual call. Do not demand the unused vanilla
  // Infantry implementation at 0x51E3B0 remain unhooked by Ares/MO.
 };
 for(auto&s:required){unsigned char b[12];if(!read(s.address,b,s.length)||memcmp(b,s.bytes,s.length))return false;}
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
static int autoLoad(uint32_t house){
 using namespace loading;
 FaultPhase phase(runtimefault::LoadCollect);loadQueryFailed=false;
 snap.loadUnits=snap.loadVehicles=snap.loadIssued=snap.loadWaiting=snap.loadUnassigned=0;
 Vec all=vec(0xA8EC78);if(!all.items)return -4;
 if(!loadInterfaces())return -12;int frame=get<int>(0xA8ED84);
 for(int i=0;i<loadLedger.count;){auto&r=loadLedger.records[i];
  bool remove=!worldObjects.contains(r.object)||!worldObjects.contains(r.vehicle)||!own(r.object,house)||!own(r.vehicle,house)||get<uint32_t>(r.object+0x10)!=r.id||get<uint32_t>(r.vehicle+0x10)!=r.vehicleID||get<uint32_t>(r.object+0x11C)!=0||int64_t(frame)-r.frame>900;
  if(!remove)remove=takenOver(r,loadingObservation(r.object),incomingVehicle(r.object));
  if(remove){
   bool boarded=worldObjects.contains(r.object)&&get<uint32_t>(r.object+0x10)==r.id&&
     get<uint32_t>(r.object+0x11C)==r.vehicle;
   finishLoadingReservation(r,match,frame,boarded);loadLedger.erase(i);
  }else ++i;
 }
 static Vehicle vehicles[MaxVehicles];static Infantry soldiers[MaxInfantry];static Composition composition;static Workspace workspace;composition.clear();
 int nv=0,ns=0;Vec selected=vec(0xA8ECB8,4096);
 bool selectedCarrier=false;
 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);
  if(p&&own(p,house)&&kind(p)==2){uint32_t t=typeOf(p,2);if(t&&get<int>(t+0x5E0)>0){selectedCarrier=true;break;}}
 }
 // Match MO's explicitly selected-carrier assignment when carriers were
 // selected. A passenger-only hotkey finds nearby owned carriers as requested.
 Vec carriers=selectedCarrier?selected:all;
 for(int i=0;i<carriers.count&&nv<MaxVehicles;++i){uint32_t p=item(carriers,i);if(!p||!loadingObject(p,house,2)||incomingVehicle(p))continue;uint32_t t=typeOf(p,2);if(!t)continue;
  int capacity=get<int>(t+0x5E0),count=get<int>(p+0x114);double limit=get<double>(t+0x388);if(capacity<=0||capacity>1024||count<0||count>256||!(limit>0&&limit<=128))continue;
  for(auto&number:composition.numbers[nv])number=0;
  int used=0;uint32_t q=get<uint32_t>(p+0x118),seen[256]{};int n=0;
  while(q>=0x10000&&n<256){bool cycle=false;for(int j=0;j<n;++j)if(seen[j]==q)cycle=true;if(cycle){used=capacity;break;}seen[n++]=q;
   int size=passengerSlots(q);if(!size){used=capacity;break;}used+=size;composition.add(nv,typeOf(q,kind(q)));q=get<uint32_t>(q+0x30);
  }
  if(used>capacity||n!=count)continue;
  uint32_t id=get<uint32_t>(p+0x10);bool duplicate=false;for(int j=0;j<nv;++j)if(vehicles[j].id==id)duplicate=true;if(!id||duplicate)continue;
  vehicles[nv++]={p,id,get<int>(p+0x9C),get<int>(p+0xA0),capacity,used,0,limit,count};
 }
 snap.loadVehicles=nv;
 bool accepts[MaxVehicles]{};
 for(int j=0;j<nv;++j)if(freeSlots(vehicles[j])>0)for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);int k=p?kind(p):-1;if((k!=1&&k!=2)||p==vehicles[j].object||!loadingObject(p,house,k))continue;uint32_t t=typeOf(p,k);int size=t?slots(get<double>(t+0x380)):0;if(size>0&&size<=freeSlots(vehicles[j])&&canEnter(p,vehicles[j].object)){accepts[j]=true;break;}}

 for(int i=0;i<selected.count;++i){uint32_t p=item(selected,i);if(!p||!own(p,house))continue;int k=kind(p);if((k!=1&&k!=2)||!loadingObject(p,house,k))continue;uint32_t t=typeOf(p,k);if(!t)continue;
  // Airborne/jumpjet passengers cannot receive native Enter. UI legality remains
  // authoritative for mods and NoManualEnter/NoManualUnload extensions.
  if(get<bool>(p+0x134)||get<int>(p+0x94)==3||get<bool>(p+0x84)||get<bool>(p+0x8D))continue;
  bool carrier=false;for(int j=0;j<nv;++j)if(accepts[j]&&vehicles[j].object==p)carrier=true;if(carrier)continue;
  ++snap.loadUnits;uint32_t id=get<uint32_t>(p+0x10);if(!id)continue;
  if(incomingVehicle(p)||loadLedger.contains(id)){++snap.loadWaiting;continue;}
  int size=slots(get<double>(t+0x380));if(!size||ns>=MaxInfantry){++snap.loadUnassigned;continue;}
  bool duplicate=false;for(int j=0;j<ns;++j)if(soldiers[j].id==id)duplicate=true;if(duplicate)continue;
  soldiers[ns++]={p,id,get<int>(p+0x9C),get<int>(p+0xA0),size,t,k};
 }
 if(!nv){snap.loadUnassigned+=ns;return 1;}
 // Include every approaching passenger, even if not currently selected.
 for(int i=0;i<all.count;++i){uint32_t p=item(all,i);int k=p?kind(p):-1;if((k!=1&&k!=2)||!loadingObject(p,house,k))continue;uint32_t target=incomingVehicle(p);if(!target)continue;
  for(int j=0;j<nv;++j)if(vehicles[j].object==target){int size=passengerSlots(p);vehicles[j].reserved+=size?size:vehicles[j].capacity;++vehicles[j].passengers;composition.add(j,typeOf(p,k));break;}
 }
 for(int i=0;i<loadLedger.count;++i){auto&r=loadLedger.records[i];if(incomingVehicle(r.object))continue;for(int j=0;j<nv;++j)if(vehicles[j].id==r.vehicleID){vehicles[j].reserved+=r.slots;++vehicles[j].passengers;composition.add(j,typeOf(r.object,r.kind));break;}}
 // A carrier which is taking passengers cannot itself be sent into another
 // carrier in the same batch. Full/forbidden carriers may still be passengers.
 faultContext.phase=runtimefault::LoadAssign;
 if(loadQueryFailed)return -15;
 static int assignment[MaxInfantry];assign(soldiers,ns,vehicles,nv,sh->loadMode==1?1:0,sh->loadRadius,composition,workspace,assignment,[](const Infantry&p,const Vehicle&v){return canEnter(p.object,v.object);});
 if(loadQueryFailed)return -15;
 faultContext.phase=runtimefault::LoadSubmit;
 for(int i=0;i<ns;++i){auto&p=soldiers[i];int j=assignment[i];if(j<0){if(p.slots)++snap.loadUnassigned;continue;}if(loadLedger.count>=MaxInfantry){++snap.loadUnassigned;continue;}auto&v=vehicles[j];
  cancelUnloadPassenger(p.object);uint32_t ticket=0;bool sent=outgoingRoom()>0&&enterEvent(house,p.id,v.id,&ticket);
  Reservation job{p.object,p.id,v.object,v.id,p.slots,frame};job.lastSent=sent?frame:-1;job.lastAttempt=frame;job.attempts=sent?0:1;job.retryAfter=sent?frame+30:frame+8;job.x=p.x;job.y=p.y;job.progress=frame;job.initial=loadingObservation(p.object);job.kind=p.kind;job.ticket=ticket;job.queuedAt=sent?sampleClock(sh):0;loadLedger.add(job);
  if(sent)++snap.loadIssued;else ++snap.loadWaiting;
 }
 return snap.loadWaiting?2:1;
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
  if(!remove)remove=get<uint32_t>(r.object+0x10)!=r.id||get<uint32_t>(r.vehicle+0x10)!=r.vehicleID||get<uint32_t>(r.object+0x11C)!=0||!own(r.object,house)||!own(r.vehicle,house)||frame-r.frame>900;
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
