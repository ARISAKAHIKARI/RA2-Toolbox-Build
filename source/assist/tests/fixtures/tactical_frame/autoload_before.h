// Frozen production autoLoad from cee2af5 / c03e4ff, before sealed-frame conversion.
static int legacyAutoLoad(uint32_t house,const compute::LoadInput*cohort){
 if(!cohort&&loadCalculationPending)return -6;
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
 int nv=0,ns=0;Vec selected=vec(0xA8ECB8,4096);const int selectedCount=cohort?cohort->ns:selected.count;
 auto selectedAt=[&](int i){if(!cohort)return item(selected,i);const auto&p=cohort->soldiers[i];return worldByID(p.id)==p.object&&manualVersions.get(p.id)==cohort->tokens[i]?p.object:0u;};
 bool selectedCarrier=cohort&&cohort->nv>0;
 for(int i=0;i<selectedCount;++i){uint32_t p=selectedAt(i);
  if(p&&own(p,house)&&kind(p)==2&&featureObjectAllowed(featuretypes::LoadTransport,p)){uint32_t t=typeOf(p,2);if(t&&get<int>(t+0x5E0)>0){selectedCarrier=true;break;}}
 }
 // Match MO's explicitly selected-carrier assignment when carriers were
 // selected. A passenger-only hotkey finds nearby owned carriers as requested.
 Vec carriers=selectedCarrier?selected:all;const int carrierCount=cohort?cohort->nv:carriers.count;
 auto carrierAt=[&](int i){if(!cohort)return item(carriers,i);const auto&v=cohort->vehicles[i];return worldByID(v.id)==v.object?v.object:0u;};
 for(int i=0;i<carrierCount&&nv<MaxVehicles;++i){uint32_t p=carrierAt(i);if(!p||!loadingObject(p,house,2)||incomingVehicle(p))continue;uint32_t t=typeOf(p,2);if(!t||!featureTypeAllowed(featuretypes::LoadTransport,t))continue;
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
 for(int j=0;j<nv;++j)if(freeSlots(vehicles[j])>0)for(int i=0;i<selectedCount;++i){uint32_t p=selectedAt(i);int k=p?kind(p):-1;if((k!=1&&k!=2)||p==vehicles[j].object||!loadingObject(p,house,k))continue;uint32_t t=typeOf(p,k);int size=t?slots(get<double>(t+0x380)):0;if(featureTypeAllowed(featuretypes::LoadPassenger,t)&&size>0&&size<=freeSlots(vehicles[j])&&canEnter(p,vehicles[j].object)){accepts[j]=true;break;}}

 for(int i=0;i<selectedCount;++i){uint32_t p=selectedAt(i);if(!p||!own(p,house))continue;int k=kind(p);if((k!=1&&k!=2)||!loadingObject(p,house,k))continue;uint32_t t=typeOf(p,k);if(!t||!featureTypeAllowed(featuretypes::LoadPassenger,t))continue;
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
 static uint32_t tokens[MaxInfantry];for(int i=0;i<ns;++i)tokens[i]=manualVersions.get(soldiers[i].id);
 if(!compute::inside(sh->computeApplied,compute::Load)){
  if(!cohort){loadFrames.clear();loadTaskSeen=sh->loadComputed;}
  auto&in=sh->loadInput;in.match=match;in.tick=sampleClock(sh);in.frame=frame;in.ns=ns;in.nv=nv;in.mode=sh->loadMode==1;in.radius=std::clamp(sh->loadRadius,1,512);
  memcpy(in.soldiers,soldiers,size_t(ns)*sizeof(Infantry));memcpy(in.vehicles,vehicles,size_t(nv)*sizeof(Vehicle));in.composition=composition;memcpy(in.tokens,tokens,size_t(ns)*4);
  for(int i=0;i<ns;++i)for(int j=0;j<nv;++j){int64_t dx=int64_t(soldiers[i].x)-vehicles[j].x,dy=int64_t(soldiers[i].y)-vehicles[j].y,limit=int64_t(in.radius)*256;
   in.compatible[i][j]=soldiers[i].slots<=freeSlots(vehicles[j])&&soldiers[i].slots<=vehicles[j].sizeLimit&&dx*dx+dy*dy<=limit*limit&&canEnter(soldiers[i].object,vehicles[j].object);
   if(loadQueryFailed)return -15;
  }
  loadCalculationPending=true;if(!cohort)loadCalculationStarted=in.tick;const uint32_t serial=compute::nextTask(sh->loadTask);loadFrames.retain(serial,in);MemoryBarrier();sh->loadTask=serial;notifyHost();return -30;
 }
 static int assignment[MaxInfantry];assign(soldiers,ns,vehicles,nv,sh->loadMode==1?1:0,sh->loadRadius,composition,workspace,assignment,[](const Infantry&p,const Vehicle&v){return canEnter(p.object,v.object);});
 if(loadQueryFailed)return -15;
 return submitLoading(house,soldiers,ns,vehicles,nv,assignment,tokens);
}
