// Same pure Planner implementation as the EXE; invoked under the sole Runtime
// dispatcher lease. It operates on a bounded raw World, then uses common admission.
static planner::Planner runtimePlanner;
static void resetRuntimePlanner(){runtimePlanner.reset();if(sh){sh->runtimeTracking=0;sh->runtimePlannerMicros=0;}}
static void runtimePlanningTick(uint32_t house,int frame,DWORD now){
 // Do not advance planner state or expire proposals while the strict
 // same-frame Runtime UID index is unavailable.
 if(indexedFrame!=frame||!commandRosterReady())return;
 uint32_t selected=compute::plannerFlags(sh->flags,sh->computeApplied,true);
 bool drop=sh->magnetronDrop&&compute::inside(sh->computeApplied,compute::MagnetronDrop);
 if(!selected&&!drop&&!runtimePlanner.hasJobs()){sh->runtimeTracking=0;return;}
 if(snap.chat||!freshness::valid(planningWorld.match,match,planningWorld.tick,planningWorld.frame,now,frame,freshness::Tactical))return;
 const int veteran=int(sh->veteranPercent),elite=int(sh->elitePercent),timeout=int(sh->pathTimeout),
  scanHz=int(sh->advancedScanHz),priority=int(sh->advancedPriority),range=int(sh->advancedRangeCells);
 const uint32_t modes=compute::magnetronIntents(sh->computeApplied,true);
 const bool observeDrop=sh->magnetronDrop!=0;
 // The local publication has already validated its active array counts.
 // Skip timing, allocations and tracking traversal for an unchanged input;
 // retain the last actual work duration rather than report a false zero.
 if(sh->computeApplied==compute::All&&runtimePlanner.alreadyProcessed(planningWorld,selected,veteran,elite,timeout,scanHz,priority,range,drop,modes,observeDrop))return;
 LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceCounter(&begin);
 auto orders=runtimePlanner.step(planningWorld,selected,veteran,elite,timeout,scanHz,priority,range,drop,modes,observeDrop);
 for(const auto&o:orders){
  if(!freshness::valid(planningWorld.match,match,planningWorld.tick,planningWorld.frame,sampleClock(sh),get<int>(0xA8ED84),freshness::Tactical)){runtimePlanner.expired(o);continue;}
  int result=admitPlannerOrder(o,house,planningWorld.match,planningWorld.tick);
  if(result>0)runtimePlanner.accepted(o,frame);else if(result<0)runtimePlanner.rejected(o,frame);else runtimePlanner.expired(o);
 }
 sh->runtimeTracking=runtimePlanner.trackingFlags();
 if(compute::inside(sh->computeApplied,compute::Ore)){snap.oreQueued=runtimePlanner.queued;snap.oreDocking=runtimePlanner.docking;snap.oreIssued=runtimePlanner.issued;snap.oreYielded=runtimePlanner.yielded;snap.oreDelivered=runtimePlanner.oreDelivered;snap.oreUnverified=runtimePlanner.oreUnverified;}
 snap.magnetShots=runtimePlanner.magnetShots;snap.magnetUnverified=runtimePlanner.magnetUnverified;
 QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);sh->runtimePlannerMicros=micros(begin,end,frequency);
}
