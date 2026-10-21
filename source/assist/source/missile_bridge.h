// Owned by the sole GameRuntime dispatcher. No new hooks or simulation writes.
extern "C" int GuardedMissileWeapon(unsigned,unsigned,unsigned*);
static missilecompute::Input missileInput;
static missilecompute::Result missileResult;
static bool missilePending,missileScanned;
static uint32_t missileSerial,missileScanTick;
static int missileSourceCursor,missileTargetCursor,missileFrame=-1;
struct MissilePending {uint32_t id=0,pointer=0,token=0,target=0,tick=0;int frame=0;};
static MissilePending missileSends[256];static int missileSendCount;
static void resetMissileControl(){
 missilePending=missileScanned=false;missileFrame=-1;missileSendCount=missileSourceCursor=missileTargetCursor=0;
 if(sh){sh->missileSources=sh->missileTargets=sh->missileIssued=0;}
}
static bool missileTarget(uint32_t p,uint32_t house){
 if(!p||kind(p)!=0||!nativeEnemy(p,house)||get<uint32_t>(p+0x11C)||get<int>(p+0x220))return false;
 uint32_t t=typeOf(p,0); // TechnoTypeClass::MissileSpawn; original INI reader 714F37.
 if(!t||!get<bool>(t+0xD68)||!featureTypeAllowed(featuretypes::MissileTarget,t))return false;
 return get<bool>(t+0x231)&&!get<bool>(t+0x233)&&!remaining(get<int>(p+0x18C),get<int>(p+0x194),get<int>(0xA8ED84));
}
static bool missileSource(uint32_t p,uint32_t house){
 if(!p||!own(p,house)||!live(p)||kind(p)<0||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||!featureObjectAllowed(featuretypes::MissileDefense,p))return false;
 const int m=get<int>(p+0xAC),q=get<int>(p+0xB4);
 // Do not stop movement, transport, deployment, or advanced click jobs.
 if((m!=1&&m!=5&&m!=6&&m!=11&&m!=13)||(q!=-1&&q!=m)||orderIndex(p)>=0||burstOwns(get<uint32_t>(p+0x10))||dodgeOwns(get<uint32_t>(p+0x10)))return false;
 if((sh->flags&AntiTheft)&&antiTheftBuilding(p)&&antiTheftThreat(get<uint32_t>(p+0x2B4),house))return false;
 if(remaining(get<int>(p+0x2EC),get<int>(p+0x2F4),get<int>(0xA8ED84))>6)return false;
 uint32_t t=typeOf(p,kind(p));if(!t||(get<int>(t+0x684)>=0&&get<int>(p+0x2FC)<=0))return false;
 planner::Actor a{};rawCombatInfo(p,t,kind(p),a,true,true);
 if(a.flags&planner::BurstFiring)return false;
 for(const auto&w:a.weapons)if((w.flags&planner::AirWeapon)&&!(w.flags&planner::HealingWeapon)&&w.range>0)return true;
 return false;
}
static int missileDamage(uint32_t p,uint32_t q){
 uint32_t weapon=0;if(!GuardedMissileWeapon(p,q,&weapon))return 0;
 auto info=rawWeaponInfo(weapon,true);int armor=get<int>(typeOf(q,0)+0x9C);
 if(armor<0||armor>10||!(info.flags&planner::AirWeapon)||!(info.selectMask&(1u<<armor))||
    (info.flags&(planner::HealingWeapon|planner::MindControlWeapon|planner::TemporalWeapon|planner::ParasiteWeapon|planner::LocomotorWeapon)))return 0;
 double damage=double(get<int>(weapon+0xA4))*info.effectiveness[armor];
 // One nominal shot is an allocation estimate, never evidence of a hit.
 return damage>0?int(std::min(damage+0.999999,1000000.0)):0;
}
static void pruneMissileSends(uint32_t now){
 for(int n=0;n<missileSendCount;){auto&s=missileSends[n];uint32_t p=worldByID(s.id);
  if(p!=s.pointer||manualVersions.get(s.id)!=s.token||!worldByID(s.target)||get<uint32_t>(p+0x2B4)==worldByID(s.target)||(DWORD(now-s.tick)>=350&&get<int>(0xA8ED84)-s.frame>=12))missileSends[n]=missileSends[--missileSendCount];else ++n;
 }
}
static void applyMissileResult(const missilecompute::Result&r,uint32_t house,int frame,DWORD now){
 if(r.match!=missileInput.match||r.tick!=missileInput.tick||r.frame!=missileInput.frame||r.count<0||r.count>missilecompute::MaxOrders)return;
 for(int n=0;n<r.count;++n){
  if(!sh->missileEnabled||snap.chat||!freshness::valid(r.match,match,r.tick,r.frame,sampleClock(sh),frame,freshness::Tactical))break;
  int si=r.orders[n].source,ti=r.orders[n].target;
  if(si<0||si>=missileInput.count||ti<0||ti>=missileInput.targetCount||missileInput.edges[si][ti].damage<=0)continue;
  auto&s=missileInput.sources[si];auto&t=missileInput.targets[ti];uint32_t p=worldByID(s.id),q=worldByID(t.id);
  if(p!=s.pointer||q!=t.pointer||manualVersions.get(s.id)!=s.token||!missileSource(p,house)||!missileTarget(q,house)||!missileDamage(p,q))continue;
  bool pending=false;for(int k=0;k<missileSendCount;++k)if(missileSends[k].id==s.id)pending=true;
  if(pending||missileSendCount>=256||get<uint32_t>(p+0x2B4)==q)continue;
  if(!planningRoom(2)||!outgoingRoom())break;
  if(nativeMission(house,s.id,1,t.id,0,0,0,0,taskarbiter::Owner::MissileDefense)){
   consumePlanning(2);++sh->missileIssued;missileSends[missileSendCount++]={s.id,p,s.token,t.id,now,frame};
  }
 }
}
static bool missileControlTick(uint32_t house,int frame,DWORD now){
 if(!sh->missileEnabled){if(missileScanned||missilePending)resetMissileControl();return false;}
 if(snap.chat||!simulationReady())return false;
 pruneMissileSends(now);
 if(missilePending){
  if(!freshness::valid(missileInput.match,match,missileInput.tick,missileInput.frame,now,frame,freshness::Tactical))missilePending=false;
  else if(sh->missileComputed==missileSerial){long seq=sh->missileResultSeq;if(seq&1)return false;MemoryBarrier();
   memcpy(&missileResult,&sh->missileResult,sizeof missileResult);MemoryBarrier();
   if(seq!=sh->missileResultSeq)return false;missilePending=false;applyMissileResult(missileResult,house,frame,now);
  }else return false;
 }
 if(frame==missileFrame)return false;
 if(missileScanned&&DWORD(now-missileScanTick)<1000u/std::clamp(uint32_t(sh->missileScanHz),1u,60u))return false;
 missileFrame=frame;missileScanTick=now;missileScanned=true;
 auto&i=missileInput;i={};i.match=match;i.tick=now;i.frame=frame;
 Vec all=vec(0xA8EC78);int visited=0;
 for(;visited<all.count&&i.targetCount<missilecompute::MaxTargets;++visited){uint32_t q=item(all,(missileTargetCursor+visited)%all.count);
  if(!missileTarget(q,house))continue;uint32_t victim=worldObjects.contains(get<uint32_t>(q+0x2B4))?get<uint32_t>(q+0x2B4):0;
  i.targets[i.targetCount++]={get<uint32_t>(q+0x10),q,get<int>(q+0x6C),uint32_t(victim&&own(victim,house))};
 }
 if(all.count)missileTargetCursor=(missileTargetCursor+visited)%all.count;
 if(i.targetCount){visited=0;
  for(;visited<all.count&&i.count<missilecompute::MaxSources;++visited){uint32_t p=item(all,(missileSourceCursor+visited)%all.count);
   if(!missileSource(p,house))continue;const int n=i.count++;auto&s=i.sources[n];s={get<uint32_t>(p+0x10),p,manualVersions.get(get<uint32_t>(p+0x10)),rawObjectID(get<uint32_t>(p+0x2B4)),0};
   for(int k=0;k<missileSendCount;++k)if(missileSends[k].id==s.id)s.pending=missileSends[k].target;
   for(int t=0;t<i.targetCount;++t){uint32_t q=i.targets[t].pointer;int damage=missileDamage(p,q);if(!damage)continue;
    int64_t dx=int64_t(get<int>(p+0x9C))-get<int>(q+0x9C),dy=int64_t(get<int>(p+0xA0))-get<int>(q+0xA0);
    if(dx< -1048576||dx>1048576||dy< -1048576||dy>1048576)continue;
    i.edges[n][t]={damage,uint32_t(std::min<int64_t>(dx*dx+dy*dy,UINT32_MAX))};
   }
  }if(all.count)missileSourceCursor=(missileSourceCursor+visited)%all.count;
 }
 sh->missileSources=i.count;sh->missileTargets=i.targetCount;
 if(!i.count||!i.targetCount)return true;
 if(compute::inside(sh->computeApplied,compute::MissileDefense))applyMissileResult(missilecompute::calculate(i),house,frame,now);
 else {InterlockedIncrement(&sh->missileInputSeq);MemoryBarrier();memcpy(&sh->missileInput,&i,sizeof i);
  missileSerial=sh->missileTask+1;sh->missileTask=missileSerial;MemoryBarrier();InterlockedIncrement(&sh->missileInputSeq);missilePending=true;notifyHost();}
 return true;
}
