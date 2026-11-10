// Native sidebar technology test. Two UI eligibility callsites use build-limit
// validation; Factory::DemandProduction and simulation rules remain native.
// The helpers below are also used by ordinary production and cameo feedback.
static constexpr uint32_t Sidebar=0x87F7E8, Tabs=Sidebar+0x1544;
#include "sidebar_policy.h"
#include "technology_mode_policy.h"
static int typeSlot(uint32_t);
static bool techPending;static bool setTechSites(bool);static bool nativeTechRetired();static bool nativeTextTechRetired();static bool requestTechParameters(uint32_t),restoreTechParameters(),techParametersRetired();static bool riskTechActive;
static bool applyTechData(uint32_t),restoreTechData(),techDataPending(),techDataOwns(uint32_t),techDataBlocksEnhancement();
// OpenTech arg2 selects the implementation; Snapshot::techOpen is 0/1/2/3/4/5.
// Keep the existing implementation as 0 and make the early-source path opt-in.
static unsigned techImplementation=0;
static int techScanAt,techScanVisible,techScanUnavailable,techScanFrame=INT32_MIN;
static unsigned techScanImplementation=UINT32_MAX;
static uint64_t techScanCatalogue;
static void requestTechRefresh(){techPending=true;techScanAt=techScanVisible=techScanUnavailable=0;techScanCatalogue=catalogueHash;}
// Persistent sidebar permission must not oscillate on host UI stalls. The
// central stop/dead-controller cleanup owns restoration; no heartbeat timeout
// may make native StripClass emit an unintended Abandon mid-production.
static bool techGate(uint32_t type,uint32_t house){
 if(!sh||sh->stop||!(sh->flags&UnsafeTech)||!snap.techOpen||!riskTechActive||house!=lastHouse||get<uint32_t>(0xA83D4C)!=house||!get<bool>(0xA8E9A0))return false;
 // Native sidebar calls can run before the next host sample. A reused House
 // address does not authorize the previous round's catalogue/allow list.
 // Keep permission during menus and host stalls in the SAME live round.
 const int frame=get<int>(0xA8ED84),session=get<int>(0xA8B238);
 if(frame<0||frame<snap.frame||frame>=INT32_MAX||session!=snap.session||
    !eventdelivery::mayQueue(session)||house==get<uint32_t>(0xAC1198)||
    get<bool>(house+0x1F5)||get<bool>(house+0x1F6)||get<bool>(house+0x1F8))return false;
 int slot=typeSlot(type);return slot>=0&&techAllowed[slot];
}

