// Frozen reference: v1.0.51-pre.3 a2c4ed3, complete original batch.
static int pre1513AutoLoad(uint32_t house,const compute::LoadInput*cohort){
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
