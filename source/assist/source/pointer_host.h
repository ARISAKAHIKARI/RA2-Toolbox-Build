// Host capture uses polling. Optional suppression lives only in the common Runtime message boundary.
static void servicePointer(){
 static pointerinput::Intent leftCapture{};static bool leftCaptured=false;
 const DWORD capturedNow=GetTickCount();
 auto visible=external::display::inspect([](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);});
 const bool inputReady=!formationInputOwns()&&shared&&!shared->viewZoomBusy&&commandFresh(capturedNow)&&foregroundGame()&&!incoming.chat&&visible.visible();
 gameinput::setPointerContext(hook,commandMatch(),inputReady);
 if(pointerMatches.active()&&(!inputReady||!extra.allHotkeys||pointerMatches.revision!=settingsRevision||
    pointerMatches.capture.match!=commandMatch()||pointerMatches.capture.inputSerial!=gameinput::pointerCancelSerial(hook)))pointerMatches.clear();
 if(!inputReady)leftCaptured=false;
 if(!shared)return;
 uint32_t allowed=0;const auto featureFlags=flags();
 if(featureFlags&AdvancedInput)allowed|=pointerinput::Advanced;
 if(featureFlags&MagnetronInput)allowed|=pointerinput::Magnetron;
 if(featureFlags&CustomCS)allowed|=pointerinput::CS;
 if(featureFlags&AdvancedEnter)allowed|=pointerinput::Enter;
 if(featureFlags&AircraftSpread)allowed|=pointerinput::AirFirst|pointerinput::AirSecond;
 if(featureFlags&DiskPrimary)allowed|=256;
 if(featureFlags&(SellDrained|SellVehicles|SellEnemy))allowed|=512;
 pointerinput::Bindings bindings{advancedKey,magnetronKey,csHoldKey,extra.enterKey,extra.airKey1,extra.airKey2,extra.diskKey,extra.sellKey,allowed};
 InterlockedIncrement(&shared->pointerPolicySeq);shared->pointerPolicy={};shared->pointerPolicy.match=commandMatch();shared->pointerPolicy.enabled=inputReady;shared->pointerPolicy.overrideNative=extra.overrideNative;shared->pointerPolicy.bindings=bindings;memcpy(shared->pointerPolicy.buttons,extra.mouseButtons,sizeof extra.mouseButtons);MemoryBarrier();InterlockedIncrement(&shared->pointerPolicySeq);
 auto&box=shared->pointer;box.cancelSerial=gameinput::pointerCancelSerial(hook);static uint32_t ack=0;
 if(ack!=box.ack){ack=box.ack;MemoryBarrier();
  if(box.intent.operation==pointerinput::C4Test){
   infantryEntryResultMatch=box.intent.match;
   infantryEntryResult=std::wstring(pointerinput::entryLabel((box.intent.bindings&pointerinput::EntrySabotage)?1:0))+L"：命令已入队 "+std::to_wstring(std::max(0,box.accepted))+L" 个";
   if(box.result==-8)infantryEntryResult+=L"；未点中建筑";
   else if(box.result==-4)infantryEntryResult+=L"；点击位置或对象已变化";
   else if(box.result==-7)infantryEntryResult+=L"；输入已过期或被新操作取消";
   else if(box.result<0)infantryEntryResult+=L"；提交未完成";
   else if(box.rejected)infantryEntryResult+=L"；跳过 "+std::to_wstring(box.rejected)+L" 个非己方有效步兵";
   notify(L"步兵强制进入建筑："+infantryEntryResult);
  }
  if(box.intent.operation==pointerinput::AirWaypoint)airway::capture(box.intent,box);
  if(pointerMatches.active()){
   pointerMatches.accepted+=std::max(0,box.accepted);pointerMatches.rejected+=std::max(0,box.rejected);
   if(box.result==-7){pointerMatches.clear();notify(L"匹配组合已失效，未提交的操作已取消。");}
   else if(!pointerMatches.waiting()){notify(L"匹配组合已接收 "+std::to_wstring(pointerMatches.accepted)+L" 项；未接收 "+std::to_wstring(pointerMatches.rejected)+L" 项。");pointerMatches.clear();}
  }else if(box.intent.operation!=pointerinput::Cancel&&box.intent.operation!=pointerinput::FollowNative&&box.intent.operation!=pointerinput::AirWaypoint&&box.intent.operation!=pointerinput::C4Test){
   if(box.result<0)notify(L"增强输入未提交：已失效、目标不支持或队列繁忙，请重新操作。");
   else notify(L"增强输入已接收 "+std::to_wstring(box.accepted)+L" 个；未接收 "+std::to_wstring(box.rejected)+L" 个。");
  }
 }
 if(box.request!=box.ack)return;
 if(pointerMatches.waiting()){
  if(DWORD(GetTickCount()-pointerMatches.capture.tick)>1500){pointerMatches.clear();notify(L"匹配组合已超时，未提交的操作已取消。");return;}
  // The previous cooperative request must be consumed, including retries,
  // before another mode can reuse the sole cooperative mailbox.
  if(shared->cooperativeRequest.serial!=pointerMatches.cooperativeBefore&&
     (shared->cooperativeRequest.serial!=cooperativeDelivered||shared->cooperativePlan!=shared->cooperativeAck||retryCooperative))return;
  pointerMatches.cooperativeBefore=shared->cooperativeRequest.serial;
  box.intent=pointerMatches.next();MemoryBarrier();InterlockedIncrement((volatile LONG*)&box.request);wakeGame();return;
 }
 gameinput::PointerPress e{};DWORD dropped=0;
 if(!gameinput::takePointer(hook,e,dropped)){if(dropped)notify(L"输入过密，部分增强请求未接收。");return;}
 if(!inputReady||e.serial!=box.cancelSerial||e.match!=commandMatch())return;
 // The explicit unload command owns this same chord. A polled native D/S/X
 // cancellation arriving afterwards must not erase its passenger follow-up.
 if(e.operation==5&&hookSmartUnload&&LOBYTE(unloadKey)&&
    pointerinput::matchesHold(e.keys,unloadKey,pointerinput::Enhanced))return;
 if(buildreserve::nativePointer(e)){leftCaptured=false;return;}
 airway::manual(e);
 // Native Z planning owns the original click; do not append another feature.
 if(airway::native()&&airway::held(e.keys)&&airway::selectedAircraft()){leftCaptured=false;return;}
 RECT client{};if(!GetClientRect(game,&client)||client.right!=e.width||client.bottom!=e.height)return;
 DWORD now=GetTickCount();if(!commandFresh(now)||!foregroundGame()||DWORD(now-e.tick)>100||snap.chat)return;
 static pointerinput::Intent in;in={};in.tick=e.tick;in.inputSerial=e.serial;in.match=e.match;
 uint32_t saleEnabled=0;
 if(featureFlags&(SellDrained|SellEnemy))saleEnabled|=pointerinput::SellDrainedInput;
 if(featureFlags&SellVehicles)saleEnabled|=pointerinput::SellVehicleInput;
 pointerinput::MouseRoute routes[pointerinput::MouseCount+1]{};
 int routeCount=pointerinput::resolveMouseMatches(e.keys,e.operation==7?6:e.operation,extra.mouseButtons,
  bindings,saleEnabled,extra.allHotkeys,routes);
 if(airway::active()&&!airway::ended&&airway::selectedAircraft()&&airway::held(e.keys)&&(e.operation==4||e.operation==6||e.operation==7)){routes[0]={pointerinput::AirWaypoint,e.operation==4?uint32_t(pointerinput::RightClick):uint32_t(pointerinput::LeftClick)};routeCount=1;}
 if(extra.c4Test&&extra.c4Mouse&&uint32_t(e.operation==7?6:e.operation)==extra.c4Mouse&&pointerinput::matchesHold(e.keys,extra.c4Key,pointerinput::Enhanced)){
  pointerinput::MouseRoute test{pointerinput::C4Test,pointerinput::PerFeatureMouse|(extra.c4Mode==1?uint32_t(pointerinput::EntrySabotage):0)|(extra.c4Mouse==4?uint32_t(pointerinput::RightClick):extra.c4Mouse==6?uint32_t(pointerinput::LeftClick):0)};
  if(extra.allHotkeys&&routeCount&&routes[0].operation!=pointerinput::FollowNative)routes[routeCount++]=test;else{routes[0]=test;routeCount=1;}
 }
 if(playertest::Available&&extra.controlAll&&e.keys[VK_MENU]&&!e.keys[VK_CONTROL]&&(e.operation==7||e.operation==6||e.operation==4)){routes[0]={e.operation==4?pointerinput::PlayerOrder:pointerinput::PlayerSelect,(e.keys[VK_SHIFT]?uint32_t(pointerinput::Shift):0)};routeCount=1;}
 if(!routeCount)return;in.operation=routes[0].operation;in.bindings=routes[0].bindings;
 if(in.operation==pointerinput::Sell&&!(in.bindings&pointerinput::Control))return;
 // Plain native clicks also carry mining hand-back, even when Auto CS is off.
 if(e.keys[VK_CONTROL]||e.keys[VK_MENU])for(int n=0;n<routeCount;++n)routes[n].bindings|=pointerinput::NativeForced;
 auto read=[](uint32_t at,void*p,size_t n){return externalReader.readFresh(at,p,n);};
 uint32_t tactical=0,items=0;int count=0,frameAfter=0;uint32_t houseAfter=0;
 if(!read(0xA83D4C,&in.house,4)||in.house<0x10000||!read(0xA8ED84,&in.frame,4)||
    !read(0x887324,&tactical,4)||tactical<0x10000||!read(tactical+0xB0,&in.cameraX,4)||!read(tactical+0xB4,&in.cameraY,4)||
    !read(0x8A00A4,&in.width,4)||!read(0x8A00A8,&in.height,4)||in.width<=0||in.height<=0||in.width>32768||in.height>32768)return;
 in.x=int(int64_t(e.point.x)*in.width/e.width);in.y=int(int64_t(e.point.y)*in.height/e.height);
 bool selectionNeeded=false;for(int route=0;route<routeCount;++route)if(routes[route].operation!=pointerinput::Sell&&routes[route].operation!=pointerinput::PlayerSelect)selectionNeeded=true;
 if(selectionNeeded&&e.operation==6&&(!leftCaptured||leftCapture.match!=in.match||leftCapture.inputSerial!=in.inputSerial))return;
 if(selectionNeeded&&e.operation==6&&leftCaptured&&in.operation==pointerinput::FollowNative&&(std::abs(int64_t(in.x)-leftCapture.x)>5||std::abs(int64_t(in.y)-leftCapture.y)>5)){leftCaptured=false;return;}
 if(selectionNeeded&&e.operation==6&&leftCaptured&&leftCapture.match==in.match&&leftCapture.inputSerial==in.inputSerial){in.count=leftCapture.count;memcpy(in.sources,leftCapture.sources,size_t(in.count)*sizeof(in.sources[0]));}
 else if(selectionNeeded){
  if(!read(0xA8ECB8+16,&count,4)||count<0||count>pointerinput::MaxSources||!read(0xA8ECB8+4,&items,4))return;
  for(int i=0;i<count;++i){auto&src=in.sources[in.count];uint32_t owner=0;
   if(!read(items+i*4,&src.pointer,4)||src.pointer<0x10000||!read(src.pointer+0x10,&src.id,4)||!src.id||!read(src.pointer+0x21C,&owner,4))return;
   if(owner==in.house||in.operation==pointerinput::PlayerOrder)++in.count;
  }
  if(!in.count){bool sale=false;for(int j=0;j<routeCount;++j)sale|=routes[j].operation==pointerinput::Sell;if(!sale)return;}
 }
 int cameraX=0,cameraY=0;
 if(!read(0xA8ED84,&frameAfter,4)||frameAfter<in.frame||frameAfter-in.frame>2||
    !read(0xA83D4C,&houseAfter,4)||houseAfter!=in.house||
    !read(tactical+0xB0,&cameraX,4)||!read(tactical+0xB4,&cameraY,4)||cameraX!=in.cameraX||cameraY!=in.cameraY||DWORD(GetTickCount()-in.tick)>100)return;
 if(e.operation==7&&in.operation==pointerinput::PlayerSelect)return;
 if(e.operation==7){leftCapture=in;leftCaptured=true;return;}
 if(e.operation==6)leftCaptured=false;
 pointerMatches.begin(in,routes,routeCount,settingsRevision);pointerMatches.cooperativeBefore=shared->cooperativeRequest.serial;
 box.intent=pointerMatches.next();MemoryBarrier();InterlockedIncrement((volatile LONG*)&box.request);wakeGame();
}