// Reservation uses the same live whitelist as full technology. The native
// build-limit path retains limits; factory ownership/power checks stay native.
static bool techParameterReservationAllowed(uint32_t type,uint32_t house);
static bool techEnhancementReservationCanBuild(uint32_t type,uint32_t house);
static bool reservationCanBuild(uint32_t type,uint32_t house){
 if(!type||!nativeDependencyReady(0x4F7870))return false;
 const bool full=techGate(type,house)||techParameterReservationAllowed(type,house);
 if(call<int>(0x4F7870,house,(void*)(uintptr_t)type,full,true)>0)return true;
 // Exact enhancement does not modify Type::TechLevel. Use the same guarded
 // native probe as cameo admission; a plain call has no authorized probe
 // token and would reject a type which the sidebar correctly accepted.
 // A failed full-mode limit test must never fall back to weaker admission.
 return !full&&techEnhancementReservationCanBuild(type,house);
}
static int typeSlot(uint32_t type){for(int i=0;i<snap.typeCount;++i)if(typePointers[i]==type)return i;return -1;}
static int typeTab(int i){if(i<0||i>=snap.typeCount||!nativeDependencyReady(0x6ABC60))return -1;auto&t=snap.types[i];return callFast<int>(0x6ABC60,t.kind,t.index,0);}
static void sidebarRedraw(int tab){if(tab<0||tab>3)return;put<bool>(Tabs+tab*3988+60,true);put<bool>(Sidebar+0x53A6,true);put<bool>(Sidebar+0x53A7,true);}
static int canonicalKind(int k){return sidebarpolicy::canonicalKind(k);}
static bool hasCameo(int kind,int index){for(int tab=0;tab<4;++tab){uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count<0||count>75)continue;
 for(int j=0;j<count;++j){uint32_t c=strip+88+j*52;if(get<int>(c)==index&&canonicalKind(get<int>(c+4))==kind)return true;}}return false;
}
static bool techSessionAllowed(){return decisions::canProduceInSession(eventdelivery::isMultiplayer(get<int>(0xA8B238)),sh&&(sh->flags&UnsafeTech));}
static uint32_t techFactory(uint32_t type,uint32_t house){
 if(!nativeDependencyReady(0x5F7900))return 0;
 // YRpp ObjectTypeClass::FindFactory(allowOccupied,requirePower,requireCanBuild,house).
 // A busy or unpowered factory still owns a cameo; the native click/production
 // code handles its operational state. Missing factories must remove cameos.
 uint32_t table=0,method=0;if(!read(type,&table,4)||!read(table+0x94,&method,4)||method<0x10000)return 0;
 return call<uint32_t>(method,type,true,false,false,(void*)(uintptr_t)house);
}
static bool techTransitionProductionPending(uint32_t house){
 // Restoring a patched sidebar can make the native prune pass cancel a
 // legacy product. Keep that mode until the player finishes/cancels it.
 const unsigned offsets[]={0x53AC,0x53B0,0x53B4,0x53B8,0x53BC,0x53CC};
 auto denied=[&](uint32_t type){return !type||typeSlot(type)<0||techLegacyEnhancementOwns(type)||(techImplementation==3&&techDataOwns(type))||!nativeDependencyReady(0x4F7870)||call<int>(0x4F7870,house,(void*)(uintptr_t)type,false,true)<=0;};
 for(unsigned offset:offsets){uint32_t factory=0,owner=0,product=0,items=0;int count=0;
  if(!read(house+offset,&factory,4))return true;if(!factory)continue;
  if(factory<0x10000||!read(factory+0x6C,&owner,4)||owner!=house||!read(factory+0x58,&product,4)||
     !read(factory+0x44,&items,4)||!read(factory+0x50,&count,4)||count<0||count>128)return true;
  if(product&&(product<0x10000||denied(typeOf(product,kind(product)))))return true;
  if(count&&(items<0x10000||uint64_t(items)+uint64_t(count)*4>UINT32_MAX))return true;
  for(int i=0;i<count;++i){uint32_t type=0;if(!read(items+uint32_t(i)*4,&type,4)||denied(type))return true;}
 }
 return false;
}
static bool ensureTechCameo(int i,uint32_t house,const sidebarpolicy::CameoSet*known=nullptr){
 if(i<0||i>=snap.typeCount||!techAllowed[i]||!typePointers[i])return false;
 if(techImplementation==2)return hasCameo(snap.types[i].kind,snap.types[i].index);
 if(!nativeDependencyReady(0x5F7900)||(techImplementation==3&&!nativeDependencyReady(0x4F7870)))return false;
 uint32_t factory=0;
 if(techImplementation==1||techImplementation==3||techmode::parameters(techImplementation)){
  // Classic keeps the original build-limit check. Data mode additionally
  // requires the full native rule check after its owned Type changes.
  uint32_t type=typePointers[i],table=0,method=0;
  if(!read(type,&table,4)||!read(table+0x94,&method,4)||method<0x10000)return false;
  if(techImplementation==3&&call<int>(0x4F7870,house,(void*)(uintptr_t)type,false,true)<=0)return false;
  factory=call<uint32_t>(method,type,true,false,true,(void*)(uintptr_t)house);
 }else factory=techFactory(typePointers[i],house);
 if(!factory)return false;
 auto&t=snap.types[i];if(known?known->contains(t.kind,t.index):hasCameo(t.kind,t.index))return true;
 if(!nativeDependenciesReady(0x6ABC60,0x6A6300))return false;
 int tab=typeTab(i);if(tab<0||tab>3||get<int>(Tabs+tab*3988+84)>=75)return false;
 if(!call<bool>(0x6A6300,Sidebar,t.kind,t.index))return false;sidebarRedraw(tab);return hasCameo(t.kind,t.index);
}
static void restoreTech(){techPending=false;techScanAt=techScanVisible=techScanUnavailable=0;techScanFrame=INT32_MIN;techScanImplementation=UINT32_MAX;snap.techOpen=0;techImplementation=0;setTechSites(false);if(!restoreTechParameters())snap.techError=-45;if(!restoreTechData())snap.techError=-44;}
static void observeMasterCameos(){
 // The master mode only owns its verified eligibility patches. Building
 // completion and other native triggers refresh the sidebar themselves;
 // neither add icons here nor request a House::RecheckTechTree.
 techScanCatalogue=catalogueHash;
 techPending=false;techScanVisible=0;
 for(int tab=0;tab<4;++tab){int count=get<int>(Tabs+tab*3988+84);if(count>=0&&count<=75)techScanVisible+=count;}
 snap.techVisible=techScanVisible;snap.techUnavailable=0;
}
static bool techNativeDependencies(unsigned implementation){
 // Master only owns its separately checked instruction sites. Other modes
 // may rebuild cameos; admit their native helpers before changing modes/data.
 return implementation==2||(nativeDependenciesReady(0x5F7900,0x6ABC60,0x6A6300)&&
  (implementation!=3||nativeDependencyReady(0x4F7870)));
}
static void refreshTech(uint32_t house,DWORD now){techRefresh=now;
 // Keep the old implementation/allow-list while production prevents a safe
 // transition. Once it clears, the host may retry its still-requested mode.
 if(snap.techError!=-43||!techTransitionProductionPending(house))snap.techError=0;
 if(!(sh->flags&UnsafeTech)){restoreTech();return;}
 if(!techNativeDependencies(techImplementation)){snap.techError=-12;techPending=true;return;}
 if(techmode::parameters(techImplementation)){
  if(!restoreTechData()){snap.techOpen=0;techPending=true;snap.techError=-44;return;}
  const bool restored=setTechSites(false);
  if(!restored||!nativeTextTechRetired()){snap.techOpen=0;techPending=true;snap.techError=-11;return;}
  if(!requestTechParameters(house)){snap.techOpen=0;techPending=true;snap.techError=-45;return;}
 }else if(techmode::data(techImplementation)){
  if(!restoreTechEnhanceParameters()){snap.techOpen=0;techPending=true;snap.techError=-45;return;}
  if(!restoreTechParameters()){snap.techOpen=0;techPending=true;snap.techError=-45;return;}
  // Data mode first retires all instruction patches, then changes only the
  // explicitly selected Type rules. Native CanBuild still gates every icon.
  const bool restored=setTechSites(false);
  if(!restored||!nativeTechRetired()){snap.techOpen=0;techPending=true;snap.techError=-11;return;}
  if(!applyTechData(house)){snap.techOpen=0;techPending=true;snap.techError=-44;return;}
 }else{
  if(!restoreTechEnhanceParameters()){snap.techOpen=0;techPending=true;snap.techError=-45;return;}
  if(!restoreTechParameters()){snap.techOpen=0;techPending=true;snap.techError=-45;return;}
  if(!restoreTechData()){snap.techOpen=0;techPending=true;snap.techError=-44;return;}
  if(!setTechSites(true)){snap.techOpen=0;techPending=false;snap.techError=-11;return;}
 }
 snap.techOpen=int(techImplementation)+1;
 if(techImplementation==2){observeMasterCameos();return;}
 if(!techPending||techScanCatalogue!=catalogueHash)requestTechRefresh();
 const int frame=get<int>(0xA8ED84);
 if(techScanFrame==frame&&techScanImplementation==techImplementation)return;
 techScanFrame=frame;techScanImplementation=techImplementation;
 // Both sidebar implementations apply the whole allowed catalogue together.
 // ensureTechCameo keeps their distinct factory checks, native capacity and
 // duplicate checks. Periodic maintenance remains once per second and does
 // not issue Produce/Abandon events.
 techScanVisible=techScanUnavailable=0;
 for(int i=0;i<snap.typeCount;++i)if(techAllowed[i]){
  if(ensureTechCameo(i,house))++techScanVisible;else ++techScanUnavailable;
 }
 techScanAt=snap.typeCount;techPending=false;
 snap.techVisible=techScanVisible;snap.techUnavailable=techScanUnavailable;
}
// Native sidebar rebuilds can remove a locally inserted icon between the
// one-second catalogue passes. Inspect the four bounded strips once per game
// frame and restore every missing legal entry, without re-querying existing
// icons. AddCameo retains native sorting, deduplication and the 75-item limit.
struct TechCameoRetry {uint32_t pointer=0,tick=0;int kind=0,index=0;};
static TechCameoRetry techCameoRetry[MaxTypes];
static uint32_t techCameoHouse=0,techCameoEpoch=0;
static uint64_t techCameoCatalogue=0;
static int techCameoFrame=INT32_MIN;static unsigned techCameoImplementation=UINT32_MAX;
static bool techCameoInventory(sidebarpolicy::CameoSet&icons){
 struct Entry {int index,kind;uint8_t remaining[44];};static_assert(sizeof(Entry)==52,"Native cameo stride");
 icons.clear();Entry entries[75];
 for(int tab=0;tab<4;++tab){const uint32_t strip=Tabs+tab*3988;int count=0,after=0;
  if(!read(strip+84,&count,4)||count<0||count>75||
     (count&&!read(strip+88,entries,size_t(count)*sizeof(Entry)))||!read(strip+84,&after,4)||after!=count)return false;
  for(int n=0;n<count;++n)if(!icons.add(entries[n].kind,entries[n].index))return false;
 }return true;
}
static void maintainTechCameos(uint32_t house,int frame,DWORD now){
 if(!sh||sh->stop||!(sh->flags&UnsafeTech)||!snap.techOpen||techPending||techImplementation==2||
    house!=lastHouse||get<uint32_t>(0xA83D4C)!=house||!get<bool>(0xA8E9A0)||frame<0||frame>=INT32_MAX||
    !sessionTracker.epoch||!techSessionAllowed()||!eventdelivery::mayQueue(get<int>(0xA8B238))||house==get<uint32_t>(0xAC1198)||get<bool>(house+0x1F5)||get<bool>(house+0x1F6)||get<bool>(house+0x1F8))return;
 if(techCameoHouse!=house||techCameoEpoch!=sessionTracker.epoch||techCameoCatalogue!=catalogueHash||
    techCameoImplementation!=techImplementation||frame<techCameoFrame){
  for(auto&r:techCameoRetry)r={};techCameoHouse=house;techCameoEpoch=sessionTracker.epoch;
  techCameoCatalogue=catalogueHash;techCameoImplementation=techImplementation;techCameoFrame=INT32_MIN;
 }
 if(frame==techCameoFrame||(frame==techScanFrame&&techScanImplementation==techImplementation))return;
 sidebarpolicy::CameoSet icons;if(!techCameoInventory(icons))return;techCameoFrame=frame;
 int visible=0,unavailable=0;
 for(int i=0;i<snap.typeCount&&i<MaxTypes;++i){auto&r=techCameoRetry[i];const auto&t=snap.types[i];
  if(!techAllowed[i]||!typePointers[i]){r={};continue;}
  if(icons.contains(t.kind,t.index)){r={};++visible;continue;}
  if(r.pointer==typePointers[i]&&r.kind==t.kind&&r.index==t.index&&DWORD(now-r.tick)<250){++unavailable;continue;}
  if(ensureTechCameo(i,house,&icons)){icons.add(t.kind,t.index);r={};++visible;}
  else{r={typePointers[i],now,t.kind,t.index};++unavailable;}
 }
 snap.techVisible=visible;snap.techUnavailable=unavailable;
}
struct FlashRef {uint32_t address;int kind,index,until;};static FlashRef flashRefs[4*75];
static void clearFlashes(){for(auto&f:flashRefs){if(f.address&&get<int>(f.address-48)==f.index&&get<int>(f.address-44)==f.kind&&get<int>(f.address)==f.until)put<int>(f.address,0);f={};}}
static void flashCameo(int type,int frame){if(type<0||type>=snap.typeCount)return;int tab=typeTab(type);if(tab!=0&&tab!=3)return;
 uint32_t strip=Tabs+tab*3988;int count=get<int>(strip+84);if(count<0||count>75)return;auto&t=snap.types[type];
 for(int j=0;j<count;++j){uint32_t c=strip+88+j*52;if(get<int>(c)==t.index&&canonicalKind(get<int>(c+4))==t.kind){int until=frame+90;put<int>(c+48,until);flashRefs[tab*75+j]={c+48,get<int>(c+4),t.index,until};sidebarRedraw(tab);return;}}
}
struct ProductionState {uint32_t factory;int type;bool busy,paused,complete,seen;};static ProductionState prodState[4];
static void scanProduction(uint32_t house,int frame,uint32_t flags){
 snap.factories=snap.busyFactories=snap.pausedFactories=snap.completedFactories=0;snap.notice=0;
 // Building tab and vehicle tab. Defenses and infantry are intentionally absent.
 const int offsets[4]={0x53BC,0x53B4,0x53B8,0x53AC};
 if(!(flags&Production))clearFlashes();
 for(int i=0;i<4;++i){auto&prev=prodState[i];uint32_t f=get<uint32_t>(house+offsets[i]);
  if(f<0x10000||get<uint32_t>(f+0x6C)!=house){prev={};continue;}
  ++snap.factories;uint32_t p=get<uint32_t>(f+0x58);int k=p>=0x10000?kind(p):-1;int type=k>=0?typeSlot(typeOf(p,k)):-1;
  bool occupied=p||get<int>(f+0x50)>0,paused=occupied&&(get<bool>(f+0x5C)||get<bool>(f+0x71)),complete=occupied&&get<int>(f+0x24)>=54;
  snap.busyFactories+=occupied;snap.pausedFactories+=paused;snap.completedFactories+=complete;
  bool event=prev.seen&&prev.factory==f&&((prev.busy&&!occupied&&!prev.complete)||(!prev.paused&&paused)||(!prev.complete&&complete));
  if((flags&Production)&&event){int changedType=type>=0?type:prev.type;int changedTab=changedType>=0?typeTab(changedType):-1;
   if(changedTab==0||changedTab==3){flashCameo(changedType,frame);++snap.noticeSerial;}
  }
  if(type<0)type=prev.factory==f?prev.type:-1;prev={f,type,occupied,paused,complete,(flags&Production)!=0};
 }
}
static void resetProduction(){memset(prodState,0,sizeof prodState);}
