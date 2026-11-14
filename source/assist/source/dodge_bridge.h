#include "tactical_frame_read.h"
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
static DodgeJob dodgeJobs[512];static int dodgeCursor,dodgeJobCursor;static uint32_t dodgeSerial,dodgeTask;static bool dodgePending;
struct DodgeThreatRef {uint32_t pointer,target;bool projectile;uint32_t id;};static DodgeThreatRef dodgeThreatRefs[8192];static int dodgeThreatCount;
static bool collectDodgeThreats(){
 SampleReadScope frameRead;if(!frameRead)return false;
 static DodgeThreatRef next[8192];int count=0;
 // BulletClass::Array and fields are checked against YRpp and native
 // Construct 4664C0: owner B0, target 10C, damage 6C, type AC.
 Vec bullets{};if(!sampleRoster(0xA8ED40,bullets))return false;
 for(int n=0;n<bullets.count&&count<8192;++n){uint32_t q=item(bullets,n);
  if(!q||get<uint32_t>(q)!=0x7E46E4||get<bool>(q+0x81)||!get<bool>(q+0x90)||get<int>(q+0x6C)<=0)continue;
  uint32_t owner=get<uint32_t>(q+0xB0);if(!worldObjects.contains(owner))continue;int k=kind(owner);
  if(k!=0&&k!=2)continue;uint32_t type=typeOf(owner,k);
  if(!type||get<bool>(type+0xD68)||(k!=0&&!get<bool>(owner+0x134)&&get<int>(owner+0x94)!=3))continue;
  next[count++]={q,get<uint32_t>(q+0x10C),true,get<uint32_t>(q+0x10)};
 }
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;for(int n=0;n<all.count&&count<8192;++n){uint32_t q=item(all,n);if(kind(q)!=2)continue;
  uint32_t t=get<uint32_t>(q+0x2B4);if(!worldObjects.contains(t))t=get<uint32_t>(q+0x5A4);if(worldObjects.contains(t))next[count++]={q,t,false,get<uint32_t>(q+0x10)};}
 if(!frameRead.complete())return false;
 memcpy(dodgeThreatRefs,next,size_t(count)*sizeof(next[0]));dodgeThreatCount=count;return true;
}
// A sealed roster is a candidate list, not current permission to dereference a
// destroyed/reused projectile during native submission or effect observation.
static bool dodgeThreatCurrent(const DodgeThreatRef&ref){
 uint32_t id=0,vt=0;uint8_t limbo=1,alive=0;int hp=0;
 if(!ref.pointer||!ref.id||!read(ref.pointer+0x10,&id,sizeof id)||id!=ref.id)return false;
 if(!ref.projectile)return worldObjects.contains(ref.pointer)&&kind(ref.pointer)==2;
 return read(ref.pointer,&vt,sizeof vt)&&vt==0x7E46E4&&read(ref.pointer+0x81,&limbo,sizeof limbo)&&!limbo&&
  read(ref.pointer+0x90,&alive,sizeof alive)&&alive&&read(ref.pointer+0x6C,&hp,sizeof hp)&&hp>0;
}
static compute::FrameHistory<dodgecompute::Input> dodgeFrames;static uint32_t dodgeApplied=0;
static dodgecompute::Input dodgeInput,dodgeCapture;static dodgecompute::Result dodgeResult;
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
static void resetDodgeControl(){dodgeCount=dodgeCursor=dodgeJobCursor=0;dodgePending=false;dodgeFrames.clear();dodgeApplied=sh?sh->dodgeComputed:0;if(sh)sh->dodgeActive=sh->dodgeIssued=0;}
static bool dodgeSource(uint32_t p,uint32_t house){
 if(!p||stickyOwns(p))return false;int k=kind(p);if((k!=1&&k!=2)||!own(p,house)||!live(p)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||!featureObjectAllowed(featuretypes::Dodge,p))return false;
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
 if(sh->dodgeEnabled)for(int n=0;n<dodgeThreatCount&&s.count<dodgecompute::MaxThreats;++n){const auto&ref=dodgeThreatRefs[n];uint32_t q=ref.pointer;if(!dodgeThreatCurrent(ref))continue;
  if(!ref.projectile){if(dodgeThreat(p,q,house)){++s.count;s.freshThreat=1;}continue;}
  int64_t dx=int64_t(get<int>(q+0x140))-px,dy=int64_t(get<int>(q+0x144))-py;
  if(dx < -8192||dx>8192||dy < -8192||dy>8192)continue;
  const uint32_t target=get<uint32_t>(q+0x10C);
  if(target!=p&&(worldObjects.contains(target)||target!=readMapCell(px/256,py/256)))continue;
  uint32_t id=get<uint32_t>(q+0x10);if(!id)continue;
  s.shots[s.count++]=id;if(j<0||!dodgeSeen(dodgeJobs[j],id))s.freshThreat=1;
 }
}
static int createDodge(const dodgecompute::Source&s){
 int n=dodgeIndex(s.id);if(n>=0)return n;
 if(dodgeCount==512){int oldest=-1;for(int k=0;k<dodgeCount;++k)if(!dodgeJobs[k].active&&(oldest<0||dodgeJobs[k].lastSeen<dodgeJobs[oldest].lastSeen))oldest=k;
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
static void applyDodgeResult(const dodgecompute::Result&r,uint32_t house,int frame,DWORD now,const dodgecompute::Input*source=nullptr){
 const auto&input=source?*source:dodgeInput;
 if(r.match!=input.match||r.tick!=input.tick||r.frame!=input.frame||r.count!=input.count||r.count<0||r.count>dodgecompute::MaxSources)return;
 for(int n=0;n<r.count;++n){if(!freshness::valid(r.match,match,r.tick,r.frame,now,frame,freshness::Tactical)||snap.chat)break;
  const auto&old=input.sources[n];uint32_t p=worldByID(old.id);int j=dodgeIndex(old.id);
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
// Both dodge modes use the same Host calculation/publication path. The third
// retained cursor records the mode so an old async result cannot cross modes.
static void dispatchDodgeCalculation(uint32_t house,int frame,DWORD now,int nextCursor,int nextJobCursor,int mode){
 const auto&i=dodgeInput;if(!i.count)return;
 if(compute::inside(sh->computeApplied,compute::Dodge)){applyDodgeResult(dodgecompute::calculate(i),house,frame,now);return;}
 if(!externalInputFrame)return;
 InterlockedIncrement(&sh->dodgeInputSeq);MemoryBarrier();sh->dodgeInput=i;
 dodgeTask=compute::nextTask(sh->dodgeTask);dodgeFrames.retain(dodgeTask,i,{nextCursor,nextJobCursor,mode});
 sh->dodgeTask=dodgeTask;MemoryBarrier();InterlockedIncrement(&sh->dodgeInputSeq);dodgePending=true;notifyHost();
}
#include "reverse_dodge_bridge.h"
// Capture only facts. Native events, identity/lease checks and receipt changes
// run after this scope has restored live reads.
static bool collectDodgeInput(uint32_t house,int frame,DWORD now,int&nextCursor,int&nextJobCursor){
 SampleReadScope frameRead;if(!frameRead)return false;
 auto&i=dodgeCapture;memset(&i,0,sizeof i);i.match=match;i.tick=now;i.frame=frame;
 int checkedJobs=0;
 for(;checkedJobs<dodgeCount&&i.count<dodgecompute::MaxSources/2;++checkedJobs){const auto&j=dodgeJobs[(dodgeJobCursor+checkedJobs)%dodgeCount];
  if(j.active&&get<uint32_t>(j.pointer+0x10)==j.id)dodgeObserve(j.pointer,house,i.sources[i.count++]);
 }
 nextJobCursor=dodgeCount?(dodgeJobCursor+checkedJobs)%dodgeCount:0;
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;int visited=0,checked=0;
 if(sh->dodgeEnabled)for(;visited<all.count&&i.count<dodgecompute::MaxSources&&checked<dodgecompute::MaxSources;++visited){uint32_t p=item(all,(dodgeCursor+visited)%all.count);if(!dodgeSource(p,house)||dodgeOwns(get<uint32_t>(p+0x10)))continue;++checked;
  dodgecompute::Source source{};dodgeObserve(p,house,source);if(source.count)i.sources[i.count++]=source;
 }
 nextCursor=all.count?(dodgeCursor+visited)%all.count:0;
 if(!frameRead.complete())return false;
 dodgeInput=i;return true;
}
static bool dodgeControlTick(uint32_t house,int frame,DWORD now){
 if(!sh->dodgeEnabled&&!dodgeCount){dodgePending=false;return false;}if(snap.chat||!simulationReady())return false;
 const bool outside=!compute::inside(sh->computeApplied,compute::Dodge);
 bool threatsCollected=false;auto collect=[&]{if(!threatsCollected)threatsCollected=collectDodgeThreats();return threatsCollected;};
 // Effect/lease maintenance is live even when no complete raw frame is ready.
 updateDodgeJobs(house,frame,now);
 if(outside&&compute::newer(sh->dodgeComputed,dodgeApplied)){
  const uint32_t completed=sh->dodgeComputed;long seq=sh->dodgeResultSeq;
  if(!(seq&1)){MemoryBarrier();dodgeResult=sh->dodgeResult;MemoryBarrier();if(seq==sh->dodgeResultSeq&&completed==sh->dodgeComputed){
   const auto*source=dodgeFrames.find(completed);
   const auto*c=dodgeFrames.cursors(completed);const int mode=sh->dodgeEnabled&&sh->dodgeMode==0?0:1;
   if(!source||!c||(*c)[2]!=mode)dodgeApplied=completed;
   else if(collect()){dodgeApplied=completed;applyDodgeResult(dodgeResult,house,frame,now,source);if(mode){dodgeCursor=(*c)[0];dodgeJobCursor=(*c)[1];}}
  }}
 }
 if(sh->dodgeEnabled&&sh->dodgeMode==0){if(!collect())return false;return reverseDodgeTick(house,frame,now);}
 reverseWasEnabled=false;
 if(outside&&!externalInputFrame)return false;
 int nextCursor=0,nextJobCursor=0;
 if(!collect()||!collectDodgeInput(house,frame,now,nextCursor,nextJobCursor))return false;
 if(!outside){dodgeCursor=nextCursor;dodgeJobCursor=nextJobCursor;}
 sh->dodgeActive=0;for(int n=0;n<dodgeCount;++n)if(dodgeJobs[n].active)++sh->dodgeActive;
 dispatchDodgeCalculation(house,frame,now,nextCursor,nextJobCursor,1);return true;
}
