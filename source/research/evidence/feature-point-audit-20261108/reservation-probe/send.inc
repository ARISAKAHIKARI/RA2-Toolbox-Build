static bool userCommand(int c){return c!=OpenTech;}
static bool queueCommand(int c,int arg,int arg2,uint32_t match,int retries=0,DWORD delayMs=0,uint32_t token=0,uint64_t typeHash=0){
 if(c==PlaceCompletedBuilding)return false; // A frozen placement is never deferred.
 if(c==DestructPlayer)return false; // Current selection is valid only at this input edge.
 if(c==AttackOwnGround)return false; // Never resolve a delayed input against a later selection.
 if(!enabled)return false;
 if(c==RevealMap&&(!extra.experimentalReveal||extra.renderReveal))return false;
 DWORD now=GetTickCount();
 if(!game||!pid||!IsWindow(game)||(shared&&lastData&&incoming.status<0))return false;
 bool queued=commandQueue.add({c,arg,arg2,pid,match,now,retries,now+delayMs,token,typeHash?typeHash:typeListHash(snap.types,snap.typeCount)});
 if(queued){diag(L"staged command="+std::to_wstring(c)+L" pid="+std::to_wstring(pid)+L" match="+std::to_wstring(match)+L" backlog="+std::to_wstring(commandQueue.size));wakeGame();}
 return queued;
}
static void send(int command,int arg=0,int arg2=0,int retries=0,bool fromQueue=false,uint32_t requestToken=0){
 if(command==PlaceCompletedBuilding){notify(L"建筑放置须使用原生事件放置快捷键重新提交。");return;}
 if((command==Produce||command==CancelTechProduction)&&!buildingmodes::toolOrders(techImplementation,snap.techOpen)){notify(L"原生面板模式请使用游戏建造栏生产或取消。");return;}
 if(!enabled){notify(L"战术控制已暂停，请先启用后再发送指令。");return;}
 if(command==AttackOwnGround&&!shared&&game)ensureBridge(); // Connect, without staging a stale selection action.
 const DWORD inputAt=GetTickCount();const uint32_t inputPID=pid,inputMatch=commandMatch();const Shared*inputConnection=shared;
 if(command==AttackOwnGround&&(arg||arg2||requestToken||fromQueue||retries||!shared||!game||!pid||!IsWindow(game)||
    !commandFocus()||!commandFresh(inputAt)||incoming.chat||shared->request!=shared->ack||shared->ack!=ack||commandQueue.size||hasSelection||
    shared->selectRequest!=shared->selectAck||shared->pointer.request!=shared->pointer.ack||shared->viewZoomBusy)){
  notify(L"攻击脚下未提交：请等待当前对局及命令连接就绪，再重新按键或点击。");return;
 }
 // Whole-House destruction is an immediate explicit input action. Never
 // carry it across a reconnect, busy bridge or deferred command queue.
 if(command==DestructPlayer&&(!extra.destructSelectedOwner||arg||arg2||requestToken||!shared||!game||!pid||!IsWindow(game)||!commandFresh(GetTickCount())||
    !commandFocus()||incoming.chat||shared->request!=shared->ack||shared->ack!=ack||commandQueue.size||fromQueue||retries||hasSelection||
    shared->selectRequest!=shared->selectAck||shared->pointer.request!=shared->pointer.ack||shared->viewZoomBusy)){
  notify(L"自毁请求未提交：请先开启功能，并等待当前对局及命令连接就绪后重新执行。");return;
 }
 const uint64_t intentTypes=typeListHash(snap.types,snap.typeCount);
 if(command==OpenTech)arg2=techImplementation;
 if(command==RevealMap&&(!extra.experimentalReveal||(!fromQueue&&!extra.manualReveal))){notify(L"请先开启允许手动地图揭示。");return;}
 if(command==RevealMap&&extra.renderReveal){notify(L"请先将黑幕绘制方式设为关闭；当前绘制补丁与原生揭示不能同时执行。");return;}
 DWORD now=GetTickCount();
 if(game&&!shared)ensureBridge();
 if(command==SmartUnload&&!hookSmartUnload){notify(L"智能卸载功能开关已关闭。");return;}
 if(!game||!pid||!IsWindow(game)){
  // The Tab/cursor bridges may already see a brand-new game while this bridge
  // is still negotiating an old mapping. Capture one explicit UI action for
  // that particular visible game PID and execute it only after a fresh round ACK.
  DWORD candidate=discoverGamePid();
  if(userCommand(command)&&candidate&&commandQueue.add({command,arg,arg2,candidate,delivery::UnknownMatch,now,retries})){
   lastFind=now-2000;notify(L"已发现游戏，指令等待新对局连接后自动提交。");
  }else notify(L"尚未发现可连接的游戏窗口。");
  return;
 }
 if(snap.status<0){notify(L"游戏操作暂不可用，本次指令未执行。");return;}
 readSnapshot();now=GetTickCount();
 if(command==AttackOwnGround&&(pid!=inputPID||!inputMatch||commandMatch()!=inputMatch||shared!=inputConnection||DWORD(now-inputAt)>150)){
  notify(L"攻击脚下未提交：输入已过期或对局已改变，请重新执行。");return;
 }
 if(command==DestructPlayer&&(pid!=inputPID||!inputMatch||commandMatch()!=inputMatch||shared!=inputConnection||DWORD(now-inputAt)>150)){
  notify(L"自毁请求未提交：输入已过期或对局已改变，请重新选择并执行。");return;
 }
 // arg=0 asks Runtime to validate every current selected unit/UID/House,
 // reject mixed owners and recheck the cross-House network adapter there.
 if(typeIndexedCommand(command,arg)&&intentTypes!=typeListHash(snap.types,snap.typeCount)){
  if(fromQueue&&(command==ProduceReserved||command==ProduceDeferred))constructionResult(-13,0,uint32_t(arg2),requestToken);
  else notify(L"单位清单已改变，请重新选择后提交。");return;
 }
 if(shared&&lastData&&incoming.status<0){notify(commandWaitReason());return;}
 bool connected=shared&&delivery::bridgeReady(incoming.status,commandFresh(now),incoming.chat,shared->request==shared->ack);
 // Mode syncing must not enqueue automatically every 50ms.
 // Explicit gameplay commands are staged while the match is loading or bridge is busy.
 if(!connected||(!fromQueue&&commandQueue.size&&userCommand(command))){
  if(command==AttackOwnGround){notify(L"攻击脚下未提交：连接正忙，请重新执行。");return;}
  if(command==DestructPlayer){notify(L"自毁请求未提交：游戏状态已改变，请重新选择所属方后点击。");return;}
  if(userCommand(command)&&queueCommand(command,arg,arg2,commandMatch(),retries,0,requestToken)){
   logCommandWait();notify(L"指令已排队，尚未提交。"+commandWaitReason());return;
  }
  if(userCommand(command)&&commandQueue.size>=delivery::Capacity){notify(L"待处理命令已满，本次未加入，请稍后再试。");return;}
  notify(commandWaitReason());
  return;
 }
 diag(L"send command="+std::to_wstring(command)+L" match="+std::to_wstring(snap.match)+L" req="+std::to_wstring(shared->request+1)+(command==SelectType?(arg2?L" selection=all":L" selection=visible"):L""));
 if(command==OpenTech){int requested=0;
  for(int i=0;i<MaxTypes;++i){shared->techAllowed[i]=i<snap.typeCount&&techAllowed(snap.types[i]);requested+=shared->techAllowed[i]!=0;}
  diag(L"Tech request implementation="+std::to_wstring(techImplementation)+L" profile="+std::to_wstring(techProfile)+L" selected="+std::to_wstring(requested)+L" match="+std::to_wstring(snap.match));
 }
 inFlightMatch=commandMatch();inFlightRetries=retries;sentAt=now;
 shared->requestTypeHash=typeListHash(snap.types,snap.typeCount);
 shared->requestToken=requestToken;shared->command=command;shared->arg=arg;shared->arg2=arg2;
 shared->requestTick=now;shared->requestMatch=commandMatch();
 MemoryBarrier();
 if(command==RevealMap)revealpermission::beforeRequest(shared->flags,extra.experimentalReveal,[&]{InterlockedIncrement((volatile LONG*)&shared->request);});
 else InterlockedIncrement((volatile LONG*)&shared->request);
 wakeGame();
}
