// Same pure Planner implementation as the EXE; invoked under the sole Runtime
// dispatcher lease. It operates on a bounded raw World, then uses common admission.
static planner::Planner runtimePlanner;
static void resetRuntimePlanner(){runtimePlanner.reset();if(sh){sh->runtimeTracking=0;sh->runtimePlannerMicros=0;}}
static void runtimePlanningTick(uint32_t house,int frame,DWORD now){
 uint32_t selected=compute::plannerFlags(sh->flags,sh->computeApplied,true);
 if(!selected&&!runtimePlanner.hasJobs()){sh->runtimeTracking=0;return;}
 if(snap.chat||!freshness::valid(planningWorld.match,match,planningWorld.tick,planningWorld.frame,now,frame,freshness::Tactical))return;
 LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceCounter(&begin);
 auto orders=runtimePlanner.step(planningWorld,selected,int(sh->veteranPercent),int(sh->elitePercent),int(sh->pathTimeout),int(sh->advancedScanHz),int(sh->advancedPriority),int(sh->advancedRangeCells));
 for(const auto&o:orders){
  if(!freshness::valid(planningWorld.match,match,planningWorld.tick,planningWorld.frame,sampleClock(sh),get<int>(0xA8ED84),freshness::Tactical)){runtimePlanner.expired(o);continue;}
  int result=admitPlannerOrder(o,house);
  if(result>0)runtimePlanner.accepted(o,frame);else if(result<0)runtimePlanner.rejected(o,frame);else runtimePlanner.expired(o);
 }
 sh->runtimeTracking=runtimePlanner.trackingFlags();
 if(compute::inside(sh->computeApplied,compute::Ore)){snap.oreQueued=runtimePlanner.queued;snap.oreDocking=runtimePlanner.docking;snap.oreIssued=runtimePlanner.issued;snap.oreYielded=runtimePlanner.yielded;snap.oreDelivered=runtimePlanner.oreDelivered;snap.oreUnverified=runtimePlanner.oreUnverified;}
 if(compute::inside(sh->computeApplied,compute::Magnetron)){snap.magnetShots=runtimePlanner.magnetShots;snap.magnetUnverified=runtimePlanner.magnetUnverified;}
 QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);sh->runtimePlannerMicros=micros(begin,end,frequency);
}
