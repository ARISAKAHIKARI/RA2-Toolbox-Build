
#include "runtime_feature_demand.h"
#include "control_link.h"
#include "hotkey_routes.h"
#include "type_hotkey.h"
#include "hook_policy.h"
#include "destruct_owner_input.h"
#include "placement_event_input.h"
#include <array>
#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
using namespace assist;
using DWORD=uint32_t;using WORD=uint16_t;using BYTE=uint8_t;using UINT=unsigned;using HWND=uintptr_t;
constexpr UINT MOD_ALT=1,MOD_CONTROL=2,MOD_SHIFT=4,MOD_NOREPEAT=0x4000;
constexpr unsigned HOTKEYF_SHIFT=1,HOTKEYF_CONTROL=2,HOTKEYF_ALT=4;
static unsigned LOBYTE(uintptr_t v){return v&255;}
static unsigned HIBYTE(uintptr_t v){return (v>>8)&255;}
static DWORD now=10000,lastData=0,pid=321;
static DWORD GetTickCount(){return now;}
struct LARGE_INTEGER{int64_t QuadPart=0;};
static void QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=int64_t(now)*1000;}
static void QueryPerformanceFrequency(LARGE_INTEGER*p){p->QuadPart=1000000;}
static bool enabled=true,focused=true,diagEnabled=false,infoAdaptive=false;
static bool debugPageActive=false,techMode=false,buildTimeTooltipEnabled=false,hasSelection=false;
static bool foregroundGame(){return focused;}
static bool secondaryDisplayAvailable(){return false;}
static int informationFPS(){return 100;}
static DWORD sampleRenderClock(void*){return now;}
static void critical(const std::wstring&){}static void diag(const std::wstring&){}static void notify(const std::wstring&){}
static void probeNativeViewportMapping(){}
static std::unique_ptr<Shared> storage=std::make_unique<Shared>();
static Shared* shared=nullptr;
static Snapshot snap{},incoming{},externalPublished{};
static const Snapshot*externalData=nullptr;
static AdditionalSettings extra;
static DWORD externalAt=0;
static bool externalSourceConfirmed=false;
static unsigned bridgeAttempts=0,samplerCalls=0,scopeAttempts=0,scopeFailures=0,wakes=0;
static uint32_t configuredFlags=0;
static uint32_t flags(){return configuredFlags;}
static uint32_t statusOptions(){return 0;}
static uint32_t commandMatch();static uint32_t displayMatch();
static bool commandChannelReady();static void mergeExternal();
static void wakeGame(){++wakes;}
static bool ensureBridge(){
 ++bridgeAttempts;shared=storage.get();shared->stop=0;
 incoming={};incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=1;lastData=now;
 return true;
}
static struct{unsigned size=0;}commandQueue;
static struct{std::set<uint32_t>stopped;}plannerState;
namespace nativeviewhost{
struct Transform{uint32_t generation=1;};
static bool sampleTransform(Shared*,Transform&t){t={};return true;}
}
namespace pipelineperf{
struct Window{Window(const wchar_t*,std::initializer_list<const wchar_t*>){}};
struct Scope{template<class F>Scope(Window&,int,F){}};
}
struct Motion{void projection(uint32_t){}void clear(){}template<class T>void update(const T&,int){}};
static Motion motion,statusMotion,enemyMotion;
static std::map<uint32_t,int>labelRasters;
static void clearEnemyDisplay(){}static void clearPassengerDisplay(){}static void rememberLiveTypes(){}
static void updateEnemyDisplay(const Snapshot&){}static void updateStatusDisplay(const Snapshot&){}
static struct{uint64_t samples=0,paints=0,uploads=0,readUs=0,maxReadUs=0;DWORD since=0,maxGap=0;}displayCounters;
namespace worldframe{
enum{Base=1,Combat=2,Passengers=4};
struct Key{uint32_t pid=321,match=1,connectionLow=1,connectionHigh=0,tick=0,serial=0;int frame=0;
 bool sameWorld(const Key&k)const{return pid==k.pid&&match==k.match;}
 bool sameFrame(const Key&k)const{return sameWorld(k)&&frame==k.frame;}
};
struct Frame{Key key;};using Token=std::shared_ptr<const Frame>;
}
static worldframe::Token externalWorldSource;
struct ReaderBoundary{
 uint32_t errors=0,lastReadError=0,lastFailedAddress=0,lastFailedSize=0,lastBytesRead=0,pageHits=0,pageFallbacks=0,lastRegionState=0,lastRegionProtect=0,lastRegionQueryError=0;
 bool valid()const{return true;}void begin(){}void setWorldDemand(uint32_t){}
 uint32_t pid()const{return 321;}uint64_t generation()const{return 1;}
 bool readLive(uint32_t,void*out,size_t n){std::memset(out,0,n);return true;}
 bool readFresh(uint32_t p,void*out,size_t n){return readLive(p,out,n);}
 worldframe::Token worldFrameToken(){auto f=std::make_shared<worldframe::Frame>();f->key.tick=now;f->key.frame=int(now/100);f->key.serial=now;return f;}
};
static ReaderBoundary externalReader;
namespace external{
// This is the missing/available IPC boundary, not the production selection
// rule. The actual function decides whether and when to acquire it.
struct WorldFrameScope{
 bool bound;WorldFrameScope(ReaderBoundary&,uint32_t,worldframe::Token={}):bound(shared!=nullptr){++scopeAttempts;if(!bound)++scopeFailures;}
 explicit operator bool()const{return bound;}bool complete()const{return bound;}
};
namespace attacktrace{struct Recorder{template<class...T>void sample(T&&...){}};}
namespace watch{static bool firstBinding(const uint32_t*k,int slot){if(slot<0||slot>=3||!(k[slot]&255))return false;for(int n=0;n<slot;++n)if(k[n]==k[slot])return false;return true;}}
}
static external::attacktrace::Recorder attackRecorder;
struct SamplerBoundary{
 Snapshot value{};unsigned flashCount=0;
 void viewport(const nativeviewhost::Transform&,bool){}void stopped(const std::set<uint32_t>&){}
 void worldEpoch(uint32_t){}void commitSample(){}void invalidateSample(const Snapshot*p){if(p)value=*p;}
 const Snapshot&state()const{return value;}void refreshCamera(DWORD){}
 void reset(){value={};flashCount=0;}
 struct Aircraft{bool current(uint32_t,int,uint32_t,DWORD,bool)const{return true;}};
 Aircraft idleAircraftDisplay()const{return {};}
 bool refreshAircraft(DWORD,uint32_t){return true;}
 const Snapshot&sample(DWORD tick,uint32_t,bool,uint32_t,uint32_t=0){
  ++samplerCalls;value={};value.status=value.readStatus=value.renderStatus=1;value.match=1;value.frame=int(now/100);value.sampledTick=tick;value.width=1024;value.height=768;return value;
 }
 struct State{int tactical=1,specialDialog=0,dialogCount=0,readable=1;};State displayState(){return {};}
 struct Probe{int stage=1;uint32_t address=0,value=0;};Probe probe(){return {};}
};
static SamplerBoundary externalSampler;
static bool keysRegistered=false,hookSmartUnload=false,hookAutoCS=false,buildAnywhereMode=false;
static int registeredHotkeyMode=0;static HWND ui=1;
static WORD hotkeys[12]{},ejectKey=0,unloadKey=0,csKey=0,idleAircraftKey=0,socialKeys[9]{};
struct Binding{std::string id;WORD key;};static std::vector<Binding>swBindings,typeBindings;
static typekeys::Tap typeTap,variantTap,formationTap;
static hotkeyroute::Registry hotkeyRegistry;static hotkeyroute::Pending hotkeyPending;
static std::vector<int>registeredKeyIDs,osRegistered;
static short GetAsyncKeyState(int){return 0;}
static bool RegisterHotKey(HWND,int id,UINT,unsigned){osRegistered.push_back(id);return true;}
static bool UnregisterHotKey(HWND,int id){osRegistered.erase(std::remove(osRegistered.begin(),osRegistered.end(),id),osRegistered.end());return true;}
static DWORD GetLastError(){return 0;}
static bool destructOwnerKeyConflict(WORD){return false;}
struct HotkeyDiagnostics{uint32_t messages=0,sampled=0,actions=0,blocked=0,fallbacks=0,lastError=0;};
static HotkeyDiagnostics hotkeyDiagnostics;
static void logHotkeyInput(const wchar_t*,int=0,uint16_t=0,DWORD=0){}

