// Included inside Planner: pure value-only target ranking, same on both sides.
 void stepAdvanced(const World&w,const Actor&a,Advanced&task,std::vector<Order>&out,int veteran,int elite,int pathTimeout,int scanHz,int priority){
  if(a.combatFirst<0||a.combatCount<0||a.combatFirst>w.combatCount||a.combatCount>w.combatCount-a.combatFirst)return;
  const Actor* best=nullptr;bool bestReady=false,bestReposition=false;double score=-1,value=-1;int64_t distance=INT64_MAX;
  const Actor* current=actor(task.target);bool currentReady=false,currentReposition=false;
  for(int n=0;n<a.combatCount;++n){const auto&probe=w.combat[a.combatFirst+n];const Actor*t=actor(probe.target);++targetChecks;
   if(!t||!positionValid(*t)||!(t->flags&Enemy)||t->flags&(Protected|Cloaked|Limbo|OffMap|Transported)||!(probe.flags&InAcquisitionRange))continue;
   bool ready=(probe.flags&InWeaponRange)!=0;if(t->id==task.target){currentReady=ready;currentReposition=(probe.flags&NeedsReposition)!=0;}
   if(t->id==task.skipTarget&&w.frame<task.skipUntil)continue;
   int64_t dx=int64_t(a.x)-t->x,dy=int64_t(a.y)-t->y,d=dx*dx+dy*dy;
   double v=nativeorders::value(t->cost,t->veterancy,veteran,elite),rank=priority==1?probe.effectiveness:v;
   bool keep=t->id==task.target,kept=best&&best->id==task.target;
   if(!best||ready>bestReady||(ready==bestReady&&(rank>score||(rank==score&&(v>value||(v==value&&((keep&&!kept)||(keep==kept&&(d<distance||(d==distance&&t->id<best->id)))))))))){
    best=t;bestReady=ready;bestReposition=(probe.flags&NeedsReposition)!=0;score=rank;value=v;distance=d;
   }
  }
  const bool moving=a.mission==2||a.mission==29||a.queued==2||a.queued==29||a.destination||a.destinationCell;
  if(task.pendingShot&&a.target==task.target&&a.mission==1&&(!moving||currentReposition))task.pendingShot=false;
  // Observe an in-flight Attack before issuing a duplicate. An in-range enemy
  // still suppresses all Move proposals while that event crosses the network.
  if(task.pendingShot&&(bestReady||bestReposition)&&best->id==task.target&&w.frame-task.sent<12){task.progress.combat(a.x,a.y,w.frame);return;}
  if(currentReady&&a.target==task.target&&!moving){
   if(a.rearmDuration>0&&a.rearmStart!=task.lastFireStamp){task.attackProgress=w.frame;task.lastFireStamp=a.rearmStart;}
   if(a.flags&(BurstFiring|SustainedFire)){task.progress.combat(a.x,a.y,w.frame);return;}
   if(!task.urgent&&task.scanned&&scanHz>0&&uint32_t(w.tick-task.scannedAt)<uint32_t(1000/std::clamp(scanHz,1,60))){task.progress.combat(a.x,a.y,w.frame);return;}
  }
  task.urgent=false;task.scanned=true;task.scannedAt=w.tick;
  if(bestReady||bestReposition){
   task.progress.combat(a.x,a.y,w.frame);
   bool attack=!task.attacking||task.target!=best->id||a.target!=best->id||a.mission!=1||(moving&&!bestReposition);
   if(attack&&w.frame-task.sent>=2)add(out,{a.id,task.token,best->id,0,1,0,CombatOrder});
   return;
  }
  // Targets inside minimum range use the native Attack mission above; walking
  // onto their occupied cell would make the range problem worse. For distant
  // targets retain the verified IFV/AA approach: vanilla pursuit can use
  // the wrong IFV/AA approach distance. Approach with a normal Move and switch
  // to an object-target Attack with an empty destination as soon as in range.
  uint32_t destination=task.cell,pursuit=0;
  if(best){int x=best->x/256,y=best->y/256;if(x>0&&x<512&&y>0&&y<512){destination=uint32_t(y*1000+x);pursuit=best->id;}}
  // Hold belongs to one goal; a completed waypoint must not pin a new pursuit.
  if(task.attacking||task.moveCell!=destination||task.pursuit!=pursuit){task.pendingShot=false;task.progress={};task.progress.begin(a.x,a.y,w.frame);}
  else task.progress.update(a.x,a.y,w.frame,a.mission,int(destination%1000)*256+128,int(destination/1000)*256+128,std::clamp(pathTimeout,30,900));
  if(task.progress.hold){if(!task.stopped)add(out,{a.id,task.token,0,0,6,0,CombatOrder});return;}
  bool pending=task.moveCell==destination&&task.pursuit==pursuit&&w.frame-task.sent<12&&!task.attacking;
  bool observed=a.mission==2&&a.destinationCell==destination;
  if(!pending&&!observed&&w.frame-task.sent>=(pursuit&&moving?12:2))add(out,{a.id,task.token,0,destination,2,11,CombatOrder,0,pursuit});
 }
