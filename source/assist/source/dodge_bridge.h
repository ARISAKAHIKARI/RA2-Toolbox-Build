// Observe shots, then submit the original synchronized Scatter event (0x07).
// Native aircraft Scatter supplies attacker coordinates; EventClass Scatter
// supplies the engine's default coordinate. Both dispatch Scatter(...,true,false).
// Do not call Scatter locally: its destination/randomness must run on all peers.
struct DodgeJob {
 uint32_t serial=0,id=0,pointer=0,token=0;int sentFrame=0,quietFrame=0,lastSeen=0,x=0,y=0;
 bool active=false,observed=false,marker=false;uint32_t shots[32]{};unsigned shotNext=0;
 gateway::Receipt receipt{};
 int reverseFrame=-1;
};
static DodgeJob dodgeJobs[128];static int dodgeCursor,dodgeJobCursor;static uint32_t dodgeSerial,dodgeTask;static bool dodgePending;
struct DodgeThreatRef {uint32_t pointer,target;bool projectile;};static DodgeThreatRef dodgeThreatRefs[8192];static int dodgeThreatCount;
static void collectDodgeThreats(){
 dodgeThreatCount=0;
 // BulletClass::Array and fields are checked against YRpp and native
 // Construct 4664C0: owner B0, target 10C, damage 6C, type AC.
 Vec bullets=vec(0xA8ED40);
 for(int n=0;n<bullets.count&&dodgeThreatCount<8192;++n){uint32_t q=item(bullets,n);
  if(!q||get<uint32_t>(q)!=0x7E46E4||get<bool>(q+0x81)||!get<bool>(q+0x90)||get<int>(q+0x6C)<=0)continue;
  uint32_t owner=get<uint32_t>(q+0xB0);if(!worldObjects.contains(owner))continue;int k=kind(owner);
  if(k!=0&&k!=2)continue;uint32_t type=typeOf(owner,k);
  if(!type||get<bool>(type+0xD68)||(k!=0&&!get<bool>(owner+0x134)&&get<int>(owner+0x94)!=3))continue;
  dodgeThreatRefs[dodgeThreatCount++]={q,get<uint32_t>(q+0x10C),true};
 }
 Vec all=vec(0xA8EC78);for(int n=0;n<all.count&&dodgeThreatCount<8192;++n){uint32_t q=item(all,n);if(kind(q)!=2)continue;
  uint32_t t=get<uint32_t>(q+0x2B4);if(!worldObjects.contains(t))t=get<uint32_t>(q+0x5A4);if(worldObjects.contains(t))dodgeThreatRefs[dodgeThreatCount++]={q,t,false};}
}
static dodgecompute::Input dodgeInput;static dodgecompute::Result dodgeResult;
static int dodgeIndex(uint32_t id){for(int n=0;n<dodgeCount;++n)if(dodgeJobs[n].id==id)return n;return -1;}
static bool dodgeOwns(uint32_t id){int n=dodgeIndex(id);return n>=0&&dodgeJobs[n].active;}
static void finishDodge(DodgeJob&j,bool cancelled=false){
 if(!j.active)return;
 actorLeaseArbiter.release(match,j.id,j.receipt.serial);
 if(cancelled)gateway::cancelled(j.receipt,gateway::Failure::Superseded);
 else if(j.observed)gateway::finished(j.receipt);
 else {j.receipt.phase=gateway::Phase::Expired;j.receipt.failure=gateway::Failure::Stale;}
 recordReceipt(j.receipt);j.active=false;
}
static void cancelDodge(uint32_t id){int n=dodgeIndex(id);if(n>=0){finishDodge(dodgeJobs[n],true);dodgeJobs[n]=dodgeJobs[--dodgeCount];}}
static void resetDodgeControl(){dodgeCount=dodgeCursor=dodgeJobCursor=0;dodgePending=false;if(sh)sh->dodgeActive=sh->dodgeIssued=0;}
static bool dodgeSource(uint32_t p,uint32_t house){
 if(!p)return false;int k=kind(p);if((k!=1&&k!=2)||!own(p,house)||!live(p)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||!featureObjectAllowed(featuretypes::Dodge,p))return false;
 planner::Actor a{};rawCombatInfo(p,typeOf(p,k),k,a,false);if(a.flags&(planner::Airborne|planner::Manipulated|planner::Protected))return false;
 int m=get<int>(p+0xAC);return m==1||m==2||m==5||m==6||m==11||m==13||m==29;
}
static int dodgeThreat(uint32_t p,uint32_t q,uint32_t house){
 if(!q||q==p||!live(q)||get<uint32_t>(q+0x11C)||get<int>(q+0x220))return 0;
 int k=kind(q);uint32_t type=typeOf(q,k);if(!type)return 0;
 int64_t dx=int64_t(get<int>(p+0x9C))-get<int>(q+0x9C),dy=int64_t(get<int>(p+0xA0))-get<int>(q+0xA0);
 if(dx< -16384||dx>16384||dy< -16384||dy>16384)return 0;int64_t distance=dx*dx+dy*dy;
 uint32_t target=get<uint32_t>(q+0x2B4),dest=k==3?0:get<uint32_t>(q+0x5A4);
 // Original INI readers: Crusher +D28, OmniCrusher +D29, resistance +D2A;
 // ObjectType::Crushable +22D. No local rank/ability/ownership modifications.
 if(nativeEnemy(q,house)&&k==2&&(get<bool>(type+0xD28)||get<bool>(type+0xD29))&&distance<=int64_t(640)*640&&
    (target==p||dest==p)&&(get<int>(q+0xAC)==2||get<int>(q+0xAC)==1)){
  uint32_t ownType=typeOf(p,kind(p));if(get<bool>(type+0xD29)?!get<bool>(ownType+0xD2A):get<bool>(ownType+0x22D))return 1;
 }
 return 0;
}
static bool dodgeBusy(uint32_t p,int frame){
 // UnitClass::Scatter 743A50 checks PrimaryFacing::IsRotating (4C9480)
 // and Destination before choosing another cell. Read the same timer fields.
 bool rotating=get<short>(p+0x388+0x14)>0&&remaining(get<int>(p+0x388+8),get<int>(p+0x388+0x10),frame)>0;
 int queued=get<int>(p+0xB4);
 return rotating||get<uint32_t>(p+0x5A4)||queued==2||queued==29;
}
static bool dodgeSeen(const DodgeJob&j,uint32_t id){for(auto shot:j.shots)if(shot==id)return true;return false;}
static void dodgeRemember(DodgeJob&j,const dodgecompute::Source&s){
 if(s.count)j.lastSeen=s.frame;
 for(int k=0;k<s.count;++k)if(s.shots[k]&&!dodgeSeen(j,s.shots[k]))j.shots[j.shotNext++%32]=s.shots[k];
}
static void dodgeObserve(uint32_t p,uint32_t house,dodgecompute::Source&s){
 s.id=get<uint32_t>(p+0x10);s.pointer=p;s.token=manualVersions.get(s.id);s.frame=get<int>(0xA8ED84);s.busy=dodgeBusy(p,s.frame);
 int j=dodgeIndex(s.id);if(j>=0){const auto&job=dodgeJobs[j];s.job=job.serial;s.active=job.active;s.sentFrame=job.sentFrame;s.quietFrame=job.quietFrame;s.observed=job.observed;}
 int px=get<int>(p+0x9C),py=get<int>(p+0xA0);
 if(sh->dodgeEnabled)for(int n=0;n<dodgeThreatCount&&s.count<dodgecompute::MaxThreats;++n){const auto&ref=dodgeThreatRefs[n];uint32_t q=ref.pointer;
  if(!ref.projectile){if(ref.target==p&&dodgeThreat(p,q,house)){++s.count;s.freshThreat=1;}continue;}
  int64_t dx=int64_t(get<int>(q+0x140))-px,dy=int64_t(get<int>(q+0x144))-py;
  if(dx < -8192||dx>8192||dy < -8192||dy>8192)continue;
  if(ref.target!=p&&(worldObjects.contains(ref.target)||ref.target!=readMapCell(px/256,py/256)))continue;
  uint32_t id=get<uint32_t>(q+0x10);if(!id)continue;
  s.shots[s.count++]=id;if(j<0||!dodgeSeen(dodgeJobs[j],id))s.freshThreat=1;
 }
}
static int createDodge(const dodgecompute::Source&s){
 int n=dodgeIndex(s.id);if(n>=0)return n;
 if(dodgeCount==128){int oldest=-1;for(int k=0;k<dodgeCount;++k)if(!dodgeJobs[k].active&&(oldest<0||dodgeJobs[k].lastSeen<dodgeJobs[oldest].lastSeen))oldest=k;
  if(oldest<0)return -1;dodgeJobs[oldest]=dodgeJobs[--dodgeCount];}
 n=dodgeCount++;auto&j=dodgeJobs[n];j={};j.serial=++dodgeSerial;if(!j.serial)j.serial=++dodgeSerial;
 j.id=s.id;j.pointer=s.pointer;j.token=s.token;j.lastSeen=s.frame;return n;
}
static void updateDodgeJobs(uint32_t house,int frame,DWORD now){
 for(int n=0;n<dodgeCount;){auto&j=dodgeJobs[n];uint32_t p=worldByID(j.id);
  if(p!=j.pointer||manualVersions.get(j.id)!=j.token||!dodgeSource(p,house)||(!j.active&&(!sh->dodgeEnabled||frame-j.lastSeen>300))){cancelDodge(j.id);continue;}
  if(j.active){
   int x=get<int>(p+0x9C),y=get<int>(p+0xA0);bool moved=x!=j.x||y!=j.y;
   // Rotation alone may have preceded the command; destination/movement or
   // a changed native Scatter marker are evidence of later engine activity.
   if(!j.observed&&(moved||get<uint32_t>(p+0x5A4)||get<int>(p+0xB4)==2||(!j.marker&&get<bool>(p+0x6B2)))){
    j.observed=true;j.quietFrame=frame;gateway::observed(j.receipt,now);recordReceipt(j.receipt);
   }
   if(moved||dodgeBusy(p,frame))j.quietFrame=frame;
   j.x=x;j.y=y;
   // Keep lower-priority automation from interrupting a long turn/path.
   actorLeaseArbiter.record(match,frame,j.id,p,j.token,taskarbiter::Owner::Dodge,j.receipt.serial);
  }
  ++n;
 }
}
static void applyDodgeResult(const dodgecompute::Result&r,uint32_t house,int frame,DWORD now){
 if(r.match!=dodgeInput.match||r.tick!=dodgeInput.tick||r.frame!=dodgeInput.frame||r.count!=dodgeInput.count||r.count<0||r.count>dodgecompute::MaxSources)return;
 for(int n=0;n<r.count;++n){if(!freshness::valid(r.match,match,r.tick,r.frame,now,frame,freshness::Tactical)||snap.chat)break;
  const auto&old=dodgeInput.sources[n];uint32_t p=worldByID(old.id);int j=dodgeIndex(old.id);
  if(p!=old.pointer||manualVersions.get(old.id)!=old.token||!dodgeSource(p,house)){cancelDodge(old.id);continue;}
  if((old.job&&(j<0||dodgeJobs[j].serial!=old.job))||(!old.job&&j>=0))continue;
  dodgecompute::Source s{};dodgeObserve(p,house,s);auto liveDecision=dodgecompute::decide(s);auto d=r.decisions[n];
  if(d.scatter!=liveDecision.scatter||d.release!=liveDecision.release)continue;
  if(d.scatter){
   if(!planningRoom(4)||!outgoingRoom())break;
   j=createDodge(s);if(j<0)continue;
   gateway::Receipt receipt;
   if(!nativeObjectEvent(get<int>(house+0x30),0x07,s.id,OrderAuthority::Local,&receipt))continue;
   consumePlanning(4);++sh->dodgeIssued;cancelBurst(s.id);
   auto&job=dodgeJobs[j];job.active=true;job.observed=false;job.sentFrame=job.quietFrame=frame;
   job.x=get<int>(p+0x9C);job.y=get<int>(p+0xA0);job.marker=get<bool>(p+0x6B2);job.receipt=receipt;
  }else if(j<0&&(s.busy&&s.count))j=createDodge(s);
  if(j>=0){dodgeRemember(dodgeJobs[j],s);if(d.release)finishDodge(dodgeJobs[j]);}
 }
}
#include "reverse_dodge_bridge.h"
static bool dodgeControlTick(uint32_t house,int frame,DWORD now){
 if(!sh->dodgeEnabled&&!dodgeCount){dodgePending=false;return false;}if(snap.chat||!simulationReady())return false;
 collectDodgeThreats();updateDodgeJobs(house,frame,now);
 if(sh->dodgeEnabled&&sh->dodgeMode==0){dodgePending=false;return reverseDodgeTick(house,frame,now);}
 reverseWasEnabled=false;
 if(dodgePending){if(!freshness::valid(dodgeInput.match,match,dodgeInput.tick,dodgeInput.frame,now,frame,freshness::Tactical))dodgePending=false;
  else if(sh->dodgeComputed==dodgeTask){long seq=sh->dodgeResultSeq;if(seq&1)return false;MemoryBarrier();dodgeResult=sh->dodgeResult;MemoryBarrier();if(seq!=sh->dodgeResultSeq)return false;dodgePending=false;applyDodgeResult(dodgeResult,house,frame,now);}else return false;}
 auto&i=dodgeInput;i={};i.match=match;i.tick=now;i.frame=frame;
 // Rotate active jobs too: a pending batch must not starve the remaining units.
 int checkedJobs=0;
 for(;checkedJobs<dodgeCount&&i.count<dodgecompute::MaxSources/2;++checkedJobs){auto&j=dodgeJobs[(dodgeJobCursor+checkedJobs)%dodgeCount];if(j.active)dodgeObserve(j.pointer,house,i.sources[i.count++]);}
 if(dodgeCount)dodgeJobCursor=(dodgeJobCursor+checkedJobs)%dodgeCount;
 Vec all=vec(0xA8EC78);int visited=0,checked=0;
 if(sh->dodgeEnabled)for(;visited<all.count&&i.count<dodgecompute::MaxSources&&checked<dodgecompute::MaxSources;++visited){uint32_t p=item(all,(dodgeCursor+visited)%all.count);if(!dodgeSource(p,house)||dodgeOwns(get<uint32_t>(p+0x10)))continue;++checked;
  dodgecompute::Source s{};dodgeObserve(p,house,s);if(s.count)i.sources[i.count++]=s;}
 if(all.count)dodgeCursor=(dodgeCursor+visited)%all.count;sh->dodgeActive=0;for(int n=0;n<dodgeCount;++n)if(dodgeJobs[n].active)++sh->dodgeActive;
 if(!i.count)return true;
 if(compute::inside(sh->computeApplied,compute::Dodge))applyDodgeResult(dodgecompute::calculate(i),house,frame,now);
 else {InterlockedIncrement(&sh->dodgeInputSeq);MemoryBarrier();sh->dodgeInput=i;dodgeTask=sh->dodgeTask+1;sh->dodgeTask=dodgeTask;MemoryBarrier();InterlockedIncrement(&sh->dodgeInputSeq);dodgePending=true;notifyHost();}return true;
}
