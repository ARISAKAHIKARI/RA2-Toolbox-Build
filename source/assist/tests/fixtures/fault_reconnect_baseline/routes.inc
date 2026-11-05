static void disconnect(){
 // reset attempts EVERY restoration even if one group must defer. Keep each
 // group's world identity and the transport until all ownership has retired.
 restorePlayerContext();reset();
 // In particular, raw direct patches have no disabled callback to fall back
 // through. Do not acknowledge stop/unmap IPC while any feature still owns one.
 if(!featureRestorationComplete())return;
 if(sh&&sh->magic==Magic&&sh->version==Version&&sh->pid==GetCurrentProcessId()&&sh->stop){MemoryBarrier();sh->stop=2;}
 InterlockedExchange(&hostWorkPending,0);if(controllerProcess)CloseHandle(controllerProcess);controllerProcess=nullptr;controllerPID=controllerCheckAt=0;restoreFrameDriver();InterlockedExchange(&messageGameThread,0);sessionTracker.update(false,0,-1,-1);hookWorld.reset();commandDormant=worldCleanupPending=false;if(sh)UnmapViewOfFile(sh);sh=nullptr;if(mapping)CloseHandle(mapping);mapping=nullptr;lastHouse=pendingAck=0;pendingAckReady=false;
}

static void tick(){
 if(!sh){wchar_t n[80];mapName(n,GetCurrentProcessId());mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,n);if(!mapping)return;sh=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));}
 // A deferred disconnect must never turn an invalid mapping into a usable
 // command connection on the next callback.
 if(!sh||sh->magic!=Magic||sh->version!=Version||sh->pid!=GetCurrentProcessId()){disconnect();return;}
 if(!sh->superweaponGrant)superweaponGrantReset();
 maintainTechParameters();playerMaintenance();controlHouseFlagMaintenance();DWORD now=sampleClock(sh);if(sh->stop||(DWORD(now-sh->heartbeat)>3000&&controllerExited(now))){
  stickyTick(true);
  // Release our automatic attack/hold orders before acknowledging detach. No game
  // field is changed locally, including when the controlling EXE has exited.
  if(missileHoldCount&&lastHouse&&get<uint32_t>(0xA83D4C)==lastHouse&&get<bool>(0xA8E9A0)){
   int cleanupFrame=get<int>(0xA8ED84);snap.frame=cleanupFrame;indexWorld(cleanupFrame);missileCleanup=true;
   bool released=releaseMissileHolds(lastHouse,cleanupFrame,now);missileCleanup=false;if(!released)return;
  }
  // Stop admitting native input before retiring the sender. A changed world
  // or an invalid pending queue may prevent safe restoration; keep the IPC
  // and message entry alive so a later game-thread tick can finish cleanup.
  // stop=2 permits the Host to remove its last message-hook subscription.
  restoreNativeSellCursor();restoreNativeMegaGateway();restoreControlObjectOwner();restoreControlCommandOwner();
  restoreFrameDriver();disconnect();return;
 }

 ++ordinaryCalls;
}

static void restoreFrameDriver(){}

static void maintainScheduler(){if(sh){sh->driverMode=scheduler::Message;sh->driverTarget=0;sh->driverStatus=scheduler::MessageActive;}}

static void dispatchFromTrigger(dispatchrouter::Trigger trigger,DWORD now,const MSG* message){
 gameentry::Scope processEntry(gameentry::Client::Assist,trigger==dispatchrouter::Trigger::Message);
 if(!processEntry)return;
 if(trigger==dispatchrouter::Trigger::Message){
  InterlockedExchange(&messageGameThread,LONG(GetCurrentThreadId()));
  if(sh&&message&&message->message==WM_NULL&&message->wParam==WakeTag&&message->lParam==sh->owner)
   InterlockedExchange(&sh->gameWakePending,0);
 }
 if(trigger!=dispatchrouter::Trigger::Message)return;
 if(InterlockedCompareExchange(&schedulerBusy,1,0)!=0)return;
 // A frame callback is a safe execution point, not an independent producer.
 // Preserve the grant on rejected entry/router/reentrancy; consume only here.
 if(!InterlockedExchange(&hostWorkPending,0)){InterlockedExchange(&schedulerBusy,0);return;}
 // Producers and event gateway do not know which provider woke this tick.
 // The shared request/ack serials and unit arbiter prevent redispatching an
 // already accepted user request when ownership changes between callbacks.
 if(!fault||(sh&&sh->stop)){
  GuardedAssistTick();
 }
 // Install / remove experimental hooks only from the original message path.
 if(trigger==dispatchrouter::Trigger::Message)maintainScheduler();
 InterlockedExchange(&schedulerBusy,0);
}

extern "C" void AssistTick(){if(gameentry::active(gameentry::Client::Assist))tick();}

extern "C" void AssistFault(){superweaponGrantReset();restoreMapVisibilityParameters();restoreControlParameters();restoreTechEnhanceParameters();restoreTechParameters();restoreSidebarView();nativePlacementReset();nativePlacementCompletionReset();restorePauseView();restoreCrcMapShadow();restoreBuildTimeTooltip();restoreNativeSellCursor();restoreNativeMegaGateway();restoreControlObjectOwner();restoreControlCommandOwner();restoreNetworkSender();restoreTechEnhancement();restorePlayerContext();faultContext={};nativeMegaQueueCallDepth=nativeObjectQueueCallDepth=0;fault=true;resetMoOrders();restoreClickHooks();resetCooperativeState();resetFormation();resetMissileControl();resetBurstControl();resetDodgeControl();clearNativeOrders();restorePlacement();restoreTech();restoreCSHook();if(sh){sh->result=-10;InterlockedIncrement(&sh->seq);sh->data.status=-2;sh->data.bridgeStatus=-2;sh->data.commandStatus=0;sh->data.labels=0;MemoryBarrier();InterlockedIncrement(&sh->seq);sh->ack=sh->request;sh->selectResult=sh->planResult=-10;sh->selectAck=sh->selectRequest;sh->planAccepted=0;sh->planAcceptedMask=sh->planRejectedMask=0;sh->planAck=sh->planRequest;notifyHost();}}

extern "C" void AssistRuntimeWatchdog(){
 if(!sh)return;
 const DWORD now=sampleClock(sh);
 if(!sh->stop&&!(DWORD(now-sh->heartbeat)>3000&&controllerExited(now)))return;
 gameentry::Scope entry(gameentry::Client::Assist);if(!entry)return;
 GuardedAssistTick(); // stop/dead-controller branch only; never normal work
}

extern "C" bool AssistRuntimeDetached(){return sh==nullptr;}
