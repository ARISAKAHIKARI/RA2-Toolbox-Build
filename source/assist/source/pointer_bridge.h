// Explicit input mailbox consumed ONLY inside the ordinary leased Assist tick.
// Original click, mission constructor and received-event functions stay intact.
static uint32_t pointerSeen,pointerCancelSeen,pointerTarget,pointerTargetID,pointerCell;
static int pointerGroundZ;static int pointerAt;static bool pointerActive;
static uint32_t pointerTokens[pointerinput::MaxSources];
static uint8_t pointerDone[pointerinput::MaxSources],pointerBatchFollow[pointerinput::MaxSources];
static uint32_t pointerBatchTickets[pointerinput::MaxSources],pointerBatchRequest;static bool pointerBatchValid;
static pointerinput::Intent pointerBatchCapture;
static void resetPointerInput(){
 pointerBatchValid=false;pointerBatchRequest=0;pointerActive=false;pointerSeen=pointerCancelSeen=pointerTarget=pointerTargetID=pointerCell=0;pointerAt=0;
 memset(pointerTokens,0,sizeof pointerTokens);memset(pointerDone,0,sizeof pointerDone);
}
static void pointerFinish(int result){
 auto&b=sh->pointer;pointerActive=false;if(result==-7)pointerBatchValid=false;b.result=result;b.rejected=b.intent.count-b.accepted;
 if(b.intent.operation==pointerinput::Sell)b.rejected=b.accepted?0:1;
 if(b.rejected<0)b.rejected=0;MemoryBarrier();b.ack=b.request;notifyHost();
}
static bool pointerLocation(const pointerinput::Intent&i){
 uint32_t tactical=get<uint32_t>(0x887324);if(tactical<0x10000)return false;
 if(i.width!=get<int>(0xA8EB84)||i.height!=get<int>(0xA8EB88)||
    i.cameraX!=get<int>(tactical+0xB0)||i.cameraY!=get<int>(tactical+0xB4))return false;
 int width=get<int>(0xB0CE30),height=get<int>(0xB0CE34);
 // ProcessClickCoords does not check upper bounds. Never query the sidebar.
 if(width<=0||height<=0||width>i.width||height>i.height)return false;
 if(i.x<0||i.y<0||i.x>=width||i.y>=height)return false;
 struct Point{int x,y;}point{i.x,i.y};struct Cell{short x,y;}cell{};
 struct Coord{int x,y,z;}coord{};uint32_t object=0;uint8_t a=0,b=0;
 // YRpp DisplayClass::ProcessClickCoords: thiscall, six pointer arguments.
 call<void>(0x692300,0x87F7E8,&point,&cell,&coord,&object,&a,&b);
 if(cell.x<=0||cell.y<=0||cell.x>=512||cell.y>=512||!readMapCell(cell.x,cell.y))return false;
 if(object&&!worldObjects.contains(object))return false;
 pointerGroundZ=coord.z;pointerCell=uint32_t(cell.y*1000+cell.x);pointerTarget=object;pointerTargetID=object?get<uint32_t>(object+0x10):0;return true;
}
static void pointerTick(){
 auto&b=sh->pointer;uint32_t request=b.request;MemoryBarrier();
 if(pointerCancelSeen!=b.cancelSerial){pointerCancelSeen=b.cancelSerial;
  if(pointerActive&&pointerSeen==request&&request!=b.ack)pointerFinish(-7);
 }
 if(request==b.ack)return;
 const auto&i=b.intent;DWORD now=sampleClock(sh);int frame=get<int>(0xA8ED84);
 if(!pointerActive||pointerSeen!=request){pointerActive=true;pointerSeen=request;pointerAt=0;b.accepted=b.rejected=0;memset(pointerDone,0,sizeof pointerDone);
  const bool next=(i.bindings&pointerinput::BatchNext)!=0,start=(i.bindings&pointerinput::BatchStart)!=0;
  const bool continuation=next&&!start&&pointerBatchValid&&request==pointerBatchRequest+1&&pointerinput::sameCapture(i,pointerBatchCapture);
  if(!nativeReady()||i.inputSerial!=b.cancelSerial||i.match!=match||i.house!=lastHouse||
     (next&&!continuation)||(next&&start)||DWORD(now-i.tick)>(continuation?1500u:150u)||frame<i.frame||frame-i.frame>(continuation?90:8)||
     i.count<0||i.count>pointerinput::MaxSources||i.operation<1||i.operation>9){pointerFinish(-7);return;}
  if(!continuation)pointerBatchValid=false;
  if(i.operation!=pointerinput::Cancel){
   bool shiftUnsupported=(i.bindings&pointerinput::Shift)&&(i.operation==pointerinput::FollowNative||(i.bindings&pointerinput::RightClick)||!(i.bindings&(pointerinput::Advanced|pointerinput::Magnetron|pointerinput::CS|pointerinput::Enter|pointerinput::AirFirst|pointerinput::AirSecond)));
   uint32_t tactical=get<uint32_t>(0x887324);
   bool location=continuation?(tactical>=0x10000&&i.width==get<int>(0xA8EB84)&&i.height==get<int>(0xA8EB88)&&i.cameraX==get<int>(tactical+0xB0)&&i.cameraY==get<int>(tactical+0xB4)):pointerLocation(i);
   if((shiftUnsupported&&i.operation!=pointerinput::PlayerSelect&&i.operation!=pointerinput::PlayerOrder)||get<bool>(0x87F7E8+0x11B3)||!location){pointerFinish(-4);return;}
   if(i.operation==pointerinput::FollowNative&&pointerTarget){pointerFinish(-4);return;}
  }
  if(start){pointerBatchCapture=i;pointerBatchValid=true;memset(pointerBatchTickets,0,sizeof pointerBatchTickets);memset(pointerBatchFollow,0,sizeof pointerBatchFollow);}
  if(start||continuation)pointerBatchRequest=request;
  for(int n=0;n<i.count;++n){uint32_t p=worldByID(i.sources[n].id);
   if(!continuation){if(p&&p==i.sources[n].pointer&&own(p,lastHouse))cancelNative(p);pointerTokens[n]=manualVersions.get(i.sources[n].id);}
   else if(pointerBatchTickets[n]){actorLeaseArbiter.release(match,i.sources[n].id,pointerBatchTickets[n]);pointerBatchTickets[n]=0;}
  }
  if(i.operation==pointerinput::Cancel){pointerFinish(1);return;}
 }
 if(!nativeReady()||i.inputSerial!=b.cancelSerial||i.match!=match||i.house!=lastHouse||DWORD(now-i.tick)>1500||frame<i.frame||frame-i.frame>90){pointerFinish(-7);return;}
 // A configured right click is observed on release and never swallowed.
 // Give the original input at least one simulation turn before appending.
 if((i.bindings&(pointerinput::RightClick|pointerinput::LeftClick))&&frame==i.frame)return;
 if(i.operation==pointerinput::PlayerSelect){
  if(!playertest::Available||!sh->controlAll||!riskControlActive||!pointerTarget||!live(pointerTarget)||!playableHouse(get<uint32_t>(pointerTarget+0x21C))){pointerFinish(-4);return;}
  if(!(i.bindings&pointerinput::Shift))unselect();
  b.accepted=get<bool>(pointerTarget+0x83)||selection(pointerTarget);pointerFinish(b.accepted?1:-4);return;
 }
 if(i.operation==pointerinput::AirWaypoint){
  b.resolvedCell=pointerCell;b.resolvedTarget=pointerTargetID;b.resolvedZ=pointerGroundZ;memset(b.sourceAccepted,0,sizeof b.sourceAccepted);
  if(!(sh->flags&AirWaypoints)||(sh->flags&NativeAirWaypoints)){pointerFinish(-4);return;}
  for(int n=0;n<i.count;++n){uint32_t p=worldByID(i.sources[n].id);if(p&&p==i.sources[n].pointer&&own(p,lastHouse)&&kind(p)==0&&!get<bool>(p+0x81)&&!get<uint32_t>(p+0x11C)){b.sourceTokens[n]=manualVersions.get(i.sources[n].id);b.sourceAccepted[n/32]|=1u<<(n%32);++b.accepted;}}
  pointerFinish(1);return;
 }
 if(pointerTarget&&(!worldObjects.contains(pointerTarget)||!live(pointerTarget)||get<uint32_t>(pointerTarget+0x10)!=pointerTargetID)){pointerFinish(-4);return;}
 if(i.operation==pointerinput::Sell){
  if(!(i.bindings&pointerinput::Control)||!pointerTarget||!sellAllowed(pointerTarget)){pointerFinish(-4);return;}
  if((i.bindings&pointerinput::PerFeatureMouse)&&
     !(i.bindings&(kind(pointerTarget)==2?pointerinput::SellVehicleInput:pointerinput::SellDrainedInput))){pointerFinish(-4);return;}
  if(nativeObjectEvent(get<int>(get<uint32_t>(pointerTarget+0x21C)+0x30),0x16,get<uint32_t>(pointerTarget+0x10),get<uint32_t>(pointerTarget+0x21C)==lastHouse?OrderAuthority::Local:OrderAuthority::SellPlayerTest)){b.accepted=1;pointerFinish(1);}
  else if(outgoingRoom()>0)pointerFinish(-6);
  return;
 }
 int examined=0,remaining=0,submitted=0;
 const int limit=(sh->planRequest!=sh->planAck||sh->cooperativePlan!=sh->cooperativeAck||(sh->flags&(AutoRepair|AutoOreReturn))||loadLedger.count||unloadCount||nativeCount)?12:24;
 const auto&blocked=sh->suppressedClick;
 bool suppressed=blocked.match==i.match&&blocked.x==i.x&&blocked.y==i.y&&
  ((i.bindings&pointerinput::RightClick)?blocked.physical==4:(i.bindings&pointerinput::LeftClick)?blocked.physical==6:false)&&
  (int32_t(i.tick-blocked.tick)>=-150&&int32_t(i.tick-blocked.tick)<=150);
 bool follow=!suppressed&&(i.operation==pointerinput::FollowNative||(i.bindings&pointerinput::RightClick));
 while(examined++<i.count){int n=pointerAt++;if(pointerAt>=i.count)pointerAt=0;if(pointerDone[n])continue;
  uint32_t p=worldByID(i.sources[n].id);
  if(i.operation==pointerinput::PlayerOrder){
   if(!playertest::Available||!sh->controlAll||!p||p!=i.sources[n].pointer||!live(p)||!playableHouse(get<uint32_t>(p+0x21C))||get<uint32_t>(p+0x11C)){pointerDone[n]=1;continue;}
   if(submitted>=limit||outgoingRoom()<=0){++remaining;continue;}
   uint32_t owner=get<uint32_t>(p+0x21C);int mission=pointerTarget?1:2;
   if(pointerTarget==p){pointerDone[n]=1;continue;}
   if(nativeMission(owner,i.sources[n].id,mission,pointerTargetID,pointerTarget?0:pointerCell,pointerTarget?0:11,0,0,taskarbiter::Owner::Generic,nullptr,-1,OrderAuthority::PlayerTest)){++b.accepted;++submitted;pointerDone[n]=1;}else ++remaining;continue;
  }
  if(!p||p!=i.sources[n].pointer||!own(p,lastHouse)||get<bool>(p+0x1C8)||get<uint32_t>(p+0x11C)||
     (!follow&&manualVersions.get(i.sources[n].id)!=pointerTokens[n])){pointerDone[n]=1;continue;}
  if(i.operation==pointerinput::C4Test){
   if(!sh->c4Test||!riskC4Active||kind(p)!=1||!pointerTarget||kind(pointerTarget)!=3||own(pointerTarget,lastHouse)||
      call<bool>(0x4F9A50,lastHouse,(void*)(uintptr_t)get<uint32_t>(pointerTarget+0x21C))||!get<bool>(typeOf(pointerTarget,3)+0x1577)){pointerDone[n]=1;continue;}
   if(submitted>=limit||outgoingRoom()<=0){++remaining;continue;}
   if(nativeMission(lastHouse,i.sources[n].id,17,pointerTargetID,pointerTargetID,0x34)){++b.accepted;++submitted;pointerDone[n]=1;}else ++remaining;
   continue;
  }
  if(pointerTarget){
   if(submitted>=limit||outgoingRoom()<=0){++remaining;continue;}
   uint32_t before=snap.queuedEvents;int result=cooperativeInput(p,pointerTarget,i.bindings);submitted+=int(snap.queuedEvents-before);if(result<0){++remaining;continue;}pointerDone[n]=1;if(result>0)++b.accepted;continue;
  }
  int k=kind(p);if(k!=1&&k!=2){pointerDone[n]=1;continue;}
  bool mag=(i.bindings&pointerinput::Magnetron)&&(sh->flags&MagnetronInput)&&rawMagnetron(p);
  bool advanced=(i.bindings&pointerinput::Advanced)&&(sh->flags&AdvancedInput)&&featureObjectAllowed(featuretypes::AdvancedAttack,p);
  bool cs=i.operation==pointerinput::AttackMove||
   ((!(i.bindings&pointerinput::PerFeatureMouse)||i.operation==pointerinput::FollowNative||(i.bindings&pointerinput::CS))&&csAllowedFor(p,(i.bindings&pointerinput::CS)!=0));
  if((i.bindings&pointerinput::PerFeatureMouse)&&!mag&&!advanced&&!cs){pointerDone[n]=1;continue;}
  if((mag||advanced||cs)&&!rawArmed(p)){pointerDone[n]=1;continue;}
  if(follow){
   if(!mag&&!advanced&&!cs){pointerDone[n]=1;continue;}
   uint32_t destination=get<uint32_t>(p+0x5A4);
   // Wait for the original move to become observable. Never edit its event.
   bool batch=(i.bindings&(pointerinput::BatchStart|pointerinput::BatchNext))!=0;
   if(!(batch&&pointerBatchFollow[n])){
    if(get<int>(p+0xAC)!=2||get<uint32_t>(p+0x2B4)||destination!=readMapCell(int(pointerCell%1000),int(pointerCell/1000))){++remaining;continue;}
    if(batch)pointerBatchFollow[n]=1;
   }
   if(mag){if(nativeOutgoing(p,2,pointerCell,0,0,i.bindings))++b.accepted;pointerDone[n]=1;continue;}
  }
  if(submitted>=limit||outgoingRoom()<=0){++remaining;continue;}
  int mission=mag?2:(advanced||cs)?29:2;
  auto owner=mag?taskarbiter::Owner::Magnetron:advanced?taskarbiter::Owner::Combat:taskarbiter::Owner::Generic;
  uint32_t ticket=0;
  if(nativeMission(lastHouse,i.sources[n].id,mission,0,pointerCell,11,0,0,owner,&ticket)){
   if(i.bindings&(pointerinput::BatchStart|pointerinput::BatchNext))pointerBatchTickets[n]=ticket;
   ++b.accepted;++submitted;pointerDone[n]=1;if(cs)++snap.csConverted;
   if(mag||advanced)nativeOutgoing(p,mission,pointerCell,0,0,i.bindings);
   else if(i.bindings&pointerinput::BatchNext)cancelNative(p); // accepted later direct order replaces this capture's earlier persistent plan
  }else ++remaining;
 }
 if(!remaining)pointerFinish(1);
}

