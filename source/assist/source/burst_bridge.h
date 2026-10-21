// Only the common GameRuntime tick collects and submits these commands.
struct BurstJob {uint32_t serial=0,id=0,pointer=0,token=0,target=0,targetPointer=0;int phase=0,sentFrame=0,fireStamp=0;};
static BurstJob burstJobs[256];static int burstCursor,burstFrame=-1;
static uint32_t burstSerial,burstTask;static bool burstPending;
static burstcompute::Input burstInput;static burstcompute::Result burstResult;
static int burstIndex(uint32_t id){for(int n=0;n<burstCount;++n)if(burstJobs[n].id==id)return n;return -1;}
static bool burstOwns(uint32_t id){return burstIndex(id)>=0;}
static void cancelBurst(uint32_t id){int n=burstIndex(id);if(n>=0)burstJobs[n]=burstJobs[--burstCount];}
static void resetBurstControl(){burstCount=burstCursor=0;burstFrame=-1;burstPending=false;if(sh)sh->burstActive=sh->burstIssued=0;}
static bool burstEnabled(uint32_t p){
 if(!refreshFeatureTypes())return false;
 const bool global=sh->burstEnabled&&featureObjectAllowed(featuretypes::BurstRefresh,p);
 const bool held=sh->burstHoldEnabled&&sh->burstHeld&&sh->foreground&&!snap.chat&&DWORD(sampleClock(sh)-sh->heartbeat)<=250&&get<bool>(p+0x83);
 return global||held;
}
static bool burstObserve(uint32_t p,uint32_t q,uint32_t house,burstcompute::Source&s){
 if(!p||!q||!own(p,house)||!live(p)||!nativeEnemy(q,house)||dodgeOwns(get<uint32_t>(p+0x10)))return false;
 int k=kind(p),tk=kind(q);if(k<0||tk<0||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||get<uint32_t>(q+0x11C)||get<int>(q+0x220))return false;
 planner::Actor source{},target{};rawCombatInfo(p,typeOf(p,k),k,source,true);rawCombatInfo(q,typeOf(q,tk),tk,target,false);
 if(source.flags&(planner::Manipulated|planner::OutOfAmmo|planner::SustainedFire))return false;
 uint32_t weapon=0;int slot=0,close=0,air=0;if(GuardedCombatWeapon(p,q,&weapon,&slot,&close,&air)<=0)return false;
 source.flags|=planner::FixedWeapon;source.weapons[0]=rawWeaponInfo(weapon,true);if(air)target.flags|=planner::Airborne;else target.flags&=~planner::Airborne;
 if(!planner::compatibleTarget(source,target,false)||source.weapons[0].flags&(planner::HealingWeapon|planner::MindControlWeapon|planner::TemporalWeapon|planner::ParasiteWeapon|planner::LocomotorWeapon))return false;
 s.id=get<uint32_t>(p+0x10);s.pointer=p;s.token=manualVersions.get(s.id);s.target=get<uint32_t>(q+0x10);s.targetPointer=q;s.weapon=weapon;
 s.frame=get<int>(0xA8ED84);s.mission=get<int>(p+0xAC);s.queued=get<int>(p+0xB4);s.currentTarget=rawObjectID(get<uint32_t>(p+0x2B4));
 s.burst=get<int>(weapon+0x9C);s.index=get<int>(p+0x3B8);s.fireStamp=get<int>(p+0x2EC);s.inRange=close;s.eligible=burstEnabled(p);
 s.moving=s.mission==2||s.mission==29||s.queued==2||s.queued==29||(k!=3&&get<uint32_t>(p+0x5A4));
 int order=orderIndex(p);if(order>=0&&nativeOrders[order].mode!=planner::OrdinaryMove)return false;
 return s.burst>=0&&s.burst<=256&&s.index>=0&&s.index<=256;
}
static void applyBurstResult(const burstcompute::Result&r,uint32_t house,int frame,DWORD now){
 if(r.match!=burstInput.match||r.tick!=burstInput.tick||r.frame!=burstInput.frame||r.count!=burstInput.count||r.count<0||r.count>burstcompute::MaxSources)return;
 for(int n=0;n<r.count;++n){
  if(!freshness::valid(r.match,match,r.tick,r.frame,now,frame,freshness::Tactical)||snap.chat)break;
  const auto&old=burstInput.sources[n];uint32_t p=worldByID(old.id),q=worldByID(old.target);int j=burstIndex(old.id);
  if(p!=old.pointer||q!=old.targetPointer||manualVersions.get(old.id)!=old.token){cancelBurst(old.id);continue;}
  if((old.job&&(j<0||burstJobs[j].serial!=old.job||burstJobs[j].phase!=old.phase))||(!old.job&&j>=0))continue;
  burstcompute::Source s{};if(!burstObserve(p,q,house,s)){cancelBurst(old.id);continue;}
  s.job=old.job;s.phase=old.phase;s.sentFrame=old.sentFrame;s.previousFire=old.previousFire;
  auto fresh=burstcompute::decide(s);const auto&d=r.decisions[n];
  if(d.done&&fresh.done){cancelBurst(old.id);continue;}
  if(d.mission<0||d.mission!=fresh.mission)continue;
  if(d.mission==13&&(s.weapon!=old.weapon||!s.eligible||burstCount>=256))continue;
  if(!planningRoom(3)||!outgoingRoom())break;
  if(nativeMission(house,s.id,d.mission,d.mission==1?s.target:0,0,0,0,0,taskarbiter::Owner::BurstRefresh)){
   consumePlanning(3);++sh->burstIssued;
   if(j<0){j=burstCount++;burstJobs[j]={++burstSerial,s.id,p,s.token,s.target,q,1,frame,s.fireStamp};}
   else {burstJobs[j].phase=2;burstJobs[j].sentFrame=frame;}
  }
 }
}
static bool burstControlTick(uint32_t house,int frame,DWORD now){
 if(!sh->burstEnabled&&!sh->burstHoldEnabled&&!burstCount){burstPending=false;return false;}
 if(snap.chat||!simulationReady())return false;
 for(int n=0;n<burstCount;){auto&j=burstJobs[n];uint32_t p=worldByID(j.id);
  if(p!=j.pointer||manualVersions.get(j.id)!=j.token||worldByID(j.target)!=j.targetPointer||!own(p,house))burstJobs[n]=burstJobs[--burstCount];else ++n;}
 if(burstPending){
  if(!freshness::valid(burstInput.match,match,burstInput.tick,burstInput.frame,now,frame,freshness::Tactical))burstPending=false;
  else if(sh->burstComputed==burstTask){long seq=sh->burstResultSeq;if(seq&1)return false;MemoryBarrier();burstResult=sh->burstResult;MemoryBarrier();if(seq!=sh->burstResultSeq)return false;burstPending=false;applyBurstResult(burstResult,house,frame,now);}
  else return false;
 }
 if(frame==burstFrame)return false;burstFrame=frame;auto&i=burstInput;i={};i.match=match;i.tick=now;i.frame=frame;
 // Pending restoration has priority; active jobs are bounded by the same scan.
 for(int n=0;n<burstCount&&i.count<burstcompute::MaxSources;){auto&j=burstJobs[n];burstcompute::Source s{};
  if(!burstObserve(j.pointer,j.targetPointer,house,s)){burstJobs[n]=burstJobs[--burstCount];continue;}s.job=j.serial;s.phase=j.phase;s.sentFrame=j.sentFrame;s.previousFire=j.fireStamp;i.sources[i.count++]=s;++n;}
 Vec all=vec(0xA8EC78);int visited=0;
 for(;visited<all.count&&i.count<burstcompute::MaxSources;++visited){uint32_t p=item(all,(burstCursor+visited)%all.count);if(!own(p,house)||burstOwns(get<uint32_t>(p+0x10))||!burstEnabled(p))continue;
  burstcompute::Source s{};if(!burstObserve(p,worldObjects.contains(get<uint32_t>(p+0x2B4))?get<uint32_t>(p+0x2B4):0,house,s)||burstcompute::decide(s).mission!=13)continue;i.sources[i.count++]=s;
 }if(all.count)burstCursor=(burstCursor+visited)%all.count;
 sh->burstActive=burstCount;if(!i.count)return true;
 if(compute::inside(sh->computeApplied,compute::BurstRefresh))applyBurstResult(burstcompute::calculate(i),house,frame,now);
 else {InterlockedIncrement(&sh->burstInputSeq);MemoryBarrier();sh->burstInput=i;burstTask=sh->burstTask+1;sh->burstTask=burstTask;MemoryBarrier();InterlockedIncrement(&sh->burstInputSeq);burstPending=true;notifyHost();}
 return true;
}