static controllink::View commandLink(){return {shared!=nullptr,lastData!=0,shared&&shared->stop!=0,
 incoming.status,incoming.commandStatus,incoming.match};}

static uint32_t commandMatch(){return controllink::identity(commandLink());}

static uint32_t displayMatch(){return externalData&&externalData->status==1?externalData->match:snap.match;}

static bool commandChannelReady(){return controllink::ready(commandLink());}



static int desiredHotkeyMode(){
 if(!foregroundGame()||snap.renderStatus!=1||snap.chat)return 0;
 if(enabled&&snap.status==1)return 2;
 return !enabled&&extra.watchSecondary&&snap.readStatus==1?1:0;
}

static UINT modifiers(WORD key){BYTE f=HIBYTE(key);return MOD_NOREPEAT|((f&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((f&HOTKEYF_ALT)?MOD_ALT:0)|((f&HOTKEYF_SHIFT)?MOD_SHIFT:0);}

static void synchronizeHotkeyKeys(){uint8_t down[256]{};for(unsigned key=0;key<256;++key)down[key]=(GetAsyncKeyState(int(key))&0x8000)!=0;hotkeyRegistry.synchronize(down,registeredHotkeyMode==1?displayMatch():commandMatch());}

static bool registerKey(int id,WORD key){return hotkeyRegistry.add(id,key,extra.allHotkeys,[](int slot,WORD chord){
 if(keygesture::sampledOnly(chord))return true;
 if(!RegisterHotKey(ui,slot,modifiers(chord),LOBYTE(chord)))return false;registeredKeyIDs.push_back(slot);return true;});}

static void registerKeys(){for(int id:registeredKeyIDs)UnregisterHotKey(ui,id);registeredKeyIDs.clear();hotkeyRegistry.clear();hotkeyPending.clear();typeTap.clear();variantTap.clear();formationTap.clear();registeredHotkeyMode=desiredHotkeyMode();keysRegistered=registeredHotkeyMode!=0;if(!keysRegistered)return;int failures=0;
 if(registeredHotkeyMode==1){
  for(int i=0;i<3;++i)if(external::watch::firstBinding(extra.watchKeys,i)&&!registerKey(1730+i,WORD(extra.watchKeys[i])))++failures;
  synchronizeHotkeyKeys();if(failures)notify(L"副屏跟踪快捷键注册失败，请更换组合。");return;
 }
 for(int i=0;i<12;++i)if((i!=8||extra.manualReveal)&&LOBYTE(hotkeys[i])&&!registerKey(1000+i,hotkeys[i]))++failures;
 for(size_t i=0;i<swBindings.size()&&i<64;++i){auto&b=swBindings[i];if((b.id=="IronCurtainSpecial"&&b.key==hotkeys[9])||(b.id=="ChronoSphereSpecial"&&b.key==hotkeys[10]))continue;if(LOBYTE(b.key)&&!registerKey(int(1020+i),b.key))++failures;}
 for(size_t i=0;i<typeBindings.size()&&i<512;++i){auto&b=typeBindings[i];if(typekeys::firstForKey(typeBindings,unsigned(i))&&!registerKey(int(1100+i),b.key))++failures;}
 for(const auto&entry:autofeatures::Entries)if(entry.setting){const WORD key=WORD(extra.automaticKeys[entry.feature]);if(LOBYTE(key)&&!registerKey(entry.action,key))++failures;}
 if(LOBYTE(ejectKey)&&!registerKey(1696,ejectKey))++failures;
 if(hookSmartUnload&&LOBYTE(unloadKey)&&!registerKey(1698,unloadKey))++failures;
 if(featureavailability::AutoCS&&hookAutoCS&&LOBYTE(csKey)&&!registerKey(1699,csKey))++failures;
 if(extra.sidebarEnabled&&LOBYTE(extra.sidebarKey)&&!registerKey(1734,WORD(extra.sidebarKey)))++failures;
 if(extra.viewZoomEnabled){for(int i=0;i<3;++i)if(!registerKey(1720+i,WORD(extra.viewZoomKeys[i])))++failures;}
 if(LOBYTE(extra.stickyKey)&&!registerKey(1723,WORD(extra.stickyKey)))++failures;
 if(buildAnywhereMode&&extra.placementImplementation==buildingmodes::PlacementEvent&&LOBYTE(extra.placementEventKey)&&!registerKey(placementeventhost::Hotkey,WORD(extra.placementEventKey)))++failures;
 for(int i=0;i<selectedmode::Count;++i)if(LOBYTE(extra.missionModeKeys[i])&&!registerKey(1724+i,WORD(extra.missionModeKeys[i])))++failures;
 if(extra.watchEnabled||extra.watchSecondary)for(int i=0;i<3;++i)if(external::watch::firstBinding(extra.watchKeys,i)&&!registerKey(1730+i,WORD(extra.watchKeys[i])))++failures;
 if(LOBYTE(idleAircraftKey)&&!registerKey(1701,idleAircraftKey))++failures;
 for(int i=0;i<9;++i)if(LOBYTE(socialKeys[i])&&!registerKey(1710+i,socialKeys[i]))++failures;
 if(LOBYTE(extra.variantKey)&&!registerKey(1700,WORD(extra.variantKey)))++failures;
 // Destruction never joins a colliding/fanout group, including later edits.
 if(extra.destructSelectedOwner&&LOBYTE(extra.destructSelectedOwnerKey)){
  if(destructOwnerKeyConflict(WORD(extra.destructSelectedOwnerKey))||!destructownerhost::exclusive(hotkeyRegistry,extra.destructSelectedOwnerKey)||
     !registerKey(destructownerhost::Hotkey,WORD(extra.destructSelectedOwnerKey)))++failures;
 }
 synchronizeHotkeyKeys();if(failures)notify(L"有 "+std::to_wstring(failures)+L" 个快捷键冲突或注册失败，请更换组合，修改后自动生效。");
}

static void discardExternalWorld(){
 externalData=nullptr;externalPublished={};externalWorldSource.reset();externalSourceConfirmed=false;externalSampler.reset();
 snap.readStatus=snap.renderStatus=0;snap.labels=snap.selected=snap.transports=snap.used=snap.capacity=snap.threatCount=snap.crateCount=0;
 motion.clear();statusMotion.clear();labelRasters.clear();clearEnemyDisplay();clearPassengerDisplay();
}

static void mergeExternal(){
 if(!externalData)return;
 if(commandMatch()&&externalPublished.match!=commandMatch()){discardExternalWorld();return;}
 const auto&d=*externalData;const DWORD displayNow=sampleRenderClock(nullptr);
 bool control=commandMatch()!=0; // preserve authoritative command identity/catalogue even when display data is delayed
 // External snapshots are display data, never authoritative for a live
 // command bridge. An incompatible EXE layout must not disable a healthy DLL.
 snap.readStatus=d.status==1&&d.sampledTick&&DWORD(displayNow-d.sampledTick)>=2000&&!externalSourceConfirmed?0:d.status;
 snap.bridgeStatus=shared&&lastData&&DWORD(GetTickCount()-lastData)<1500?incoming.status:0;
 snap.commandStatus=commandChannelReady()?1:0;
 const bool externalValid=d.status==1&&d.sampledTick&&
                           (DWORD(displayNow-d.sampledTick)<2000||externalSourceConfirmed);
 snap.renderStatus=externalValid&&d.renderStatus==1?1:0;
 snap.status=gateway::chooseVisibleStatus({snap.readStatus,snap.bridgeStatus,snap.commandStatus,snap.renderStatus});
 if(!externalValid){motion.clear();statusMotion.clear();labelRasters.clear();clearEnemyDisplay();return;}
 if(!control){snap.frame=d.frame;snap.session=d.session;snap.chat=d.chat;}
 snap.match=controllink::mergedIdentity(commandLink(),d.match);
 snap.width=d.width;snap.height=d.height;snap.sampledTick=d.sampledTick;snap.cameraX=d.cameraX;snap.cameraY=d.cameraY;
 snap.labels=std::clamp(d.labels,0,MaxLabels);memcpy(snap.label,d.label,size_t(snap.labels)*sizeof(Label));
 snap.selected=d.selected;snap.transports=d.transports;snap.used=d.used;snap.capacity=d.capacity;snap.credits=d.credits;
 snap.factories=d.factories;snap.busyFactories=d.busyFactories;snap.pausedFactories=d.pausedFactories;snap.completedFactories=d.completedFactories;
 // Command catalogue and superweapons stay with the authenticated bridge
 // identity; a delayed callback does not authorize an external epoch switch.
 if(!control){
  snap.typeCount=std::clamp(d.typeCount,0,MaxTypes);memcpy(snap.types,d.types,size_t(snap.typeCount)*sizeof(Type));
  snap.swCount=std::clamp(d.swCount,0,MaxSW);memcpy(snap.supers,d.supers,size_t(snap.swCount)*sizeof(Super));
 }
 snap.threatCount=std::clamp(d.threatCount,0,MaxThreats);memcpy(snap.threats,d.threats,size_t(snap.threatCount)*sizeof(Threat));
 snap.crateCount=std::clamp(d.crateCount,0,MaxCrates);memcpy(snap.crates,d.crates,size_t(snap.crateCount)*sizeof(CrateMark));
 if(snap.readStatus==1){rememberLiveTypes();if(snap.renderStatus==1)motion.update(snap,d.frame);else motion.clear();if(labelRasters.size()>size_t(snap.labels+32)){std::unordered_set<uint32_t> live;for(int i=0;i<snap.labels;++i)live.insert(snap.label[i].id);for(auto i=labelRasters.begin();i!=labelRasters.end();)if(!live.count(i->first))i=labelRasters.erase(i);else ++i;}}else{motion.clear();labelRasters.clear();}
 updateEnemyDisplay(d);updateStatusDisplay(d);
}

static void sampleExternal(DWORD now){
 probeNativeViewportMapping();
 nativeviewhost::Transform viewportTransform;const bool viewportKnown=nativeviewhost::sampleTransform(shared,viewportTransform);
 externalSampler.viewport(viewportTransform,viewportKnown);
 if(viewportKnown){motion.projection(viewportTransform.generation);statusMotion.projection(viewportTransform.generation);enemyMotion.projection(viewportTransform.generation);}
 static pipelineperf::Window timings(L"information-sample",{L"total"});pipelineperf::Scope total(timings,0,critical);
 if(shared&&shared->viewZoomBusy)return;
 
 if(!externalReader.valid())return;
 const DWORD displayNow=sampleRenderClock(nullptr);
 const DWORD period=((enabled&&foregroundGame())||secondaryDisplayAvailable())?DWORD(1000/informationFPS()):250;
 if(!externalAt||DWORD(displayNow-externalAt)>=period){
  const DWORD gap=externalAt?DWORD(displayNow-externalAt):0;
  LARGE_INTEGER sampleStart{},sampleEnd{},sampleFrequency{};QueryPerformanceCounter(&sampleStart);
  externalAt=displayNow;externalReader.begin();externalSourceConfirmed=false;
  if(externalWorldSource&&((commandMatch()&&externalWorldSource->key.match!=commandMatch())||externalWorldSource->key.pid!=externalReader.pid()||
    ((uint64_t(externalWorldSource->key.connectionHigh)<<32)|externalWorldSource->key.connectionLow)!=externalReader.generation()))discardExternalWorld();
  externalReader.setWorldDemand(worldframe::Base|((flags()&(Capacity|AllTransports))?worldframe::Passengers:0));
  external::WorldFrameScope worldScope(externalReader,commandMatch());
  if(!worldScope){
   if(externalWorldSource){external::WorldFrameScope retained(externalReader,commandMatch(),externalWorldSource);if(!retained)discardExternalWorld();}
   if(shared)wakeGame();return;
  }
  const auto source=externalReader.worldFrameToken();
  if(externalWorldSource&&(!externalWorldSource->key.sameWorld(source->key)||source->key.frame<externalWorldSource->key.frame))discardExternalWorld();
  struct UIKey {uint32_t selected=0,flags=0,status=0,viewport=0;bool enemies=false;};static UIKey previousUI;
  UIKey ui{2166136261u,flags(),statusOptions(),viewportTransform.generation,extra.enemyMarkers};
  struct Selected {uint32_t vt,items;int capacity;uint32_t flags;int count,step;}selected{};
  std::array<uint32_t,4096>selectedItems{};
  if(externalReader.readLive(0xA8ECB8,&selected,sizeof selected)&&selected.count>=0&&selected.count<=4096&&selected.capacity>=selected.count&&(!selected.count||externalReader.readLive(selected.items,selectedItems.data(),size_t(selected.count)*4))){
   for(int i=0;i<selected.count;++i)ui.selected=(ui.selected^selectedItems[i])*16777619u;
  }else ui.selected=0;
  const bool sameUI=ui.selected==previousUI.selected&&ui.flags==previousUI.flags&&ui.status==previousUI.status&&ui.viewport==previousUI.viewport&&ui.enemies==previousUI.enemies;
  externalSampler.worldEpoch(source->key.match);
  if(externalWorldSource&&externalWorldSource->key.sameFrame(source->key)&&externalWorldSource->key.serial==source->key.serial&&sameUI){
   externalSampler.refreshCamera(displayNow);externalPublished=externalSampler.state();externalPublished.sampledTick=source->key.tick;externalData=&externalPublished;externalSourceConfirmed=true;mergeExternal();return;
  }
  externalSampler.stopped(plannerState.stopped);const auto&candidate=externalSampler.sample(source->key.tick,flags(),extra.enemyMarkers,statusOptions());
  if(!worldScope.complete()){externalSampler.invalidateSample(&externalPublished);return;}
  externalSampler.commitSample();
  externalPublished=candidate;externalData=&externalPublished;externalWorldSource=source;externalSourceConfirmed=true;previousUI=ui;
  if(externalData->status==1)attackRecorder.sample([](uint32_t p,void*out,size_t n){return externalReader.readFresh(p,out,n);},
   [](const std::string&s){critical(std::wstring(s.begin(),s.end()));},now,externalReader.generation(),externalData->match,
   "flags="+std::to_string(flags())+" global="+std::to_string(extra.magnetronGlobal)+" drop="+std::to_string(extra.magnetronDrop)+
   "toolEnabled="+std::to_string(enabled)+" sent="+std::to_string(incoming.queuedEvents));
  QueryPerformanceCounter(&sampleEnd);QueryPerformanceFrequency(&sampleFrequency);
  const uint64_t cost=sampleFrequency.QuadPart>0?uint64_t(sampleEnd.QuadPart-sampleStart.QuadPart)*1000000/uint64_t(sampleFrequency.QuadPart):0;
  auto&dc=displayCounters;if(!dc.since)dc.since=displayNow;
  ++dc.samples;dc.maxGap=std::max(dc.maxGap,gap);dc.readUs+=cost;dc.maxReadUs=std::max(dc.maxReadUs,cost);
  const auto displayState=externalSampler.displayState();
  static int previousVisible=-1;int visible=externalData->renderStatus;
  if(visible!=previousVisible){previousVisible=visible;
   critical(L"overlay visibility="+std::to_wstring(visible)+L" tactical="+std::to_wstring(displayState.tactical)+
    L" specialDialog="+std::to_wstring(displayState.specialDialog)+L" dialogCount="+std::to_wstring(displayState.dialogCount)+
    L" readable="+std::to_wstring(displayState.readable)+L" frame="+std::to_wstring(externalData->frame));
  }
  DWORD elapsed=displayNow-dc.since;
  if(elapsed>=5000){
   critical(L"display targetHz="+std::to_wstring(informationFPS())+L" adaptive="+std::to_wstring(infoAdaptive)+L" windowMs="+std::to_wstring(elapsed)+
    L" samples="+std::to_wstring(dc.samples)+L" paintCalls="+std::to_wstring(dc.paints)+L" uploads="+std::to_wstring(dc.uploads)+
    L" meanReadUs="+std::to_wstring(dc.samples?dc.readUs/dc.samples:0)+L" maxReadUs="+std::to_wstring(dc.maxReadUs)+
    L" maxSampleGapMs="+std::to_wstring(dc.maxGap)+L" labels="+std::to_wstring(externalData->labels)+
    L" foreground="+std::to_wstring(foregroundGame()));
   dc={};dc.since=displayNow;
  }
  if(diagEnabled){
   static int previousProbe=-1;static DWORD previousProbeAt=0;
   const auto p=externalSampler.probe();
   if(int(p.stage)!=previousProbe||DWORD(now-previousProbeAt)>10000){
    previousProbe=int(p.stage);previousProbeAt=now;
    diag(L"layout probe="+std::to_wstring(previousProbe)+L" address="+std::to_wstring(p.address)+
         L" value="+std::to_wstring(p.value)+L" pid="+std::to_wstring(pid));
   }
  }
  if(diagEnabled&&externalReader.errors){
   static DWORD reportedAt=0,reportedCode=~0u;
   if(reportedCode!=externalReader.lastReadError||DWORD(now-reportedAt)>=5000){
    reportedAt=now;reportedCode=externalReader.lastReadError;
    diag(L"external ReadProcessMemory error="+std::to_wstring(reportedCode)+
       L" address="+std::to_wstring(externalReader.lastFailedAddress)+
       L" length="+std::to_wstring(externalReader.lastFailedSize)+
       L" got="+std::to_wstring(externalReader.lastBytesRead)+
       L" pageHits="+std::to_wstring(externalReader.pageHits)+
       L" pageFallbacks="+std::to_wstring(externalReader.pageFallbacks)+
       L" vqState="+std::to_wstring(externalReader.lastRegionState)+
       L" vqProtect="+std::to_wstring(externalReader.lastRegionProtect)+
       L" vqError="+std::to_wstring(externalReader.lastRegionQueryError));
   }
  }
 }
 else if((enabled&&foregroundGame())||secondaryDisplayAvailable()){externalReader.begin();externalSampler.refreshCamera(displayNow);if(externalData){externalPublished=externalSampler.state();externalData=&externalPublished;}}
 mergeExternal();
}
static void oneTick(){
if(registeredHotkeyMode!=desiredHotkeyMode())registerKeys();
sampleExternal(now);uint32_t configured=flags();
 bool needsBridge=debugPageActive||runtimeFeatureDemand(configured,extra,techMode,buildTimeTooltipEnabled)||(shared&&shared->minerDefenseActive)||(externalData&&externalData->status==1)||commandQueue.size||hasSelection||externalSampler.flashCount;
 if(needsBridge&&!shared)ensureBridge();
}

static unsigned checks=0;
static void check(bool okay,const char*name){++checks;if(!okay)throw std::runtime_error(name);}
static void reset(const std::string&scenario){
 now+=20000;lastData=0;shared=nullptr;snap={};incoming={};externalPublished={};externalData=nullptr;externalAt=0;externalSourceConfirmed=false;externalWorldSource.reset();externalSampler.reset();
 bridgeAttempts=samplerCalls=scopeAttempts=scopeFailures=wakes=0;configuredFlags=0;
 enabled=focused=true;debugPageActive=techMode=buildTimeTooltipEnabled=hasSelection=false;
 commandQueue.size=0;extra={};extra.missionModeKeys[selectedmode::OwnGround]=0;
 hotkeys[11]=WORD('L'|((HOTKEYF_CONTROL|HOTKEYF_ALT)<<8));
 keysRegistered=false;registeredHotkeyMode=0;registeredKeyIDs.clear();osRegistered.clear();hotkeyRegistry.clear();hotkeyPending.clear();
 if(scenario=="default_own_ground")extra=AdditionalSettings{};
 if(scenario=="auto_repair_demand")configuredFlags=AutoRepair;
 if(scenario=="already_connected")ensureBridge();
 check(hotkeys[11]==0x064c,"AutoBoard remains bound to Ctrl+Alt+L");
 check(!extra.enemyMarkers&&!extra.watchSecondary&&!buildTimeTooltipEnabled,"no hidden display/tooltip opt-in");
 const bool expectedDemand=scenario=="default_own_ground"||scenario=="auto_repair_demand";
 check(runtimeFeatureDemand(flags(),extra,techMode,buildTimeTooltipEnabled)==expectedDemand,"actual production runtimeFeatureDemand matches preconditions");
}
static void publishCommandBoundary(){
 if(!shared)return;incoming.status=incoming.bridgeStatus=incoming.commandStatus=1;incoming.match=1;incoming.chat=0;lastData=now;snap=incoming;mergeExternal();
}
int main(){
 try{
  for(const std::string scenario:{"manual_only","default_own_ground","auto_repair_demand","already_connected"}){
   reset(scenario);
   for(unsigned tick=0;tick<12;++tick){now+=100;oneTick();publishCommandBoundary();}
   const bool expectedReady=!REGRESSED_SOURCE||scenario!="manual_only";
   const bool autoBoardRegistered=std::find(osRegistered.begin(),osRegistered.end(),1011)!=osRegistered.end();
   check((bridgeAttempts>0)==expectedReady,"bridge connection decision");
   check((externalData!=nullptr)==expectedReady,"display publication decision");
   check((registeredHotkeyMode==2)==expectedReady,"command registration mode");
   check(autoBoardRegistered==expectedReady,"real registerKeys includes AutoBoard only after mode admission");
   check((samplerCalls>0)==expectedReady,"raw sampler admission");
   if(!expectedReady)check(scopeFailures==12&&bridgeAttempts==0,"missing Runtime IPC loops without any connection attempt");
   std::cout<<"{\"scenario\":\""<<scenario<<"\",\"ticks\":12,\"feature_demand\":"<<runtimeFeatureDemand(flags(),extra,techMode,buildTimeTooltipEnabled)<<",\"bridge_attempts\":"<<bridgeAttempts<<",\"world_scope_attempts\":"<<scopeAttempts<<",\"world_scope_failures\":"<<scopeFailures<<",\"sampler_calls\":"<<samplerCalls<<",\"external_published\":"<<(externalData!=nullptr)<<",\"command_ready\":"<<commandChannelReady()<<",\"mode\":"<<registeredHotkeyMode<<",\"autoboard_registered\":"<<autoBoardRegistered<<"}\n";
  }
  std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"scenarios\":4}\n";
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 2;}
}
