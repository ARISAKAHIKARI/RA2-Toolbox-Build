// One explicit selected-unit order, using the existing game-thread event lane.
// All storage is static: do not value-initialize this large batch on the stack.
namespace ownground {
constexpr int Capacity=8192;
struct Source{uint32_t pointer=0,id=0,token=0,cell=0;int slot=0;bool self=false;};
struct Job{Source sources[Capacity]{};int count=0,next=0,sent=0,unsupported=0;uint32_t request=0,epoch=0,started=0;int frame=-1;bool pending=false,voiced=false;};
static Job job;
static void finish(int result){
 if(job.pending&&sh&&sh->request==job.request){sh->result=result;sh->count=job.sent;pendingAck=job.request;pendingAckReady=true;}
 job.pending=false;job.count=job.next=0;
}
}
static void resetOwnGround(){ownground::finish(-7);}
static bool ownGroundEligible(uint32_t p,uint32_t house){
 const int k=kind(p);
 return worldObjects.contains(p)&&own(p,house)&&live(p)&&(k==1||k==2)&&
  !get<uint32_t>(p+0x11C)&&!get<uint32_t>(p+0x2B0)&&!get<bool>(p+0x1C8);
}
static void ownGroundTick(uint32_t house,int frame,DWORD now){
 auto&j=ownground::job;if(!j.pending)return;
 if(!sh||sh->stop||fault||j.epoch!=match||sh->request!=j.request||DWORD(now-j.started)>3500||
    !simulationReady()||get<bool>(0xA8BC60+25)){ownground::finish(-7);return;}
 if(j.frame==frame)return;j.frame=frame;
 while(j.next<j.count){
  const auto&s=j.sources[j.next];
  if(!ownGroundEligible(s.pointer,house)||get<uint32_t>(s.pointer+0x10)!=s.id||
     worldByID(s.id)!=s.pointer||manualVersions.get(s.id)!=s.token||stickyOwns(s.pointer)||
     get<int>(s.pointer+0xAC)==6||get<int>(s.pointer+0xB4)==6){++j.next;continue;}
  if(s.self){
   // Direct firing is local simulation work, and is never used in MP. Re-read
   // the captured weapon and require the source still to occupy its own cell.
   if(!eventdelivery::isSingleplayer(get<int>(0xA8B238))){++j.unsupported;++j.next;continue;}
   const uint32_t type=typeOf(s.pointer,kind(s.pointer));
   const uint32_t weapon=type?rawWeapon(type,get<float>(s.pointer+0x150),s.slot):0;
   if(weapon<0x10000||get<int>(weapon+0xB4)!=0||get<int>(weapon+0xB8)!=0||get<bool>(weapon+0x134)||
      get<int>(s.pointer+0x9C)/256!=int(s.cell%1000)||get<int>(s.pointer+0xA0)/256!=int(s.cell/1000)){++j.next;continue;}
   // Share the existing frame budget even though this path emits no event.
   if(!outgoingRoom())break;
   --outgoingBudget;const int shot=ownGroundShot(s.pointer,s.slot,j.voiced);
   ++j.next;if(shot<0)++j.unsupported;
   if(shot>0)++j.sent;
   continue;
  }
  if(!outgoingRoom())break;
  // Only accepted explicit input supersedes automation. Remaining members
  // retain their original manual token while waiting for the shared budget.
  cancelNative(s.pointer,false);
  if(!nativeMission(house,s.id,1,s.cell,0,0,0,0,taskarbiter::Owner::Generic,nullptr,11)){
   ownground::finish(-6);return;
  }
  ++j.next;++j.sent;
  if(!j.voiced){j.voiced=true;inputFeedback(s.pointer,0,false);}
 }
 // Continuations never repeat the explicit command's acknowledgement.
 if(j.next==j.count)ownground::finish(j.unsupported?-43:1);
}
static void attackOwnGroundCommand(uint32_t house,DWORD now,uint32_t request){
 auto&j=ownground::job;
 j.count=j.next=j.sent=j.unsupported=0;j.request=request;j.epoch=match;j.started=now;j.frame=-1;j.pending=true;j.voiced=false;
 if(!eventdelivery::mayQueue(get<int>(0xA8B238))){ownground::finish(-40);return;}
 if(!simulationReady()||get<bool>(0xA8BC60+25)){ownground::finish(-11);return;}
 Vec selected=vec(0xA8ECB8);int sticky=0;
 if(selected.count>ownground::Capacity){ownground::finish(-42);return;}
 for(int i=0;i<selected.count;++i){
  const uint32_t p=item(selected,i);if(!ownGroundEligible(p,house))continue;
  if(stickyOwns(p)||get<int>(p+0xAC)==6||get<int>(p+0xB4)==6){++sticky;continue;}
  const uint32_t type=typeOf(p,kind(p));int slots[2]{};const int count=type?rawWeaponSlots(p,type,slots):0;
  if(!count)continue;
  const uint32_t weapon=rawWeapon(type,get<float>(p+0x150),slots[0]);if(weapon<0x10000)continue;
  const int gx=get<int>(p+0x9C),gy=get<int>(p+0xA0);if(gx<0||gy<0)continue;
  const int x=gx/256,y=gy/256;
  if(x<0||x>=512||y<0||y>=512||!readMapCell(x,y))continue;
  const uint32_t cell=uint32_t(y*1000+x);if(!cell)continue;
  // Ordinary zero-range origin is the unit's exact subcell coordinate.
  // AreaFire instead uses the base cell center in native IsCloseEnough.
  const bool self=get<int>(weapon+0xB4)==0&&get<int>(weapon+0xB8)==0&&!get<bool>(weapon+0x134);
  j.sources[j.count++]={p,get<uint32_t>(p+0x10),manualVersions.get(get<uint32_t>(p+0x10)),cell,slots[0],self};
 }
 if(!j.count){ownground::finish(sticky?-41:1);return;}
 ownGroundTick(house,get<int>(0xA8ED84),now);
}
