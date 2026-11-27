// This is a single-player, local-owner event extension. No rule writes,
// input suppression, code patches, or alternate event receiver.
static offlinetools::Tickets offlineToolTickets;
static bool offlineToolVehicleCell(uint32_t p){
 int x=0,y=0;if(!readLiveWorld(p+0x9C,&x,4)||!readLiveWorld(p+0xA0,&y,4))return false;
 uint32_t c=readMapCell(x/256,y/256),q=0;if(!c||!readLiveWorld(c+0xE4,&q,4))return false;
 for(unsigned n=0;q>=0x10000&&n<256;++n){uint32_t vt=0,next=0;if(!readLiveWorld(q,&vt,4))return false;
  if(vt==0x7E3EBC)return true;
  if(!readLiveWorld(q+0x30,&next,4)||q==next)return false;q=next;
 }return false;
}
static bool offlineToolTarget(offlinetools::Mode mode,uint32_t p,uint32_t house,uint32_t&id,bool&repairing,bool strict){
 uint32_t owner=0,vt=0;int hp=0;uint8_t alive=0,onMap=0,limbo=1;
 if(p<0x10000||!readLiveWorld(p,&vt,4)||!readLiveWorld(p+0x10,&id,4)||!id||
    !readLiveWorld(p+0x21C,&owner,4)||owner!=house||!readLiveWorld(p+0x6C,&hp,4)||hp<=0||
    !readLiveWorld(p+0x74,&alive,1)||!readLiveWorld(p+0x81,&limbo,1)||!readLiveWorld(p+0x90,&onMap,1)||!alive||limbo||!onMap)return false;
 if(strict&&(!worldObjects.contains(p)||worldByID(id)!=p))return false;
 if(mode==offlinetools::Repair&&vt==0x7E3EBC){
  uint32_t t=0,tvt=0;int maximum=0;uint8_t active=0,repairable=0,clickable=0;
  if(!readLiveWorld(p+0x520,&t,4)||t<0x10000||!readLiveWorld(t,&tvt,4)||tvt!=0x7E4570||
     !readLiveWorld(t+0xA0,&maximum,4)||!readLiveWorld(t+0xCCC,&repairable,1)||!readLiveWorld(t+0x157A,&clickable,1)||
     !readLiveWorld(p+0x6E8,&active,1)||repairable>1||clickable>1||active>1)return false;
  repairing=active!=0;return offlinetools::repairTarget(hp,maximum,repairing,repairable!=0,clickable!=0);
 }
 if(mode==offlinetools::Sell&&vt==0x7F5C70){uint32_t drained=0;
  if(!readLiveWorld(p+0x1D0,&drained,4))return false;
  return offlinetools::sellTarget(true,true,drained!=0,strict?vehicleSellCell(p):offlineToolVehicleCell(p));
 }return false;
}
static void offlineToolPreview(uint32_t house,int frame,DWORD now){
 if(!sh->offlineToolMask||!nativeReady()||!offlinetools::offline(snap.session))return;
 const auto seq=sh->offlineToolPointSeq;static uint32_t seenMatch=0,seenOwner=0;static long seen=-1;
 if((seq&1)||(!seq)||(seen==seq&&seenMatch==match&&seenOwner==sh->owner))return;
 MemoryBarrier();const auto p=sh->offlineToolPoint;MemoryBarrier();if(seq!=sh->offlineToolPointSeq)return;
 seen=seq;seenMatch=match;seenOwner=sh->owner;
 offlinetools::Hint hint{};auto mode=offlinetools::Mode(p.tool);nativeviewportprojection::Transform transform{};
 if(p.match==match&&p.house==house&&p.frame>=0&&p.frame<=frame&&DWORD(now-p.tick)<=100&&
    (mode==offlinetools::Repair||mode==offlinetools::Sell)&&(sh->offlineToolMask&mode)&&
    offlinetools::current(readLiveWorld)==mode&&offlinetools::denied(mode,readLiveWorld)&&
    nativeViewTransform(transform)&&transform.generation==p.generation){
  pointerinput::Intent i{};i.x=p.x;i.y=p.y;i.width=p.width;i.height=p.height;i.cameraX=p.cameraX;i.cameraY=p.cameraY;
  const auto oldTarget=pointerTarget,oldID=pointerTargetID,oldCell=pointerCell;const int oldZ=pointerGroundZ;
  if(pointerLocation(i,nullptr,false)){
   uint32_t id=0;bool repairing=false;if(offlineToolTarget(mode,pointerTarget,house,id,repairing,false))hint={p,pointerTarget,id};
  }
  pointerTarget=oldTarget;pointerTargetID=oldID;pointerCell=oldCell;pointerGroundZ=oldZ;
 }
 InterlockedIncrement(&sh->offlineToolHintSeq);sh->offlineToolHint=hint;MemoryBarrier();InterlockedIncrement(&sh->offlineToolHintSeq);
}
static void offlineToolClick(const pointerinput::Intent&i,DWORD now,int frame){
 const auto mode=i.operation==pointerinput::OfflineRepair?offlinetools::Repair:offlinetools::Sell;
 if(!nativeReady()||!offlinetools::offline(snap.session)||!(sh->offlineToolMask&mode)||i.match!=match||i.house!=lastHouse||
    i.inputSerial!=sh->pointer.cancelSerial||i.count!=1||i.bindings!=pointerinput::LeftClick||
    i.frame<0||frame<i.frame||DWORD(now-i.tick)>150||
    offlinetools::current(readLiveWorld)!=mode||!offlinetools::denied(mode,readLiveWorld)||!pointerLocation(i)){
  pointerFinish(-4);return;
 }
 uint32_t id=0;bool repairing=false;
 if(!offlineToolTarget(mode,pointerTarget,lastHouse,id,repairing,true)||i.sources[0].id!=id||i.sources[0].pointer!=pointerTarget){
  pointerFinish(-4);return;
 }
 if(mode==offlinetools::Repair&&pendingRepairs.contains(id,pointerTarget)){pointerFinish(-4);return;}
 auto*ticket=offlineToolTickets.admit(match,id,pointerTarget,mode,repairing,frame,now);
 if(!ticket||(mode==offlinetools::Repair&&!repairing&&!pendingRepairs.room(id))){pointerFinish(-4);return;}
 const bool sent=mode==offlinetools::Repair?repairEvent(lastHouse,id):nativeObjectEvent(get<int>(lastHouse+0x30),0x16,id);
 if(!sent){pointerFinish(-6);return;}
 *ticket={id,pointerTarget,now,frame,repairing,mode};
 if(mode==offlinetools::Repair&&!repairing)pendingRepairs.queued(id,pointerTarget);
 sh->pointer.accepted=1;pointerFinish(1);
}
