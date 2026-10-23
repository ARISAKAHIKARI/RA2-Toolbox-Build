// Owned by the sole GameRuntime dispatcher. All holds/wakes are native events.
extern "C" int GuardedCombatWeapon(unsigned,unsigned,unsigned*,int*,int*,int*);
extern "C" int GuardedObjectAir(unsigned,int*);
static missilecompute::Input missileInput;
static missilecompute::Result missileResult;
static bool missilePending,missileScanned;
static uint32_t missileSerial,missileScanTick,missileSettings;
static int missileSourceCursor,missileTargetCursor,missileFrame=-1;
struct MissileSend {uint32_t id=0,pointer=0,token=0,target=0,tick=0;int frame=0,mission=-1;bool held=false;int holdStart=-1,releasedAt=-1;};
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
static void publishMissileHeld(){missileHoldCount=0;for(int n=0;n<missileSendCount;++n)missileHoldCount+=missileSends[n].held;if(sh)sh->missileHeld=missileHoldCount;}
static void resetMissileControl(){
 missileManualCount=0;missilePending=missileScanned=false;missileFrame=-1;missileSendCount=missileSourceCursor=missileTargetCursor=missileHoldCount=0;
 for(auto&m:missileMotion)m={};missileMotionCursor=0;
 if(sh){sh->missileSources=sh->missileTargets=sh->missileIssued=sh->missileHeld=0;}
}
static bool missileTarget(uint32_t p,uint32_t house){
 if(!p||!nativeEnemy(p,house)||get<uint32_t>(p+0x11C)||get<int>(p+0x220))return false;
 int k=kind(p);uint32_t t=typeOf(p,k);if(!t||!featureTypeAllowed(featuretypes::MissileTarget,t))return false;
 if(sh->missileScope==0){if(k!=0||!get<bool>(t+0xD68))return false;}
 else {int airborne=0;if(k==3||!GuardedObjectAir(p,&airborne)||!airborne)return false;}
 return get<bool>(t+0x231)&&!get<bool>(t+0x233)&&!remaining(get<int>(p+0x18C),get<int>(p+0x194),get<int>(0xA8ED84));
}
static bool missileSource(uint32_t p,uint32_t house){
 if(!p||!own(p,house)||kind(p)<0||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||!featureObjectAllowed(featuretypes::MissileDefense,p))return false;
 if(formationOwns(get<uint32_t>(p+0x10))||missileManuallyOwned(p))return false;
 int at=missileRecord(get<uint32_t>(p+0x10));bool held=at>=0&&missileSends[at].held&&missileSends[at].token==manualVersions.get(get<uint32_t>(p+0x10));
 const int m=get<int>(p+0xAC),q=get<int>(p+0xB4);
 if((m!=1&&m!=5&&m!=6&&m!=11&&m!=13&&!(m==0&&held))||(q!=-1&&q!=m)||orderIndex(p)>=0||burstOwns(get<uint32_t>(p+0x10))||dodgeOwns(get<uint32_t>(p+0x10)))return false;
 // Never commandeer an explicit non-air attack or a moving unit.
 uint32_t target=get<uint32_t>(p+0x2B4);
 if(m==1&&target&&!missileTarget(target,house))return false;
 if(kind(p)!=3&&get<uint32_t>(p+0x5A4))return false;
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
 if(GuardedCombatWeapon(p,q,&weapon,&slot,&close,&airborne)<=0||!airborne)return e;
 auto info=rawWeaponInfo(weapon,true);int armor=get<int>(typeOf(q,kind(q))+0x9C);
 if(armor<0||armor>10||!(info.flags&planner::AirWeapon)||!(info.selectMask&(1u<<armor))||
    (info.flags&(planner::HealingWeapon|planner::MindControlWeapon|planner::TemporalWeapon|planner::ParasiteWeapon|planner::LocomotorWeapon)))return e;
 double damage=double(get<int>(weapon+0xA4))*info.effectiveness[armor];if(damage<=0)return e;
 double dx=double(get<int>(q+0x9C))-get<int>(p+0x9C),dy=double(get<int>(q+0xA0))-get<int>(p+0xA0),dz=double(get<int>(q+0xA4))-get<int>(p+0xA4);
 double distance=std::sqrt(dx*dx+dy*dy+dz*dz);if(!std::isfinite(distance)||distance>1048576)return e;
 int eta=0;
 if(!close){
  int bonus=std::clamp(get<int>(typeOf(p,kind(p))+0x68C),-32768,32768);
  double range=std::max(0,info.range+bonus),gap=distance-range;
  // A short surrounding band buys time for Sleep delivery. Native range is
  // never inferred from this approximation, including IFV and elevated targets.
  if(gap>1024||distance<info.minimum)return e;
  const MissileMotion* motion=nullptr;for(const auto&m:missileMotion)if(m.id==get<uint32_t>(q+0x10)&&m.pointer==q){motion=&m;break;}
  if(motion&&motion->known){double closing=distance>0?-(dx*motion->vx+dy*motion->vy+dz*motion->vz)/distance:0;
   if(closing<=0.5)return e;eta=std::max(1,int(std::ceil(std::max(0.0,gap)/closing)));if(eta>missilecompute::LookAhead)return e;
  }else eta=missilecompute::LookAhead;
 }
 e.damage=int(std::min(damage+0.999999,1000000.0));e.distance=uint32_t(std::min(distance*distance,double(UINT32_MAX)));e.eta=eta;e.close=close;return e;
}
static void pruneMissileSends(uint32_t now){
 for(int n=0;n<missileSendCount;){auto&s=missileSends[n];uint32_t p=worldByID(s.id);
  if(p!=s.pointer||!p||!own(p,lastHouse)||manualVersions.get(s.id)!=s.token){missileSends[n]=missileSends[--missileSendCount];continue;}
  int m=get<int>(p+0xAC);bool observed=s.mission>=0&&m==s.mission&&(m!=1||get<uint32_t>(p+0x2B4)==worldByID(s.target));
  if(observed){if(m!=0){if(s.held)s.releasedAt=get<int>(0xA8ED84);s.held=false;s.holdStart=-1;}s.mission=-1;}
  if(s.mission>=0&&DWORD(now-s.tick)>=350&&get<int>(0xA8ED84)-s.frame>=12)s.mission=-1;
  if(!s.held&&s.mission<0&&(s.releasedAt<0||get<int>(0xA8ED84)-s.releasedAt>=missilecompute::ReleaseGraceFrames)){missileSends[n]=missileSends[--missileSendCount];continue;}++n;
 }publishMissileHeld();
}
static bool submitMissile(uint32_t p,int action,uint32_t target,uint32_t house,int frame,DWORD now){
 uint32_t id=get<uint32_t>(p+0x10);int at=missileRecord(id);if(at<0&&missileSendCount>=256)return false;
 if(!planningRoom(2)||!outgoingRoom())return false;
 if(!nativeMission(house,id,action,target,0,0,0,0,taskarbiter::Owner::MissileDefense))return false;
 consumePlanning(2);++sh->missileIssued;if(at<0){at=missileSendCount++;missileSends[at]={};}
 bool held=missileSends[at].held||action==0;int holdStart=missileSends[at].holdStart,releasedAt=missileSends[at].releasedAt;if(action==0&&holdStart<0)holdStart=frame;
 missileSends[at]={id,p,manualVersions.get(id),target,now,frame,action,held,holdStart,releasedAt};publishMissileHeld();return true;
}
static bool releaseMissileHolds(uint32_t house,int frame,DWORD now){
 pruneMissileSends(now);
 for(int n=0;n<missileSendCount;++n){auto&s=missileSends[n];if(!s.held||s.mission==5||s.mission==1)continue;
  uint32_t p=worldByID(s.id);if(!p)continue;
  // Queue Guard after a pending Sleep as well, so disable cannot strand a unit.
  if(!submitMissile(p,5,0,house,frame,now))break;
 }
 return missileHoldCount==0;
}
static void applyMissileResult(const missilecompute::Result&r,uint32_t house,int frame,DWORD now){
 if(r.match!=missileInput.match||r.tick!=missileInput.tick||r.frame!=missileInput.frame||r.count<0||r.count>missilecompute::MaxOrders)return;
 if(missileSettings!=(sh->missileScope|(sh->missileAllocation<<8)))return;
 for(int n=0;n<r.count;++n){
  if(!sh->missileEnabled||snap.chat||!freshness::valid(r.match,match,r.tick,r.frame,sampleClock(sh),frame,freshness::Tactical))break;
  int si=r.orders[n].source,ti=r.orders[n].target,action=r.orders[n].action;
  if(si<0||si>=missileInput.count||(action!=0&&action!=1&&action!=5))continue;
  auto&s=missileInput.sources[si];uint32_t p=worldByID(s.id);
  if(p!=s.pointer||manualVersions.get(s.id)!=s.token)continue;
  int at=missileRecord(s.id);if(at>=0&&missileSends[at].mission>=0)continue;
  uint32_t q=0;
  if(action==5){if(at<0||!missileSends[at].held)continue;}
  else {
   if(!missileSource(p,house))continue;
   if(action==1){if(ti<0||ti>=missileInput.targetCount)continue;auto&t=missileInput.targets[ti];q=worldByID(t.id);
    if(q!=t.pointer||!missileTarget(q,house))continue;auto e=missileEdge(p,q);if(!e.damage||!e.close)continue;
    if(get<int>(p+0xAC)==1&&get<uint32_t>(p+0x2B4)==q&&!(at>=0&&missileSends[at].held))continue;
   }else {if(ti<0||ti>=missileInput.targetCount)continue;const auto&t=missileInput.targets[ti];uint32_t v=worldByID(t.id);if(v!=t.pointer||!missileTarget(v,house))continue;auto edge=missileEdge(p,v);if(!edge.damage||edge.close||edge.eta>missilecompute::LookAhead)continue;}
  }
  if(!submitMissile(p,action,q?get<uint32_t>(q+0x10):0,house,frame,now))break;
 }
}
static bool missileControlTick(uint32_t house,int frame,DWORD now){
 pruneMissileSends(now);
 if(!sh->missileEnabled){missilePending=false;releaseMissileHolds(house,frame,now);if(!missileSendCount)resetMissileControl();return false;}
 if(snap.chat||!simulationReady())return false;
 uint32_t settings=sh->missileScope|(sh->missileAllocation<<8);
 if(settings!=missileSettings){missilePending=missileScanned=false;missileFrame=-1;missileSettings=settings;}
 if(missilePending){
  if(!freshness::valid(missileInput.match,match,missileInput.tick,missileInput.frame,now,frame,freshness::Tactical))missilePending=false;
  else if(sh->missileComputed==missileSerial){long seq=sh->missileResultSeq;if(seq&1)return false;MemoryBarrier();memcpy(&missileResult,&sh->missileResult,sizeof missileResult);MemoryBarrier();
   if(seq!=sh->missileResultSeq)return false;missilePending=false;applyMissileResult(missileResult,house,frame,now);
  }else return false;
 }
 if(frame==missileFrame)return false;
 if(missileScanned&&DWORD(now-missileScanTick)<1000u/std::clamp(uint32_t(sh->missileScanHz),1u,60u))return false;
 missileFrame=frame;missileScanTick=now;missileScanned=true;
 auto&i=missileInput;i={};i.match=match;i.tick=now;i.frame=frame;i.mode=sh->missileAllocation;
 Vec all=vec(0xA8EC78);int visited=0;
 for(;visited<all.count&&i.targetCount<64;++visited){uint32_t q=item(all,(missileTargetCursor+visited)%all.count);if(!missileTarget(q,house))continue;
  sampleMissileMotion(q,frame);uint32_t victim=worldObjects.contains(get<uint32_t>(q+0x2B4))?get<uint32_t>(q+0x2B4):0;
  i.targets[i.targetCount++]={get<uint32_t>(q+0x10),q,get<int>(q+0x6C),uint32_t(victim&&own(victim,house)?(std::abs(int64_t(get<int>(q+0x9C))-get<int>(victim+0x9C))+std::abs(int64_t(get<int>(q+0xA0))-get<int>(victim+0xA0))<=3*256?2:1):0)};
 }if(all.count)missileTargetCursor=(missileTargetCursor+visited)%all.count;
 auto add=[&](uint32_t p){if(i.count>=64||!p)return;for(int n=0;n<i.count;++n)if(i.sources[n].pointer==p)return;
  uint32_t id=get<uint32_t>(p+0x10);int at=missileRecord(id);bool held=at>=0&&missileSends[at].held;
  if(!held&&!missileSource(p,house))return;
  int n=i.count++;auto&s=i.sources[n];s={id,p,manualVersions.get(id),rawObjectID(get<uint32_t>(p+0x2B4)),0};
  s.cooldown=remaining(get<int>(p+0x2EC),get<int>(p+0x2F4),frame);s.held=held;
  if(at>=0){auto&record=missileSends[at];s.pending=record.target;s.pendingMission=record.mission;s.holdFrames=record.holdStart>=0?std::max(0,frame-record.holdStart):0;s.releaseFrames=record.releasedAt>=0?std::max(0,missilecompute::ReleaseGraceFrames-(frame-record.releasedAt)):0;}
  if(missileSource(p,house))for(int t=0;t<i.targetCount;++t)i.edges[n][t]=missileEdge(p,i.targets[t].pointer);
 };
 // Held actors are always revisited, including when targets vanish/filter out.
 for(int n=0;n<missileSendCount;++n)if(missileSends[n].held)add(worldByID(missileSends[n].id));
 visited=0;if(i.targetCount)for(;visited<all.count&&i.count<64;++visited)add(item(all,(missileSourceCursor+visited)%all.count));
 if(all.count)missileSourceCursor=(missileSourceCursor+visited)%all.count;
 sh->missileSources=i.count;sh->missileTargets=i.targetCount;if(!i.count)return true;
 if(compute::inside(sh->computeApplied,compute::MissileDefense))applyMissileResult(missilecompute::calculate(i),house,frame,now);
 else {InterlockedIncrement(&sh->missileInputSeq);MemoryBarrier();memcpy(&sh->missileInput,&i,sizeof i);missileSerial=sh->missileTask+1;sh->missileTask=missileSerial;MemoryBarrier();InterlockedIncrement(&sh->missileInputSeq);missilePending=true;notifyHost();}
 return true;
}
