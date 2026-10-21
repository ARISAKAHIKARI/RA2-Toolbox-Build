// Air evasion starts after an observed shot; crush evasion remains anticipatory.
// All evasion and restoration use ordinary synchronized mission events.
struct DodgeJob {uint32_t serial=0,id=0,pointer=0,token=0,destination=0,target=0,originalDestination=0;int sentFrame=0,moves=0,mission=5,tag=0;uint32_t shots[8]{};unsigned shotNext=0;};
static DodgeJob dodgeJobs[128];static int dodgeCursor,dodgeFrame=-1;static uint32_t dodgeSerial,dodgeTask;static bool dodgePending;
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
static bool dodgeOwns(uint32_t id){return dodgeIndex(id)>=0;}
static void cancelDodge(uint32_t id){int n=dodgeIndex(id);if(n>=0)dodgeJobs[n]=dodgeJobs[--dodgeCount];}
static void resetDodgeControl(){dodgeCount=dodgeCursor=0;dodgeFrame=-1;dodgePending=false;if(sh)sh->dodgeActive=sh->dodgeIssued=0;}
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
static void dodgeCells(uint32_t p,dodgecompute::Source&s){
 int cx=s.x/256,cy=s.y/256;uint32_t here=readMapCell(cx,cy);if(!here)return;int land=get<int>(here+0xEC);
 static const int dx[]={-1,0,1,-1,1,-1,0,1},dy[]={-1,-1,-1,0,0,1,1,1};
 for(int radius=1;radius<=2;++radius)for(int n=0;n<8;++n){int x=cx+dx[n]*radius,y=cy+dy[n]*radius;uint32_t cell=readMapCell(x,y);
  if(!cell||get<uint32_t>(cell+0x124)||get<uint32_t>(cell+0xE4)||get<int>(cell+0xEC)!=land)continue;
  s.options[s.cells++]={uint32_t(y*1000+x),x*256+128,y*256+128};
 }
}
static void dodgeObserve(uint32_t p,uint32_t house,dodgecompute::Source&s){
 s.id=get<uint32_t>(p+0x10);s.pointer=p;s.token=manualVersions.get(s.id);s.x=get<int>(p+0x9C);s.y=get<int>(p+0xA0);s.frame=get<int>(0xA8ED84);
 int j=dodgeIndex(s.id);if(j>=0){auto&job=dodgeJobs[j];s.job=job.serial;s.sentFrame=job.sentFrame;s.moves=job.moves;s.destination=job.destination;s.restoreMission=job.mission;s.restoreTarget=job.target;s.restoreDestination=job.originalDestination;s.restoreTag=job.tag;}
 else {
  int m=get<int>(p+0xAC);uint32_t q=get<uint32_t>(p+0x2B4),dest=get<uint32_t>(p+0x5A4);
  // If evasion interrupts our pending Stop, retain its original attack target.
  int bj=burstIndex(s.id);if(bj>=0)q=burstJobs[bj].targetPointer;
  if(q&&worldObjects.contains(q)&&nativeEnemy(q,house)){s.restoreMission=1;s.restoreTarget=get<uint32_t>(q+0x10);}
  else if((m==2||m==29)&&dest>=0x10000){int x=get<short>(dest+0x24),y=get<short>(dest+0x26);if(readMapCell(x,y)==dest){s.restoreMission=2;s.restoreTag=11;s.restoreDestination=uint32_t(y*1000+x);}}
 }
 if(sh->dodgeEnabled)for(int n=0;n<dodgeThreatCount&&s.count<dodgecompute::MaxThreats;++n){const auto&ref=dodgeThreatRefs[n];uint32_t q=ref.pointer;
  if(!ref.projectile){if(ref.target==p&&dodgeThreat(p,q,house))s.threats[s.count++]={get<uint32_t>(q+0x10),q,get<int>(q+0x9C),get<int>(q+0xA0),1};continue;}
  int x=get<int>(q+0x140),y=get<int>(q+0x144);
  int64_t dx=int64_t(x)-s.x,dy=int64_t(y)-s.y;
  if(dx < -8192||dx>8192||dy < -8192||dy>8192)continue;
  // Ground attacks point at a Cell; unit attacks retain their actual target.
  if(ref.target!=p&&(worldObjects.contains(ref.target)||ref.target!=readMapCell(s.x/256,s.y/256)))continue;
  double vx=get<double>(q+0xE8),vy=get<double>(q+0xF0);int ix=0,iy=0;
  if(vx>=-1000000&&vx<=1000000&&vy>=-1000000&&vy<=1000000){double ax=vx<0?-vx:vx,ay=vy<0?-vy:vy,scale=ax>ay?ax:ay;if(scale>0.001){ix=int(vx*256/scale);iy=int(vy*256/scale);}}
  uint32_t id=get<uint32_t>(q+0x10);bool seen=false;if(j>=0)for(auto shot:dodgeJobs[j].shots)if(shot==id)seen=true;
  if(id&&!seen)s.freshShot=1;
  s.threats[s.count++]={id,q,x,y,2,ix,iy};
 }
 if(s.count)dodgeCells(p,s);
}
static void applyDodgeResult(const dodgecompute::Result&r,uint32_t house,int frame,DWORD now){
 if(r.match!=dodgeInput.match||r.tick!=dodgeInput.tick||r.frame!=dodgeInput.frame||r.count!=dodgeInput.count||r.count<0||r.count>dodgecompute::MaxSources)return;
 for(int n=0;n<r.count;++n){if(!freshness::valid(r.match,match,r.tick,r.frame,now,frame,freshness::Tactical)||snap.chat)break;
  const auto&old=dodgeInput.sources[n];uint32_t p=worldByID(old.id);int j=dodgeIndex(old.id);
  if(p!=old.pointer||manualVersions.get(old.id)!=old.token||!dodgeSource(p,house)){cancelDodge(old.id);continue;}
  if((old.job&&(j<0||dodgeJobs[j].serial!=old.job))||(!old.job&&j>=0))continue;
  dodgecompute::Source s{};dodgeObserve(p,house,s);auto liveDecision=dodgecompute::decide(s);auto d=r.decisions[n];
  if(d.mission!=liveDecision.mission||d.target!=liveDecision.target||d.destination!=liveDecision.destination||d.done!=liveDecision.done)continue;
  if(d.mission<0){if(d.done)cancelDodge(s.id);continue;}
  if(d.mission==1){uint32_t q=worldByID(d.target);if(!q||!nativeEnemy(q,house)){cancelDodge(s.id);continue;}
   planner::Actor actor{},target{};rawCombatInfo(p,typeOf(p,kind(p)),kind(p),actor,true);rawCombatInfo(q,typeOf(q,kind(q)),kind(q),target,false);
   if(!(probeCombatPair(p,q,actor,target,0).flags&planner::InWeaponRange)){cancelDodge(s.id);continue;}}
  if(!d.done&&dodgeCount>=128&&j<0)continue;if(!planningRoom(4)||!outgoingRoom())break;
  if(nativeMission(house,s.id,d.mission,d.target,d.destination,d.tag,0,0,taskarbiter::Owner::Dodge)){
   consumePlanning(4);++sh->dodgeIssued;cancelBurst(s.id);
   if(d.done){cancelDodge(s.id);continue;}
   if(j<0){j=dodgeCount++;dodgeJobs[j]={++dodgeSerial,s.id,p,s.token,0,s.restoreTarget,s.restoreDestination,frame,0,s.restoreMission,s.restoreTag};}
   auto&job=dodgeJobs[j];if(s.freshShot)job.moves=0;
   for(int k=0;k<s.count;++k)if(s.threats[k].kind==2){uint32_t id=s.threats[k].id;bool seen=false;for(auto shot:job.shots)if(shot==id)seen=true;if(id&&!seen)job.shots[job.shotNext++%8]=id;}
   job.destination=d.destination;job.sentFrame=frame;++job.moves;
  }
 }
}
static bool dodgeControlTick(uint32_t house,int frame,DWORD now){
 if(!sh->dodgeEnabled&&!dodgeCount){dodgePending=false;return false;}if(snap.chat||!simulationReady())return false;collectDodgeThreats();
 for(int n=0;n<dodgeCount;){auto&j=dodgeJobs[n];uint32_t p=worldByID(j.id);if(p!=j.pointer||manualVersions.get(j.id)!=j.token||!dodgeSource(p,house))dodgeJobs[n]=dodgeJobs[--dodgeCount];else ++n;}
 if(dodgePending){if(!freshness::valid(dodgeInput.match,match,dodgeInput.tick,dodgeInput.frame,now,frame,freshness::Tactical))dodgePending=false;
  else if(sh->dodgeComputed==dodgeTask){long seq=sh->dodgeResultSeq;if(seq&1)return false;MemoryBarrier();dodgeResult=sh->dodgeResult;MemoryBarrier();if(seq!=sh->dodgeResultSeq)return false;dodgePending=false;applyDodgeResult(dodgeResult,house,frame,now);}else return false;}
 // The host may wake again after FireWeapon within this same logical frame.
 dodgeFrame=frame;auto&i=dodgeInput;i={};i.match=match;i.tick=now;i.frame=frame;
 for(int n=0;n<dodgeCount&&i.count<dodgecompute::MaxSources;++n)dodgeObserve(dodgeJobs[n].pointer,house,i.sources[i.count++]);
 Vec all=vec(0xA8EC78);int visited=0,checked=0;
 if(sh->dodgeEnabled)for(;visited<all.count&&i.count<dodgecompute::MaxSources&&checked<dodgecompute::MaxSources;++visited){uint32_t p=item(all,(dodgeCursor+visited)%all.count);if(!dodgeSource(p,house)||dodgeOwns(get<uint32_t>(p+0x10)))continue;++checked;
  dodgecompute::Source s{};dodgeObserve(p,house,s);if(s.count)i.sources[i.count++]=s;}
 if(all.count)dodgeCursor=(dodgeCursor+visited)%all.count;sh->dodgeActive=dodgeCount;if(!i.count)return true;
 if(compute::inside(sh->computeApplied,compute::Dodge))applyDodgeResult(dodgecompute::calculate(i),house,frame,now);
 else {InterlockedIncrement(&sh->dodgeInputSeq);MemoryBarrier();sh->dodgeInput=i;dodgeTask=sh->dodgeTask+1;sh->dodgeTask=dodgeTask;MemoryBarrier();InterlockedIncrement(&sh->dodgeInputSeq);dodgePending=true;notifyHost();}return true;
}
