static void serviceCommandQueue(DWORD now){
 if(!shared||!enabled)return;
 bool revealQueued=commandQueue.has(RevealMap);
 std::vector<delivery::Item> expiredReservations;
 for(int i=0;i<commandQueue.size;++i){const auto&it=commandQueue.slots[i];
  if((it.command==ProduceReserved||it.command==ProduceDeferred)&&
     (it.pid!=pid||uint32_t(now-it.created)>delivery::MaxAgeMs||
      (commandMatch()!=0&&it.match!=delivery::UnknownMatch&&it.match!=commandMatch())))expiredReservations.push_back(it);
 }
 int expired=commandQueue.expire(now,pid,commandMatch(),commandMatch()!=0);
 for(const auto&it:expiredReservations)constructionResult(-13,0,uint32_t(it.arg2),it.token);
 if(revealQueued&&!commandQueue.has(RevealMap)&&extra.mapStrategy==3&&autoRevealDone!=commandMatch()&&
    (shared->request==shared->ack||shared->command!=RevealMap))autoRevealMatch=0;
 if(expired){diag(L"discarded stale queued commands="+std::to_wstring(expired));notify(L"等待的命令已过期或对局改变，已取消，避免误操作。");}
 delivery::Item queued{};
 if(commandQueue.peek(now,pid,commandMatch(),commandMatch()!=0,queued)&&commandFocus()&&
    delivery::bridgeReady(incoming.status,commandFresh(now),incoming.chat,shared->request==shared->ack)){
  commandQueue.consume();
  if(typeIndexedCommand(queued.command,queued.arg)&&queued.typeHash!=typeListHash(snap.types,snap.typeCount)){
   if(queued.command==ProduceReserved||queued.command==ProduceDeferred)constructionResult(-13,0,uint32_t(queued.arg2),queued.token);
   else notify(L"单位清单已改变，旧的待处理指令已取消。");
  }
  else send(queued.command,queued.arg,queued.arg2,queued.retries,true,queued.token);
 }
 static DWORD waitLoggedAt=0;
 if(commandQueue.size&&DWORD(now-waitLoggedAt)>=2000){waitLoggedAt=now;logCommandWait();}
 if(commandFresh(now)&&shared->request==shared->ack&&(snap.techOpen!=(techMode?techImplementation+1:0)||(techMode&&techAcknowledgedHash!=typeListHash(snap.types,snap.typeCount)))&&!snap.techError)send(OpenTech,techMode);
}
