// Owned by the sole GameRuntime dispatcher. All attacks/reservations/releases are native events.

static compute::FrameHistory<missilecompute::Input> missileFrames;static uint32_t missileApplied=0;
static missilecompute::Input missileInput;
static missilecompute::Result missileResult;
static bool missilePending,missileScanned;
static uint32_t missileSerial,missileScanTick,missileSettings;
static int missileSourceCursor,missileTargetCursor,missileManagedCursor,missileFrame=-1;
struct MissileSend {uint32_t id=0,pointer=0,token=0,target=0,tick=0;int frame=0,mission=-1;bool held=false;int holdStart=-1,releasedAt=-1;bool controlled=false;int advanceStart=-1,anchorX=0,anchorY=0;int observedRearm=-1,shotUntil=-1,shotDamage=0,shotHP=0;uint32_t shotTarget=0;};
static MissileSend missileSends[256];static int missileSendCount;
struct MissileMotion {uint32_t id=0,pointer=0;int frame=-1,x=0,y=0,z=0;double vx=0,vy=0,vz=0;bool known=false;};
static MissileMotion missileMotion[64];static int missileMotionCursor;
struct MissileManual {uint32_t id=0,pointer=0;int frame=0;};
static MissileManual missileManual[256];static int missileManualCount;
static void pauseMissileForManual(uint32_t id,uint32_t pointer){if(!sh||!sh->missileEnabled)return;for(int n=0;n<missileManualCount;++n)if(missileManual[n].id==id){missileManual[n]={id,pointer,get<int>(0xA8ED84)};return;}if(missileManualCount<256)missileManual[missileManualCount++]={id,pointer,get<int>(0xA8ED84)};}
static bool missileManuallyOwned(uint32_t p){uint32_t id=get<uint32_t>(p+0x10);for(int n=0;n<missileManualCount;++n)if(missileManual[n].id==id){auto&r=missileManual[n];int m=get<int>(p+0xAC),q=get<int>(p+0xB4);
 bool ended=p!=r.pointer||(!get<uint32_t>(p+0x2B4)&&!get<uint32_t>(p+0x5A4)&&(m==5||m==6||m==11||m==13)&&(q==-1||q==m)&&get<int>(0xA8ED84)-r.frame>=12);
 if(ended){r=missileManual[--missileManualCount];return false;}return true;}return false;}
