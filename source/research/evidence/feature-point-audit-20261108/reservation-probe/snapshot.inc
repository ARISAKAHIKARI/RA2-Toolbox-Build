static void readSnapshot(){
 static Snapshot candidate;
 if(shared)for(int tries=0;tries<2;++tries){uint32_t a=shared->seq;if(a&1)continue;if((!lastData&&!a)||a==snapshotSeq)break;
  MemoryBarrier();memcpy(&candidate,&shared->data,sizeof candidate);MemoryBarrier();if(a!=uint32_t(shared->seq))continue;
  incoming=candidate;
  bool first=!lastData;snap=incoming;snapshotSeq=a;lastData=GetTickCount();
  logCommandReceipts(incoming);
  static int loggedPlacement=0,loggedNativeAir=0,loggedRenderReveal=0,loggedC4=0;
  if(incoming.placementStatus!=loggedPlacement||incoming.nativeAirStatus!=loggedNativeAir||incoming.renderRevealStatus!=loggedRenderReveal||incoming.c4TestStatus!=loggedC4){
   autolog::write(L"experimental-patches placement="+std::to_wstring(incoming.placementStatus)+L" nativeAir="+std::to_wstring(incoming.nativeAirStatus)+L" renderReveal="+std::to_wstring(incoming.renderRevealStatus)+L" c4Beta="+std::to_wstring(incoming.c4TestStatus)+L" installed="+std::to_wstring(incoming.hookInstalled)+L" conflicts="+std::to_wstring(incoming.hookConflicts));
   loggedPlacement=incoming.placementStatus;loggedNativeAir=incoming.nativeAirStatus;loggedRenderReveal=incoming.renderRevealStatus;loggedC4=incoming.c4TestStatus;
  }
  static uint32_t loggedMatch=0,loggedFlags=0,loggedSent=0;static int loggedReady=-1;static DWORD entryAt=0,entryLogAt=0;
  const DWORD logNow=GetTickCount();const bool changed=incoming.match!=loggedMatch||incoming.commandStatus!=loggedReady||shared->flags!=loggedFlags;
  if(incoming.match!=loggedMatch){entryAt=logNow;entryLogAt=0;loggedSent=0;}
  if(changed||(entryAt&&DWORD(logNow-entryAt)<3000&&DWORD(logNow-entryLogAt)>=250)){
   diag(L"entry-state match="+std::to_wstring(incoming.match)+L" frame="+std::to_wstring(incoming.frame)+
    L" session="+std::to_wstring(incoming.session)+L" ready="+std::to_wstring(incoming.commandStatus)+
    L" flags="+std::to_wstring(shared->flags)+L" queued="+std::to_wstring(incoming.queueSent)+
    L" newQueued="+std::to_wstring(incoming.queueSent-loggedSent)+L" rejected="+std::to_wstring(incoming.commandRejected)+
    L" pendingCommand="+std::to_wstring(shared->request!=shared->ack)+L" pointer="+std::to_wstring(shared->pointer.request!=shared->pointer.ack)+
    L" patches="+std::to_wstring(incoming.inputPatchMask));
   loggedMatch=incoming.match;loggedReady=incoming.commandStatus;loggedFlags=shared->flags;loggedSent=incoming.queueSent;entryLogAt=logNow;
  }
  static DWORD optionalProbeAt=0,optionalProbePid=0;static uint64_t optionalProbeGeneration=0;
  if(first){resetStartupProbeDiagnostics();optionalProbeAt=logNow-5000;optionalProbePid=0;optionalProbeGeneration=0;}
  const uint64_t probeGeneration=externalReader.generation();
  if(pid&&incoming.status==1&&(pid!=optionalProbePid||probeGeneration!=optionalProbeGeneration||DWORD(logNow-optionalProbeAt)>=5000)){
   optionalProbeAt=logNow;optionalProbePid=pid;optionalProbeGeneration=probeGeneration;logStartupProbeDifferences(true);
  }
  static DWORD activationLogAt=0;static int activationState=0;
  if(incoming.commandStatus>=2&&incoming.commandStatus<=4&&
     (activationState!=incoming.commandStatus||DWORD(logNow-activationLogAt)>=5000)){
   activationState=incoming.commandStatus;activationLogAt=logNow;
   int players=0;uint8_t oos=0;int ring[3]{};
   externalReader.readFresh(0xA8B54C,&players,4);externalReader.readFresh(0xA8B8C2,&oos,1);externalReader.readFresh(0x8B41F8,ring,sizeof ring);
   critical(L"activation-wait state="+std::to_wstring(incoming.commandStatus)+L" frame="+std::to_wstring(incoming.frame)+
    L" players="+std::to_wstring(players)+L" nativeOOS="+std::to_wstring(oos)+L" DoList="+std::to_wstring(ring[0])+L"/"+std::to_wstring(ring[1])+L"/"+std::to_wstring(ring[2]));
   if(incoming.commandStatus==3)logStartupProbeDifferences(false);
  }else if(incoming.commandStatus==1)activationState=0;
  snap.bridgeStatus=incoming.status;
  snap.commandStatus=incoming.commandStatus;
  if(first)diag(L"command bridge status="+std::to_wstring(incoming.status));
  if(incoming.status<0){
   bridgeBlocked=true;std::vector<delivery::Item> canceledReservations;
   for(int i=0;i<commandQueue.size;++i)if(commandQueue.slots[i].command==ProduceReserved||commandQueue.slots[i].command==ProduceDeferred)canceledReservations.push_back(commandQueue.slots[i]);
   commandQueue.clear();for(const auto&it:canceledReservations)constructionResult(-13,0,uint32_t(it.arg2),it.token);
   hasSelection=false;
  }else bridgeAttempts=0;break;
 }
 mergeExternal();logFeatureActivation(GetTickCount());
}
