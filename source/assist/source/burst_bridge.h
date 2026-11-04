// Only the common GameRuntime tick collects and submits these commands.
#include "burst_eligibility.h"
static bursteligibility::Source burstSource(uint32_t p){
 // This runs under GuardedAssistTick on the game's owning thread. These are
 // validated Techno/Type addresses, so avoid per-field ReadProcessMemory calls.
 int k=kind(p);return bursteligibility::inspect(p,typeOf(p,k),k,[](uint32_t at,void*out,size_t size){memcpy(out,(const void*)(uintptr_t)at,size);return true;});
}
struct BurstJob {uint32_t serial=0,id=0,pointer=0,token=0,target=0,targetPointer=0;int phase=0,sentFrame=0,fireStamp=0;}; // -1 waits only for the first shot
static BurstJob burstJobs[256];static int burstCursor,burstJobCursor,burstFrame=-1;
static uint32_t burstSerial,burstTask;static bool burstPending;
static DWORD burstSampleAt;
static burstcompute::Input burstInput;static burstcompute::Result burstResult;
static int burstIndex(uint32_t id){for(int n=0;n<burstCount;++n)if(burstJobs[n].id==id)return n;return -1;}
static bool burstOwns(uint32_t id){return burstIndex(id)>=0;}
static void cancelBurst(uint32_t id){int n=burstIndex(id);if(n>=0)burstJobs[n]=burstJobs[--burstCount];}
static void resetBurstControl(){burstCount=burstCursor=burstJobCursor=0;burstFrame=-1;burstSampleAt=0;burstPending=false;if(sh)sh->burstActive=sh->burstIssued=0;}
static bool burstEnabled(uint32_t p){
 if(!refreshFeatureTypes())return false;
 const auto source=burstSource(p);
 const bool global=sh->burstEnabled&&bursteligibility::automatic(source)&&featureObjectAllowed(featuretypes::BurstRefresh,p);
 const bool held=sh->burstHoldEnabled&&sh->burstHeld&&bursteligibility::held(source)&&sh->foreground&&!snap.chat&&DWORD(sampleClock(sh)-sh->heartbeat)<=250&&get<bool>(p+0x83);
 return global||held;
}
static bool burstObserve(uint32_t p,uint32_t q,uint32_t house,burstcompute::Source&s){
 if(!p||!q||!own(p,house)||!live(p)||!nativeEnemy(q,house)||dodgeOwns(get<uint32_t>(p+0x10)))return false;
 int k=kind(p),tk=kind(q);if(k<0||tk<0||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||get<uint32_t>(q+0x11C)||get<int>(q+0x220))return false;
 if(burstSource(p).managed)return false;
 planner::Actor source{},target{};rawCombatInfo(p,typeOf(p,k),k,source,true);rawCombatInfo(q,typeOf(q,tk),tk,target,false);
 if(source.flags&(planner::Manipulated|planner::OutOfAmmo|planner::SustainedFire))return false;
 uint32_t weapon=0;int slot=0,close=0,air=0;if(combatWeaponQuery(p,q,&weapon,&slot,&close,&air)<=0)return false;
 source.flags|=planner::FixedWeapon;source.weapons[0]=rawWeaponInfo(weapon,true);if(air)target.flags|=planner::Airborne;else target.flags&=~planner::Airborne;
 if(!planner::compatibleTarget(source,target,false)||source.weapons[0].flags&(planner::HealingWeapon|planner::MindControlWeapon|planner::TemporalWeapon|planner::ParasiteWeapon|planner::LocomotorWeapon))return false;
 s.id=get<uint32_t>(p+0x10);s.pointer=p;s.token=manualVersions.get(s.id);s.target=get<uint32_t>(q+0x10);s.targetPointer=q;s.weapon=weapon;
 s.frame=get<int>(0xA8ED84);s.mission=get<int>(p+0xAC);s.queued=get<int>(p+0xB4);s.currentTarget=rawObjectID(get<uint32_t>(p+0x2B4));
 s.burst=get<int>(weapon+0x9C);s.index=get<int>(p+0x3B8);s.fireStamp=get<int>(p+0x2EC);s.inRange=close;s.eligible=burstEnabled(p);
 s.moving=s.mission==2||s.mission==29||s.queued==2||s.queued==29||(k!=3&&get<uint32_t>(p+0x5A4));
 int order=orderIndex(p);if(order>=0&&nativeOrders[order].mode!=planner::OrdinaryMove)return false;
 return s.burst>=0&&s.burst<=256;
}
static void applyBurstResult(const burstcompute::Result&r,uint32_t house,int frame,DWORD now,bool synchronous=false){
 if(r.match!=burstInput.match||r.tick!=burstInput.tick||r.frame!=burstInput.frame||r.count!=burstInput.count||r.count<0||r.count>burstcompute::MaxSources)return;
 // Restore stopped units first, then admit new units, then repeat existing
 // cycles. A continuous cycle must not permanently occupy the whole budget.
 for(int pass=0;pass<3;++pass)for(int n=0;n<r.count;++n){
  int phase=burstInput.sources[n].phase;
  if((phase==1?0:phase<=0?1:2)!=pass)continue;
  if(!freshness::valid(r.match,match,r.tick,r.frame,now,frame,freshness::Tactical)||snap.chat)break;
  const auto&old=burstInput.sources[n];uint32_t p=worldByID(old.id),q=worldByID(old.target);int j=burstIndex(old.id);
  if(p!=old.pointer||q!=old.targetPointer||manualVersions.get(old.id)!=old.token){cancelBurst(old.id);continue;}
  if((old.job&&(j<0||burstJobs[j].serial!=old.job||burstJobs[j].phase!=old.phase))||(!old.job&&j>=0))continue;
  burstcompute::Source s{};if(synchronous)s=old;else if(!burstObserve(p,q,house,s)){cancelBurst(old.id);continue;}
  s.job=old.job;s.phase=old.phase;s.sentFrame=old.sentFrame;s.previousFire=old.previousFire;
  auto fresh=burstcompute::decide(s);const auto&d=r.decisions[n];
  if(d.done&&fresh.done){cancelBurst(old.id);continue;}
  if(d.mission<0||d.mission!=fresh.mission)continue;
  if(d.mission==13&&(s.weapon!=old.weapon||!s.eligible||(j<0&&burstCount>=256)))continue;
  if(!planningRoom(3)||!outgoingRoom())return;
  if(nativeMission(house,s.id,d.mission,d.mission==1?s.target:0,0,0,0,0,taskarbiter::Owner::BurstRefresh)){
   consumePlanning(3);++sh->burstIssued;
   if(j<0){j=burstCount++;burstJobs[j]={++burstSerial,s.id,p,s.token,s.target,q,1,frame,s.fireStamp};}
   else {burstJobs[j].phase=d.mission==13?1:2;burstJobs[j].sentFrame=frame;if(d.mission==13)burstJobs[j].fireStamp=s.fireStamp;}
  }
 }
}
static bool burstControlTick(uint32_t house,int frame,DWORD now){
 if(!sh->burstEnabled&&!sh->burstHoldEnabled&&!burstCount){burstPending=false;return false;}
 if(snap.chat||!simulationReady())return false;
 for(int n=0;n<burstCount;){auto&j=burstJobs[n];uint32_t p=worldByID(j.id);
  if(p!=j.pointer||manualVersions.get(j.id)!=j.token||worldByID(j.target)!=j.targetPointer||!own(p,house))burstJobs[n]=burstJobs[--burstCount];else ++n;}
 // This is a two-command sequencer, not a shot-detection planner. Run at the
 // common game-thread entry without waiting for a host calculation roundtrip.
 burstPending=false;if(frame==burstFrame&&DWORD(now-burstSampleAt)<4)return false;burstFrame=frame;burstSampleAt=now;
 auto&i=burstInput;i={};i.match=match;i.tick=now;i.frame=frame;
 // Rotate persistent cycles and reserve snapshot slots for newly attacking
 // units. Actual Attack restoration still has priority in applyBurstResult.
 int checked=0;
 while(checked<burstCount&&i.count<burstcompute::MaxSources/2){int n=(burstJobCursor+checked)%burstCount;auto&j=burstJobs[n];burstcompute::Source s{};
  if(!burstObserve(j.pointer,j.targetPointer,house,s)){burstJobs[n]=burstJobs[--burstCount];continue;}s.job=j.serial;s.phase=j.phase;s.sentFrame=j.sentFrame;s.previousFire=j.fireStamp;i.sources[i.count++]=s;++checked;}
 if(burstCount)burstJobCursor=(burstJobCursor+checked)%burstCount;
 Vec all=vec(0xA8EC78);int visited=0;
 for(;visited<all.count&&i.count<burstcompute::MaxSources;++visited){uint32_t p=item(all,(burstCursor+visited)%all.count);
  if(!p||get<int>(p+0xAC)!=1||!get<uint32_t>(p+0x2B4)||!own(p,house)||burstOwns(get<uint32_t>(p+0x10))||!burstEnabled(p))continue;
  burstcompute::Source s{};if(!burstObserve(p,worldObjects.contains(get<uint32_t>(p+0x2B4))?get<uint32_t>(p+0x2B4):0,house,s)||!s.eligible||!s.inRange||s.moving||s.burst<2||burstCount>=256)continue;
  auto&job=burstJobs[burstCount++];job={++burstSerial,s.id,p,s.token,s.target,s.targetPointer,-1,frame,s.fireStamp};
  s.job=job.serial;s.phase=-1;s.sentFrame=frame;s.previousFire=s.fireStamp;i.sources[i.count++]=s;
 }if(all.count)burstCursor=(burstCursor+visited)%all.count;
 sh->burstActive=burstCount;if(!i.count)return true;
 applyBurstResult(burstcompute::calculate(i),house,frame,now,true);
 return true;
}
