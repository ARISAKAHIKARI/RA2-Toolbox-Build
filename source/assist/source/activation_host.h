// Host-only diagnostics. These observations neither grant work nor retry an
// acknowledged command; option maintenance stays on its existing path.
static void logFeatureActivation(DWORD now){
 if(!shared)return;
 const uint32_t desiredFlags=flags();
 uint64_t key=1469598103934665603ull;
 auto mix=[&](uint32_t value){key^=value;key*=1099511628211ull;};
 mix(pid);mix(settingsRevision);mix(desiredFlags);mix(shared->flags);mix(shared->stop);mix(shared->readOnly);mix(shared->readOnlyRequest);mix(shared->readOnlyAck);
 mix(techMode);mix(techImplementation);mix(buildTimeTooltipEnabled);
 mix(extra.controlRequest());mix(extra.renderReveal);mix(extra.techEnhance);mix(extra.nativeAirNetwork);
 mix(extra.networkHouseRequested());mix(extra.commandOwnerRequested());mix(extra.nativeMegaGateway);mix(extra.nativeSellCursor);mix(extra.crcMapShadow);
 mix(extra.missileDefense);mix(extra.dodgeEnabled);mix(extra.magnetronGlobal);mix(extra.magnetronDrop);mix(extra.formationEnabled);mix(extra.lowDamageFilter);mix(extra.lowDamageNetwork);
 mix(incoming.status);mix(incoming.commandStatus);mix(incoming.hookInstalled);mix(incoming.hookConflicts);
 mix(incoming.placementStatus);mix(incoming.nativeAirStatus);mix(incoming.renderRevealStatus);mix(incoming.techOpen);mix(incoming.techError);
 mix(shared->techEnhanceStatus);mix(shared->playerControlStatus);mix(shared->networkHouseStatus);mix(shared->controlCommandOwnerStatus);mix(shared->nativeMegaGatewayStatus);mix(shared->nativeSellCursorStatus);mix(shared->crcMapShadowStatus);mix(shared->buildTimeTooltipStatus);mix(shared->lowDamageStatus);
 debugentries::Observation sellCursorEntry{},sellOwnerEntry{};bool sellDebugValid=false;
 if(extra.nativeSellCursor&&shared->nativeSellCursorStatus<0){
  const int cursorIndex=debugentries::index(debugentries::id(debugentries::Kind::Patch,0x6929E5));
  const int ownerIndex=debugentries::index(debugentries::id(debugentries::Kind::Patch,0x4AC179));
  if(cursorIndex>=0&&ownerIndex>=0)for(unsigned attempt=0;attempt<3;++attempt){
   const LONG before=InterlockedCompareExchange(&shared->debugSnapshotSeq,0,0);if(before&1)continue;
   sellCursorEntry=shared->debugSnapshot.entries[cursorIndex];sellOwnerEntry=shared->debugSnapshot.entries[ownerIndex];MemoryBarrier();
   if(before==InterlockedCompareExchange(&shared->debugSnapshotSeq,0,0)){
    sellDebugValid=sellCursorEntry.id==debugentries::id(debugentries::Kind::Patch,0x6929E5)&&sellOwnerEntry.id==debugentries::id(debugentries::Kind::Patch,0x4AC179);break;
   }
  }
 }
 mix(sellDebugValid);if(sellDebugValid)for(const auto&o:{sellCursorEntry,sellOwnerEntry}){
  mix(uint32_t(o.signature));mix(uint32_t(o.hook));mix(uint32_t(o.reason));mix(uint32_t(o.error));
 }
 static activation::LogCadence cadence;
 const bool blocked=incoming.status<0||incoming.commandStatus!=activation::Ready||shared->stop;
 if(!cadence.due(key,now,blocked))return;
 critical(L"feature-activation requestedFlags="+std::to_wstring(desiredFlags)+L" publishedFlags="+std::to_wstring(shared->flags)+
  L" gate="+activation::name(incoming.commandStatus)+L" gateCode="+std::to_wstring(incoming.commandStatus)+
  L" bridgeStatus="+std::to_wstring(incoming.status)+L" stop="+std::to_wstring(shared->stop)+L" readOnly="+std::to_wstring(shared->readOnly)+L" readOnlyRequest="+std::to_wstring(shared->readOnlyRequest)+L" readOnlyAck="+std::to_wstring(shared->readOnlyAck)+L" blocked="+std::to_wstring(bridgeBlocked)+
  L" match="+std::to_wstring(incoming.match)+L" frame="+std::to_wstring(incoming.frame)+L" ageMs="+std::to_wstring(lastData?DWORD(now-lastData):~0u)+
  L" patches="+std::to_wstring(incoming.hookInstalled)+L" conflicts="+std::to_wstring(incoming.hookConflicts)+
  L" requested/applied tech="+std::to_wstring(techMode?techImplementation+1:0)+L"/"+std::to_wstring(incoming.techOpen)+L" techError="+std::to_wstring(incoming.techError)+
  L" techEnhance="+std::to_wstring(extra.techEnhance)+L"/"+std::to_wstring(shared->techEnhanceStatus)+
  L" placement="+std::to_wstring(buildAnywhereMode)+L"/"+std::to_wstring(incoming.placementStatus)+
  L" map="+std::to_wstring(extra.renderReveal)+L"/"+std::to_wstring(incoming.renderRevealStatus)+
  L" air="+std::to_wstring(extra.nativeAirWaypoints?extra.nativeAirNetwork+1:0)+L"/"+std::to_wstring(incoming.nativeAirStatus)+
  L" control="+std::to_wstring(extra.controlRequest())+L"/"+std::to_wstring(shared->playerControlStatus)+
  L" network="+std::to_wstring(extra.networkHouseRequested())+L"/"+std::to_wstring(shared->networkHouseStatus)+
  L" commandOwner="+std::to_wstring(extra.commandOwnerRequested())+L"/"+std::to_wstring(shared->controlCommandOwnerStatus)+
  L" mega="+std::to_wstring(extra.nativeMegaGateway)+L"/"+std::to_wstring(shared->nativeMegaGatewayStatus)+
  L" sellCursor="+std::to_wstring(extra.nativeSellCursor)+L"/"+std::to_wstring(shared->nativeSellCursorStatus)+
  L" sellCursorReason="+(nativesell::failureText(shared->nativeSellCursorStatus)?std::wstring(nativesell::failureText(shared->nativeSellCursorStatus)):L"none")+
  L" crc="+std::to_wstring(extra.crcMapShadow)+L"/"+std::to_wstring(shared->crcMapShadowStatus)+
  L" tooltip="+std::to_wstring(buildTimeTooltipEnabled)+L"/"+std::to_wstring(shared->buildTimeTooltipStatus)+
  L" lowDamage="+std::to_wstring(extra.lowDamageFilter?(extra.lowDamageNetwork?1:2):0)+L"/"+std::to_wstring(shared->lowDamageStatus));
 if(sellDebugValid){
  std::wstring detail=L"native-sell-admission status="+std::to_wstring(shared->nativeSellCursorStatus)+L" session="+std::to_wstring(incoming.session);
  for(const auto&o:{sellCursorEntry,sellOwnerEntry})detail+=L" entry="+std::to_wstring(o.id&0x00ffffffu)+L" signature="+std::to_wstring(uint32_t(o.signature))+L" hook="+std::to_wstring(uint32_t(o.hook))+L" reason="+std::to_wstring(uint32_t(o.reason))+L" error="+std::to_wstring(o.error)+L" ageMs="+std::to_wstring(DWORD(now-o.tick));
  critical(detail);
 }
}