static int missileRecord(uint32_t id){for(int n=0;n<missileSendCount;++n)if(missileSends[n].id==id)return n;return -1;}
static void publishMissileHeld(){missileHoldCount=0;for(int n=0;n<missileSendCount;++n)missileHoldCount+=missileSends[n].held||missileSends[n].controlled;if(sh)sh->missileHeld=missileHoldCount;}
static void resetMissileControl(){
 missileManualCount=0;missileFrames.clear();missileApplied=sh?sh->missileComputed:0;missilePending=missileScanned=false;missileFrame=-1;missileSendCount=missileSourceCursor=missileTargetCursor=missileManagedCursor=missileHoldCount=0;
 for(auto&m:missileMotion)m={};missileMotionCursor=0;
 if(sh){sh->missileSources=sh->missileTargets=sh->missileIssued=sh->missileHeld=0;}
}
static bool missileTarget(uint32_t p,uint32_t house){
 if(!p||!nativeEnemy(p,house)||get<uint32_t>(p+0x11C)||get<int>(p+0x220))return false;
 int k=kind(p);uint32_t t=typeOf(p,k);if(!t||!featureTypeAllowed(featuretypes::MissileTarget,t))return false;
 if(sh->missileScope==0){if(k!=0||!get<bool>(t+0xD68))return false;}
 else {int airborne=0;if(k==3||!objectAirQuery(p,&airborne)||!airborne)return false;}
 return get<bool>(t+0x231)&&!get<bool>(t+0x233)&&!remaining(get<int>(p+0x18C),get<int>(p+0x194),get<int>(0xA8ED84));
}
static bool missileSource(uint32_t p,uint32_t house){
 if(!p||stickyOwns(p)||!own(p,house)||kind(p)<0||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||!featureObjectAllowed(featuretypes::MissileDefense,p))return false;
 if(formationOwns(get<uint32_t>(p+0x10))||missileManuallyOwned(p))return false;
 int at=missileRecord(get<uint32_t>(p+0x10));bool held=at>=0&&missileSends[at].held&&missileSends[at].token==manualVersions.get(get<uint32_t>(p+0x10));
 const int m=get<int>(p+0xAC),q=get<int>(p+0xB4);
 if((m!=1&&m!=5&&m!=6&&m!=11&&m!=13&&!(m==0&&held))||(q!=-1&&q!=m)||orderIndex(p)>=0||burstOwns(get<uint32_t>(p+0x10))||dodgeOwns(get<uint32_t>(p+0x10)))return false;
 // Never commandeer explicit movement. Only our own observed Attack may move.
 uint32_t target=get<uint32_t>(p+0x2B4);
 if(m==1&&target&&!missileTarget(target,house))return false;
 if(kind(p)!=3&&(m==6||get<uint32_t>(p+0x5A4))&&
    !(m==1&&at>=0&&missileSends[at].controlled&&missileSends[at].pointer==p&&
      missileSends[at].token==manualVersions.get(get<uint32_t>(p+0x10))&&target==worldByID(missileSends[at].target)))return false;
 if((sh->flags&AntiTheft)&&antiTheftBuilding(p)&&antiTheftThreat(target,house))return false;
 uint32_t t=typeOf(p,kind(p));if(!t||(get<int>(t+0x684)>=0&&get<int>(p+0x2FC)<=0))return false;
 planner::Actor a{};rawCombatInfo(p,t,kind(p),a,true,true);if(a.flags&planner::BurstFiring)return false;
 for(const auto&w:a.weapons)if((w.flags&planner::AirWeapon)&&!(w.flags&planner::HealingWeapon)&&w.range>0)return true;
 return false;
}
static void sampleMissileMotion(uint32_t p,int frame){
 uint32_t id=get<uint32_t>(p+0x10);int at=-1;
 for(int n=0;n<64;++n)if(missileMotion[n].id==id&&missileMotion[n].pointer==p){at=n;break;}
 if(at<0){at=missileMotionCursor++%64;missileMotion[at]={};}
 auto&m=missileMotion[at];if(m.frame==frame)return;
 int x=get<int>(p+0x9C),y=get<int>(p+0xA0),z=get<int>(p+0xA4),dt=frame-m.frame;
 m.known=m.id==id&&dt>0&&dt<=30;
 if(m.known){m.vx=double(int64_t(x)-m.x)/dt;m.vy=double(int64_t(y)-m.y)/dt;m.vz=double(int64_t(z)-m.z)/dt;
  if(std::abs(m.vx)>512||std::abs(m.vy)>512||std::abs(m.vz)>512)m.known=false;}
 m.id=id;m.pointer=p;m.frame=frame;m.x=x;m.y=y;m.z=z;
}
static missilecompute::Edge missileEdge(uint32_t p,uint32_t q){
 missilecompute::Edge e{};e.close=0;uint32_t weapon=0;int slot=0,close=0,airborne=0;
 if(combatWeaponQuery(p,q,&weapon,&slot,&close,&airborne)<=0||!airborne)return e;
 auto info=rawWeaponInfo(weapon,true);int armor=get<int>(typeOf(q,kind(q))+0x9C);
 if(armor<0||armor>10||!(info.flags&planner::AirWeapon)||!(info.selectMask&(1u<<armor))||
    (info.flags&(planner::HealingWeapon|planner::MindControlWeapon|planner::TemporalWeapon|planner::ParasiteWeapon|planner::LocomotorWeapon)))return e;
 double damage=double(get<int>(weapon+0xA4))*info.effectiveness[armor];if(damage<=0)return e;
 if(sh->lowDamageFilter&&!(get<double>(get<uint32_t>(weapon+0xAC)+0xA0+armor*8)>0.02))return e;
 double dx=double(get<int>(q+0x9C))-get<int>(p+0x9C),dy=double(get<int>(q+0xA0))-get<int>(p+0xA0),dz=double(get<int>(q+0xA4))-get<int>(p+0xA4);
 double distance=std::sqrt(dx*dx+dy*dy+dz*dz);if(!std::isfinite(distance)||distance>1048576)return e;
 int eta=0;
 if(!close){
  int bonus=std::clamp(get<int>(typeOf(p,kind(p))+0x68C),-32768,32768);
  double range=std::max(0,info.range+bonus),gap=distance-range;
  // The selected raw/native range query controls the in-range estimate. This
  // gap only limits acquisition; the synced Attack mission owns actual fire.
  if(gap>missilecompute::AdvanceLeash||distance<info.minimum)return e;
  int at=missileRecord(get<uint32_t>(p+0x10));
  if(at>=0){const auto&r=missileSends[at];int frame=get<int>(0xA8ED84);
   if(r.releasedAt>=0&&frame-r.releasedAt<missilecompute::ReleaseGraceFrames)return e;
   if(kind(p)!=3&&r.advanceStart>=0){double ax=double(get<int>(p+0x9C))-r.anchorX,ay=double(get<int>(p+0xA0))-r.anchorY;
    double tx=double(get<int>(q+0x9C))-r.anchorX,ty=double(get<int>(q+0xA0))-r.anchorY;
    if(frame-r.advanceStart>=missilecompute::MaxAdvanceFrames||ax*ax+ay*ay>=double(missilecompute::AdvanceLeash)*missilecompute::AdvanceLeash||
       std::sqrt(tx*tx+ty*ty)>range+missilecompute::AdvanceLeash)return e;
   }
  }
  const MissileMotion* motion=nullptr;for(const auto&m:missileMotion)if(m.id==get<uint32_t>(q+0x10)&&m.pointer==q){motion=&m;break;}
  if(motion&&motion->known){double closing=distance>0?-(dx*motion->vx+dy*motion->vy+dz*motion->vz)/distance:0;
   if(sh->missileReserve&&(closing<=.5||gap/closing>missilecompute::LookAhead))return e;
   if(kind(p)!=3&&closing<-.5)return e;
   eta=closing>.5?std::min(missilecompute::LookAhead,std::max(1,int(std::ceil(std::max(0.0,gap)/closing)))):missilecompute::LookAhead;
  }else eta=missilecompute::LookAhead;
 }
 e.damage=int(std::min(damage+0.999999,1000000.0));e.distance=uint32_t(std::min(distance*distance,double(UINT32_MAX)));e.eta=eta;e.close=close;return e;
}
static void pruneMissileSends(uint32_t now){
 for(int n=0;n<missileSendCount;){auto&s=missileSends[n];uint32_t p=worldByID(s.id);
  if(p!=s.pointer||!p||!live(p)||!own(p,lastHouse)||manualVersions.get(s.id)!=s.token){missileSends[n]=missileSends[--missileSendCount];continue;}
  const int frame=get<int>(0xA8ED84),rearm=get<int>(p+0x2EC);uint32_t shot=worldByID(s.shotTarget);
  if(s.shotDamage&&(frame>=s.shotUntil||!shot||!missileTarget(shot,lastHouse)||get<int>(shot+0x6C)!=s.shotHP))s.shotDamage=0;
  // A newly started native rearm timer is observed fire evidence, not a hit.
  // Reserve one estimated shot briefly, then expire or clear on HP change.
  if(sh->missileReserve&&s.controlled&&rearm!=s.observedRearm&&get<int>(p+0xAC)==1&&remaining(rearm,get<int>(p+0x2F4),frame)>0){
   uint32_t q=worldByID(s.target);if(q&&get<uint32_t>(p+0x2B4)==q&&missileTarget(q,lastHouse)){auto e=missileEdge(p,q);if(e.damage&&e.close){s.shotTarget=s.target;s.shotDamage=e.damage;s.shotHP=get<int>(q+0x6C);s.shotUntil=frame+missilecompute::ShotReserveFrames;}}
  }s.observedRearm=rearm;
  int m=get<int>(p+0xAC);bool observed=s.mission>=0&&m==s.mission&&(m==0?!get<uint32_t>(p+0x2B4):m!=1||get<uint32_t>(p+0x2B4)==worldByID(s.target));
  if(observed){if(m!=0){s.held=false;s.holdStart=-1;}if(m==5){s.controlled=false;s.advanceStart=-1;s.releasedAt=get<int>(0xA8ED84);}s.mission=-1;}
  if(s.mission>=0&&DWORD(now-s.tick)>=350&&get<int>(0xA8ED84)-s.frame>=12)s.mission=-1;
  if(!s.held&&!s.controlled&&s.mission<0&&(s.releasedAt<0||get<int>(0xA8ED84)-s.releasedAt>=missilecompute::ReleaseGraceFrames)){missileSends[n]=missileSends[--missileSendCount];continue;}++n;
 }publishMissileHeld();
}
static bool submitMissile(uint32_t p,int action,uint32_t target,uint32_t house,int frame,DWORD now,bool advance=false){
 uint32_t id=get<uint32_t>(p+0x10);int at=missileRecord(id);if(at<0&&missileSendCount>=256)return false;
 if(!planningRoom(2)||!outgoingRoom())return false;
 if(!nativeMission(house,id,action,action==0?0:target,0,0,0,0,taskarbiter::Owner::MissileDefense))return false;
 consumePlanning(2);++sh->missileIssued;if(at<0){at=missileSendCount++;missileSends[at]={};missileSends[at].observedRearm=get<int>(p+0x2EC);}
 auto&r=missileSends[at];r.id=id;r.pointer=p;r.token=manualVersions.get(id);r.target=target;r.tick=now;r.frame=frame;r.mission=action;
 if(action==1||action==0)r.controlled=true;
 if(action==0){r.held=true;r.holdStart=frame;}
 if(advance&&r.advanceStart<0){r.advanceStart=frame;r.anchorX=get<int>(p+0x9C);r.anchorY=get<int>(p+0xA0);}
 publishMissileHeld();return true;
}
static bool releaseMissileHolds(uint32_t house,int frame,DWORD now){
 pruneMissileSends(now);
 for(int n=0;n<missileSendCount;++n){auto&s=missileSends[n];if((!s.held&&!s.controlled)||s.mission==5)continue;
  uint32_t p=worldByID(s.id);if(!p)continue;
  // Explicit disable/exit also interrupts a burst: the user asked to stop
  // automation. Normal automatic releases below wait for burst completion.
  // Queue Guard after a pending Attack too, so disable ends automatic pursuit.
  if(!submitMissile(p,5,0,house,frame,now))break;
 }
 return missileHoldCount==0;
}
static void applyMissileResult(const missilecompute::Result&r,uint32_t house,int frame,DWORD now,const missilecompute::Input*source=nullptr){
 const auto&input=source?*source:missileInput;
 if(r.match!=input.match||r.tick!=input.tick||r.frame!=input.frame||r.count<0||r.count>missilecompute::MaxOrders)return;
 if(missileSettings!=(sh->missileScope|(sh->missileAllocation<<8)|(sh->missileReserve<<16)|(sh->lowDamageFilter<<17)))return;
 for(int n=0;n<r.count;++n){
  if(!sh->missileEnabled||snap.chat||!freshness::valid(r.match,match,r.tick,r.frame,sampleClock(sh),frame,freshness::Tactical))break;
  int si=r.orders[n].source,ti=r.orders[n].target,action=r.orders[n].action;
  if(si<0||si>=input.count||(action!=missilecompute::Advance&&action!=0&&action!=1&&action!=5))continue;
  auto&s=input.sources[si];uint32_t p=worldByID(s.id);
  if(p!=s.pointer||manualVersions.get(s.id)!=s.token)continue;
  int at=missileRecord(s.id);if(at>=0&&missileSends[at].mission>=0)continue;
  uint32_t q=0;bool outside=false;
  if(action==5){
   if(at<0||(!missileSends[at].held&&!missileSends[at].controlled))continue;
   // External computation may return after a native burst has begun. Recheck
   // here as well as at snapshot time before an automatic Guard interrupts it.
   planner::Actor firing{};rawCombatInfo(p,typeOf(p,kind(p)),kind(p),firing,true,true);
   if(firing.flags&planner::BurstFiring)continue;
  }
  else {
   if(!missileSource(p,house))continue;
   if(ti<0||ti>=input.targetCount)continue;auto&t=input.targets[ti];q=worldByID(t.id);
   if(q!=t.pointer||!missileTarget(q,house))continue;auto e=missileEdge(p,q);if(!e.damage||(action==1&&!e.close))continue;outside=!e.close;
   if((action==0||(action==missilecompute::Advance&&kind(p)==3&&outside))&&at>=0&&missileSends[at].held&&get<int>(p+0xAC)==0&&!get<uint32_t>(p+0x2B4)){missileSends[at].target=get<uint32_t>(q+0x10);continue;}
   if(get<uint32_t>(p+0x2B4)==q&&at>=0&&missileSends[at].controlled){
    if(action==1&&get<int>(p+0xAC)==1&&!missileSends[at].held)continue;
    if(action==missilecompute::Advance&&missileSends[at].advanceStart>=0&&
       (get<int>(p+0xAC)==1||(kind(p)==3&&get<int>(p+0xAC)==0&&missileSends[at].held)))continue;
   }
  }
  // Hold is shared by buildings and mobile guns. The assigned ID stays in
  // tool metadata; Sleep carries no firing target. Legacy Advance remains opt-in.
  int mission=action;if(action==missilecompute::Advance)mission=kind(p)==3&&outside?0:1;
  if(!submitMissile(p,mission,q?get<uint32_t>(q+0x10):0,house,frame,now,action==missilecompute::Advance))break;
 }
}
static bool missileControlTick(uint32_t house,int frame,DWORD now){
 pruneMissileSends(now);
 if(!sh->missileEnabled){missilePending=false;releaseMissileHolds(house,frame,now);if(!missileSendCount)resetMissileControl();return false;}
 if(snap.chat||!simulationReady())return false;
 uint32_t settings=sh->missileScope|(sh->missileAllocation<<8)|(sh->missileReserve<<16)|(sh->lowDamageFilter<<17);
 if(settings!=missileSettings){missileFrames.clear();missileApplied=sh->missileComputed;missilePending=missileScanned=false;missileFrame=-1;missileSettings=settings;}
 const bool outside=!compute::inside(sh->computeApplied,compute::MissileDefense);
 if(outside&&compute::newer(sh->missileComputed,missileApplied)){
  const uint32_t completed=sh->missileComputed;long seq=sh->missileResultSeq;
  if(!(seq&1)){MemoryBarrier();missileResult=sh->missileResult;MemoryBarrier();if(seq==sh->missileResultSeq&&completed==sh->missileComputed){missileApplied=completed;if(const auto*source=missileFrames.find(completed)){applyMissileResult(missileResult,house,frame,now,source);if(const auto*c=missileFrames.cursors(completed)){missileSourceCursor=(*c)[0];missileTargetCursor=(*c)[1];missileManagedCursor=(*c)[2];}}}}
 }
 if(outside&&!externalInputFrame)return false;
 if(frame==missileFrame)return false;
 if(!outside&&missileScanned&&DWORD(now-missileScanTick)<1000u/std::clamp(uint32_t(sh->missileScanHz),1u,60u))return false;
 missileFrame=frame;missileScanTick=now;missileScanned=true;
 auto&i=missileInput;i={};i.match=match;i.tick=now;i.frame=frame;i.mode=sh->missileAllocation|(sh->missileReserve?missilecompute::ReserveSleep:0);
 Vec all=vec(0xA8EC78);int visited=0;
 for(;visited<all.count&&i.targetCount<64;++visited){uint32_t q=item(all,(missileTargetCursor+visited)%all.count);if(!missileTarget(q,house))continue;
  sampleMissileMotion(q,frame);uint32_t victim=worldObjects.contains(get<uint32_t>(q+0x2B4))?get<uint32_t>(q+0x2B4):0;
  i.targets[i.targetCount++]={get<uint32_t>(q+0x10),q,get<int>(q+0x6C),uint32_t(victim&&own(victim,house)?(std::abs(int64_t(get<int>(q+0x9C))-get<int>(victim+0x9C))+std::abs(int64_t(get<int>(q+0xA0))-get<int>(victim+0xA0))<=3*256?2:1):0)};
 }const int nextTargetCursor=all.count?(missileTargetCursor+visited)%all.count:0;if(!outside)missileTargetCursor=nextTargetCursor;
 auto add=[&](uint32_t p){if(i.count>=64||!p)return;for(int n=0;n<i.count;++n)if(i.sources[n].pointer==p)return;
  uint32_t id=get<uint32_t>(p+0x10);int at=missileRecord(id);bool held=at>=0&&missileSends[at].held;
  bool managed=at>=0&&missileSends[at].controlled;
  if(!held&&!managed&&!missileSource(p,house))return;
  int n=i.count++;auto&s=i.sources[n];s={id,p,manualVersions.get(id),rawObjectID(get<uint32_t>(p+0x2B4)),0};
  s.cooldown=remaining(get<int>(p+0x2EC),get<int>(p+0x2F4),frame);s.held=held;s.managed=managed;
  if(at>=0){auto&record=missileSends[at];s.pending=record.target;s.pendingMission=record.mission;s.advanceFrames=record.advanceStart>=0?std::max(0,frame-record.advanceStart):-1;s.releaseFrames=record.releasedAt>=0?std::max(0,missilecompute::ReleaseGraceFrames-(frame-record.releasedAt)):0;s.committedTarget=record.shotTarget;s.committedDamage=record.shotDamage;}
  if((held||managed)&&s.pendingMission<0){planner::Actor firing{};rawCombatInfo(p,typeOf(p,kind(p)),kind(p),firing,true,true);if(firing.flags&planner::BurstFiring)s.pendingMission=missilecompute::TransientBusy;}
  if(missileSource(p,house))for(int t=0;t<i.targetCount;++t)i.edges[n][t]=missileEdge(p,i.targets[t].pointer);
 };
 // Revisit leases fairly, leaving half the batch for newly available guns.
 // Persistent advances must not monopolize all 64 slots in a large army.
 int revisited=0,managedBudget=i.targetCount?32:64;
 for(;revisited<missileSendCount&&i.count<managedBudget;++revisited){auto&r=missileSends[(missileManagedCursor+revisited)%missileSendCount];if(r.held||r.controlled)add(worldByID(r.id));}
 const int nextManagedCursor=missileSendCount?(missileManagedCursor+revisited)%missileSendCount:0;if(!outside)missileManagedCursor=nextManagedCursor;
 visited=0;if(i.targetCount)for(;visited<all.count&&i.count<64;++visited)add(item(all,(missileSourceCursor+visited)%all.count));
 const int nextSourceCursor=all.count?(missileSourceCursor+visited)%all.count:0;if(!outside)missileSourceCursor=nextSourceCursor;
 sh->missileSources=i.count;sh->missileTargets=i.targetCount;if(!i.count)return true;
 if(compute::inside(sh->computeApplied,compute::MissileDefense))applyMissileResult(missilecompute::calculate(i),house,frame,now);
 else {InterlockedIncrement(&sh->missileInputSeq);MemoryBarrier();memcpy(&sh->missileInput,&i,sizeof i);missileSerial=compute::nextTask(sh->missileTask);missileFrames.retain(missileSerial,i,{nextSourceCursor,nextTargetCursor,nextManagedCursor});sh->missileTask=missileSerial;MemoryBarrier();InterlockedIncrement(&sh->missileInputSeq);missilePending=true;notifyHost();}
 return true;
}

