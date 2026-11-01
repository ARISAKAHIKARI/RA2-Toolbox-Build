#include "../../shared/source/feature_availability.h"
#include "../../shared/source/modern_ui.h"
#include "../../shared/source/ui_message_queue.h"
#include "../../shared/source/overlay_stack.h"
#include "../../shared/source/overlay_compositor.h"
#include "../../shared/source/release_version.h"
#include "shroud_policy.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifndef UNICODE
#define UNICODE
#endif
#define _UNICODE
#include <windows.h>
#include "../../shared/source/bounded_shutdown.h"
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include "protocol.h"
#include "reveal_permission.h"

#include "../../shared/source/host_game_input.h"
#include "../../shared/source/command_cursor_hint.h"
#include "../../shared/source/automatic_log.h"
#include "../../shared/source/game_build_diagnostics.h"
#include <tlhelp32.h>
#include "../../shared/source/message_query.h"
#include "scheduler_policy.h"
#include "hook_policy.h"
#include "command_delivery.h"
#include "control_link.h"
#include "event_delivery.h"
#include "tech_profile.h"
#include "../../shared/source/viewport_scale.h"
#include "../../shared/source/overlay_recovery.h"
#include "../../shared/source/overlay_display_lease.h"
#include "../../shared/source/monitor_refresh.h"
#include "../../shared/source/secondary_display.h"
#include "type_catalog.h"
#include "planner_task.h"
#include "value_task.h"
#include "../../suite/source/settings_categories.h"
#include "../../external_reader/source/render_profile.h"
#include "clock_ms.h"
#include "label_motion.h"
#include "crate_names.h"
#include "type_hotkey.h"
#include "hotkey_routes.h"
#include "pointer_batch.h"
#include "ui_layout.h"
#include "additional_settings.h"
#include "signatures.h"
#include "../../external_reader/source/watch_slots.h"
#include "view_zoom.h"
#include "../../external_reader/source/cooperative_reader.h"
#include "../../external_reader/source/shared_reader.h"
#include "../../external_reader/source/process_discovery.h"
#include "../../external_reader/source/tactical_sampler.h"
#ifdef RA2_UNIFIED
#include "suite_host.h"
#endif
using namespace assist;using namespace Gdiplus;
extern "C" __declspec(dllexport) const uint32_t AssistHostProtocolInfo[2]={Version,sizeof(Shared)};
extern "C" __declspec(dllexport) const MappingPrefixValue AssistHostMappingPrefix{};
namespace renderupgrade {static void stop();static void shutdown();}
namespace minimap {static void hide();static void clear();static void destroy();}
namespace watchwindows {static void hide();}
static uint32_t computeRequested=compute::Default;
static bool renderUpgradeEnabled=false,unifiedOverlayEnabled=false,renderPhysicalOutput=false,renderMaterialRefresh=true,renderHighRefresh=true,renderVulkanEnabled=true;static int renderUpgradeLayer=1,renderPaletteRange=1,renderPixelScale=3,renderSceneDetail=1,renderColorPrecision=1;
static bool renderProcessing=false,renderXbrzEnabled=false;
static bool renderParticleEffects=true,renderLaserEffects=true,renderVoxelLighting=true,renderSpriteLighting=true;
static bool harvesterCounterEnabled=false,powerDeltaEnabled=false,buildTimeTooltipEnabled=false;
static int effectiveRenderDetail(){return 0;} // recovery: native source geometry
static int effectiveRenderPrecision(){return 0;}
static int effectiveRenderPalette(){return 0;}
static int effectiveRenderScale(){return 1;} // nearest source pixels, no reconstruction
static bool effectiveRenderInterpolation(){return false;}
static bool minimapEnabled=false,minimapSecondary=false;static int minimapMode=0,minimapScale=10,minimapPosition=0,minimapX=20,minimapY=60,minimapAlignX=0,minimapAlignY=0;
static int minimapSecondaryX=10000,minimapSecondaryY=10000,minimapSecondaryWidth=320;
static WORD socialKeys[9]{}; // Cheer, taunts 1..8; empty by default.
static WORD idleAircraftKey=MAKEWORD('P',HOTKEYF_CONTROL|HOTKEYF_ALT);
static void hideAircraftButton();static void destroyAircraftButton();
static void clearPassengerDisplay();
static void serviceAirPath();static void resetAirPath();static void airPathResult(int,int,uint32_t,uint32_t);
static void serviceConstruction();static void hideConstruction();static void resetConstruction();static void destroyConstruction();static void constructionResult(int,int,uint32_t,uint32_t);
static HWND ui,overlay,statusLabel,swList,techList,techSearch,swHotkey,rateEdit,marginEdit,check[7],keys[12],repairCheck,repairEdit;
static HFONT font,titleFont;static HBRUSH bgBrush,editBrush;static HWND game;static DWORD pid,gameThread;static HANDLE mapping,gameProcess;static HMODULE module;static gameinput::Subscription hook;static Shared*shared;
static void hideWorldOverlay(){overlaycompositor::host().hide(overlaycompositor::World);if(overlay)ShowWindow(overlay,SW_HIDE);}
static uint32_t lastReceiptWrite=0,lastReceiptMatch=0;
static Snapshot snap{},incoming{};static uint32_t ack,notice,oldMatch,snapshotSeq=~0u;static std::wstring root,statusText,toast;static DWORD toastUntil,lastFind,lastData,lastPulse;static int fps=visual::DefaultFPS,topMargin=20,repairMs=2000;static bool enabled=true,ready,techEnabled;static std::vector<int> techIndices;static std::wstring lastSearch;
static bool infoAdaptive=false;
static monitorrefresh::Tracker informationMonitor;
static int informationFPS(){return infoAdaptive?informationMonitor.rate():std::clamp(fps,5,500);}
static void publishSuiteDisplay();
struct DisplayCounters {DWORD since=0,samples=0,paints=0,uploads=0,maxGap=0;uint64_t readUs=0,maxReadUs=0;};
static DisplayCounters displayCounters;
static overlayui::Recovery overlayRecovery;
static int labelScalePercent=100,constructionScalePercent=100;
static visual::Motion motion;static visual::LabelView labelViews[MaxLabels];
#include "enemy_clusters.h"
static external::enemies::Frame clusteredEnemies;
static std::vector<enemyclusters::Group> enemyGroups;
static visual::LabelView clusteredEnemyViews[MaxLabels];
static visual::Motion enemyMotion;static Snapshot enemySample{};static visual::LabelView enemyViews[MaxLabels];
static_assert(external::enemies::Maximum<=MaxLabels,"Enemy markers reuse the existing display Motion capacity");
struct EnemyRaster {std::wstring text;ARGB color{};std::unique_ptr<Bitmap> image;};
using EnemyRasterKey=std::tuple<uint32_t,uint32_t,int>;
static std::map<EnemyRasterKey,EnemyRaster> enemyRasters;static uint32_t enemyRasterMatch=0;
static void clearEnemyDisplay(){enemyMotion.clear();enemyRasters.clear();enemyGroups.clear();clusteredEnemies.count=0;enemyRasterMatch=0;}
struct LabelRaster {std::wstring text;ARGB color{};float width{};int used{},capacity{},centis{},shield{},frozenCentis{};std::unique_ptr<Bitmap> image;};static std::unordered_map<uint32_t,LabelRaster> labelRasters;
static visual::Motion statusMotion;static visual::LabelView statusViews[MaxLabels];static Snapshot statusSample;
static_assert(external::status::Maximum<=MaxLabels,"Unit status markers share the existing Motion capacity");
static assist::PlayerTestInfo playerChoices[32]{};static int playerChoiceCount;static uint32_t playerChoicesMatch;
static DWORD playerChoicesTick,playerChoicesPid;static std::wstring destructPendingTarget;
static int playerChoicesCurrent=-1;static bool destructForeignNoticeShown;
static constexpr const wchar_t* DestructForeignNotice=L"联机跨所属方自毁需要“保留事件所属方”已启用；当前关闭、恢复中或不可用，本次不会发送。";
static uint64_t techAcknowledgedHash=0;static bool techMode;static bool autoRevealNewMatch=false;static uint32_t autoRevealMatch=0,autoRevealRetry=0,autoRevealDone=0,autoRevealPID=0;
static uint32_t schedulerMode=0,schedulerTarget=0,megaQueueMode=0,objectQueueMode=0;
static HWND schedulerEntryCombo,megaQueueCombo,objectQueueCombo;
static typecatalog::Catalogue savedTypes;static uint32_t catalogueRevision=0;
static uint32_t settingsRevision=1;
static bool shiftClickEnabled=true;static int shiftClickMode=0,shiftClickGap=0,shiftSidebarWidth=160,shiftSidebarTop=200;
static bool magnetronMode=false;static WORD magnetronKey;static HWND magnetronCheck,magnetronKeyEdit;
static bool hookAdvanced=true,hookSmartUnload=false,hookEject=false,hookAutoCS=false,diagEnabled=false;
static std::wstring bridgeFailure;static bool gameWindowFromYrpp=false;
static HWND hookChecks[4];static DWORD attachAt=0;static uint32_t paintRevision;static bool keysRegistered;static int scrollY;
static assist::delivery::Queue commandQueue;
static uint32_t inFlightMatch=0;static int inFlightRetries=0;static DWORD sentAt=0,lastStallLog=0;
static DWORD commandLatency,selectionLatency,plannerMicros,lastProcessCheck;
static external::ReadSession externalReader;
static external::TacticalSampler<external::SharedReadView> externalSampler(external::SharedReadView{&externalReader});
static DWORD externalAt,bridgeRetryAt;static int bridgeAttempts;static bool bridgeBlocked;
static const Snapshot* externalData;
static bool ensureBridge();static void mergeExternal();
static bool updatingControls=false;
struct ControlUpdate {bool previous=updatingControls;ControlUpdate(){updatingControls=true;}~ControlUpdate(){updatingControls=previous;}};

// 0=Ra2Tool predefined list, 1=Ra2Tool-style [tech] custom list, 2=legacy all/none.
static int techProfile=0,techImplementation=0;static bool techDirectProduce=false;
static techprofile::Lists ra2CustomTech;
static bool techDefault=true;static std::set<std::string> techExceptions;
static std::set<std::string> profileExceptions[3];
static bool techAllowed(const assist::Type& t){
 if(techImplementation==2)return true;
 bool base=techprofile::allowed(ra2CustomTech,techProfile,t.kind,t.index,techDefault);
 return base != (techExceptions.count(t.id)!=0);
}
static WORD hotkeys[12];static const int actions[]={Repair,CombatSelection,EmptyTransports,DamagedSelection,IdleHarvester,IdleEngineer,IdleTransport,ClearBeacons,RevealMap,SelectIron,SelectChrono,AutoBoard};
static std::vector<int> registeredKeyIDs;
static const wchar_t* labels[]={L"自动维修",L"选择作战单位",L"选择空载运输载具",L"选择受损单位",L"选择空闲矿车",L"选择空闲工程师",L"定位空闲运输载具",L"清除信标",L"高危：地图全开",L"铁幕目标选择",L"超时空目标选择",L"自动装车"};
static const char* iniKeys[]={"Repair","Combat","EmptyTransports","Damaged","IdleHarvester","IdleEngineer","IdleTransport","ClearBeacons","RevealMap","IronCurtain","ChronoSphere","AutoLoad"};
struct Binding{std::string id;WORD key;};static std::vector<Binding> swBindings;
static HWND typeList,typeKey,typeCS,csCheck,csKeyEdit,csStatus,tapEdit,typeIdEdit;
static std::vector<Binding> typeBindings;static std::set<std::string> csUnits;
static HWND advancedKeyEdit,unloadKeyEdit,nativeStatus,ejectCheck,ejectKeyEdit;static WORD advancedKey,unloadKey,ejectKey;static bool ejectMode;static int advancedScanHz=10,advancedTargetPriority=0,advancedRangeCells=2;static int pathTimeout=90,veteranPercent=200,elitePercent=300;
static WORD csKey,csHoldKey;static bool csMode,csHoldEnabled,oreMode,buildAnywhereMode;static int doubleMs=350,toastMs=6000;
static HWND oreCheck,oreStatus,anywhereCheck,toastEdit,csHoldCheck,csHoldEdit,detailCheck;
static bool showDetails=false;
static bool repairUnrepairable=false;
static oreconfig::Config oreTypes{};static std::wstring oreMiners=L"HARV,CMIN",oreRefineries=L"GAREFN,NAREFN";
static featuretypes::Config featureTypes{};static std::wstring featureTypeText[featuretypes::Count];static int featureTypeIndex=0;
static uint8_t pendingSelectionTypes[MaxTypes];
static delivery::Item pendingSelection;static bool hasSelection;static uint32_t selectionAck;
static std::atomic<HWND> calculationWindow{nullptr};static std::atomic<DWORD> calculationPID{0};
static void calculationCompleted(){if(HWND window=calculationWindow.load()){const DWORD process=calculationPID.load();if(process)PostMessageW(window,DataMessage,process,0);}}
static void bindCalculations(){calculationPID.store(pid);calculationWindow.store(ui);}
static planner::Task plannerTask;
static asyncwork::Latest<planner::TaskInput,planner::TaskOutput> plannerWorker([](const planner::TaskInput&i){return plannerTask.run(i);},calculationCompleted);
static planner::Statistics plannerState;static uint64_t planningGeneration=1,planningSerial=0;static bool planningBusy=false;
static std::vector<planner::Receipt> plannerReceipts;
static compute::ValueTask<dodgecompute::Input,dodgecompute::Result> dodgeTask(dodgecompute::calculate,calculationCompleted);
static compute::ValueTask<burstcompute::Input,burstcompute::Result> burstTask(burstcompute::calculate,calculationCompleted);
static compute::ValueTask<missilecompute::Input,missilecompute::Result> missileTask(missilecompute::calculate,calculationCompleted);
static compute::ValueTask<unloadcompute::Input,unloadcompute::Result> unloadTask(unloadcompute::calculate,calculationCompleted);
static compute::ValueTask<compute::LoadInput,compute::LoadResult> loadTask([](compute::LoadInput&i){static auto work=std::make_unique<loading::Workspace>();return compute::load(i,*work);},calculationCompleted);
static compute::ValueTask<compute::RepairInput,compute::RepairResult> repairTask(compute::repair,calculationCompleted);
static void resetPlanning(){calculationWindow.store(nullptr);calculationPID.store(0);++planningGeneration;planningBusy=false;plannerWorker.invalidate();plannerState={};plannerReceipts.clear();plannerMicros=0;dodgeTask.invalidate();burstTask.invalidate();missileTask.invalidate();unloadTask.invalidate();loadTask.invalidate();repairTask.invalidate();}
static long worldRevision=-1;static uint32_t plannerAck;static int submittedPlanFrame;static std::vector<planner::Order> submittedPlan;
static typekeys::Tap typeTap,variantTap,formationTap;
static hotkeyroute::Registry hotkeyRegistry;static hotkeyroute::Pending hotkeyPending;
static pointerinput::PendingMatches pointerMatches;
static std::wstring infantryEntryResult;static uint32_t infantryEntryResultMatch=0;
static void serviceHotkeyBatch();
static bool formationInputOwns(){return shared&&(shared->formationPreview.active||int32_t(shared->formationConsumedUntil-sampleClock(shared))>0);}
static bool formationHotkey(WORD);
static AdditionalSettings extra;static HWND extraControls[ExtraControlCount];
static std::unordered_map<std::string,cooperative::Quota> targetQuotas;static std::vector<std::string> quotaTypeIDs;
static uint32_t cooperativeDelivered=0,cooperativeReceipt=0;
static cooperative::Request cooperativePending{},cooperativeSourcePool{};
static std::vector<cooperative::Assignment> submittedCooperative,acceptedCooperative,processedCooperative;
static bool retryCooperative=false;
static bool saveAdditionalSettings();static uint32_t additionalFlags();static void syncAdditional();static void serviceCooperative();static void servicePointer();static void fillExtraTypes(bool force);
static void changedSetting();static void syncSecondaryLayoutControls();
static void syncSettingDependencies(bool force=false);
static void readSnapshot();static void syncTypeConfig();static void fillTypeBindings(bool force=false);static void typeBindingFields();
static std::wstring path(const wchar_t*s){return root+L"\\"+s;}static std::wstring wide(const char*s){std::wstring w;while(*s)w+=wchar_t((unsigned char)*s++);return w;}
static std::wstring hoverHelp;
static void setStatus(const std::wstring&s){if(statusText!=s){statusText=s;if(hoverHelp.empty())SetWindowTextW(statusLabel,s.c_str());}}
static void notify(const std::wstring&s){toast=s;toastUntil=GetTickCount()+DWORD(toastMs);setStatus(s);}
static void diag(const std::wstring &s){
 if(!diagEnabled||root.empty())return;
 HANDLE f=CreateFileW(path(L"Assist-debug.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(f==INVALID_HANDLE_VALUE)return;
 std::wstring line=L"["+std::to_wstring(GetTickCount())+L"] "+s+L"\r\n";
 int size=WideCharToMultiByte(CP_UTF8,0,line.c_str(),int(line.size()),nullptr,0,nullptr,nullptr);
 if(size>0){std::string data(size,'\0');WideCharToMultiByte(CP_UTF8,0,line.c_str(),int(line.size()),&data[0],size,nullptr,nullptr);DWORD written;WriteFile(f,data.data(),DWORD(data.size()),&written,nullptr);}
 CloseHandle(f);
}
static void critical(const std::wstring& s){autolog::write(s);diag(s);}
static const wchar_t* runtimeStage(LONG stage){
 switch(stage){
 case runtime::CallbackSeen:return L"callback-seen";case runtime::Connected:return L"connected";
 case runtime::EntryRejected:return L"entry-rejected";case runtime::LegacyRuntime:return L"legacy-DLL-loaded";
 case runtime::ControllerBusy:return L"other-controller-alive";case runtime::OwnerOpenFailed:return L"owner-open-failed";
 case runtime::OwnerNotAlive:return L"owner-not-alive";case runtime::WatchdogPinFailed:return L"watchdog-pin-failed";
 case runtime::WatchdogTimerFailed:return L"watchdog-timer-failed";case runtime::MappingFailed:return L"mapping-failed";
 default:return L"no-reported-callback";
 }
}
static std::wstring hex32(DWORD value){wchar_t out[16];swprintf(out,16,L"0x%08X",unsigned(value));return out;}
static void logLoadedGameModules(){
 autolog::ErrorGuard error;
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
 if(snapshot==INVALID_HANDLE_VALUE){autolog::write(L"game-modules unavailable Win32="+std::to_wstring(GetLastError()));return;}
 MODULEENTRY32W entry{};entry.dwSize=sizeof entry;std::wstring line=L"game-modules pid="+std::to_wstring(pid)+L" names-and-bases=";
 unsigned count=0;if(Module32FirstW(snapshot,&entry))do{if(++count>64){line+=L" [truncated]";break;}
  line+=std::wstring(entry.szModule)+L"@"+hex32(DWORD(uintptr_t(entry.modBaseAddr)))+L";";
 }while(Module32NextW(snapshot,&entry));CloseHandle(snapshot);autolog::write(line);
}
static void logBridgeHealth(bool force=false){
 static DWORD loggedAt=0,previousPid=0;static LONG previousStage=0;static DWORD previousError=0,previousFault=0;static int previousStatus=0;
 static bool previousData=false,previousSubscribed=false;static uint32_t previousMatch=0;
 static int previousCS=0;static uint32_t previousQueued=0,previousRejected=0;
 const DWORD now=GetTickCount();const auto h=gameinput::health(hook);
 static DWORD roundStarted=0;
 static DWORD nativePid=0,nativeSerial=0,probePid=0,probeAt=0;
 static uint32_t probeMatch=0;static std::wstring previousProbe;
 if(h.nativeValid&&h.nativeSerial&&(nativePid!=pid||nativeSerial!=h.nativeSerial)){
  nativePid=pid;nativeSerial=h.nativeSerial;
  const wchar_t* site=h.nativeAddress==0x6F49DE?L"discovery-owner-null-candidate":
   h.nativeAddress==0x70AF6C?L"sight-owner-null-candidate":h.nativeAddress==0x65DC17?L"entry-edge-owner-null-candidate":L"other";
  autolog::write(L"native-exception firstChance=1 fatal=unknown pid="+std::to_wstring(pid)+L" serial="+std::to_wstring(h.nativeSerial)+
   L" code="+hex32(h.nativeCode)+L" address="+hex32(h.nativeAddress)+L" site="+site+
   L" operation="+std::to_wstring(h.nativeOperation)+L" access="+hex32(h.nativeAccess)+
   L" thread="+std::to_wstring(h.nativeThread)+L" tick="+std::to_wstring(h.nativeTick)+
   L" eax="+hex32(h.nativeEax)+L" ecx="+hex32(h.nativeEcx)+L" esi="+hex32(h.nativeEsi)+L" ebp="+hex32(h.nativeEbp));
 }
 if(pid&&incoming.status==1&&(pid!=previousPid||incoming.match!=previousMatch||previousStatus!=1)){
  roundStarted=now;
  logLoadedGameModules();
 }
 // Platform DLLs may change code after the first attachment. Sample a small
 // fixed byte set on the host, logging only changes, never authorizing patches.
 if(pid&&incoming.status==1&&(pid!=probePid||incoming.match!=probeMatch||DWORD(now-probeAt)>=1000)){
  auto current=gamebuild::describe([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);});
  if(pid!=probePid||incoming.match!=probeMatch||current!=previousProbe){
   autolog::write(L"runtime-code-evidence frame="+std::to_wstring(incoming.frame)+L" "+current);
   if(probePid==pid)logLoadedGameModules();previousProbe=std::move(current);
  }
  probePid=pid;probeMatch=incoming.match;probeAt=now;
 }
 const bool hasDiagnostic=h.diagnosticValid||h.diagnosticCached;
 const LONG stage=hasDiagnostic?h.stage:runtime::Unseen;
 const DWORD error=h.postError?h.postError:(hasDiagnostic?h.error:0);
 const bool data=lastData&&DWORD(now-lastData)<1500;
 // Input conversions can bypass the host request mailbox. Observe their existing
 // published counters too; log on the host at most twice/sec during sustained input.
 const bool ordersChanged=incoming.csConverted!=previousCS||incoming.queueSent!=previousQueued||incoming.commandRejected!=previousRejected;
 if(!force&&pid==previousPid&&stage==previousStage&&error==previousError&&data==previousData&&
    h.subscribed==previousSubscribed&&h.faultSerial==previousFault&&incoming.status==previousStatus&&incoming.match==previousMatch&&DWORD(now-loggedAt)<(ordersChanged?500u:roundStarted&&DWORD(now-roundStarted)<10000?1000u:5000u))return;
 loggedAt=now;previousPid=pid;previousStage=stage;previousError=error;previousData=data;
 previousSubscribed=h.subscribed;previousMatch=incoming.match;previousFault=h.faultSerial;previousStatus=incoming.status;
 previousCS=incoming.csConverted;previousQueued=incoming.queueSent;previousRejected=incoming.commandRejected;
 // This runs on the host UI thread. Runtime only writes fixed-size IPC fields.
 autolog::write(std::wstring(L"bridge-health pid=")+std::to_wstring(pid)+L" expectedTid="+std::to_wstring(gameThread)+
  L" hwnd="+std::to_wstring(uintptr_t(game))+(gameWindowFromYrpp?L" windowSource=YRpp-0xB73550":L" windowSource=validated-fallback-or-none")+L" subscription="+std::to_wstring(hook)+
  L" stage="+(hasDiagnostic?runtimeStage(stage):L"diagnostic-busy-or-unavailable")+L" diagnosticValid="+std::to_wstring(h.diagnosticValid)+L" diagnosticCached="+std::to_wstring(h.diagnosticCached)+
  L" callbackCount="+std::to_wstring(h.callbacks)+L" callbackTid="+std::to_wstring(h.callbackThread)+
  L" callbackAgeMs="+(h.callbacks?std::to_wstring(int32_t(now-h.callbackTick)<0?0u:DWORD(now-h.callbackTick)):L"unknown")+
  L" runtimeBase="+std::to_wstring(h.runtimeBase)+L" hookRva="+std::to_wstring(h.hookRva)+
  L" dispatches="+std::to_wstring(h.dispatches)+L" enabled="+std::to_wstring(h.enabled)+
  L" ready="+std::to_wstring(h.ready)+L" wakePending="+std::to_wstring(h.wakePending)+
  L" postError="+std::to_wstring(h.postError)+L" runtimeError="+std::to_wstring(h.error)+
  L" faultSerial="+std::to_wstring(h.faultSerial)+L" exceptionCode="+hex32(h.faultCode)+
  L" exceptionAddress="+hex32(h.faultAddress)+
  L" accessOp="+std::to_wstring(h.faultOperation)+L" accessAddress="+hex32(h.faultAccess)+
  L" faultBoundary="+std::to_wstring(h.faultBoundary)+L" faultThread="+std::to_wstring(h.faultThread)+
  L" faultTick="+std::to_wstring(h.faultTick)+L" faultCommand="+std::to_wstring(h.faultCommand)+
  L" faultPhase="+std::to_wstring(h.faultPhase)+L" faultSource="+hex32(h.faultSource)+L" faultTarget="+hex32(h.faultTarget)+
  L" exceptionObserver="+std::to_wstring(h.observeState)+L" observerError="+std::to_wstring(h.observeError)+
  L" nativeExceptionCount="+std::to_wstring(h.nativeSerial)+
  L" match="+std::to_wstring(incoming.match)+L" status="+std::to_wstring(incoming.status)+
  L" commandStatus="+std::to_wstring(incoming.commandStatus)+L" fresh="+std::to_wstring(data)+
  L" request="+std::to_wstring(shared?shared->request:0)+L" ack="+std::to_wstring(shared?shared->ack:0)+
  L" controlFrame="+std::to_wstring(incoming.frame)+L" techOpen="+std::to_wstring(incoming.techOpen)+
  L" techVisible="+std::to_wstring(incoming.techVisible)+L" techUnavailable="+std::to_wstring(incoming.techUnavailable)+
  L" techError="+std::to_wstring(incoming.techError)+
  L" session="+std::to_wstring(incoming.session)+L" featureFlags="+hex32(shared?shared->flags:0)+
  L" csHook="+std::to_wstring(incoming.csHookStatus)+L" csConverted="+std::to_wstring(incoming.csConverted)+
  L" nativeHook="+std::to_wstring(incoming.nativeHookStatus)+L" clickHook="+std::to_wstring(shared?shared->clickHookStatus:0)+
  L" inputPatchMask="+std::to_wstring(incoming.inputPatchMask)+L" hookConflicts="+std::to_wstring(incoming.hookConflicts)+
  L" installedCodePatches="+std::to_wstring(incoming.hookInstalled)+L" nativeAirMode="+std::to_wstring(incoming.nativeAirStatus)+
  L" zoomStatus="+std::to_wstring(shared?shared->viewZoomStatus:0)+L" zoomApplied="+std::to_wstring(shared?shared->viewZoomApplied:100)+L" refreshMode="+std::to_wstring(shared?shared->scrollRefreshMode:0)+L" scrollHz="+std::to_wstring(shared?shared->scrollRefreshHz:0)+L" scrollStatus="+std::to_wstring(shared?shared->scrollRefreshStatus:0)+L" scrollDraws="+std::to_wstring(shared?shared->scrollRefreshDraws:0)+L" extraRenderMilliHz="+std::to_wstring(shared?shared->scrollRefreshRateMilli:0)+L" extraRenderMaxGapUs="+std::to_wstring(shared?shared->scrollRefreshMaxGapUs:0)+
  L" renderReveal="+std::to_wstring(shared?shared->renderReveal:0)+L" renderStatus="+std::to_wstring(incoming.renderRevealStatus)+
  L" computeRequested="+hex32(shared?shared->computeRequested:0)+L" computeApplied="+hex32(shared?shared->computeApplied:0)+
  L" placementStatus="+std::to_wstring(incoming.placementStatus)+L" controlStatus="+std::to_wstring(shared?shared->playerControlStatus:0)+
  L" houseRequested="+std::to_wstring(shared?shared->networkHousePreserve:0)+L" houseStatus="+std::to_wstring(shared?shared->networkHouseStatus:0)+
  L" crcShadowRequested="+std::to_wstring(shared?shared->crcMapShadow:0)+L" crcShadowStatus="+std::to_wstring(shared?shared->crcMapShadowStatus:0)+
  L" crcShadowTracked="+std::to_wstring(shared?shared->crcMapShadowTracked:0)+L" crcShadowSubstitutions="+std::to_wstring(shared?shared->crcMapShadowSubstitutions:0)+L" crcShadowFaults="+std::to_wstring(shared?shared->crcMapShadowFaults:0)+
  L" burstEnabled="+std::to_wstring(shared?shared->burstEnabled:0)+L" burstHeld="+std::to_wstring(shared?shared->burstHeld:0)+
  L" burstActive="+std::to_wstring(shared?shared->burstActive:0)+L" burstIssued="+std::to_wstring(shared?shared->burstIssued:0)+
  L" burstInside="+std::to_wstring(shared&&compute::inside(shared->computeApplied,compute::BurstRefresh))+
  L" burstTask="+std::to_wstring(shared?shared->burstTask:0)+L" burstComputed="+std::to_wstring(shared?shared->burstComputed:0)+
  L" dodgeActive="+std::to_wstring(shared?shared->dodgeActive:0)+L" dodgeIssued="+std::to_wstring(shared?shared->dodgeIssued:0)+
  L" queueSent="+std::to_wstring(incoming.queueSent)+L" queueFull="+std::to_wstring(incoming.queueFull)+
  L" commandRejected="+std::to_wstring(incoming.commandRejected)+L" lastSubmitError="+std::to_wstring(incoming.lastSubmitError)+
  L" entryContended="+std::to_wstring(incoming.entryContended)+L" entryWrongThread="+std::to_wstring(incoming.entryWrongThread)+
  L" scanUs="+std::to_wstring(incoming.scanMicros)+L" controlUs="+std::to_wstring(incoming.controlMicros)+
  L" worldUs="+std::to_wstring(incoming.worldMicros)+L" displayUs="+std::to_wstring(incoming.displayMicros));
}
static int value(const wchar_t*sec,const wchar_t*key,int def){return GetPrivateProfileIntW(sec,key,def,path(L"Assist.ini").c_str());}
static bool saveWriteOK=true;
static void write(const wchar_t*sec,const wchar_t*key,int n){
 if(!WritePrivateProfileStringW(sec,key,std::to_wstring(n).c_str(),path(L"Assist.ini").c_str()))saveWriteOK=false;
}
static bool writeSection(const wchar_t* section,const std::vector<std::pair<std::string,int>>& entries){
 std::wstring block;
 for(const auto& entry:entries){
  if(!typecatalog::validId(entry.first))continue;
  block+=wide(entry.first.c_str())+L"="+std::to_wstring(entry.second);
  block.push_back(L'\0');
 }
 block.push_back(L'\0');if(block.size()==1)block.push_back(L'\0');
 return WritePrivateProfileSectionW(section,block.c_str(),path(L"Assist.ini").c_str())!=FALSE;
}
// Type names/IDs are persisted separately from the live snapshot. This keeps both
// lists editable after the game exits, without altering engine memory or hooks.
static std::wstring catalogueKey(size_t i){wchar_t name[32];swprintf(name,32,L"Type%04u",unsigned(i));return name;}
static std::wstring catalogueValue(const assist::Type& t){
 std::wstring name=(const wchar_t*)t.name;
 for(auto&c:name)if(c==L'|'||c==L'\n'||c==L'\r')c=L' ';
 return std::to_wstring(t.kind)+L"|"+std::to_wstring(t.index)+L"|"+
        std::to_wstring(t.naval)+L"|"+std::to_wstring(t.cost)+L"|"+
        wide(t.id)+L"|"+name;
}
static bool saveCatalogue(const std::vector<size_t>* dirty=nullptr){
 static uint32_t writtenRevision=~0u;if(!dirty&&writtenRevision==catalogueRevision)return true;
 // One profile operation replaces the whole generated catalogue; hundreds of
 // synchronous per-type INI writes used to block the UI after first discovery.
 std::wstring block=L"Count="+std::to_wstring(savedTypes.types.size());block.push_back(L'\0');
 for(size_t i=0;i<savedTypes.types.size();++i){block+=catalogueKey(i)+L"="+catalogueValue(savedTypes.types[i]);block.push_back(L'\0');}
 block.push_back(L'\0');
 bool okay=WritePrivateProfileSectionW(L"TypeCatalogue",block.c_str(),path(L"Assist.ini").c_str())!=FALSE;
 if(okay)writtenRevision=catalogueRevision;return okay;
}
static void loadCatalogue(){
 const auto file=path(L"Assist.ini");
 int count=std::clamp(int(GetPrivateProfileIntW(L"TypeCatalogue",L"Count",0,file.c_str())),0,int(typecatalog::MaxSavedTypes));
 for(int i=0;i<count;++i){
  wchar_t line[256]{};auto key=catalogueKey(size_t(i));
  GetPrivateProfileStringW(L"TypeCatalogue",key.c_str(),L"",line,256,file.c_str());
  std::wstring fields[6],raw=line;size_t from=0;bool complete=true;
  for(int k=0;k<5;++k){size_t sep=raw.find(L'|',from);if(sep==std::wstring::npos){complete=false;break;}fields[k]=raw.substr(from,sep-from);from=sep+1;}
  if(!complete)continue;fields[5]=raw.substr(from);
  assist::Type t{};t.kind=_wtoi(fields[0].c_str());t.index=_wtoi(fields[1].c_str());
  t.naval=_wtoi(fields[2].c_str());t.cost=_wtoi(fields[3].c_str());
  if(t.kind!=-1&&t.kind!=3&&t.kind!=7&&t.kind!=16&&t.kind!=40)continue;
  if(t.index<-1||t.index>4095||fields[4].size()>=sizeof(t.id)||fields[5].size()>=std::size(t.name))continue;
  bool ascii=true;std::string id;
  for(wchar_t c:fields[4]){if(c<33||c>126){ascii=false;break;}id.push_back(char(c));}
  if(!ascii||!typecatalog::validId(id))continue;
  std::memcpy(t.id,id.data(),id.size());
  for(size_t c=0;c<fields[5].size();++c)t.name[c]=uint16_t(fields[5][c]);
  if(!t.name[0])for(size_t c=0;c<id.size()&&c<std::size(t.name)-1;++c)t.name[c]=uint16_t(id[c]);
  savedTypes.remember(t);
 }
}
static void rememberLiveTypes(){
 if(snap.status!=1||snap.typeCount<=0)return;
 static uint32_t learnedMatch;static int learnedCount=-1;
 if(learnedMatch==snap.match&&learnedCount==snap.typeCount)return;learnedMatch=snap.match;learnedCount=snap.typeCount;
 std::vector<size_t> dirty;
 for(int i=0;i<snap.typeCount&&i<MaxTypes;++i){
  if(savedTypes.remember(snap.types[i])){
   int n=savedTypes.find(typecatalog::idOf(snap.types[i]),snap.types[i].kind);
   if(n>=0)dirty.push_back(size_t(n));
  }
 }
 if(!dirty.empty()){
  ++catalogueRevision;
  if(!saveCatalogue(&dirty))diag(L"TypeCatalogue write failed: check Assist.ini permissions");
 }
}
static UINT modifiers(WORD key){BYTE f=HIBYTE(key);return MOD_NOREPEAT|((f&HOTKEYF_CONTROL)?MOD_CONTROL:0)|((f&HOTKEYF_ALT)?MOD_ALT:0)|((f&HOTKEYF_SHIFT)?MOD_SHIFT:0);}
static std::wstring keyText(WORD key){return keygesture::describe(key,modernui::hotkeyName(key));}
static bool foregroundGame(){if(!game||!IsWindow(game)||IsIconic(game))return false;HWND fg=GetForegroundWindow();if(fg==game)return true;DWORD fgPid=0;GetWindowThreadProcessId(fg,&fgPid);return fgPid&&fgPid==pid;}
static void wakeGame(){
 if(gameinput::wake(hook))lastPulse=GetTickCount();
}
// Secondary read-only displays keep their own lifetime when the game overlay
// or command module is disabled. A missing/invalid world still stops rendering.
static bool secondaryDisplayRequested(){return minimapSecondary||extra.watchSecondary;}
static secondarydisplay::Monitor secondaryDisplayMonitor;
static bool secondaryDisplayAvailable(){return secondaryDisplayRequested()&&secondaryDisplayMonitor.get(game,ui,GetTickCount()).available();}
static bool displayFrameRequested(){return snap.renderStatus==1&&((enabled&&foregroundGame())||secondaryDisplayAvailable());}
static int desiredHotkeyMode(){
 if(!foregroundGame()||snap.renderStatus!=1||snap.chat)return 0;
 if(enabled&&snap.status==1)return 2;
 return !enabled&&extra.watchSecondary&&snap.readStatus==1?1:0;
}
static int registeredHotkeyMode=0;
static uint32_t commandMatch();static uint32_t displayMatch();
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
 if(LOBYTE(ejectKey)&&!registerKey(1696,ejectKey))++failures;
 if(hookSmartUnload&&LOBYTE(unloadKey)&&!registerKey(1698,unloadKey))++failures;
 if(featureavailability::AutoCS&&hookAutoCS&&LOBYTE(csKey)&&!registerKey(1699,csKey))++failures;
 if(extra.viewZoomEnabled){for(int i=0;i<3;++i)if(!registerKey(1720+i,WORD((i==0?VK_OEM_MINUS:i==1?VK_OEM_PLUS:VK_BACK)|(6<<8))))++failures;}
 if(LOBYTE(extra.stickyKey)&&!registerKey(1723,WORD(extra.stickyKey)))++failures;
 for(int i=0;i<selectedmode::Count;++i)if(LOBYTE(extra.missionModeKeys[i])&&!registerKey(1724+i,WORD(extra.missionModeKeys[i])))++failures;
 if(extra.watchEnabled||extra.watchSecondary)for(int i=0;i<3;++i)if(external::watch::firstBinding(extra.watchKeys,i)&&!registerKey(1730+i,WORD(extra.watchKeys[i])))++failures;
 if(LOBYTE(idleAircraftKey)&&!registerKey(1701,idleAircraftKey))++failures;
 for(int i=0;i<9;++i)if(LOBYTE(socialKeys[i])&&!registerKey(1710+i,socialKeys[i]))++failures;
 if(LOBYTE(extra.variantKey)&&!registerKey(1700,WORD(extra.variantKey)))++failures;
 synchronizeHotkeyKeys();if(failures)notify(L"有 "+std::to_wstring(failures)+L" 个快捷键冲突或注册失败，请更换组合，修改后自动生效。");
}
static std::wstring serializeIDs(const std::set<std::string>& ids){
 std::wstring result;for(const auto& id:ids){if(!result.empty())result+=L",";result+=wide(id.c_str());}return result;
}
static std::set<std::string> parseIDs(const std::wstring& text){
 std::set<std::string> ids;std::string id;
 for(wchar_t c:text+L","){
  if(c==L','){if(typecatalog::validId(id))ids.insert(id);id.clear();}
  else if(c>32&&c<127)id+=char(c);
 }
 return ids;
}
static void chooseTechProfile(int next){
 profileExceptions[techProfile]=techExceptions;
 techProfile=std::clamp(next,0,2);
 techExceptions=profileExceptions[techProfile];
}
static bool save(){++settingsRevision;saveWriteOK=true;for(int i=0;i<12;++i)write(L"Hotkeys",wide(iniKeys[i]).c_str(),hotkeys[i]);write(L"Display",L"FPS",fps);write(L"Display",L"AdaptiveFPS",infoAdaptive);write(L"Display",L"LabelRefreshVersion",1);write(L"Display",L"TopMargin",topMargin);
 write(L"Display",L"LabelScalePercent",labelScalePercent);write(L"Display",L"ConstructionScalePercent",constructionScalePercent);
 for(int i=0;i<7;++i){const wchar_t*names[]={L"Capacity",L"Iron",L"Production",L"AllTransports",L"Psychic",L"Crates",L"TextPanel"};write(L"Display",names[i],SendMessageW(check[i],BM_GETCHECK,0,0)==BST_CHECKED);}
 write(L"Modes",L"AutoRepair",SendMessageW(repairCheck,BM_GETCHECK,0,0)==BST_CHECKED);write(L"Modes",L"RepairUnrepairable",repairUnrepairable);write(L"Experimental",L"NativeTechProduction",techMode);write(L"Modes",L"RepairMs",repairMs);
 write(L"Tech",L"Profile",techProfile);write(L"Tech",L"Implementation",techImplementation);write(L"Tech",L"DirectProduce",techDirectProduce);write(L"Tech",L"DefaultAllow",techDefault);
 profileExceptions[techProfile]=techExceptions;
 std::wstring ids=serializeIDs(techExceptions);
 saveWriteOK=(WritePrivateProfileStringW(L"Tech",L"Exceptions",ids.c_str(),path(L"Assist.ini").c_str())!=FALSE)&&saveWriteOK;
 for(int p=0;p<3;++p){std::wstring key=L"Exceptions"+std::to_wstring(p),v=serializeIDs(profileExceptions[p]);
  saveWriteOK=(WritePrivateProfileStringW(L"Tech",key.c_str(),v.c_str(),path(L"Assist.ini").c_str())!=FALSE)&&saveWriteOK;}
 for(auto&b:swBindings)write(L"SuperHotkeys",wide(b.id.c_str()).c_str(),b.key);
 // Replace each multisz section with one Win32 profile operation. Do not
 // delete the old section first: a crash between erase/rebuild lost bindings.
 std::vector<std::pair<std::string,int>> bindingItems,csItems;
 for(const auto&b:typeBindings)if(LOBYTE(b.key))bindingItems.push_back({b.id,b.key});
 for(const auto&id:csUnits)csItems.push_back({id,1});
 saveWriteOK=writeSection(L"TypeHotkeys",bindingItems)&&saveWriteOK;
 saveWriteOK=writeSection(L"AutoCSUnits",csItems)&&saveWriteOK;
 write(L"Click",L"SidebarWidth",shiftSidebarWidth);write(L"Click",L"SidebarTop",shiftSidebarTop);write(L"Click",L"Enabled",shiftClickEnabled);write(L"Click",L"Mode",shiftClickMode);write(L"Click",L"IntervalMs",shiftClickGap);write(L"Display",L"ToastMs",toastMs);write(L"Modes",L"AutoOreReturn",oreMode);write(L"Modes",L"MagnetronMove",magnetronMode);write(L"NativeOrders",L"MagnetronHoldKey",magnetronKey);write(L"Experimental",L"BuildAnywhere",buildAnywhereMode);write(L"AutoCS",L"HoldEnabled",csHoldEnabled);write(L"AutoCS",L"HoldKey",csHoldKey);
 write(L"OreTypes",L"CustomMiners",oreTypes.customMiners);write(L"OreTypes",L"CustomRefineries",oreTypes.customRefineries);
 saveWriteOK=(WritePrivateProfileStringW(L"OreTypes",L"Miners",oreMiners.c_str(),path(L"Assist.ini").c_str())!=FALSE)&&saveWriteOK;
 saveWriteOK=(WritePrivateProfileStringW(L"OreTypes",L"Refineries",oreRefineries.c_str(),path(L"Assist.ini").c_str())!=FALSE)&&saveWriteOK;
 write(L"TypeSelection",L"DoublePressMs",doubleMs);write(L"AutoCS",L"Enabled",csMode);write(L"AutoCS",L"ToggleKey",csKey);
 write(L"NativeOrders",L"EjectAssist",ejectMode);write(L"NativeOrders",L"EjectToggleKey",ejectKey);write(L"NativeOrders",L"VeteranPercent",veteranPercent);write(L"NativeOrders",L"ElitePercent",elitePercent);
 write(L"NativeOrders",L"TargetRangeExtraCells",advancedRangeCells);write(L"NativeOrders",L"TargetPriority",advancedTargetPriority);write(L"NativeOrders",L"TargetScanHz",advancedScanHz);write(L"NativeOrders",L"AdvancedHoldKey",advancedKey);write(L"NativeOrders",L"UnloadKey",unloadKey);write(L"NativeOrders",L"PathTimeoutFrames",pathTimeout);
 write(L"Hooks",L"Advanced",hookAdvanced);write(L"Hooks",L"SmartUnload",hookSmartUnload);
 write(L"Hooks",L"Eject",hookEject);write(L"Hooks",L"AutoCS",hookAutoCS);
 write(L"Display",L"CrateResults",0);
 write(L"Display",L"DetailedStatus",showDetails);
 write(L"Scheduler",L"Entry",int(schedulerTarget));
 write(L"Orders",L"MegaMissionBackend",int(megaQueueMode));
 write(L"Orders",L"ObjectEventBackend",int(objectQueueMode));
 saveWriteOK=saveAdditionalSettings()&&saveWriteOK;saveWriteOK=saveCatalogue()&&saveWriteOK;syncTypeConfig();return saveWriteOK;
}
static uint32_t flags(){const uint32_t bits[]={Capacity,Iron,Production,AllTransports,Psychic,Crates,TextPanel};uint32_t r=0;for(int i=0;i<7;++i)if(SendMessageW(check[i],BM_GETCHECK,0,0)==BST_CHECKED)r|=bits[i];if(SendMessageW(repairCheck,BM_GETCHECK,0,0)==BST_CHECKED)r|=AutoRepair;r|=hookpolicy::resolve({hookAdvanced,hookSmartUnload,hookEject,hookAutoCS},true,ejectMode,csMode);if(magnetronMode||extra.magnetronGlobal)r|=MagnetronInput|NativeOrders;if(oreMode)r|=AutoOreReturn;if(buildAnywhereMode)r|=BuildAnywhere;if(csHoldEnabled)r|=CustomCS;if(repairUnrepairable)r|=RepairUnrepairable;return r|additionalFlags();}
static bool commandFocus(){
 HWND fg=GetForegroundWindow();
#ifdef RA2_UNIFIED
 return delivery::focusIsRelevant(foregroundGame(),suite::frontend.contains(fg));
#else
 HWND rootUi=ui?GetAncestor(ui,GA_ROOT):nullptr;
 return delivery::focusIsRelevant(foregroundGame(),
  fg==ui||fg==rootUi||(fg&&(GetAncestor(fg,GA_ROOT)==ui||GetAncestor(fg,GA_ROOT)==rootUi)));
#endif
}
static controllink::View commandLink(){return {shared!=nullptr,lastData!=0,shared&&shared->stop!=0,
 incoming.status,incoming.commandStatus,incoming.match};}
static uint32_t commandMatch(){return controllink::identity(commandLink());}
static uint32_t displayMatch(){return externalData&&externalData->status==1?externalData->match:snap.match;}
static bool uninitializedBridgeTimedOut(){return shared&&!commandMatch()&&attachAt&&DWORD(GetTickCount()-attachAt)>8000&&(!lastData||DWORD(GetTickCount()-lastData)>8000)&&shared->request==shared->ack;}
static bool commandChannelReady(){return controllink::ready(commandLink());}
static bool commandFresh(DWORD now){return commandChannelReady()&&DWORD(now-lastData)<1500;}
static bool networkHouseReady(){return extra.networkHouseRequested()&&shared&&shared->networkHousePreserve&&shared->networkHouseStatus==1;}
static std::wstring commandWaitReason(){
 if(!shared)return bridgeFailure.empty()?L"正在连接游戏，指令尚未提交。":bridgeFailure;
 const auto health=gameinput::health(hook);
 if(health.diagnosticValid&&health.stage<0)return L"无法建立游戏连接，请查看 logs/Runtime.log。";
 if(health.postError)return L"游戏暂未响应，指令尚未提交。";
 if(!lastData||!incoming.match){
  if(!health.callbacks)return L"只读连接可用，尚未收到游戏内 Hook 回调；详情见 logs/Runtime.log。";
  if(!health.dispatches)return L"Hook 已回调，正在等待游戏内连接完成；详情见 logs/Runtime.log。";
  return L"游戏已连接，等待可操作的玩家与对局。";
 }
 if(shared->stop)return L"游戏连接正在重建，请稍候。";
 if(incoming.status<0)return L"游戏操作发生异常，已停止发送指令；请重启游戏。详情见 logs/Runtime.log。";
 if(DWORD(GetTickCount()-lastData)>=1500)return L"游戏数据已超过 1.5 秒未更新，等待恢复。";
 if(incoming.status!=1)return L"当前对局尚不可操作，等待游戏恢复。";
 if(incoming.commandStatus==2)return L"等待本局远端 FrameInfo 执行及摘要匹配；功能补丁和自动命令尚未启用。";
 if(incoming.commandStatus==3)return L"游戏入口签名暂不兼容，尚未启用功能；每秒重新核对，请查看 logs/Runtime.log。";
 if(incoming.commandStatus==4)return L"已观察到同步摘要不匹配，功能保持停用；请保留双方 SYNC 日志。";
 if(incoming.commandStatus!=1)return foregroundGame()?L"游戏已暂停或正在显示菜单，恢复后继续。":L"请切回游戏并恢复运行。";
 if(incoming.chat)return L"请关闭游戏聊天输入框后继续。";
 if(shared->request!=shared->ack)return L"正在等待上一条指令处理完成。";
 return L"正在处理已排队的指令。";
}
static void logCommandWait(){
 const auto health=gameinput::health(hook);
 critical(L"command waiting: "+commandWaitReason()+L" pid="+std::to_wstring(pid)+
  L" thread="+std::to_wstring(gameThread)+L" match="+std::to_wstring(commandMatch())+
  L" status="+std::to_wstring(incoming.status)+L" commandStatus="+std::to_wstring(incoming.commandStatus)+
  L" chat="+std::to_wstring(incoming.chat)+L" dataAge="+std::to_wstring(lastData?DWORD(GetTickCount()-lastData):0)+
  L" dispatches="+std::to_wstring(health.dispatches)+L" readyBits="+std::to_wstring(health.ready)+
  L" wakePending="+std::to_wstring(health.wakePending)+L" postError="+std::to_wstring(health.postError)+
  L" request="+std::to_wstring(shared?shared->request:0)+L" ack="+std::to_wstring(shared?shared->ack:0));
}
static bool userCommand(int c){return c!=OpenTech;}
static DWORD discoverGamePid(); // Same window filter used by attach(), without installing a second Hook.
static bool queueCommand(int c,int arg,int arg2,uint32_t match,int retries=0,DWORD delayMs=0,uint32_t token=0,uint64_t typeHash=0){
 if(!enabled)return false;
 if(c==RevealMap&&(!extra.experimentalReveal||extra.renderReveal))return false;
 DWORD now=GetTickCount();
 if(!game||!pid||!IsWindow(game)||(shared&&lastData&&incoming.status<0))return false;
 bool queued=commandQueue.add({c,arg,arg2,pid,match,now,retries,now+delayMs,token,typeHash?typeHash:typeListHash(snap.types,snap.typeCount)});
 if(queued){diag(L"staged command="+std::to_wstring(c)+L" pid="+std::to_wstring(pid)+L" match="+std::to_wstring(match)+L" backlog="+std::to_wstring(commandQueue.size));wakeGame();}
 return queued;
}
static void send(int command,int arg=0,int arg2=0,int retries=0,bool fromQueue=false,uint32_t requestToken=0){
 if(!enabled){notify(L"战术控制已暂停，请先启用后再发送指令。");return;}
 // Whole-House destruction is an immediate explicit button action. Never
 // carry it across a reconnect, busy bridge or deferred command queue.
 if(command==DestructPlayer&&(!shared||!game||!pid||!IsWindow(game)||!commandFresh(GetTickCount())||
    !commandFocus()||incoming.chat||shared->request!=shared->ack||shared->ack!=ack||commandQueue.size||fromQueue||retries)){
  notify(L"自毁请求未提交：请等待当前对局和命令连接就绪后重新点击。");return;
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
 if(command==DestructPlayer&&(playerChoicesPid!=pid||playerChoicesMatch!=commandMatch())){
  notify(L"自毁请求未提交：对局已改变，请重新选择所属方。");return;
 }
 if(command==DestructPlayer){
  // Without the complete active adapter, native multiplayer sending rewrites
  // the event House. Runtime rechecks adapter availability at final enqueue.
  long seq=shared->seq;if(seq&1){notify(L"所属方列表正在更新，本次自毁请求未提交。");return;}
  MemoryBarrier();int session=shared->data.session,current=shared->playerCurrent;bool preserve=networkHouseReady();MemoryBarrier();
  if(seq!=shared->seq){notify(L"所属方列表正在更新，本次自毁请求未提交。");return;}
  if(eventdelivery::isMultiplayer(session)&&(current<0||(requestToken!=uint32_t(current)&&!preserve))){notify(DestructForeignNotice);return;}
 }
 if(typeIndexedCommand(command,arg)&&intentTypes!=typeListHash(snap.types,snap.typeCount)){
  if(fromQueue&&(command==ProduceReserved||command==ProduceDeferred))constructionResult(-13,0,uint32_t(arg2),requestToken);
  else notify(L"单位清单已改变，请重新选择后提交。");return;
 }
 if(shared&&lastData&&incoming.status<0){notify(commandWaitReason());return;}
 bool connected=shared&&delivery::bridgeReady(incoming.status,commandFresh(now),incoming.chat,shared->request==shared->ack);
 // Mode syncing must not enqueue automatically every 50ms.
 // Explicit gameplay commands are staged while the match is loading or bridge is busy.
 if(!connected||(!fromQueue&&commandQueue.size&&userCommand(command))){
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
// A priority mailbox within the same Shared protocol. Game-side selection is
// consumed by tick() under the same manager lease, without a separate hook.
static uint32_t pendingSelectionObject=0;
static void serviceSelection(){
 if(!shared)return;DWORD now=GetTickCount();uint32_t receipt=shared->selectAck;
 if(receipt!=selectionAck){MemoryBarrier();selectionAck=receipt;
  selectionLatency=DWORD(now-shared->selectTick);
  if(!hasSelection&&shared->selectResult==-8)notify(L"请先关闭游戏聊天输入框。");
  if(diagEnabled)diag(L"selection ack="+std::to_wstring(receipt)+L" result="+std::to_wstring(shared->selectResult)+L" count="+std::to_wstring(shared->selectCount));
 }
 if(!hasSelection)return;
 if(pendingSelection.pid!=pid||(pendingSelection.match&&pendingSelection.match!=snap.match)||DWORD(now-pendingSelection.created)>1500){hasSelection=false;return;}
 if(!commandFocus()||!commandFresh(now)||snap.chat)return;
 if(shared->selectRequest!=shared->selectAck){wakeGame();return;}
 shared->selectObjectID=pendingSelectionObject;shared->selectIndex=pendingSelection.arg;shared->selectGlobal=pendingSelection.arg2;
 shared->selectTick=now;shared->selectMatch=snap.match;memcpy(shared->selectTypes,pendingSelectionTypes,sizeof pendingSelectionTypes);
 MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->selectRequest);hasSelection=false;
 wakeGame();
}
static void sendSelection(int index,bool global,const uint8_t* mask=nullptr,uint32_t object=0){
 if(!enabled){notify(L"战术控制已暂停，请先启用后再选兵。");return;}
 pendingSelectionObject=object;
 if(mask)memcpy(pendingSelectionTypes,mask,sizeof pendingSelectionTypes);else memset(pendingSelectionTypes,0,sizeof pendingSelectionTypes);
 if(!shared)ensureBridge();
 pendingSelection={SelectType,index,int(global),pid,commandFresh(GetTickCount())?snap.match:delivery::UnknownMatch,GetTickCount()};hasSelection=true;
 serviceSelection();
}
static void logPlanning(const planner::World&w,const wchar_t* reason){
 static DWORD lastLog;DWORD now=GetTickCount();if(lastLog&&DWORD(now-lastLog)<5000)return;lastLog=now;
 int miners=0,full=0,paused=0,automatic=0;uint32_t firstID=0;double firstLoad=0;int firstLimit=0,firstMission=0;
 for(int i=0;i<w.count;++i){const auto&a=w.actors[i];if(!(a.flags&planner::Harvester))continue;
  ++miners;double load=ore::storageAmount(a.storage);full+=a.storageLimit>0&&load>=a.storageLimit;
  paused+=(a.flags&planner::OrePaused)!=0;automatic+=ore::automaticMission(a.mission);
  if(!firstID){firstID=a.id;firstLoad=load;firstLimit=a.storageLimit;firstMission=a.mission;}
 }
 critical(L"planner reason="+std::wstring(reason)+L" match="+std::to_wstring(w.match)+
  L" worldFrame="+std::to_wstring(w.frame)+L" controlFrame="+std::to_wstring(snap.frame)+L" ageMs="+std::to_wstring(DWORD(now-w.tick))+
  L" oreEnabled="+std::to_wstring(shared&&(shared->flags&AutoOreReturn)!=0)+L" miners="+std::to_wstring(miners)+
  L" full="+std::to_wstring(full)+L" manualPaused="+std::to_wstring(paused)+L" automaticMission="+std::to_wstring(automatic)+
  L" refineries="+std::to_wstring(w.refineryCount)+L" sampleMiner="+std::to_wstring(firstID)+
  L" cargo="+std::to_wstring(firstLoad)+L" capacity="+std::to_wstring(firstLimit)+L" mission="+std::to_wstring(firstMission)+
  L" jobs="+std::to_wstring(plannerState.queued)+L" issued="+std::to_wstring(plannerState.issued)+
  L" oreBypasses="+std::to_wstring(plannerState.oreBypasses)+L" yieldAttempts="+std::to_wstring(plannerState.oreYieldAttempts)+L" turnTimeouts="+std::to_wstring(plannerState.oreTurnTimeouts)+
  L" planRequest="+std::to_wstring(shared->planRequest)+L" planAck="+std::to_wstring(shared->planAck)+
  L" result="+std::to_wstring(shared->planResult)+L" accepted="+std::to_wstring(shared->planAcceptedMask)+L" rejected="+std::to_wstring(shared->planRejectedMask));
}
template<class Input,class Output>static void serviceValueTask(compute::ValueTask<Input,Output>&worker,volatile uint32_t&request,volatile uint32_t&computed,
 const Input&source,Output&destination,volatile long*inputSequence=nullptr,volatile long*resultSequence=nullptr,uint32_t cadence=0){
 // Publish a completed in-flight result against its original source serial.
 // A newer captured frame must not starve a slower but still valid result.
 auto result=worker.get();
 if(result&&result->generation==planningGeneration&&compute::newer(result->task,computed)&&result->value.match==uint32_t(snap.match)&&
    freshness::valid(result->value.match,snap.match,result->value.tick,result->value.frame,GetTickCount(),snap.frame,freshness::Cooperative)){
  if(resultSequence)InterlockedIncrement(resultSequence);MemoryBarrier();destination=result->value;computed=result->task;MemoryBarrier();if(resultSequence)InterlockedIncrement(resultSequence);wakeGame();
 }
 const uint32_t task=request;if(!task||task==computed)return;
 const long batch=shared->computeFrameSeq;if(batch&1)return;
 const long seq=inputSequence?*inputSequence:0;if(seq&1)return;MemoryBarrier();
 if(worker.needs(planningGeneration,task,seq)&&worker.ready(GetTickCount(),cadence)){
  auto input=std::make_shared<Input>();memcpy(input.get(),&source,sizeof(Input));MemoryBarrier();
  if(batch!=shared->computeFrameSeq||task!=request||(inputSequence&&seq!=*inputSequence)||input->match!=uint32_t(snap.match)||input->frame!=shared->computeFrame||input->match!=shared->computeFrameMatch)return;
  worker.submit(planningGeneration,task,seq,std::move(input));worker.started(GetTickCount());
 }
}
static void serviceCalculationTasks(){
 if(!shared||!commandFresh(GetTickCount()))return;bindCalculations();
 serviceValueTask(dodgeTask,shared->dodgeTask,shared->dodgeComputed,shared->dodgeInput,shared->dodgeResult,&shared->dodgeInputSeq,&shared->dodgeResultSeq);
 serviceValueTask(burstTask,shared->burstTask,shared->burstComputed,shared->burstInput,shared->burstResult,&shared->burstInputSeq,&shared->burstResultSeq);
 serviceValueTask(missileTask,shared->missileTask,shared->missileComputed,shared->missileInput,shared->missileResult,&shared->missileInputSeq,&shared->missileResultSeq,1000u/std::clamp(uint32_t(shared->missileScanHz),1u,60u));
 serviceValueTask(unloadTask,shared->unloadTask,shared->unloadComputed,shared->unloadInput,shared->unloadResult,&shared->unloadInputSeq,&shared->unloadResultSeq);
 serviceValueTask(loadTask,shared->loadTask,shared->loadComputed,shared->loadInput,shared->loadResult,nullptr,&shared->loadResultSeq);
 serviceValueTask(repairTask,shared->repairTask,shared->repairComputed,shared->repairInput,shared->repairResult,nullptr,&shared->repairResultSeq);
}
static planner::Parameters plannerParameters(){
 planner::Parameters p;p.flags=compute::plannerFlags(shared->flags,shared->computeApplied,false);p.modes=compute::magnetronIntents(shared->computeApplied,false);
 p.veteran=veteranPercent;p.elite=elitePercent;p.timeout=pathTimeout;p.hz=advancedScanHz;p.priority=advancedTargetPriority;p.range=advancedRangeCells;
 p.drop=extra.magnetronDrop&&!compute::inside(shared->computeApplied,compute::MagnetronDrop);p.observeDrop=extra.magnetronDrop;return p;
}
static void plannerStatistics(){
 shared->plannerTracking=plannerState.tracking;
 if(!compute::inside(shared->computeApplied,compute::Ore)){snap.oreYielded=plannerState.yielded;snap.oreQueued=plannerState.queued;snap.oreDocking=plannerState.docking;snap.oreIssued=plannerState.issued;
  snap.oreDelivered=plannerState.oreDelivered;snap.oreUnverified=plannerState.oreUnverified;}
 snap.magnetShots=incoming.magnetShots+plannerState.magnetShots;snap.magnetUnverified=incoming.magnetUnverified+plannerState.magnetUnverified;
}
static void servicePlanner(){
 if(!shared||!enabled||snap.status!=1||!commandFresh(GetTickCount()))return;bindCalculations();
 plannerStatistics();
 const uint32_t receipt=shared->planAck;
 if(receipt!=plannerAck){MemoryBarrier();plannerAck=receipt;const uint32_t accepted=shared->planAcceptedMask,rejected=shared->planRejectedMask;
  if(diagEnabled)diag(L"plan ack="+std::to_wstring(receipt)+L" result="+std::to_wstring(shared->planResult)+L" accepted="+std::to_wstring(accepted)+L" rejected="+std::to_wstring(rejected));
  for(size_t i=0;i<submittedPlan.size()&&i<planner::MaxOrders;++i){
   auto kind=planner::Receipt::Expired;if(accepted&(1u<<i))kind=planner::Receipt::Accepted;
   else if((rejected&(1u<<i))&&shared->planResult!=-7)kind=planner::Receipt::Rejected;
   plannerReceipts.push_back({submittedPlan[i],submittedPlanFrame,kind});
  }
  submittedPlan.clear();
 }
 if(shared->planRequest!=shared->planAck)return;
 // Only a completed immutable value can enter the existing command mailbox.
 // Changing settings, chat, source expiry or a new bridge rejects old orders;
 // expiration is returned to the worker before its next stateful step.
 if(planningBusy){
  auto result=plannerWorker.get();if(!result||result->generation!=planningGeneration||result->serial!=planningSerial)return;
  planningBusy=false;if(result->match==uint32_t(snap.match)){plannerState=result->statistics;plannerMicros=result->micros;}else{plannerState={};plannerMicros=0;}plannerStatistics();
  if(result->parameters==plannerParameters()&&!snap.chat&&freshness::valid(result->match,snap.match,result->tick,result->frame,GetTickCount(),snap.frame,freshness::Tactical)){
   submittedPlan=result->orders;
   if(!submittedPlan.empty()){
    memcpy(shared->plan,submittedPlan.data(),submittedPlan.size()*sizeof(planner::Order));shared->planCount=int(submittedPlan.size());shared->planMatch=result->match;shared->planTick=result->tick;shared->planFrame=result->frame;submittedPlanFrame=result->frame;
    MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->planRequest);wakeGame();return;
   }
  }else for(const auto&o:result->orders)plannerReceipts.push_back({o,result->frame,planner::Receipt::Expired});
 }
 const auto parameters=plannerParameters();if(!parameters.flags&&!parameters.drop&&!parameters.observeDrop&&!plannerState.tracking&&plannerReceipts.empty())return;
 const long batch=shared->computeFrameSeq;if(batch&1)return;
 const long seq=shared->worldSeq;if((seq&1)||seq==worldRevision||snap.chat)return;
 auto input=std::make_shared<planner::World>();auto&w=*input;MemoryBarrier();
 w.match=shared->world.match;w.tick=shared->world.tick;w.frame=shared->world.frame;w.count=shared->world.count;w.intentCount=shared->world.intentCount;w.refineryCount=shared->world.refineryCount;w.inputSerial=shared->world.inputSerial;w.combatCount=shared->world.combatCount;
 if(!planner::validWorldCounts(w.count,w.intentCount,w.refineryCount)||w.combatCount<0||w.combatCount>planner::MaxCombatProbes)return;
 memcpy(w.combat,shared->world.combat,size_t(w.combatCount)*sizeof(planner::CombatProbe));memcpy(w.actors,shared->world.actors,size_t(w.count)*sizeof(planner::Actor));memcpy(w.intents,shared->world.intents,size_t(w.intentCount)*sizeof(planner::Intent));memcpy(w.refineries,shared->world.refineries,size_t(w.refineryCount)*sizeof(planner::Refinery));MemoryBarrier();
 if(seq!=shared->worldSeq||batch!=shared->computeFrameSeq||w.frame!=shared->computeFrame||w.match!=shared->computeFrameMatch)return;
 if(w.match==uint32_t(snap.match)&&w.frame>snap.frame){wakeGame();return;}
 worldRevision=seq;
 if(!freshness::valid(w.match,snap.match,w.tick,w.frame,GetTickCount(),snap.frame,freshness::Tactical)){logPlanning(w,L"stale-source");wakeGame();return;}
 planner::TaskInput job;job.generation=planningGeneration;job.serial=++planningSerial;job.world=std::move(input);job.parameters=plannerParameters();job.receipts=std::move(plannerReceipts);plannerReceipts.clear();
 planningBusy=true;plannerWorker.submit(std::move(job));
}

struct RetiredBridge {Shared*state;HANDLE mapping;HMODULE module;gameinput::Subscription hook;HWND window;DWORD lastWake;DWORD thread;std::shared_ptr<const external::ProcessConnection> process;};
static std::vector<RetiredBridge> retiredBridges;
static bool bridgeClosePending=false;
static constexpr UINT_PTR BridgeCloseTimer=3902;
static void wakeRetiredBridge(RetiredBridge&r){
 // HWND destruction does not destroy a thread's message queue. Wake that
 // queue directly so the existing hook can acknowledge its own stop.
 gameinput::wake(r.hook);
 HANDLE thread=OpenThread(SYNCHRONIZE,FALSE,r.thread);
 bool ended=thread?WaitForSingleObject(thread,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;
 if(thread)CloseHandle(thread);
 if(!ended)return;
 auto candidate=external::discovery::scan(r.state->pid).best;
 if(candidate.pid!=r.state->pid||!candidate.window||!candidate.thread)return;
 auto cleanup=gameinput::install(gameentry::Client::Assist,candidate.thread,r.state->pid,candidate.window);
 if(!cleanup)return;
 gameinput::release(gameentry::Client::Assist,r.hook);r.hook=cleanup;r.window=candidate.window;r.thread=candidate.thread;
 gameinput::wake(r.hook);
}
static void reapBridges(){DWORD now=GetTickCount();for(size_t i=0;i<retiredBridges.size();){auto&r=retiredBridges[i];bool dead=r.process&&!r.process->alive();if(!r.process){HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,r.state->pid);dead=process?WaitForSingleObject(process,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;if(process)CloseHandle(process);}
 if(r.state->stop==2||dead){DWORD finished=r.state->pid;gameinput::release(gameentry::Client::Assist,r.hook);UnmapViewOfFile(r.state);CloseHandle(r.mapping);retiredBridges.erase(retiredBridges.begin()+i);if(finished==pid)bridgeRetryAt=0;}
 else{if(DWORD(now-r.lastWake)>=100){r.lastWake=now;wakeRetiredBridge(r);}++i;}}
}
static void releaseBridge(){
 commandcursor::clear();
 if(hook)logBridgeHealth(true); // preserve the last IPC evidence before unmapping
 resetPlanning();submittedPlan.clear();worldRevision=-1;plannerAck=0;hasSelection=false;selectionAck=0;cooperativeDelivered=cooperativeReceipt=0;retryCooperative=false;submittedCooperative.clear();acceptedCooperative.clear();processedCooperative.clear();cooperativeSourcePool={};
 if(shared&&hook){shared->stop=1;wakeGame();retiredBridges.push_back({shared,mapping,module,hook,game,GetTickCount(),gameThread,externalReader.connectionIdentity()});}
 else{if(hook)gameinput::release(gameentry::Client::Assist,hook);if(shared)UnmapViewOfFile(shared);if(mapping)CloseHandle(mapping);}
 hook=0;shared=nullptr;mapping=nullptr;module=nullptr;lastData=attachAt=lastPulse=0;incoming={};snapshotSeq=~0u;lastReceiptWrite=lastReceiptMatch=0;reapBridges();
}
static void detach(){renderupgrade::stop();clearPassengerDisplay();watchwindows::hide();minimap::clear();hideAircraftButton();resetConstruction();resetAirPath();hotkeyPending.clear();pointerMatches.clear();
 resetPlanning();submittedPlan.clear();worldRevision=-1;plannerAck=0;hasSelection=false;selectionAck=0;
 DWORD exitCode=0;bool exitKnown=gameProcess&&GetExitCodeProcess(gameProcess,&exitCode);
 critical(L"detach pid="+std::to_wstring(pid)+L" processExitCode="+(exitKnown?hex32(exitCode):L"unavailable")+
  L" stillActive="+std::to_wstring(exitKnown&&exitCode==STILL_ACTIVE));commandQueue.clear();inFlightMatch=0;inFlightRetries=0;sentAt=0;lastStallLog=0;typeTap.clear();
 releaseBridge();externalData=nullptr;externalSampler.reset();externalReader.close();externalAt=0;
 bridgeAttempts=0;bridgeBlocked=false;bridgeRetryAt=0;bridgeFailure.clear();
 if(gameProcess)CloseHandle(gameProcess);gameProcess=nullptr;lastProcessCheck=0;game=nullptr;pid=0;gameThread=0;snap={};overlayRecovery.suspend();motion.clear();statusMotion.clear();labelRasters.clear();clearEnemyDisplay();hideWorldOverlay();
}
static bool boundGameAlive(){
 if(!game||!IsWindow(game))return false;
 DWORD actualPid=0;DWORD actualThread=GetWindowThreadProcessId(game,&actualPid);
 if(actualPid!=pid||actualThread!=gameThread||!actualPid)return false;
 return (gameProcess&&WaitForSingleObject(gameProcess,0)==WAIT_TIMEOUT)||externalReader.alive();
}
// The process list is authoritative for discovery; window selection is a
// separate step. This avoids rejecting protected launchers just because the
// game window is hidden, owned, or not the largest top-level window.
static DWORD discoverGamePid(){return external::discovery::scan().best.pid;}
static bool bridgeFailed(const std::wstring&reason,DWORD error=0){
 const auto text=reason+(error?L"（错误代码 "+std::to_wstring(error)+L"）":L"");
 if(bridgeFailure!=text){bridgeFailure=text;critical(L"command bridge failure: "+text+L" pid="+std::to_wstring(pid)+L" thread="+std::to_wstring(gameThread));}
 setStatus(text);return false;
}
static bool ensureBridge(){
 if(!enabled)return false; // A secondary display alone never installs the command runtime.
 if(shared)return true;
 // Runtime feature maintenance still checks local playable state and exact signatures.
 if(!game)return false;
 if(!gameinput::runtimeAvailable())return bridgeFailed(L"运行组件缺失或版本不匹配，请完整更新工具箱",GetLastError());DWORD now=GetTickCount();
 if(!game||bridgeBlocked||(bridgeRetryAt&&int32_t(now-bridgeRetryAt)<0))return false;
 gameinput::NativeWindowProbe native{};
 if(!gameinput::nativeWindow(pid,gameThread,&native)){
  // Readiness is not an installation failure. Retry promptly without using
  // the failure backoff or leaving an uninitialized mapping behind.
  static DWORD lastProbe=0,probePid=0;
  if(pid!=probePid||DWORD(now-lastProbe)>=5000){lastProbe=now;probePid=pid;
   critical(L"native-window-check read="+std::to_wstring(native.read)+L" raw="+hex32(native.raw)+
    L" owner="+std::to_wstring(native.owner)+L" actualTid="+std::to_wstring(native.thread)+
    L" expectedPid="+std::to_wstring(pid)+L" expectedTid="+std::to_wstring(gameThread)+L" error="+std::to_wstring(native.error));
  }
  bridgeRetryAt=now+250;return bridgeFailed(L"等待游戏主窗口就绪，功能将自动连接",native.error);
 }
 ++bridgeAttempts;bridgeRetryAt=now+(bridgeAttempts>=3?30000:2000);
 critical(L"bridge-connect-attempt="+std::to_wstring(bridgeAttempts)+L" pid="+std::to_wstring(pid)+L" thread="+std::to_wstring(gameThread));
 wchar_t n[100];mappingName(n,pid);
 mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Shared),n);
 DWORD mappingError=GetLastError();if(!mapping)return bridgeFailed(L"创建命令共享内存失败",mappingError);
 if(mappingError==ERROR_ALREADY_EXISTS){
  Shared*prior=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));
  const DWORD viewError=prior?0:GetLastError();
  bool valid=prior&&prior->magic==Magic&&prior->version==Version&&prior->pid==pid;
  bool ours=valid&&prior->owner==GetCurrentProcessId(),ownerDead=false;
  if(valid&&!ours){HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,prior->owner);ownerDead=process?WaitForSingleObject(process,0)==WAIT_OBJECT_0:GetLastError()==ERROR_INVALID_PARAMETER;if(process)CloseHandle(process);}
  bool stopped=valid&&(ours||ownerDead)&&prior->stop==2;
  if(!stopped){
   if(valid&&(ours||ownerDead)){
    bool pending=false;for(const auto&r:retiredBridges)if(r.state->pid==pid)pending=true;
    if(!pending){prior->stop=1;HMODULE cleanupModule=gameinput::runtimeModule();auto cleanupHook=gameinput::install(gameentry::Client::Assist,gameThread,pid,game);
     if(cleanupHook){retiredBridges.push_back({prior,mapping,cleanupModule,cleanupHook,game,now-100,gameThread,externalReader.connectionIdentity()});prior=nullptr;mapping=nullptr;reapBridges();}
    }
   }
   std::wstring reason=!valid?L"已有命令映射的大小/协议不匹配；请退出游戏和旧工具箱，再启动新版本。":
    (!ours&&!ownerDead?L"另一工具箱仍持有此游戏的命令连接；请先关闭旧工具箱。":L"旧命令桥尚未完成卸载；若持续等待，请退出游戏并重新启动。");
   if(prior)UnmapViewOfFile(prior);if(mapping)CloseHandle(mapping);mapping=nullptr;
   return bridgeFailed(reason,viewError);
  }
  UnmapViewOfFile(prior);
 }
 shared=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));if(!shared){DWORD error=GetLastError();releaseBridge();return bridgeFailed(L"映射命令共享内存失败",error);}
 uint32_t hostWindow=gameinput::windowToken(ui);if(!hostWindow){releaseBridge();return bridgeFailed(L"工具箱窗口句柄无法与 32 位运行组件互通",ERROR_INVALID_WINDOW_HANDLE);}
 memset(shared,0,sizeof *shared);shared->magic=Magic;shared->version=Version;shared->pid=pid;shared->owner=GetCurrentProcessId();shared->hostWindow=hostWindow;shared->heartbeat=now;shared->flags=flags();shared->scanMs=8;shared->repairMs=repairMs;shared->externalDisplay=1;shared->driverMode=schedulerMode;shared->driverTarget=schedulerTarget;
 shared->megaQueueMode=megaQueueMode;shared->objectQueueMode=objectQueueMode;shared->driverStatus=0;syncAdditional();syncTypeConfig();
 shared->clockTick=now;shared->clockStart=0;shared->clockFrequency=0;
 module=gameinput::runtimeModule();
 {
  if(!gameinput::assistIdentity(module,Version,sizeof(Shared),MappingPrefix.text)){
   bridgeBlocked=true;releaseBridge();
   diag(L"Assist bridge protocol/structure mismatch");
   return bridgeFailed(L"运行组件版本不匹配，请完整更新工具箱。");
  }
 }
 hook=gameinput::install(gameentry::Client::Assist,gameThread,pid,game);
 if(!hook){DWORD error=GetLastError();std::wstring stage=gameinput::installationStage();releaseBridge();
  const auto capacity=gameinput::capacity();
  critical(L"hook-install-failed stage="+stage+L" Win32="+std::to_wstring(error)+
   L" hostBits="+std::to_wstring(sizeof(void*)*8)+L" targets="+std::to_wstring(capacity.targets)+L"/8 subscriptions="+std::to_wstring(capacity.subscriptions)+L"/32");logLoadedGameModules();
  if(stage==L"target-slots")return bridgeFailed(L"8 个游戏进程连接槽已占用；这不是功能数量限制，请查看旧连接清理日志",error);
  if(stage==L"subscription-slots")return bridgeFailed(L"32 个功能订阅槽已占用，请查看旧订阅清理日志",error);
  return bridgeFailed(L"无法建立游戏操作连接（"+stage+L"）",error);}
 bridgeFailure.clear();ack=notice=0;lastData=0;attachAt=now;wakeGame();
 wchar_t dllPath[32768]={},transportName[96];uint64_t moduleBase=0,hookRva=0;
#ifdef _WIN64
 GetModuleFileNameW(nullptr,dllPath,32768);wchar_t*name=wcsrchr(dllPath,L'\\');
 if(name&&size_t(name-dllPath)+1+sizeof(L"GameRuntime.dll")/sizeof(wchar_t)<=32768)wcscpy(name+1,L"GameRuntime.dll");else dllPath[0]=0;
 moduleBase=runtimebroker::client().moduleBase();hookRva=runtimebroker::client().hookRva();
#else
 GetModuleFileNameW(module,dllPath,32768);moduleBase=uintptr_t(module);hookRva=uintptr_t(GetProcAddress(module,"GameRuntimeHook"))-uintptr_t(module);
#endif
 runtime::mapName(transportName,pid,GetCurrentProcessId());
 critical(std::wstring(L"hook-installed dll=")+dllPath+L" moduleValidationBase="+std::to_wstring(moduleBase)+
  L" export=GameRuntimeHook stdcallArgs=3 hookType=WH_GETMESSAGE rva="+
  std::to_wstring(hookRva)+
  L" pid="+std::to_wstring(pid)+L" thread="+std::to_wstring(gameThread)+L" hwnd="+std::to_wstring(uintptr_t(game))+
  L" runtimeMap="+transportName+L" runtimeVersion="+std::to_wstring(runtime::Version)+L" runtimeBytes="+std::to_wstring(sizeof(runtime::Control))+
  L" assistMap="+n+L" assistVersion="+std::to_wstring(Version)+L" assistBytes="+std::to_wstring(sizeof(Shared)));
 logBridgeHealth(true);return true;
}
static bool attach(){
 auto scan=external::discovery::scan();
 if(!scan.best.pid){
  if(scan.snapshotError)setStatus(L"进程快照失败，Win32="+std::to_wstring(scan.snapshotError));
  else if(!scan.processCount)setStatus(L"未发现兼容游戏进程（支持随机进程名）");
  else setStatus(L"找到多个游戏进程但没有匹配窗口，无法安全选择只读 PID");
  return false;
 }
 const auto&c=scan.best;
 if(!externalReader.open(c.pid)){
  DWORD code=externalReader.lastOpenError;
  setStatus(L"找到游戏进程，但只读连接失败 (Win32="+std::to_wstring(code)+L")");
  diag(L"external OpenProcess failed pid="+std::to_wstring(c.pid)+L" error="+std::to_wstring(code));return false;
 }
 pid=c.pid;gameThread=c.thread;game=external::discovery::resolveYrppWindow(externalReader,pid,c.window,&gameThread,&gameWindowFromYrpp);
 critical(L"read-attached pid="+std::to_wstring(pid)+L" hwnd="+std::to_wstring(uintptr_t(game))+L" tid="+std::to_wstring(gameThread)+(gameWindowFromYrpp?L" windowSource=YRpp-0xB73550":L" windowSource=validated-fallback-or-none"));
 critical(gamebuild::describe([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);}));
 wchar_t imagePath[32768]{};DWORD pathLength=32768;
 if(externalReader.imagePath(imagePath,&pathLength))critical(std::wstring(L"game-image path=")+imagePath);
 logLoadedGameModules();
 gameProcess=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
 if(!gameProcess)gameProcess=OpenProcess(SYNCHRONIZE,FALSE,pid); // Liveness still works when exit-code query access is denied.
 setStatus(L"已连接只读 PID "+std::to_wstring(pid)+
  (game?L"，正在校验内存布局…":L"，尚无可用窗口；控制功能保持关闭"));return true;
}
static EnemyRasterKey enemyRasterKey(const external::enemies::Marker&m){return {m.type,m.owner,m.quantity};}
static uint32_t statusOptions(){return (extra.parasiteMarker?external::status::Parasite:0)|(extra.crateBuffMarkers?external::status::CrateBuffs:0);}
static void updateStatusDisplay(const Snapshot&source){
 const auto&f=externalSampler.unitStatuses();
 if(!statusOptions()||snap.renderStatus!=1||!external::status::fresh(f,source.match,sampleRenderClock(nullptr))){statusMotion.clear();return;}
 statusSample.labels=f.count;statusSample.match=f.match;statusSample.sampledTick=f.tick;statusSample.frame=f.frame;
 statusSample.cameraX=source.cameraX;statusSample.cameraY=source.cameraY;
 for(int i=0;i<f.count;++i){const auto&m=f.items[i];statusSample.label[i]={m.id,m.x,m.y};}
 statusMotion.update(statusSample,f.frame);
}
static void updateEnemyDisplay(const Snapshot&source){
 const auto&f=externalSampler.enemyMarkers();
 if(!extra.enemyMarkers||snap.renderStatus!=1||!external::enemies::fresh(f,source.match,sampleRenderClock(nullptr))){clearEnemyDisplay();return;}
 if(enemyRasterMatch!=f.match){clearEnemyDisplay();enemyRasterMatch=f.match;}
 enemySample.labels=f.count;enemySample.match=f.match;enemySample.sampledTick=f.tick;enemySample.frame=f.frame;
 enemySample.cameraX=source.cameraX;enemySample.cameraY=source.cameraY;
 for(int i=0;i<f.count;++i){const auto&m=f.items[i];enemySample.label[i]={m.id,m.x,m.y,0,0,0,0,0,m.color,0};}
 enemyMotion.update(enemySample,f.frame);
 enemyGroups=enemyclusters::group(f);
 // Count-specific text is shared across equal groups; bound historical counts.
 if(enemyRasters.size()>4096)enemyRasters.clear();
}
static void mergeExternal(){
 if(!externalData)return;const auto&d=*externalData;const DWORD displayNow=sampleRenderClock(nullptr);
 bool control=commandMatch()!=0; // preserve authoritative command identity/catalogue even when display data is delayed
 // External snapshots are display data, never authoritative for a live
 // command bridge. An incompatible EXE layout must not disable a healthy DLL.
 snap.readStatus=d.status==1&&d.sampledTick&&DWORD(displayNow-d.sampledTick)>=2000?0:d.status;
 snap.bridgeStatus=shared&&lastData&&DWORD(GetTickCount()-lastData)<1500?incoming.status:0;
 snap.commandStatus=commandChannelReady()?1:0;
 const bool externalValid=d.status==1&&d.sampledTick&&
                           DWORD(displayNow-d.sampledTick)<2000;
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
#include "../../external_reader/source/attack_trace.h"
static external::attacktrace::Recorder attackRecorder;
static void sampleExternal(DWORD now){
 if(shared&&shared->viewZoomBusy)return;
 
 if(!externalReader.valid())return;
 const DWORD displayNow=sampleRenderClock(nullptr);
 const DWORD period=((enabled&&foregroundGame())||secondaryDisplayAvailable())?DWORD(1000/informationFPS()):250;
 if(!externalAt||DWORD(displayNow-externalAt)>=period){
  const DWORD gap=externalAt?DWORD(displayNow-externalAt):0;
  LARGE_INTEGER sampleStart{},sampleEnd{},sampleFrequency{};QueryPerformanceCounter(&sampleStart);
  externalAt=displayNow;externalReader.begin();externalSampler.stopped(plannerState.stopped);externalData=&externalSampler.sample(displayNow,flags(),extra.enemyMarkers,statusOptions());
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
 else if((enabled&&foregroundGame())||secondaryDisplayAvailable()){externalReader.begin();externalSampler.refreshCamera(displayNow);}
 mergeExternal();
}
static void schedulerDiagnostic(){
 if(!shared)return;
 static int lastMode=-1,lastStatus=999;
 int mode=int(shared->driverMode),status=int(shared->driverStatus);
 if(mode==lastMode&&status==lastStatus)return;
 lastMode=mode;lastStatus=status;
 diag(L"scheduler mode="+std::to_wstring(mode)+L" status="+std::to_wstring(status)+
      L" entry="+std::to_wstring(shared->driverTarget)+
      L" gameFrameCallbacks="+std::to_wstring(shared->driverTicks));
}
static void logCommandReceipts(const Snapshot& s){
 if(!diagEnabled)return;
 if(s.match!=lastReceiptMatch){lastReceiptMatch=s.match;
  lastReceiptWrite=s.receiptWrite-s.receiptCount;}
 uint32_t available=s.receiptWrite-lastReceiptWrite;
 if(available>RecentReceiptCapacity){lastReceiptWrite=s.receiptWrite-RecentReceiptCapacity;available=RecentReceiptCapacity;}
 for(uint32_t j=0;j<available;++j){
  const auto& e=s.recentReceipts[(lastReceiptWrite+j)%RecentReceiptCapacity];
  if(!e.serial||e.match!=s.match)continue;
  diag(L"task receipt serial="+std::to_wstring(e.serial)+
    L" match="+std::to_wstring(e.match)+L" unit="+std::to_wstring(e.unit)+
    L" target="+std::to_wstring(e.target)+L" owner="+std::to_wstring(e.owner)+
    L" phase="+std::to_wstring(e.phase)+L" failure="+std::to_wstring(e.failure));
 }
 lastReceiptWrite=s.receiptWrite;
}
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
  static DWORD activationLogAt=0;static int activationState=0;
  if(incoming.commandStatus>=2&&incoming.commandStatus<=4&&
     (activationState!=incoming.commandStatus||DWORD(logNow-activationLogAt)>=5000)){
   activationState=incoming.commandStatus;activationLogAt=logNow;
   int players=0;uint8_t oos=0;int ring[3]{};
   externalReader.readFresh(0xA8B54C,&players,4);externalReader.readFresh(0xA8B8C2,&oos,1);externalReader.readFresh(0x8B41F8,ring,sizeof ring);
   critical(L"activation-wait state="+std::to_wstring(incoming.commandStatus)+L" frame="+std::to_wstring(incoming.frame)+
    L" players="+std::to_wstring(players)+L" nativeOOS="+std::to_wstring(oos)+L" DoList="+std::to_wstring(ring[0])+L"/"+std::to_wstring(ring[1])+L"/"+std::to_wstring(ring[2]));
   if(incoming.commandStatus==3)for(const auto&s:Signatures){uint8_t actual[12]{};
    if(!externalReader.readFresh(s.address,actual,s.length)){critical(L"startup-probe-unreadable site="+hex32(s.address));continue;}
    if(memcmp(actual,s.bytes,s.length)){std::wstring expected,found;wchar_t byte[4];
     for(unsigned j=0;j<s.length;++j){swprintf(byte,4,L"%02X",unsigned(s.bytes[j]));expected+=byte;swprintf(byte,4,L"%02X",unsigned(actual[j]));found+=byte;}
     critical(L"startup-probe-difference site="+hex32(s.address)+L" expected="+expected+L" actual="+found);
    }
   }
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
 mergeExternal();
}
struct TextRaster{std::wstring text;float size;ARGB color;std::unique_ptr<Bitmap> image;};static std::vector<TextRaster> textRasters;
static void drawText(Graphics&g,const std::wstring&s,float x,float y,float size,Color color){
 static thread_local std::vector<TextRaster> textRasters;
 if(s.empty())return;for(auto&r:textRasters)if(r.text==s&&r.size==size&&r.color==color.GetValue()){g.DrawImage(r.image.get(),x-4,y-4,float(r.image->GetWidth()),float(r.image->GetHeight()));return;}
 if(textRasters.size()>=256)textRasters.clear();int w=std::max(16,int(s.size()*size+12)),h=int(size*2.2f+12);
 auto bitmap=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);{Graphics raster(bitmap.get());raster.Clear(Color(0,0,0,0));raster.SetSmoothingMode(SmoothingModeAntiAlias);
 FontFamily family(L"Microsoft YaHei UI");GraphicsPath path;StringFormat fmt;fmt.SetFormatFlags(StringFormatFlagsNoWrap);path.AddString(s.c_str(),-1,&family,FontStyleBold,size,PointF(4,4),&fmt);Pen edge(Color(235,5,9,16),3);edge.SetLineJoin(LineJoinRound);raster.DrawPath(&edge,&path);SolidBrush brush(color);raster.FillPath(&brush,&path);}
 g.DrawImage(bitmap.get(),x-4,y-4,float(w),float(h));textRasters.push_back({s,size,color.GetValue(),std::move(bitmap)});
}
struct GlyphRaster {wchar_t ch;ARGB color;float advance;std::unique_ptr<Bitmap> image;};static std::vector<GlyphRaster> glyphRasters;
static const GlyphRaster& glyph(wchar_t ch,Color color){
 static thread_local std::vector<GlyphRaster> glyphRasters;
 for(auto&r:glyphRasters)if(r.ch==ch&&r.color==color.GetValue())return r;
 auto bm=std::make_unique<Bitmap>(25,34,PixelFormat32bppPARGB);{Graphics g(bm.get());g.Clear(Color(0,0,0,0));g.SetSmoothingMode(SmoothingModeAntiAlias);
 FontFamily family(L"Microsoft YaHei UI");GraphicsPath p;StringFormat fmt;fmt.SetFormatFlags(StringFormatFlagsNoWrap);p.AddString(&ch,1,&family,FontStyleBold,13,PointF(4,4),&fmt);Pen edge(Color(235,5,9,16),3);edge.SetLineJoin(LineJoinRound);g.DrawPath(&edge,&p);SolidBrush brush(color);g.FillPath(&brush,&p);}
 glyphRasters.push_back({ch,color.GetValue(),ch==L' '?4.f:ch<128?7.5f:13.f,std::move(bm)});return glyphRasters.back();
}
static void drawLabel(Graphics&g,const visual::LabelView&l,float sx,float sy,int labelScalePercent){
 static thread_local std::unordered_map<uint32_t,LabelRaster> labelRasters;if(labelRasters.size()>4096)labelRasters.clear();
 const float zoom=viewport::factor(labelScalePercent);
 if(l.advanced){SolidBrush red(l.advanced==2?Color(255,255,190,45):Color(255,255,0,0));PointF mark[]={{l.x*sx-3*zoom,l.y*sy-7*zoom},{l.x*sx+3*zoom,l.y*sy-7*zoom},{l.x*sx,l.y*sy-3*zoom}};g.FillPolygon(&red,mark,3);}
 auto&r=labelRasters[l.id];ARGB owner=l.ownerColor?l.ownerColor:0xffeeeeeeu;
 if(!r.image||r.used!=l.used||r.capacity!=l.capacity||r.centis!=l.centis||r.frozenCentis!=l.frozenCentis||r.color!=owner){
  std::vector<std::pair<std::wstring,Color>> lines;auto seconds=[](int n){wchar_t t[40];swprintf(t,40,L"%d.%02ds",n/100,n%100);return std::wstring(t);};
  if(l.frozenCentis)lines.push_back({seconds(l.frozenCentis),Color(255,100,220,255)});
  if(l.centis)lines.push_back({seconds(l.centis),Color(owner)});
  if(l.capacity)lines.push_back({std::to_wstring(l.used)+L"/"+std::to_wstring(l.capacity),Color(owner)});
  float width=0;for(const auto&line:lines){float row=0;for(wchar_t c:line.first)row+=glyph(c,line.second).advance;width=std::max(width,row);}
  int w=std::max(1,int(std::ceil(width))+10);r.image=std::make_unique<Bitmap>(w,std::max(34,int(lines.size())*14+20),PixelFormat32bppPARGB);
  {Graphics strip(r.image.get());strip.Clear(Color(0,0,0,0));float y=0;for(const auto&line:lines){float row=0;for(wchar_t c:line.first)row+=glyph(c,line.second).advance;float x=(width-row)*.5f;for(wchar_t c:line.first){auto&tile=glyph(c,line.second);strip.DrawImage(tile.image.get(),x,y,25.f,34.f);x+=tile.advance;}y+=14;}}
  r.color=owner;r.width=width;r.used=l.used;r.capacity=l.capacity;r.centis=l.centis;r.frozenCentis=l.frozenCentis;r.shield=int(lines.size());
 }
 if(r.shield)g.DrawImage(r.image.get(),l.x*sx-(r.width*.5f+4)*zoom,l.y*sy-(20+(r.shield-1)*14.f)*zoom,r.image->GetWidth()*zoom,r.image->GetHeight()*zoom);
}
#include "game_assets_host.h"
#include "formation_host.h"
#include "status_draw.h"
#include "passenger_draw.h"
#include "watch_host.h"
static void drawEnemyMarker(Graphics&g,const external::enemies::Marker&m,const visual::LabelView&v,float sx,float sy){
 static thread_local std::map<EnemyRasterKey,EnemyRaster> enemyRasters;if(enemyRasters.size()>4096)enemyRasters.clear();
 float x=v.x*sx,y=v.y*sy;Pen edge(Color(245,5,9,16),4.f),mark(Color(m.color),2.f);
 if(m.kind==3){g.DrawRectangle(&edge,x-7,y-7,14.f,14.f);g.DrawRectangle(&mark,x-7,y-7,14.f,14.f);}
 else{PointF points[]={{x,y-7},{x+7,y},{x,y+7},{x-7,y}};g.DrawPolygon(&edge,points,4);g.DrawPolygon(&mark,points,4);}
 std::wstring text;
 for(int i=0;i<(m.quantity>1?14:18)&&m.name[i];++i)if(m.name[i]>=32&&m.name[i]!=127)text.push_back(wchar_t(m.name[i]));
 if(!text.empty()&&text.back()>=0xD800&&text.back()<=0xDBFF)text.pop_back();
 if(text.empty())text+=m.kind==3?L"建筑":L"单位";
 if(m.quantity>1)text+=L" ×"+std::to_wstring(m.quantity);
 auto&r=enemyRasters[enemyRasterKey(m)];
 if(!r.image||r.text!=text){
  const int width=std::clamp(int(text.size())*13+12,48,280);r.image=std::make_unique<Bitmap>(width,36,PixelFormat32bppPARGB);
  Graphics raster(r.image.get());raster.Clear(Color(0,0,0,0));raster.SetSmoothingMode(SmoothingModeAntiAlias);
  FontFamily family(L"Microsoft YaHei UI");GraphicsPath path;StringFormat fmt;fmt.SetFormatFlags(StringFormatFlagsNoWrap);
  path.AddString(text.c_str(),-1,&family,FontStyleBold,12,PointF(4,4),&fmt);Pen outline(Color(245,5,9,16),3);outline.SetLineJoin(LineJoinRound);
  raster.DrawPath(&outline,&path);SolidBrush brush(Color(255,255,235,235));raster.FillPath(&brush,&path);r.text=text;
 }
 g.DrawImage(r.image.get(),x+8,y+4,float(r.image->GetWidth()),float(r.image->GetHeight()));
}
#include "native_hotkeys_host.h"
#include "air_path_host.h"
static void serviceAirPath(){airway::service();}
static void resetAirPath(){airway::reset();}
static void airPathResult(int r,int n,uint32_t id,uint32_t token){airway::result(r,n,id,token);}
#include "aircraft_indicator.h"
#include "minimap_overlay.h"
#include "render_upgrade_host.h"
static void requestReservationSound(){
 // Best-effort UI feedback must never occupy or delay a production order.
 if(!shared||!commandFresh(GetTickCount())||shared->uiSoundRequest!=shared->uiSoundAck)return;
 shared->uiSoundTick=GetTickCount();shared->uiSoundMatch=commandMatch();
 MemoryBarrier();InterlockedIncrement((volatile LONG*)&shared->uiSoundRequest);wakeGame();
}
static void publishReservationPolicy(const construction::InputPolicy&p){
 if(!shared)return;InterlockedIncrement(&shared->reservePolicySeq);shared->reservePolicy=p;MemoryBarrier();InterlockedIncrement(&shared->reservePolicySeq);
}
static bool takeReservationClick(construction::InputClick&click){
 if(!shared||shared->reserveInput==shared->reserveConsumed)return false;
 uint32_t serial=shared->reserveInput;MemoryBarrier();click=shared->reserveClick;MemoryBarrier();
 if(serial!=shared->reserveInput)return false;
 publishReservationPolicy({}); // close authorization before releasing the input slot
 MemoryBarrier();shared->reserveConsumed=serial;return true;
}
static overlaydisplay::ReadLease informationDisplayLease;
static overlaydisplay::Context passiveProbeContext;
static uint32_t passiveProbeTick=0;static bool passiveProbeReady=false,passiveProbeEligible=false;
static overlaydisplay::Context passiveDisplayContext(const RECT&client){
 return {uintptr_t(game),pid,displayMatch(),int(client.right-client.left),int(client.bottom-client.top),snap.width,snap.height};
}
static overlaydisplay::State passiveDisplayState(const external::display::State&state,const RECT&client){
 const bool eligible=(enabled&&foregroundGame()&&!snap.chat)||secondaryDisplayAvailable();
 passiveProbeContext=passiveDisplayContext(client);passiveProbeTick=GetTickCount();passiveProbeEligible=eligible;
 const auto result=informationDisplayLease.inspect(passiveProbeContext,eligible,state.readable,state.visible(),snap.renderStatus==1,passiveProbeTick);
 passiveProbeReady=result==overlaydisplay::State::Ready;return result;
}
static overlaydisplay::State passiveDisplayState(){
 RECT client{};if(!game||!GetClientRect(game,&client)){passiveProbeReady=false;informationDisplayLease.reset();return overlaydisplay::State::Hidden;}
 const bool eligible=(enabled&&foregroundGame()&&!snap.chat)||secondaryDisplayAvailable();
 // Reservation hotkeys, queue lanes and hover/input checks share one good
 // display observation. Cache neither failed reads nor a HOLD deadline.
 if(passiveProbeReady&&snap.renderStatus==1&&passiveProbeEligible==eligible&&passiveProbeContext==passiveDisplayContext(client)&&uint32_t(GetTickCount()-passiveProbeTick)<8)return overlaydisplay::State::Ready;
 return passiveDisplayState(external::display::inspect([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);}),client);
}
static bool constructionDisplayVisible(){
 return enabled&&foregroundGame()&&!snap.chat&&passiveDisplayState()!=overlaydisplay::State::Hidden;
}
static bool constructionDisplayFresh(){
 return enabled&&foregroundGame()&&!snap.chat&&passiveDisplayState()==overlaydisplay::State::Ready;
}
#include "construction_host.h"
#include "../../external_reader/source/sidebar_shapes.h"
static external::sidebar::Cache sidebarShapes;
static void serviceConstruction(){buildreserve::service();}
static void hideConstruction(){buildreserve::hide();}
static void resetConstruction(){buildreserve::reset();}
static void destroyConstruction(){buildreserve::destroy();}
static void constructionResult(int r,int n,uint32_t id,uint32_t token){buildreserve::result(r,n,id,token);}
struct Surface{HDC dc{};HBITMAP bitmap{},old{};BYTE*bits{};int w{},h{};~Surface(){clear();}void clear(){if(dc){if(old&&old!=HGDI_ERROR)SelectObject(dc,old);DeleteDC(dc);}if(bitmap)DeleteObject(bitmap);dc=nullptr;bitmap=old=nullptr;bits=nullptr;w=h=0;}bool resize(int x,int y){if(x==w&&y==h&&dc)return true;clear();if(x<1||y<1||x>8192||y>8192)return false;BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=x;bi.bmiHeader.biHeight=-y;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;HDC screen=GetDC(nullptr);dc=CreateCompatibleDC(screen);bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,(void**)&bits,nullptr,0);ReleaseDC(nullptr,screen);if(!bitmap||!dc){clear();return false;}old=(HBITMAP)SelectObject(dc,bitmap);if(!old||old==HGDI_ERROR){clear();return false;}w=x;h=y;return true;}};static Surface surface;
#include "watch_windows.h"
#include "world_presentation.h"
namespace worldrender {
struct Job {uint32_t pid,match,tick;int w,h,clientW,clientH;POINT origin;std::function<void(Graphics&)> draw;overlaycompositor::Context context;POINT relative{};};
struct Frame {uint32_t pid,match,tick;int w,h,clientW,clientH;POINT origin;std::vector<uint32_t> pixels;overlaycompositor::Context context;POINT relative{};};
static asyncwork::Latest<Job,Frame> worker([](const Job&j)->std::shared_ptr<const Frame>{
 if(j.w<1||j.h<1||j.w>8192||j.h>8192||uint64_t(j.w)*j.h>16*1024*1024)return {};
 auto out=std::make_shared<Frame>();out->pid=j.pid;out->match=j.match;out->tick=j.tick;out->w=j.w;out->h=j.h;out->clientW=j.clientW;out->clientH=j.clientH;out->origin=j.origin;out->context=j.context;out->relative=j.relative;out->pixels.resize(size_t(j.w)*j.h);
 Bitmap bm(j.w,j.h,j.w*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(out->pixels.data()));{Graphics g(&bm);j.draw(g);}return out;
});
static std::shared_ptr<const Frame> displayed;
static bool active=false;
static void submit(Job input){
 active=true;input.context=overlaycompositor::host().capture(uintptr_t(game),pid);
 if(input.context.token&&(input.context.cameraX!=snap.cameraX||input.context.cameraY!=snap.cameraY))input.context.token=0;
 POINT origin{};ClientToScreen(game,&origin);input.relative={input.origin.x-origin.x,input.origin.y-origin.y};
 worker.submit(std::move(input));
}
static void invalidate(){overlaycompositor::host().hide(overlaycompositor::World);if(!active)return;active=false;worker.invalidate();displayed.reset();}
static void present(){auto frame=worker.get();
 auto context=overlaycompositor::host().capture(uintptr_t(game),pid);
 if(context.token&&IsWindowVisible(overlay))ShowWindow(overlay,SW_HIDE);
 if(!frame||frame->pid!=pid||frame->match!=snap.match){overlaycompositor::host().hide(overlaycompositor::World);return;}
 RECT client{};GetClientRect(game,&client);if(client.right!=frame->clientW||client.bottom!=frame->clientH)return;
 if(context.token){
  // Immutable pixels are shared with the single compositor; no extra HWND is
  // shown for labels, passenger bubbles, timers, formation or world markers.
  auto pixels=std::shared_ptr<const std::vector<uint32_t>>(frame,&frame->pixels);
  if(frame->context.token!=context.token||frame->context.cameraX!=context.cameraX||frame->context.cameraY!=context.cameraY){overlaycompositor::host().hide(overlaycompositor::World);return;}
  overlaycompositor::host().publish({overlaycompositor::World,10,frame->context,{frame->relative.x,frame->relative.y,frame->w,frame->h},true,255,std::move(pixels)});
  if(IsWindowVisible(overlay))ShowWindow(overlay,SW_HIDE);
  displayed=frame;return;
 }
 overlaycompositor::host().hide(overlaycompositor::World);
 if((frame==displayed&&IsWindowVisible(overlay))||DWORD(GetTickCount()-frame->tick)>500)return;
 if(!surface.resize(frame->w,frame->h))return;memcpy(surface.bits,frame->pixels.data(),frame->pixels.size()*4);
 POINT origin{};ClientToScreen(game,&origin);origin.x+=frame->relative.x;origin.y+=frame->relative.y;POINT zero{};
 SIZE size{frame->w,frame->h};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
 bool ok=UpdateLayeredWindow(overlay,nullptr,&origin,&size,surface.dc,&zero,0,&blend,ULW_ALPHA)!=0;
 overlayRecovery.submitted(ok);if(ok){displayed=frame;++displayCounters.uploads;overlaystack::place(overlay,overlaystack::Layer::World,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);}
}
}
static void paint(){
 ++displayCounters.paints;
 if(enabled||secondaryDisplayRequested())sampleExternal(GetTickCount()); // Display cadence is independent of command servicing.
 const auto displayState=external::display::inspect([](uint32_t at,void*out,size_t n){return externalReader.readFresh(at,out,n);});
 RECT r{};GetClientRect(game,&r);POINT origin{};ClientToScreen(game,&origin);
 // The scene renderer validates its own live view. An unrelated information
 // scan can fail for one refresh without invalidating that completed scene.
 // Pass the readable/hidden distinction through before the legacy UI gates.
 renderupgrade::update(origin,r,displayState);
 const auto passive=passiveDisplayState(displayState,r);
 if(passive==overlaydisplay::State::Hidden){worldrender::invalidate();overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();minimap::hide();watchwindows::hide();hideConstruction();clearPassengerDisplay();return;}
 if(passive==overlaydisplay::State::Hold){
  // Retain existing pixels only. Do not rebuild from a failed information
  // sample, refresh an input authorization, or renew the read-failure lease.
  if(!enabled||!foregroundGame()||snap.chat){worldrender::invalidate();overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();minimap::hidePrimary();watchwindows::hidePrimary();hideConstruction();clearPassengerDisplay();return;}
  overlaycompositor::host().touch(overlaycompositor::World,uintptr_t(game),pid);return;
 }
 const bool battlefield=displayState.visible();
 const DWORD renderNow=sampleRenderClock(shared);
 {static worldpresentation::ContextRevision prior;auto c=overlaycompositor::host().capture(uintptr_t(game),pid);
  if(prior.changed(c.token,c.cameraX,c.cameraY,snap.cameraX,snap.cameraY))++paintRevision;}
 minimap::update(origin,r,battlefield);watchhost::update(renderNow);watchwindows::update(origin,r,battlefield);
 if(!enabled||!foregroundGame()||snap.chat){worldrender::invalidate();overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();hideConstruction();clearPassengerDisplay();return;}
 DWORD now=sampleClock(shared);int labelCount=motion.render(renderNow,labelViews);
 const auto&statusFrame=externalSampler.unitStatuses();
 int statusCount=statusOptions()&&externalData&&external::status::fresh(statusFrame,externalData->match,renderNow)?std::min(statusFrame.count,statusMotion.render(renderNow,statusViews)):0;
 const auto&rawEnemyFrame=externalSampler.enemyMarkers();
 int rawEnemyCount=extra.enemyMarkers&&externalData&&external::enemies::fresh(rawEnemyFrame,externalData->match,renderNow)?std::min(rawEnemyFrame.count,enemyMotion.render(renderNow,enemyViews)):0;
 int enemyCount=rawEnemyCount?enemyclusters::render(rawEnemyFrame,enemyViews,rawEnemyCount,enemyGroups,clusteredEnemies,clusteredEnemyViews):0;
 std::copy(clusteredEnemyViews,clusteredEnemyViews+enemyCount,enemyViews);
 const auto&enemyFrame=clusteredEnemies;
 updateAircraftButton(origin,r);formationhost::update();
 auto opaquePanels=overlaystack::opaqueBoxes(game,snap.width,snap.height);
 static std::vector<RECT> priorOpaquePanels;if(priorOpaquePanels.size()!=opaquePanels.size()||(!opaquePanels.empty()&&memcmp(priorOpaquePanels.data(),opaquePanels.data(),opaquePanels.size()*sizeof(RECT)))){priorOpaquePanels=opaquePanels;++paintRevision;}
 auto passengerBubbles=passengerView(renderNow);
 auto airLines=airway::lines();
 auto nativeBadges=buildreserve::nativeBadges();
 uint32_t pausedTabs=0,idleTabs=0;
 if(flags()&Production)externalSampler.productionReminders(extra.productionReminderMask,idleTabs,pausedTabs);
 idleTabs|=pausedTabs;
 idleTabs=sidebarShapes.update([](uint32_t at,void*out,size_t size){return externalReader.readFresh(at,out,size);},renderNow,pid,commandMatch(),snap.width,snap.height,idleTabs,pausedTabs);
 bool tabPhase=(renderNow/400)%2==0;bool spiderPhase=(renderNow/650)%2==0;
 bool panel=SendMessageW(check[6],BM_GETCHECK,0,0)==BST_CHECKED;
 bool hasToast=panel&&now<toastUntil;bool summary=panel&&snap.transports;
 worldrender::present();
 if(!formationhost::visible()&&!labelCount&&!hasToast&&!summary&&!snap.threatCount&&!snap.crateCount&&!enemyCount&&!statusCount&&!idleTabs&&passengerBubbles.empty()&&airLines.empty()&&nativeBadges.empty()){worldrender::invalidate();overlayRecovery.suspend();hideWorldOverlay();return;}
 viewport::Scale view;if(!view.set(snap.width,snap.height,r.right,r.bottom)){hideWorldOverlay();return;}
 const int physicalW=r.right,physicalH=r.bottom;r.right=view.gameWidth;r.bottom=view.gameHeight;
 const float sx=1,sy=1; // Positions, glyphs, icons and bounds share game-pixel space.
 float top=float(std::clamp(topMargin,0,std::max(0,int(r.bottom)-90)));
 float panelW=std::min(float(std::max(1,int(r.right)-16)),std::max(330.f,hasToast?float(toast.size()*12+24):330.f));
 float panelX=(r.right-panelW)*.5f;
 int left=r.right,up=r.bottom,right=0,bottom=0;
 auto include=[&](float x,float y,float w,float h){int x0=std::clamp(int(std::floor(x)),0,int(r.right)),y0=std::clamp(int(std::floor(y)),0,int(r.bottom));int x1=std::clamp(int(std::ceil(x+w)),0,int(r.right)),y1=std::clamp(int(std::ceil(y+h)),0,int(r.bottom));if(x1>x0&&y1>y0){left=std::min(left,x0);up=std::min(up,y0);right=std::max(right,x1);bottom=std::max(bottom,y1);}};
 if(formationhost::visible())include(0,0,r.right,r.bottom);
 for(const auto&b:passengerBubbles)include(b.left,b.top,b.width,b.height+6);
 for(const auto&b:nativeBadges)include(b.x*sx,b.y*sy,b.w*sx,b.h*sy);
 for(auto l:airLines)include(std::min(l.x,l.tx)*sx-6,std::min(l.y,l.ty)*sy-6,std::abs(l.x-l.tx)*sx+12,std::abs(l.y-l.ty)*sy+12);
 if(idleTabs)for(int i=0;i<4;++i)if(idleTabs&(1u<<i)){const auto&f=sidebarShapes.frames[i];include(f.x*sx,f.y*sy,f.width*sx,f.height*sy);}
 if(hasToast||summary)include(panelX-3,top-3,panelW+6,(hasToast&&summary)?62.f:32.f);
 for(int i=0;i<labelCount;++i){auto&l=labelViews[i];float z=viewport::factor(labelScalePercent);include(l.x*sx-140*z,l.y*sy-70*z,280*z,110*z);}
 for(int i=0;i<snap.crateCount;++i){auto&c=snap.crates[i];include(c.x*sx-110,c.y*sy-33,220,46);}
 for(int i=0;i<snap.threatCount;++i){auto&t=snap.threats[i];include(std::min(t.x,t.tx)*sx-10,std::min(t.y,t.ty)*sy-10,std::abs(t.x-t.tx)*sx+20,std::abs(t.y-t.ty)*sy+20);}
 for(int i=0;i<statusCount;++i){const auto&v=statusViews[i];const auto&m=statusFrame.items[i];auto b=statusbadges::bounds(v.x,v.y,m.buffs,m.parasite!=0);include(b.x,b.y,b.w,b.h);}
 for(int i=0;i<enemyCount;++i){const auto&v=enemyViews[i];if(external::enemies::onScreen(v.x,v.y,enemyFrame.width,enemyFrame.height))include(v.x*sx-10,v.y*sy-12,300,54);}
 if(right<=left||bottom<=up){hideWorldOverlay();return;}
 static Snapshot drawn{};static visual::LabelView drawnViews[MaxLabels];static int drawnCount;static bool lastToast,lastPanel,lastSummary;static std::wstring oldToast;static int oldW,oldH,oldMargin;static uint32_t revision;
 static external::enemies::Marker drawnEnemies[external::enemies::Maximum];static visual::LabelView drawnEnemyViews[MaxLabels];static int drawnEnemyCount,drawnEnemyWidth,drawnEnemyHeight;
 static external::status::Marker drawnStatuses[external::status::Maximum];static visual::LabelView drawnStatusViews[MaxLabels];static int drawnStatusCount,drawnStatusWidth,drawnStatusHeight;
 static bool drawnSpiderPhase=false;
 static uint32_t drawnIdleTabs=0,drawnShapeRevision=0;static bool drawnTabPhase=false;
 static std::vector<PassengerVisual> drawnPassengers;
 static std::vector<airway::Line> drawnAirLines;static std::vector<construction::NativeBadge> drawnBadges;
 bool changed=(statusCount&&drawnSpiderPhase!=spiderPhase)||drawnPassengers!=passengerBubbles||drawnBadges!=nativeBadges||drawnShapeRevision!=sidebarShapes.revision||drawn.width!=snap.width||drawn.height!=snap.height||drawnAirLines.size()!=airLines.size()||(!airLines.empty()&&memcmp(drawnAirLines.data(),airLines.data(),airLines.size()*sizeof(airway::Line)))||drawnIdleTabs!=idleTabs||(idleTabs&&drawnTabPhase!=tabPhase)||oldW!=physicalW||oldH!=physicalH||oldMargin!=topMargin||revision!=paintRevision||lastPanel!=panel||lastSummary!=summary||lastToast!=hasToast||(hasToast&&oldToast!=toast)||drawnCount!=labelCount||drawn.transports!=snap.transports||drawn.used!=snap.used||drawn.capacity!=snap.capacity||memcmp(drawnViews,labelViews,sizeof(visual::LabelView)*labelCount)||drawn.threatCount!=snap.threatCount||drawn.crateCount!=snap.crateCount||memcmp(drawn.threats,snap.threats,sizeof(Threat)*snap.threatCount)||memcmp(drawn.crates,snap.crates,sizeof(CrateMark)*snap.crateCount);
 changed=changed||drawnEnemyCount!=enemyCount||drawnEnemyWidth!=enemyFrame.width||drawnEnemyHeight!=enemyFrame.height||memcmp(drawnEnemyViews,enemyViews,sizeof(visual::LabelView)*enemyCount)||memcmp(drawnEnemies,enemyFrame.items,sizeof(external::enemies::Marker)*enemyCount);
 changed=changed||drawnStatusCount!=statusCount||drawnStatusWidth!=statusFrame.width||drawnStatusHeight!=statusFrame.height||memcmp(drawnStatusViews,statusViews,sizeof(visual::LabelView)*statusCount)||memcmp(drawnStatuses,statusFrame.items,sizeof(external::status::Marker)*statusCount);
 origin.x+=view.left(left);origin.y+=view.top(up);
 changed=worldpresentation::recoveryDue(overlayRecovery,GetTickCount(),{uintptr_t(game),pid,snap.match,physicalW,physicalH},
  overlaycompositor::host().capture(uintptr_t(game),pid).token!=0,IsWindowVisible(overlay)!=0)||changed;
 if(!changed){if(overlaycompositor::host().capture(uintptr_t(game),pid).token)return;if(!worldrender::displayed)return;RECT shown{};GetWindowRect(overlay,&shown);if(shown.left!=origin.x||shown.top!=origin.y||!IsWindowVisible(overlay))overlaystack::place(overlay,overlaystack::Layer::World,origin.x,origin.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);return;}
 int w=((view.width(right-left)+65)/64)*64,h=((view.height(bottom-up)+33)/32)*32;
 drawn.transports=snap.transports;drawn.used=snap.used;drawn.capacity=snap.capacity;drawn.threatCount=snap.threatCount;drawn.crateCount=snap.crateCount;memcpy(drawn.threats,snap.threats,sizeof(Threat)*snap.threatCount);memcpy(drawn.crates,snap.crates,sizeof(CrateMark)*snap.crateCount);drawnCount=labelCount;memcpy(drawnViews,labelViews,sizeof(visual::LabelView)*labelCount);oldW=physicalW;oldH=physicalH;oldMargin=topMargin;revision=paintRevision;lastPanel=panel;lastSummary=summary;lastToast=hasToast;oldToast=toast;
 drawnEnemyCount=enemyCount;drawnEnemyWidth=enemyFrame.width;drawnEnemyHeight=enemyFrame.height;memcpy(drawnEnemies,enemyFrame.items,sizeof(external::enemies::Marker)*enemyCount);memcpy(drawnEnemyViews,enemyViews,sizeof(visual::LabelView)*enemyCount);
 drawnStatusCount=statusCount;drawnStatusWidth=statusFrame.width;drawnStatusHeight=statusFrame.height;memcpy(drawnStatuses,statusFrame.items,sizeof(external::status::Marker)*statusCount);memcpy(drawnStatusViews,statusViews,sizeof(visual::LabelView)*statusCount);
 drawnSpiderPhase=spiderPhase;drawnPassengers=passengerBubbles;drawn.width=snap.width;drawn.height=snap.height;drawnShapeRevision=sidebarShapes.revision;drawnAirLines=airLines;drawnBadges=nativeBadges;drawnIdleTabs=idleTabs;drawnTabPhase=tabPhase;
 auto labels=std::vector<visual::LabelView>(labelViews,labelViews+labelCount),enemies=std::vector<visual::LabelView>(enemyViews,enemyViews+enemyCount),statuses=std::vector<visual::LabelView>(statusViews,statusViews+statusCount);
 auto threats=std::vector<Threat>(snap.threats,snap.threats+snap.threatCount);auto crates=std::vector<CrateMark>(snap.crates,snap.crates+snap.crateCount);
 std::array<external::sidebar::Frame,4> shapes; // immutable sidebar SHP pixels
 for(int i=0;i<4;++i)if(idleTabs&(1u<<i))shapes[i]=sidebarShapes.frames[i];
 auto draw=[=,labelViews=std::move(labels),enemyViews=std::move(enemies),statusViews=std::move(statuses),formation=formationhost::drawing(),used=snap.used,capacity=snap.capacity,toast=toast,labelScale=labelScalePercent](Graphics&g) mutable {
 g.SetSmoothingMode(SmoothingModeAntiAlias);g.ScaleTransform(view.x,view.y);g.TranslateTransform(-float(left),-float(up));
  for(const auto&b:opaquePanels)g.SetClip(RectF(float(b.left),float(b.top),float(b.right-b.left),float(b.bottom-b.top)),CombineModeExclude);
  if(idleTabs&&tabPhase){auto state=g.Save();g.SetInterpolationMode(InterpolationModeNearestNeighbor);g.SetPixelOffsetMode(PixelOffsetModeHalf);
   for(int i=0;i<4;++i)if(idleTabs&(1u<<i)){auto&f=shapes[i];Bitmap shape(f.width,f.height,f.width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(f.pixels.data()));g.DrawImage(&shape,RectF(f.x*sx,f.y*sy,f.width*sx,f.height*sy));}g.Restore(state);
  }
  for(const auto&b:nativeBadges){SolidBrush tag(Color(230,97,67,14));g.FillRectangle(&tag,b.x*sx,b.y*sy,b.w*sx,b.h*sy);
   std::wstring numbers;for(int i=0;i<construction::Capacity;++i)if(b.positions&(1<<i))numbers+=std::to_wstring(i+1);
   drawText(g,numbers,b.x*sx+2,b.y*sy,std::max(8.f,10.f*sy),Color(255,255,239,158));}
  auto enemyClip=g.Save();g.SetClip(RectF(0,0,enemyFrame.width*sx,enemyFrame.height*sy),CombineModeIntersect);
  for(int i=0;i<enemyCount;++i){const auto&v=enemyViews[i];if(external::enemies::onScreen(v.x,v.y,enemyFrame.width,enemyFrame.height))drawEnemyMarker(g,enemyFrame.items[i],v,sx,sy);}g.Restore(enemyClip);
  Pen flightPen(Color(210,130,235,160),1.5f);flightPen.SetEndCap(LineCapArrowAnchor);for(auto l:airLines){g.DrawLine(&flightPen,float(l.x)*sx,float(l.y)*sy,float(l.tx)*sx,float(l.ty)*sy);g.DrawEllipse(&flightPen,float(l.tx)*sx-3,float(l.ty)*sy-3,6.f,6.f);}
  Pen threatPen(Color(185,245,114,223),1.6f);threatPen.SetEndCap(LineCapArrowAnchor);
  for(int i=0;i<threats.size();++i){auto&t=threats[i];float x=t.x*sx,y=t.y*sy,tx=t.tx*sx,ty=t.ty*sy;g.DrawLine(&threatPen,x,y,tx,ty);g.DrawEllipse(&threatPen,tx-7,ty-7,14.f,14.f);}
  Pen cratePen(Color(245,255,205,82),2.f);
  for(int i=0;i<crates.size();++i){auto&c=crates[i];float x=c.x*sx,y=c.y*sy;g.DrawRectangle(&cratePen,x-10,y-9,20.f,18.f);std::wstring text=c.preview<0?L"随机箱":std::wstring(L"预设 · ")+crateName(c.preview);drawText(g,text,x-text.size()*6,y-29,12,Color(255,255,216,117));}
  auto statusClip=g.Save();g.SetClip(RectF(0,0,float(statusFrame.width),float(statusFrame.height)),CombineModeIntersect);
  for(int i=0;i<statusCount;++i)drawUnitStatus(g,statusFrame.items[i],statusViews[i],spiderPhase);g.Restore(statusClip);
  for(int i=0;i<labelCount;++i)drawLabel(g,labelViews[i],sx,sy,labelScale);
  for(const auto&b:passengerBubbles)drawPassengerBubble(g,b);
  formationhost::draw(g,formation);
  float y=top;
  if(summary){std::wstring text=L"载员 "+std::to_wstring(used)+L" / "+std::to_wstring(capacity)+L" · 剩余 "+std::to_wstring(std::max(0,capacity-used));SolidBrush bg(Color(150,13,21,31));g.FillRectangle(&bg,panelX,y,panelW,26.f);drawText(g,text,panelX+10,y+4,13,Color::White);y+=30;}
  if(hasToast){SolidBrush bg(Color(155,13,21,31));g.FillRectangle(&bg,panelX,y,panelW,27.f);drawText(g,toast,panelX+10,y+4,12,Color(255,220,233,247));y+=31;}
 };
 worldrender::submit({pid,snap.match,GetTickCount(),w,h,physicalW,physicalH,origin,std::move(draw)});

}
#include "frame_clock.h"
// Resolve on the UI/control thread; drawing only reads the cached target.
static void syncInformationRate(bool force=false){
 HWND fallback=ui;
#ifdef RA2_UNIFIED
 if(suite::parent())fallback=suite::parent();
#endif
 if(infoAdaptive)informationMonitor.update(game,fallback,GetTickCount(),force);
 const int rate=informationFPS();static int lastRate=0;static bool lastAdaptive=false;
 clockState(displayFrameRequested(),rate);
 if(force||rate!=lastRate||infoAdaptive!=lastAdaptive){
  lastRate=rate;lastAdaptive=infoAdaptive;++paintRevision;
  if(rateEdit)EnableWindow(rateEdit,!infoAdaptive);
  if(auto status=GetDlgItem(ui,9161))SetWindowTextW(status,(L"当前："+std::to_wstring(rate)+L" Hz").c_str());
  publishSuiteDisplay();
 }
}
static WORD binding(const char*id){for(auto&b:swBindings)if(b.id==id)return b.key;return 0;}
static std::vector<std::string> visibleSuperIDs;
static std::string selectedSuperID(){
 int row=int(SendMessageW(swList,CB_GETCURSEL,0,0));
 return row>=0&&row<int(visibleSuperIDs.size())?visibleSuperIDs[row]:std::string{};
}
static void fillSW(){
 if(SendMessageW(swList,CB_GETDROPPEDSTATE,0,0))return;
 static std::wstring old;std::wstring hash;
 for(int i=0;i<snap.swCount;++i){auto&s=snap.supers[i];hash+=wide(s.id)+L"|"+std::to_wstring(s.ready)+L"|"+std::to_wstring(s.frames/15)+L"|"+keyText(binding(s.id))+L";";}
 if(hash==old)return;old=hash;
 std::string keep=selectedSuperID();ControlUpdate guard;
 SendMessageW(swList,CB_RESETCONTENT,0,0);visibleSuperIDs.clear();int chosen=0;
 for(int i=0;i<snap.swCount;++i){auto&s=snap.supers[i];std::wstring name=(const wchar_t*)s.name;
  std::wstring line=name+L"  "+(s.ready?L"就绪":std::to_wstring((s.frames+14)/15)+L" 游戏秒")+L"  ["+keyText(binding(s.id))+L"]";
  int row=int(SendMessageW(swList,CB_ADDSTRING,0,(LPARAM)line.c_str()));
  if(row<0)continue;visibleSuperIDs.push_back(s.id);if(keep==s.id)chosen=row;
 }
 if(!visibleSuperIDs.empty())SendMessageW(swList,CB_SETCURSEL,chosen,0);
 const auto selected=selectedSuperID();
 SendMessageW(swHotkey,HKM_SETHOTKEY,selected.empty()?0:binding(selected.c_str()),0);
}
static void fillTech(bool force=false){
 wchar_t text[160]{};GetWindowTextW(techSearch,text,160);std::wstring search=text;
 for(auto&c:search)c=towlower(c);
 static uint32_t revisionFilled=~0u;static std::wstring lastFilter;
 if(!force&&revisionFilled==catalogueRevision&&lastFilter==search)return;
 revisionFilled=catalogueRevision;lastFilter=search;
 int oldPos=int(SendMessageW(techList,LB_GETCURSEL,0,0));
 std::string selectedID=(oldPos>=0&&oldPos<int(techIndices.size()))?
   typecatalog::idOf(savedTypes.types[techIndices[oldPos]]):std::string();
 SendMessageW(techList,WM_SETREDRAW,FALSE,0);SendMessageW(techList,LB_RESETCONTENT,0,0);
 techIndices.clear();int chosen=-1;
 for(size_t i=0;i<savedTypes.types.size();++i){
  auto&t=savedTypes.types[i];
  std::wstring name=(const wchar_t*)t.name;
  std::wstring line=(techAllowed(t)?L"[开] ":L"[关] ")+name+L"  ["+wide(t.id)+L"]";
  if(t.kind>=0)line+=L"  $"+std::to_wstring(t.cost);
  std::wstring low=line;for(auto&c:low)c=towlower(c);
  if(!search.empty()&&low.find(search)==std::wstring::npos)continue;
  int pos=int(SendMessageW(techList,LB_ADDSTRING,0,(LPARAM)line.c_str()));
  if(pos>=0){techIndices.push_back(int(i));if(typecatalog::idOf(t)==selectedID)chosen=pos;}
 }
 if(chosen>=0)SendMessageW(techList,LB_SETCURSEL,chosen,0);
 SendMessageW(techList,WM_SETREDRAW,TRUE,0);InvalidateRect(techList,nullptr,TRUE);
}
static int liveTypeIndex(const assist::Type& t){
 if(snap.status!=1)return -1;
 for(int i=0;i<snap.typeCount&&i<MaxTypes;++i)
  if(snap.types[i].kind==t.kind&&typecatalog::idOf(snap.types[i])==typecatalog::idOf(t))return i;
 return -1;
}

static std::wstring submitDiagnostic(){
 static const wchar_t* reasons[]={L"无提交层错误",L"对局代次不匹配",L"不支持的会话类型",L"命令桥未就绪",L"聊天框占用",L"非游戏线程",L"队列已满或帧预算耗尽",L"构造参数被拒绝",L"构造失败",L"游戏计时回调不可用"};
 int e=snap.lastSubmitError;
 return std::wstring(e>=0&&e<10?reasons[e]:L"未知提交错误")+L"；会话="+std::to_wstring(snap.session)+
  L"，已入队="+std::to_wstring(snap.queueSent)+L"，全局未提交累计="+std::to_wstring(snap.commandRejected)+
  L"，满="+std::to_wstring(snap.queueFull)+L"，计时失败="+std::to_wstring(snap.queueTimingFailure);
}
static void result(){if(!shared)return;uint32_t a=shared->ack;if(a==ack)return;MemoryBarrier();ack=a;commandLatency=sentAt?DWORD(GetTickCount()-sentAt):0;int r=shared->result,n=shared->count,c=shared->command;
 diag(L"ack command="+std::to_wstring(c)+L" result="+std::to_wstring(r)+L" count="+std::to_wstring(n)+L" ack="+std::to_wstring(a));
 if(c==DestructPlayer){
  std::wstring message;
  if(r>0&&n==1)message=L"自毁请求已入队："+destructPendingTarget+L"；实际执行以游戏为准。";
  else if(r==-42)message=std::wstring(L"自毁请求未入队：")+DestructForeignNotice;
  else if(r==-4||r==-7)message=L"自毁请求未入队：所选所属方已失效或对局已改变，请重新选择。";
  else if(r==-8)message=L"自毁请求未入队：请先关闭游戏聊天输入框。";
  else if(r==-6)message=L"自毁请求未入队：事件队列已满或提交失败，请稍后重新点击。";
  else if(r==-12)message=L"自毁请求未入队：当前游戏版本不支持此事件入口。";
  else message=L"自毁请求未入队；请确认当前对局与所选所属方仍有效。";
  if(r==-10||r==-15){critical(L"Destruct command exception result="+std::to_wstring(r));logBridgeHealth(true);}
  if(r==-6)critical(L"Destruct submission failed: "+submitDiagnostic());
  SetWindowTextW(GetDlgItem(ui,9186),message.c_str());notify(message);return;
 }
 if(c==AirPathMove){airPathResult(r,n,uint32_t(shared->arg),shared->requestToken);return;}
 if(c==StickyGuard&&r==-40){notify(L"当前会话不支持 Sticky 指令。");return;}
 if(selectedmode::index(c)>=0&&r==-40){notify(L"当前会话不支持任务模式指令。");return;}
 if(selectedmode::index(c)>=0&&r==-41){notify(L"所选单位仍处于 Sticky，普通任务模式不能保证解除。");return;}
 if(c==ProduceReserved||c==ProduceDeferred){constructionResult(r,n,uint32_t(shared->arg2),shared->requestToken);return;}
 // A newer selection (e.g. a double-tap upgrade) supersedes this ACK.
 // Do not overwrite it with a retry of the older single-tap request.
 if(c==SelectType&&commandQueue.has(SelectType)){
  diag(L"selection ACK superseded by newer pending selection");return;
 }
 const bool sameTypes=!typeIndexedCommand(c,shared->arg)||shared->requestTypeHash==typeListHash(snap.types,snap.typeCount);
 if(c==SelectType&&sameTypes&&snap.status==1&&
    delivery::retrySelection(r,inFlightRetries,GetTickCount(),sentAt,
      inFlightMatch,snap.match,foregroundGame(),snap.chat)){
  if(queueCommand(c,shared->arg,shared->arg2,inFlightMatch,1,60)){
   diag(L"retry transient selection after same-match stale ACK");return;
  }
 }
 // Retry only after an explicit expired/no-game receipt, never on missing ACK.
 // A command already submitted to the game must never be replayed speculatively.
 if((c==RevealMap||c==AutoBoard||c==Produce)&&(r==-7||r==-3||
    (c==Produce&&r==-6)||(c==RevealMap&&r==-14))&&inFlightRetries<(r==-14?20:c==Produce?6:2)&&
    sameTypes&&snap.status==1&&inFlightMatch==snap.match&&game&&pid){
  if(queueCommand(c,shared->arg,shared->arg2,inFlightMatch,inFlightRetries+1,r==-14?300:c==Produce?50:100)){
   notify(L"游戏暂未接收指令，已安排有限重试，无需重复按键。");return;
  }
 }
 if(c==RevealMap&&r<0&&extra.mapStrategy==3){autoRevealMatch=0;autoRevealRetry=GetTickCount()+2000;}
 if(c==RevealMap&&r==-22){notify(L"全图 CRC 原值镜像尚未就绪，本次未揭示；请查看实验功能状态。若本局已揭示，请在新对局揭示前开启。");return;}
 if(r==-10||r==-15){critical(L"command exception result="+std::to_wstring(r)+L" command="+std::to_wstring(c));logBridgeHealth(true);}
 if(r<0){const wchar_t*s=r==-10?L"游戏操作发生异常，已暂停发送指令；请重启游戏。详情见 logs/Runtime.log。":r==-15?L"无法确认本次装车条件，操作未执行；其他功能仍可用。详情见 logs/Runtime.log。":r==-3?L"当前没有可操作的对局。":r==-5?L"超级武器尚未就绪或当前不可用。":r==-6?L"指令未能提交，详细原因已记录到日志。":r==-7?L"指令已过期或对局已改变，本次未执行。":r==-8?L"请先关闭游戏聊天输入框。":r==-9?L"缺少可用的匹配工厂 / 供电，或已达建造上限。":r==-11?L"此游戏版本暂不支持科技生产，功能未启用。":r==-12?L"此游戏版本暂不支持该操作。":r==-22?L"喝彩或嘲讽尚在冷却，请稍后再试。":r==-23?L"本局已关闭嘲讽；快捷键不能更改其他玩家的接收设置。":r==-21?L"地图揭示未获允许或绘制方式冲突，本次未执行。":r==-20?L"当前版本不支持此操作。":r==-13?L"此操作仅支持单机。":r==-14?L"地图尚未完成初始化，稍后重试。":L"所选目标已失效，请刷新后重试。";if(r==-6)critical(L"command submission failed: "+submitDiagnostic());notify(s);return;}
 if(c==Repair)notify(n?L"已安排维修 "+std::to_wstring(n)+L" 座受损建筑。":L"没有需要维修的己方建筑。");
 else if(c==RevealMap){autoRevealDone=inFlightMatch;notify(L"已执行当前对局的地图揭示。");}
 else if(c==Cheer)notify(L"喝彩已提交。");
 else if(c==Taunt)notify(L"嘲讽已交给原生发送命令。");
 else if(c==ClearBeacons)notify(L"已清理己方信标。");
 else if(c==OpenTech){techAcknowledgedHash=shared->requestTypeHash;techEnabled=shared->arg!=0;notify(techEnabled?L"科技生产已启用，允许 "+std::to_wstring(n)+L" 项。请从本面板选择单位生产。":L"科技生产已关闭。");}
 else if(c==SwitchPlayer||c==RestorePlayer){pointerMatches.clear();commandQueue.clear();notify(c==RestorePlayer?L"已恢复原操控方。":L"已切换操控方；自动功能将按新操控方重新启动。");}
 else if(c==CancelTechProduction)notify(L"已请求取消生产。");
 else if(c==Produce)notify(L"生产请求已提交；是否接受以游戏生产队列为准。");
 else if(c==SelectSuper||c==SelectIron||c==SelectChrono)notify(L"已选择超级武器，请在地图上选择目标。");
 else if(c==SmartUnload)notify(n?L"已安排卸载 "+std::to_wstring(n)+L" 辆载具。":L"没有可卸载的已选载具。");
 else if(c==StickyGuard)notify(eventdelivery::isMultiplayer(snap.session)?L"Sticky 同步请求已入队："+std::to_wstring(n)+L" 个单位；联机退出尚未实现。":L"已切换 Sticky 驻守："+std::to_wstring(n)+L" 个单位；手动下令解除。");
 else if(selectedmode::index(c)>=0)notify(n?std::wstring(selectedmode::label(selectedmode::index(c)))+L"请求已入队："+std::to_wstring(n)+L" 个单位；实际执行以游戏为准。":L"没有可下令的己方地面单位。");
 else if(c==CancelAdvanced)notify(L"已取消所选单位的自动任务。");
 else if(c==AutoBoard){if(!snap.loadUnits)notify(L"请先框选要装车的步兵或载具。");else if(!snap.loadVehicles)notify(L"所选或附近没有可用的己方运输车辆。");else notify(L"装车指令已提交 "+std::to_wstring(n)+L" 个；已在途中 "+std::to_wstring(snap.loadWaiting)+L" 个；未分配 "+std::to_wstring(snap.loadUnassigned)+L" 个。"+(r==2?L"已自动排队，无需重复按键。":L""));}
 else notify(n?L"已选择 / 定位 "+std::to_wstring(n)+L" 个单位。":L"没有符合条件的己方单位。");
}
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
static void publishSuiteDisplay(){
 if(!ready)return;
 external::presentation::Profile profile;profile.motion=renderHighRefresh;profile.gpu=renderVulkanEnabled;profile.material=effectiveRenderInterpolation();profile.detail=effectiveRenderDetail();profile.precision=effectiveRenderPrecision();profile.palette=effectiveRenderPalette();profile.scale=effectiveRenderScale();
 profile.particles=renderParticleEffects;profile.lasers=renderLaserEffects;profile.voxelLighting=renderVoxelLighting;profile.spriteLighting=renderSpriteLighting;external::presentation::settings().publish(profile);
#ifdef RA2_UNIFIED
 suite::DisplayState state;
 state.values={extra.scrollRefreshMode,int(extra.enemyMarkers),int(SendMessageW(check[1],BM_GETCHECK,0,0)==BST_CHECKED),int(SendMessageW(check[0],BM_GETCHECK,0,0)==BST_CHECKED),int(SendMessageW(check[3],BM_GETCHECK,0,0)==BST_CHECKED),int(extra.frozen),int(extra.parasiteMarker),int(extra.crateBuffMarkers),labelScalePercent,fps,shroudpolicy::uiIndex(extra.mapStrategy),int(extra.passengerBubbles),int(extra.dangerPassengers),int(infoAdaptive)};
 for(int i=suite::RenderUpgrade;i<suite::DisplayOptionCount;++i){auto control=GetDlgItem(ui,suite::displaySpecs[i].id);if(!control)continue;const auto type=suite::displaySpecs[i].control;
  if(type==suite::DisplayControl::Check)state.values[i]=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
  else if(type==suite::DisplayControl::Choice)state.values[i]=int(SendMessageW(control,CB_GETCURSEL,0,0));
  else if(type==suite::DisplayControl::Hotkey)state.values[i]=int(SendMessageW(control,HKM_GETHOTKEY,0,0));
  else{BOOL ok=FALSE;state.values[i]=int(GetDlgItemInt(ui,suite::displaySpecs[i].id,&ok,FALSE));}
 }
 wchar_t renderText[1024]{};GetDlgItemTextW(ui,9303,renderText,1024);state.renderStatus=renderText;
 state.infoRate=informationFPS();
 state.assistEnabled=enabled;state.refreshStatus=shared?shared->scrollRefreshStatus:0;
 state.rateMilli=shared?int(shared->scrollRefreshRateMilli):0;state.gapUs=shared?int(shared->scrollRefreshMaxGapUs):0;
 suite::publishDisplaySettings(state);
#endif
}
static void updateModeStatus(DWORD now){
 int phase=!enabled?1:!game?2:snap.status<0?3:(snap.status!=1||(shared&&commandMatch()&&snap.commandStatus!=1))?4:0;
 static int oldPhase=-1;static DWORD last; if(phase==oldPhase&&DWORD(now-last)<200)return;oldPhase=phase;last=now;publishSuiteDisplay();
 auto riskText=[&](bool wanted,int state)->std::wstring{if(state==-2)return L"启用或关闭失败，请查看日志";if(state==-1)return L"不支持此游戏版本";if(!wanted)return state==1?L"等待恢复":L"关闭";return phase?L"等待游戏就绪":state==1?L"已启用（实验性）":L"等待生效";};
 std::wstring enhancement=L"高危 · 科技加强：";
 if(shared&&shared->techEnhanceStatus<0)enhancement+=shared->techEnhanceCount?L"仍有未还原字段（"+std::to_wstring(shared->techEnhanceCount)+L" 项），请查看日志":L"字段不兼容或已被修改，请查看日志";
 else if(!extra.techEnhance)enhancement+=L"关闭";
 else if(!shared||phase)enhancement+=L"等待游戏就绪";
 else if(shared->techEnhanceStatus==1)enhancement+=L"已启用（"+std::to_wstring(shared->techEnhanceCount)+L" 项）";
 else enhancement+=L"等待生效";
 SetWindowTextW(GetDlgItem(ui,9098),enhancement.c_str());
 std::wstring riskStatus=L"高危 · 随处建造："+riskText(buildAnywhereMode,snap.placementStatus);
 std::wstring airStatus=L"飞机路径点：";airStatus+=!(extra.airWaypoints||extra.nativeAirWaypoints)?L"关闭":phase?L"等待游戏就绪":extra.airWaypoints||extra.airPathKey||!airway::native()?L"兼容模式":snap.nativeAirStatus==-2?L"启用或关闭失败，请查看日志":snap.nativeAirStatus==-1?L"不支持此游戏版本":snap.nativeAirStatus==3?L"高危 · 原生下单":snap.nativeAirStatus==4?L"高危 · 完整原生":snap.nativeAirStatus==1?L"高危 · 原生模式":L"等待生效";
 SetWindowTextW(GetDlgItem(ui,866),airStatus.c_str());
 SetWindowTextW(GetDlgItem(ui,807),riskStatus.c_str());
 std::wstring renderText=(extra.mapStrategy!=0)?L"高危 · 地图显示：":L"地图显示：";renderText+=extra.mapStrategy==3?(autoRevealDone&&autoRevealDone==commandMatch()?L"自动地图全开已执行":L"等待自动地图全开"):snap.renderRevealStatus==-2?L"启用或关闭失败，请查看日志":!extra.renderReveal?L"关闭":phase?L"等待游戏就绪":snap.renderRevealStatus==1?L"已启用":L"等待生效";
 SetWindowTextW(GetDlgItem(ui,809),renderText.c_str());
 {int z=shared?shared->viewZoomStatus:viewzoom::Waiting;std::wstring text=L"视角缩放：";
  if(!shared)text+=extra.viewZoomEnabled?L"等待连接":L"关闭";
  else switch(z){case viewzoom::Off:text+=L"100%";break;case viewzoom::Applied:text+=std::to_wstring(shared->viewZoomApplied)+L"% · "+std::to_wstring(shared->viewZoomWidth)+L" × "+std::to_wstring(shared->viewZoomHeight);break;
   case viewzoom::Renderer:text+=L"需要带 Zoom 接口的 CnC-DDraw";break;case viewzoom::Native:text+=L"游戏版本不兼容，或游戏使用原生窗口模式";break;
   case viewzoom::Dimensions:text+=L"目标解析度超出范围（640×480 至 8192×8192）";break;
   case viewzoom::ResizeFailed:text+=L"切换失败，已恢复上一次倍率";break;case viewzoom::RestoreFailed:text+=L"恢复失败，请重新启动游戏";break;
   case viewzoom::Fault:text+=L"原生切换异常，请重新启动游戏";break;
   case viewzoom::Presentation:text+=L"存在黑边或渲染器改变视窗大小；请用等比例普通窗口模式";break;
   default:text+=L"等待游戏前台就绪 / 松开鼠标";break;}
  SetWindowTextW(GetDlgItem(ui,9093),text.c_str());
 }
 SetWindowTextW(GetDlgItem(ui,888),!extra.scrollRefreshMode?L"高刷策略：仅光标（沿用光标页设置）":shared&&shared->scrollRefreshStatus<0?L"高刷策略：接口不兼容，保持原版画面刷新":shared&&shared->scrollRefreshStatus==1?(extra.scrollRefreshMode==3?L"高刷策略：强制高刷 · 实验性功能已启用":extra.scrollRefreshMode==2?L"高刷策略：全部 · 实验性功能已启用":L"高刷策略：光标＋拖屏 · 实验性功能已启用"):L"高刷策略：等待游戏前台就绪");
 if(shared&&shared->scrollRefreshStatus==1&&shared->scrollRefreshRateMilli){wchar_t label[160];swprintf(label,160,L"%s · 绘制调用 %.1f Hz（非屏幕 FPS） · 最长间隔 %.1f ms",extra.scrollRefreshMode==3?L"强制高刷":extra.scrollRefreshMode==2?L"全部（实验性）":L"光标＋拖屏（实验性）",shared->scrollRefreshRateMilli/1000.0,shared->scrollRefreshMaxGapUs/1000.0);SetWindowTextW(GetDlgItem(ui,888),label);}
 SetWindowTextW(GetDlgItem(ui,9184),!extra.lowDamageFilter?L"低伤害过滤：关闭":shared&&shared->lowDamageStatus==1?L"高危 · 低伤害过滤：原生索敌及工具自动命令":shared&&shared->lowDamageStatus==2?L"低伤害过滤：仅工具自动命令":L"低伤害过滤：等待就绪或补丁冲突");
 {
  const bool fresh=shared&&commandFresh(now);
  auto stateText=[&](bool requested,int state){return !fresh?(requested?L"等待对局及连接就绪":L"关闭"):
   state<0?L"不可用，请检查游戏版本或功能冲突":state==2?L"正在处理待发事件并恢复":
   state==1?(requested?L"已启用（实验性）":L"正在关闭"):requested?L"等待适配启用":L"关闭";};
  std::wstring network=L"高危 · 事件所属方保留：";if(extra.dataControlRequested()){network+=L"控制标记模式下不请求启用";if(shared&&shared->networkHouseStatus)network+=L"（旧状态待还原或检查）";}else network+=stateText(extra.networkHouseRequested(),shared?shared->networkHouseStatus:0);
  SetWindowTextW(GetDlgItem(ui,9188),network.c_str());
  std::wstring sell=L"高危 · 原生变卖光标：";
  if(fresh&&extra.nativeSellCursor&&eventdelivery::isMultiplayer(incoming.session)&&!networkHouseReady()&&shared->nativeSellCursorStatus==0)sell+=L"等待事件所属方保留已启用";
  else sell+=stateText(extra.nativeSellCursor,shared?shared->nativeSellCursorStatus:0);
  SetWindowTextW(GetDlgItem(ui,9190),sell.c_str());
  std::wstring mega=L"高危 · 原生成功入口观察：";mega+=stateText(extra.nativeMegaGateway,shared?shared->nativeMegaGatewayStatus:0);
  SetWindowTextW(GetDlgItem(ui,9192),mega.c_str());
  std::wstring owner=L"高危 · 控制命令所属方：";
  if(extra.dataControlRequested())owner+=L"控制标记模式下不请求启用";
  else if(extra.controlCommandOwner&&!extra.controlAll)owner+=L"等待开启跨玩家操控";
  else if(fresh&&extra.commandOwnerRequested()&&eventdelivery::isMultiplayer(incoming.session)&&!networkHouseReady())owner+=L"等待事件所属方保留已启用";
  else owner+=stateText(extra.commandOwnerRequested(),shared?shared->controlCommandOwnerStatus:0);
  SetWindowTextW(GetDlgItem(ui,9194),owner.c_str());
  const int cs=shared?shared->crcMapShadowStatus:0;std::wstring crc=L"高危 · 全图 CRC 原值镜像：";
  if(!fresh)crc+=extra.crcMapShadow?L"等待新对局及连接":L"关闭";
  else if(cs==-4||cs==-3)crc+=L"原值不可信，请在新对局揭示前开启";
  else if(cs==-5)crc+=L"仅支持联机的自动地图全开";
  else if(cs==-6)crc+=L"暂不支持Ares/Phobos扩展";
  else if(cs<0)crc+=L"站点不兼容或恢复冲突";
  else if(cs==1){crc+=extra.crcMapShadow?L"实验已启用":L"正在关闭";crc+=L" · 跟踪"+std::to_wstring(shared->crcMapShadowTracked)+L"方 · 替代"+std::to_wstring(shared->crcMapShadowSubstitutions)+L"次";}
  else crc+=extra.crcMapShadow?L"等待适配":L"关闭";
  SetWindowTextW(GetDlgItem(ui,9197),crc.c_str());
 }
 SetWindowTextW(GetDlgItem(ui,813),(L"步兵强制进入建筑（暂不可用）："+(extra.c4Test&&snap.c4TestStatus==1&&!phase&&infantryEntryResultMatch==commandMatch()&&!infantryEntryResult.empty()?infantryEntryResult:riskText(extra.c4Test,snap.c4TestStatus))).c_str());
 std::wstring progress=snap.techOpen==2?L"经典兼容 · 生产进度：":L"生产进度：";uint32_t owner=0;
 if(techMode&&commandFresh(now)&&externalReader.readFresh(0xA83D4C,&owner,4)&&owner>=0x10000){
  const int offsets[]={0x53BC,0x53CC,0x53B0,0x53B4,0x53B8,0x53AC};const wchar_t* names[]={L"建筑",L"防御",L"步兵",L"战车",L"舰船",L"飞机"};
  for(int j=0;j<6;++j){uint32_t f=0,p=0,own=0;int step=0;bool valid=externalReader.readFresh(owner+offsets[j],&f,4);
   progress+=names[j];progress+=L" ";
   if(valid&&f>=0x10000&&externalReader.readFresh(f+0x6C,&own,4)&&own==owner&&externalReader.readFresh(f+0x58,&p,4)&&externalReader.readFresh(f+0x24,&step,4))progress+=p?std::to_wstring(std::clamp(step,0,54)*100/54)+L"%":L"空闲";
   else progress+=L"—";progress+=(j==1?L"\n":L"   ");
  }
 }else progress+=L"未启用 / 等待游戏";
 SetWindowTextW(GetDlgItem(ui,815),progress.c_str());
 const wchar_t*phaseText=phase==1?L"已暂停":phase==2?L"未连接":phase==3?L"暂不可用":commandMatch()?L"等待游戏恢复":L"正在读取对局";
 std::wstring nt=phase?phaseText:snap.nativeHookStatus<0?L"战术暂不可用，相关功能已停用":L"移动任务 "+std::to_wstring(snap.advancedUnits)+L" · 待处理乘员 "+std::to_wstring(snap.unloadPending)+L" · 逃生跟踪 "+std::to_wstring(snap.ejectWatched);
 if(!phase&&commandQueue.size)nt+=L" | 待处理命令 "+std::to_wstring(commandQueue.size);
 if(!phase&&magnetronMode)nt+=L" | 磁电攻击 "+std::to_wstring(snap.magnetShots)+
   L" · 未确认 "+std::to_wstring(snap.magnetUnverified);
 if(snap.hookConflicts>0)nt+=L" · 检测到功能冲突，请查看日志";
 if(shared&&snap.lastSubmitError)nt+=L" · 部分指令未执行，请查看日志";
 static std::wstring oldNativeText;if(nt!=oldNativeText){oldNativeText=nt;SetWindowTextW(nativeStatus,nt.c_str());}
 bool csRequested=(csMode&&hookAutoCS)||csHoldEnabled;
 std::wstring csText=L"自动 CS："+std::wstring(featureavailability::AutoCS?(csMode?(hookAutoCS?L"开":L"未允许"):L"关"):L"暂不可用")+L" · 按键 CS："+(csHoldEnabled?(LOBYTE(csHoldKey)?L"开":L"仅鼠标"):L"关");
 if(phase)csText+=L" · "+std::wstring(phaseText);else if(csRequested)csText+=snap.csHookStatus<0?L" · 暂不可用":snap.csHookStatus==1?L" · 已就绪":L" · 等待对局";
 static std::wstring oldCSText;if(csText!=oldCSText){oldCSText=csText;SetWindowTextW(csStatus,csText.c_str());}
 std::wstring oreText=oreMode?L"排队 "+std::to_wstring(snap.oreQueued)+L" · 进场 "+std::to_wstring(snap.oreDocking):L"自动卸矿已关闭";
 if(oreMode){oreText+=L" · 完成卸矿 "+std::to_wstring(snap.oreDelivered)+
   L" · 未确认 "+std::to_wstring(snap.oreUnverified);}
 if(phase)oreText+=L" · "+std::wstring(phaseText);else if(snap.oreManual)oreText+=L" · 手动接管 "+std::to_wstring(snap.oreManual);
 static std::wstring oldOreText;if(oreText!=oldOreText){oldOreText=oreText;SetWindowTextW(oreStatus,oreText.c_str());}
}
static void updatePlayerTestUI(bool force=false){
 if(!playertest::Available)return;
 const DWORD now=GetTickCount();
 const bool ready=enabled&&game&&pid&&shared&&commandFresh(now)&&!incoming.chat;
 const bool idle=ready&&shared->request==shared->ack&&shared->ack==ack&&!commandQueue.size;
 const int row=int(SendDlgItemMessageW(ui,862,CB_GETCURSEL,0,0));
 const bool cached=ready&&playerChoicesTick&&DWORD(now-playerChoicesTick)<500&&playerChoicesPid==pid&&playerChoicesMatch==commandMatch();
 const bool cachedAllowed=row>=0&&row<playerChoiceCount&&(!eventdelivery::isMultiplayer(incoming.session)||(playerChoicesCurrent>=0&&(playerChoices[row].index==playerChoicesCurrent||networkHouseReady())));
 EnableWindow(GetDlgItem(ui,862),idle&&cached&&playerChoiceCount>0);
 EnableWindow(GetDlgItem(ui,9185),idle&&cached&&cachedAllowed);
 if(!ready){playerChoicesTick=0;return;}
 static DWORD last=0;if(!force&&DWORD(now-last)<200)return;last=now;
 playerChoicesTick=0;EnableWindow(GetDlgItem(ui,862),FALSE);EnableWindow(GetDlgItem(ui,9185),FALSE);
 PlayerTestInfo list[32]{};int count=0,current=-1,status=0;uint32_t epoch=0;
 long before=shared->seq;if(before&1)return;MemoryBarrier();count=shared->playerCount;
 if(count<0||count>32)return;memcpy(list,shared->players,size_t(count)*sizeof(list[0]));current=shared->playerCurrent;status=shared->playerControlStatus;epoch=shared->data.match;
 MemoryBarrier();if(before!=shared->seq)return;
 bool changed=count!=playerChoiceCount||current!=playerChoicesCurrent||pid!=playerChoicesPid||epoch!=playerChoicesMatch||memcmp(list,playerChoices,size_t(count)*sizeof(list[0]));
 if(changed){int old=int(SendDlgItemMessageW(ui,862,CB_GETCURSEL,0,0));
  PlayerTestInfo selected{};if(pid==playerChoicesPid&&epoch==playerChoicesMatch&&old>=0&&old<playerChoiceCount)selected=playerChoices[old];
  SendDlgItemMessageW(ui,862,CB_RESETCONTENT,0,0);int chosen=-1;
  for(int i=0;i<count;++i){list[i].name[20]=0;std::wstring label=L"所属方 #"+std::to_wstring(list[i].index)+L" · "+std::wstring((wchar_t*)list[i].name)+L" · "+(list[i].human?L"人类":L"AI")+(list[i].index==current?L" · 当前本方":L"");
   SendDlgItemMessageW(ui,862,CB_ADDSTRING,0,(LPARAM)label.c_str());if(list[i].pointer==selected.pointer&&list[i].id==selected.id&&list[i].index==selected.index)chosen=i;}
  memcpy(playerChoices,list,sizeof list);playerChoiceCount=count;playerChoicesMatch=epoch;SendDlgItemMessageW(ui,862,CB_SETCURSEL,chosen,0);
 }
 playerChoicesTick=now;playerChoicesPid=pid;playerChoicesCurrent=current;
 const int selected=int(SendDlgItemMessageW(ui,862,CB_GETCURSEL,0,0));
 const bool foreign=selected>=0&&selected<count&&eventdelivery::isMultiplayer(incoming.session)&&(current<0||(list[selected].index!=current&&!networkHouseReady()));
 EnableWindow(GetDlgItem(ui,862),idle&&epoch==commandMatch()&&count>0);
 EnableWindow(GetDlgItem(ui,9185),idle&&epoch==commandMatch()&&selected>=0&&selected<count&&!foreign);
 if(foreign){SetWindowTextW(GetDlgItem(ui,9186),DestructForeignNotice);destructForeignNoticeShown=true;}
 else if(destructForeignNoticeShown){SetWindowTextW(GetDlgItem(ui,9186),L"自毁：所选所属方可提交实验请求；已入队不代表已自毁。");destructForeignNoticeShown=false;}
 std::wstring label=extra.controlImplementation==1?L"高危 · 原生控制判定 Hook：":L"高危 · 控制标记：";
 label+=status<0?L"应用或还原未完成，请检查日志":!extra.controlAll?(status?L"等待还原":L"关闭"):status==int(extra.controlRequest())?(status==2?L"标记已应用，联机效果未验证":L"已启用，联机效果未验证"):L"等待切换完成";
 label+=L" · 当前玩家 "+std::to_wstring(current+1);SetWindowTextW(GetDlgItem(ui,865),label.c_str());
}
#include "shift_click_host.h"
#include "command_cursor_host.h"
// Read-only pinning has no dependency on the game command queue. Retain the
// same key validation and single/double-press routing as normal shortcuts.
static void dispatchReadOnlyWatchKeys(const hotkeyroute::Group& group){
 for(int id:group.actions)if(id>=1730&&id<=1732)watchhost::pin(extra.watchKeys[id-1730]);
}
static bool readOnlyWatchHotkey(WPARAM id,LPARAM event){
 if(enabled)return false;
 if(desiredHotkeyMode()!=1||id<1730||id>1732)return true;
 const auto* group=hotkeyRegistry.find(int(id));
 WORD key=MAKEWORD(HIWORD(event),((LOWORD(event)&MOD_SHIFT)?HOTKEYF_SHIFT:0)|((LOWORD(event)&MOD_CONTROL)?HOTKEYF_CONTROL:0)|((LOWORD(event)&MOD_ALT)?HOTKEYF_ALT:0));
 if(!group||hotkeyroute::canonical(key)!=group->key)return true;
 hotkeyroute::Group selected;
 if(hotkeyRegistry.press(int(id),GetTickCount(),displayMatch(),selected))dispatchReadOnlyWatchKeys(selected);
 return true;
}
static void serviceReadOnlyWatchKeys(){
 bool active=desiredHotkeyMode()==1;uint32_t match=displayMatch();
 if(!active){hotkeyRegistry.cancel();return;}
 uint8_t down[256]{};for(unsigned key=0;key<256;++key)down[key]=(GetAsyncKeyState(int(key))&0x8000)!=0;
 // Registration synchronizes held keys. The registry also synchronizes its
 // first sample/epoch change without erasing a valid WM_HOTKEY already seen.
 hotkeyRegistry.sample(down,GetTickCount(),match,[](const hotkeyroute::Group& group){dispatchReadOnlyWatchKeys(group);});
 hotkeyroute::Group selected;
 while(hotkeyRegistry.expired(GetTickCount(),match,selected))dispatchReadOnlyWatchKeys(selected);
}
// Stop command work while preserving the shared read-only session and pins.
static void pauseAssistControls(){
 if(!secondaryDisplayRequested()){detach();clockState(false,informationFPS());registerKeys();setStatus(L"已暂停。");return;}
 commandQueue.clear();sentAt=0;inFlightMatch=0;inFlightRetries=0;hotkeyPending.clear();pointerMatches.clear();
 resetConstruction();resetAirPath();releaseBridge();
 worldrender::invalidate();overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();hideConstruction();
 minimap::hidePrimary();watchwindows::hidePrimary();clearPassengerDisplay();
 sampleExternal(GetTickCount());registerKeys();clockState(displayFrameRequested(),informationFPS());
 setStatus(L"战术控制已暂停，副屏信息继续显示。");
}
static bool serviceReadOnlyDisplays(DWORD now){
 if(enabled)return false;
 if(shared)releaseBridge();
 sampleExternal(now);serviceReadOnlyWatchKeys();clockState(displayFrameRequested(),informationFPS());
 if(snap.renderStatus!=1){minimap::hide();watchwindows::hide();}
 return true;
}
static void tick(){reapBridges();logBridgeHealth();DWORD now=GetTickCount();static DWORD lastControl;if(lastControl&&DWORD(now-lastControl)<8)return;lastControl=now;syncSettingDependencies();syncInformationRate();if(!displayFrameRequested()){minimap::hide();watchwindows::hide();watchhost::suspend();}else if(!secondaryDisplayAvailable()){minimap::hideSecondary();watchwindows::hideSecondary();}updateModeStatus(now);updatePlayerTestUI();if(registeredHotkeyMode!=desiredHotkeyMode())registerKeys();if(!enabled&&!secondaryDisplayRequested()){if(pid||shared)detach();clockState(false,informationFPS());return;}
 if(pid&&DWORD(now-lastProcessCheck)>=250){lastProcessCheck=now;
  if(!externalReader.alive()){detach();lastFind=now-2000;}
  else {DWORD nextThread=0;bool authoritative=false;
   HWND next=external::discovery::resolveYrppWindow(externalReader,pid,boundGameAlive()?game:nullptr,&nextThread,&authoritative);
   if(next&&(next!=game||nextThread!=gameThread)){
    critical(L"game window rebound pid="+std::to_wstring(pid)+L" thread="+std::to_wstring(nextThread));
    releaseBridge();commandQueue.clear();sentAt=0;inFlightMatch=0;resetConstruction();resetAirPath();
    game=next;gameThread=nextThread;gameWindowFromYrpp=authoritative;bridgeRetryAt=0;bridgeBlocked=false;bridgeAttempts=0;
   }else if(!next){game=nullptr;lastFind=now-2000;}
  }
 }
 if(!game){commandcursor::clear();minimap::hide();watchwindows::hide();
  clockState(false,informationFPS());
  if(pid&&externalReader.valid()){
   // Read-only attachment is independent of the window/WH_GETMESSAGE bridge.
   // Never send commands while HWND/thread ID are unavailable.
   if(!externalReader.alive()){detach();lastFind=now-2000;}
   else {
    sampleExternal(now);
    if(DWORD(now-lastFind)>=2000){lastFind=now;
     const auto status=externalSampler.probe();
     setStatus(std::wstring(L"只读 PID ")+std::to_wstring(pid)+L" · "+
       external::layout::description(status.stage)+L" · Win32="+
       std::to_wstring(externalReader.lastReadError)+L"（无窗口，不发送指令）");
     const auto found=external::discovery::scan(pid);
     DWORD nextThread=0;
     HWND resolved=external::discovery::resolveYrppWindow(externalReader,pid,found.best.window,&nextThread,&gameWindowFromYrpp);
     if(resolved){
      if(shared&&gameThread!=nextThread){releaseBridge();commandQueue.clear();sentAt=0;inFlightMatch=0;}
      game=resolved;gameThread=nextThread;
      critical(L"read-only PID acquired game window "+std::to_wstring(pid));
     }
    }
    return;
   }
  }
  if(DWORD(now-lastFind)>=2000){lastFind=now;attach();}
  return;
 }
 if(serviceReadOnlyDisplays(now))return;
 sampleExternal(now);uint32_t configured=flags();
 bool needsBridge=buildTimeTooltipEnabled||(externalData&&externalData->status==1)||techMode||extra.techEnhance||extra.dodgeEnabled||extra.missileDefense||commandQueue.size||hasSelection||externalSampler.flashCount||(configured&(AutoCS|AdvancedInput|MagnetronInput|AutoOreReturn|CustomCS|BuildAnywhere|AutoRepair|EjectAssist|SpecialClickFlags));
 if(needsBridge&&!shared)ensureBridge();
 now=GetTickCount(); // ensureBridge may have created attachAt after tick entry.
 if(shared){
  shared->heartbeat=now;shared->foreground=foregroundGame();shared->flags=configured;syncAdditional();shared->scanMs=8;shared->repairMs=repairMs;
  gameinput::setFastTactical(hook,shared->dodgeEnabled||shared->dodgeActive||shared->scrollRefreshHz);
  DWORD wakePeriod=shared->foreground?16:(shared->dodgeEnabled||shared->dodgeActive||shared->missileEnabled||shared->missileHeld||((configured|shared->plannerTracking)&(AdvancedInput|MagnetronInput|AutoOreReturn|AirWaypoints|AntiTheft))?64:500);
  if(DWORD(now-lastPulse)>=wakePeriod)wakeGame();
  readSnapshot();schedulerDiagnostic();serviceSelection();result();serviceCommandQueue(GetTickCount());serviceCalculationTasks();servicePlanner();serviceCooperative();servicePointer();serviceCommandCursorHint();serviceHotkeyBatch();serviceConstruction();serviceAirPath();
  static renderpresentation::StatusCadence computeStatusCadence;
  gameinput::setFrameSampling(hook,enabled&&shared->computeApplied!=compute::All);
  const uint64_t computeStatusKey=uint64_t(shared->computeApplied)|(uint64_t(computeRequested)<<32)|(uint64_t(showDetails)<<63);
  if(ready&&GetDlgItem(ui,833)&&computeStatusCadence.due(computeStatusKey,now)){
  std::wstring text=L"当前生效：";for(int i=0;i<compute::Count;++i){if(i)text+=i%2?L" · ":L"\r\n";text+=compute::label(i);text+=compute::inside(shared->computeApplied,compute::Feature(i))?L"：游戏内":L"：工具箱";}
  if(shared->computeApplied!=computeRequested)text+=L"\r\n配置已修改，下一局生效。";
  if(showDetails)text+=L"\r\n游戏内计算耗时 "+std::to_wstring(shared->runtimePlannerMicros)+L" 微秒。";
  if(showDetails){const long seq=shared->computeFrameSeq;if(!(seq&1)){const int frame=shared->computeFrame;const uint32_t stamp=shared->computeFrameTick,serial=shared->computeFrameSerial;MemoryBarrier();if(seq==shared->computeFrameSeq)text+=L"\r\n统一决策样本：游戏帧 "+std::to_wstring(frame)+L" · 批次 "+std::to_wstring(serial)+L" · 年龄 "+std::to_wstring(DWORD(GetTickCount()-stamp))+L" ms · "+(gameinput::nativeFrameSampling(hook)?L"原生帧通知":L"帧号检测");}}
  static std::wstring previous;if(previous!=text){previous=text;SetDlgItemTextW(ui,833,text.c_str());}
 }
 now=GetTickCount(); // snapshot/queue services may have advanced lastData/sentAt.
  if(uninitializedBridgeTimedOut()){
   critical(L"uninitialized command bridge timeout; retry before any match is authenticated");logBridgeHealth(true);logLoadedGameModules();releaseBridge();
  }else if(lastData&&DWORD(now-lastData)>8000){
   // A live game's delayed callback is not a new match. Keep mapping, epoch,
   // outstanding serial and job state; never replay an unacknowledged request.
   wakeGame();
  }
 }
 updateModeStatus(now);
 // A responsive overlay is not proof that a command was consumed.
 // Re-wake the game queue and log the stall; never re-submit an unacknowledged order.
 if(shared&&shared->request!=shared->ack&&sentAt&&DWORD(now-sentAt)>1000&&
    DWORD(now-lastStallLog)>1500){
  lastStallLog=now;
  diag(L"command ack pending ms="+std::to_wstring(DWORD(now-sentAt))+L" command="+std::to_wstring(shared->command)+L" request="+std::to_wstring(shared->request)+L" ack="+std::to_wstring(shared->ack)+L" dataAgeMs="+std::to_wstring(lastData?DWORD(now-lastData):0));
  wakeGame(); // Only the game-side entry may acknowledge execution.
 }
 static int lastLoggedPatchMask=-1;
 static uint32_t lastQueueFull=0,lastTimingFaults=0,lastPlanDeferred=0,lastRejected=0,lastSent=0;
 if(diagEnabled&&lastData&&(snap.queueFull!=lastQueueFull||snap.queueTimingFailure!=lastTimingFaults||snap.planDeferred!=lastPlanDeferred||snap.commandRejected!=lastRejected||snap.queueSent!=lastSent)){
  diag(L"mega_queue status="+std::to_wstring(snap.megaQueueStatus)+L" ok="+
      std::to_wstring(snap.megaQueueAccepted)+L" rejected="+std::to_wstring(snap.megaQueueRejected));
 diag(L"outqueue sent="+std::to_wstring(snap.queueSent)+L" full="+std::to_wstring(snap.queueFull)+L" timingErrors="+std::to_wstring(snap.queueTimingFailure)+L" planDeferred="+std::to_wstring(snap.planDeferred)+L" queued="+std::to_wstring(snap.queuedEvents)+L" observedMission="+std::to_wstring(snap.observedEvents)+L" readStatus="+std::to_wstring(snap.readStatus)+L" bridgeStatus="+std::to_wstring(snap.bridgeStatus));
  lastQueueFull=snap.queueFull;lastTimingFaults=snap.queueTimingFailure;lastPlanDeferred=snap.planDeferred;lastRejected=snap.commandRejected;lastSent=snap.queueSent;
  diag(L"submit "+submitDiagnostic()+L" entryThread="+std::to_wstring(snap.entryThread)+L" entries="+std::to_wstring(snap.entryCalls)+L" reentrant="+std::to_wstring(snap.entryReentrant)+L" contended="+std::to_wstring(snap.entryContended)+L" wrongThread="+std::to_wstring(snap.entryWrongThread));
 }
 if(lastData&&snap.inputPatchMask!=lastLoggedPatchMask){lastLoggedPatchMask=snap.inputPatchMask;diag(L"hook patches installed mask="+std::to_wstring(lastLoggedPatchMask));}
 clockState(displayFrameRequested(),informationFPS());
 const bool passiveVisible=snap.renderStatus==1||passiveDisplayState()!=overlaydisplay::State::Hidden;
 if(!passiveVisible){overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();minimap::hide();watchwindows::hide();hideConstruction();}
 else if(!foregroundGame()||snap.chat){overlayRecovery.suspend();hideWorldOverlay();hideAircraftButton();minimap::hidePrimary();watchwindows::hidePrimary();hideConstruction();}
 if(snap.status==-2){setStatus(L"游戏操作异常，指令功能已暂停；信息显示仍可使用。");if(!passiveVisible)hideWorldOverlay();return;}
 if(snap.status==-1){setStatus(L"当前游戏版本不兼容，指令功能已暂停；信息显示仍可使用。");if(!passiveVisible)hideWorldOverlay();return;}
 if(snap.status!=1){
  if(int32_t(now-toastUntil)>=0){const auto p=externalSampler.probe();
   setStatus(L"正在读取当前对局；若长时间未恢复，请查看日志。");
  }
  if(!passiveVisible)hideWorldOverlay();return;
 }
 if(snap.bridgeStatus==1&&snap.readStatus!=1&&int32_t(now-toastUntil)>=0){
  const auto p=externalSampler.probe();
  setStatus(L"游戏已连接，正在更新单位信息。");
 }
 if(snap.match!=oldMatch){diag(L"match changed "+std::to_wstring(oldMatch)+L" -> "+std::to_wstring(snap.match));oldMatch=snap.match;notice=snap.noticeSerial;techEnabled=false;toastUntil=0;fillTech(true);typeTap.clear();fillTypeBindings(true);autoRevealMatch=autoRevealDone=0;}
 if(autoRevealPID!=pid){autoRevealPID=pid;autoRevealMatch=autoRevealDone=autoRevealRetry=0;}
 if(autoRevealNewMatch&&extra.experimentalReveal&&int32_t(now-autoRevealRetry)>=0&&commandFresh(now)&&snap.status==1&&snap.match&&autoRevealMatch!=commandMatch()){
  if(queueCommand(RevealMap,0,0,commandMatch()))autoRevealMatch=commandMatch();
 }
 notice=snap.noticeSerial;
 syncTypeConfig();
 static DWORD panelTick;if(DWORD(now-panelTick)<200)return;panelTick=now;
 fillSW();fillTech();fillTypeBindings();fillExtraTypes(false);
 if(int32_t(now-toastUntil)>=0&&snap.readStatus==1){
  if(!commandFresh(now)||commandQueue.size||(shared&&shared->request!=shared->ack)){setStatus(commandWaitReason());return;}
  auto ms=[](uint32_t us){wchar_t text[24];swprintf(text,24,L"%.2f",us/1000.0);return std::wstring(text);};
  std::wstring s=L"选中 "+std::to_wstring(snap.selected)+L" · 指令响应 "+std::to_wstring(commandLatency)+L" ms · 选兵响应 "+std::to_wstring(selectionLatency)+L" ms";
  if(showDetails)s+=L"\r\n处理 "+ms(snap.controlMicros)+L" · 读取 "+ms(snap.worldMicros)+L" · 显示 "+ms(snap.displayMicros)+L" · 规划 "+ms(plannerMicros)+L" ms";
  setStatus(s);
 }

}
struct PageControl {HWND hwnd;int page;uilayout::Rect rect;int visibleHeight;std::wstring help;};static std::vector<PageControl> pageControls;static int activePage=0;static HWND featureSearch;
static int pageBottom(){int bottom=uilayout::BodyTop;for(const auto&c:pageControls)if(c.page==activePage)bottom=std::max(bottom,c.rect.y+c.visibleHeight);return bottom+6;}
static unsigned panelDpi=96;static int scrollX=0;static bool arrangingPanel=false;
static int logical(int n){return MulDiv(n,96,int(panelDpi));}
static void layoutPanel(){
 if(!ui||arrangingPanel)return;arrangingPanel=true;
 RECT client{};GetClientRect(ui,&client);int height=logical(client.bottom),width=logical(client.right),maximum=uilayout::maxScroll(pageBottom(),height);scrollY=std::clamp(scrollY,0,maximum);scrollX=std::clamp(scrollX,0,std::max(0,uilayout::Width-width));
 auto px=[](int n){return modernui::scale(n,panelDpi);};HDWP batch=BeginDeferWindowPos(int(pageControls.size()));
 for(const auto&c:pageControls){int cid=GetDlgCtrlID(c.hwnd);if(cid>=910&&cid<=915)ShowWindow(c.hwnd,activePage==0?SW_SHOWNA:SW_HIDE);auto r=uilayout::visible(c.rect,c.page,height,width,scrollY);r.x-=scrollX;
  if(c.page>=0){int top=std::max(0,uilayout::BodyTop-r.y),bottom=std::min(c.visibleHeight,uilayout::bodyBottom(height)-r.y);bool shown=c.page==activePage&&bottom>top;
   HRGN region=CreateRectRgn(0,shown?px(top):0,px(r.w),shown?px(bottom):0);if(!SetWindowRgn(c.hwnd,region,FALSE))DeleteObject(region);
  }
  UINT flags=SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW;
  if(c.page>=0)flags|=c.page==activePage?SWP_SHOWWINDOW:SWP_HIDEWINDOW;
  if(batch)batch=DeferWindowPos(batch,c.hwnd,nullptr,px(r.x),px(r.y),px(r.w),px(r.h),flags);
  else SetWindowPos(c.hwnd,nullptr,px(r.x),px(r.y),px(r.w),px(r.h),flags);
 }
 if(!batch||!EndDeferWindowPos(batch))for(const auto&c:pageControls){int cid=GetDlgCtrlID(c.hwnd);if(cid>=910&&cid<=915)ShowWindow(c.hwnd,activePage==0?SW_SHOWNA:SW_HIDE);auto r=uilayout::visible(c.rect,c.page,height,width,scrollY);r.x-=scrollX;SetWindowPos(c.hwnd,nullptr,px(r.x),px(r.y),px(r.w),px(r.h),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS|SWP_NOREDRAW|(c.page<0?0:c.page==activePage?SWP_SHOWWINDOW:SWP_HIDEWINDOW));}
 int viewport=std::max(1,uilayout::bodyBottom(height)-uilayout::BodyTop);SCROLLINFO si{sizeof si,SIF_RANGE|SIF_PAGE|SIF_POS|SIF_DISABLENOSCROLL};si.nMin=0;si.nMax=maximum+viewport-1;si.nPage=viewport;si.nPos=scrollY;SetScrollInfo(ui,SB_VERT,&si,TRUE);
 si.nMax=uilayout::Width-1;si.nPage=std::max(1,width);si.nPos=scrollX;SetScrollInfo(ui,SB_HORZ,&si,TRUE);
 arrangingPanel=false;RedrawWindow(ui,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static void applyPanelDpi(unsigned dpi){
 dpi=modernui::panelDpi(dpi,false);if(dpi==panelDpi&&ready)return;panelDpi=dpi;
 HFONT next=modernui::makeFont(modernui::BodyPixels,panelDpi);if(next){for(const auto&c:pageControls){SendMessageW(c.hwnd,WM_SETFONT,(WPARAM)next,FALSE);if(GetPropW(c.hwnd,L"RA2.Heading.Font"))modernui::heading(c.hwnd,panelDpi);}auto old=font;font=next;if(old)DeleteObject(old);}
 layoutPanel();
}
static void scrollTo(int position){scrollY=position;layoutPanel();}
// Pack complete control rows into the five product categories.
// Row intervals keep labels/inputs aligned while removing old page whitespace.
#include "hover_help.h"
static void finalizeTacticalLayout(){
 std::vector<int> originalPages;for(const auto&c:pageControls)originalPages.push_back(c.page);
 int bottoms[suite::categories::Count];std::fill(std::begin(bottoms),std::end(bottoms),uilayout::BodyTop+12);
 for(int section:{0,1,2,5,3,4}){
  std::vector<size_t> items;for(size_t i=0;i<pageControls.size();++i)if(originalPages[i]==section)items.push_back(i);
  std::stable_sort(items.begin(),items.end(),[](size_t a,size_t b){return pageControls[a].rect.y<pageControls[b].rect.y;});
  for(size_t at=0;at<items.size();){size_t end=at+1;const int top=pageControls[items[at]].rect.y;int bottom=top+pageControls[items[at]].visibleHeight;
   while(end<items.size()&&pageControls[items[end]].rect.y<bottom){auto&c=pageControls[items[end++]];bottom=std::max(bottom,c.rect.y+c.visibleHeight);}
   std::vector<int> ids;std::wstring rowHelp;for(size_t i=at;i<end;++i){int id=GetDlgCtrlID(pageControls[items[i]].hwnd);if(id){ids.push_back(id);if(rowHelp.empty())rowHelp=controlHelp(id,L"");}}
   int category=suite::categories::row(section,ids.data(),unsigned(ids.size()));int&y=bottoms[category];
   for(size_t i=at;i<end;++i){auto&c=pageControls[items[i]];wchar_t label[512]{};GetWindowTextW(c.hwnd,label,512);if(c.help.empty())c.help=controlHelp(GetDlgCtrlID(c.hwnd),L"");if(c.help.empty())c.help=rowHelp.empty()?controlHelp(0,label):rowHelp;c.rect.y=y+c.rect.y-top;c.page=category;
#ifdef RA2_UNIFIED
    if(category==suite::categories::Video){c.page=-3;ShowWindow(c.hwnd,SW_HIDE);}
#endif
    modernui::tip(c.hwnd,c.help);
   }
   y+=bottom-top+14;at=end;
  }
 }
#ifdef RA2_UNIFIED
 for(auto&c:pageControls){int id=GetDlgCtrlID(c.hwnd);if(id==430||(id>=910&&id<=915)){c.page=-3;ShowWindow(c.hwnd,SW_HIDE);}}
#endif
 activePage=suite::categories::Control;scrollY=scrollX=0;layoutPanel();
}

static HWND placedControl(uilayout::Placement placement,const wchar_t*cls,const wchar_t*text,int id,DWORD style){
 if(!wcscmp(text,L"自动保存"))return HWND{};
 auto r=placement.rect;HWND c=CreateWindowExW((!wcscmp(cls,L"EDIT")||!wcscmp(cls,L"LISTBOX"))?WS_EX_CLIENTEDGE:0,cls,text,WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS|style,r.x,r.y,r.w,r.h,ui,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
 SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE);modernui::style(c);if(id==989||id==832||(id>=900&&id<=905))modernui::heading(c);pageControls.push_back({c,placement.page,r,!wcscmp(cls,L"COMBOBOX")?26:r.h});if(placement.page==-3)ShowWindow(c,SW_HIDE);return c;
}
static HWND control(const wchar_t*cls,const wchar_t*text,int x,int y,int w,int h,int id,DWORD style=0){return placedControl(uilayout::legacy(id,{x,y,w,h}),cls,text,id,style);}
static HWND pageControl(int page,const wchar_t*cls,const wchar_t*text,int x,int y,int w,int h,int id,DWORD style=0){return placedControl({page,{x,y+8,w,h}},cls,text,id,style);}
static void bindSuper(){std::string id=selectedSuperID();if(id.empty()){notify(L"进入对局并获得超武后即可绑定。");return;}WORD key=(WORD)SendMessageW(swHotkey,HKM_GETHOTKEY,0,0);bool found=false;for(auto&b:swBindings)if(b.id==id){b.key=key;found=true;}if(!found)swBindings.push_back({id,key});bool persisted=save();registerKeys();fillSW();if(!persisted)notify(L"快捷键已生效，但自动保存失败，请检查 Assist.ini 写入权限。");}
#include "type_bindings_ui.h"
#include "extra_settings_io.h"
#include "pointer_host.h"
#include "hotkey_host.h"
#include "auto_settings_ui.h"
#include "ui_dependencies.h"
static_assert(BridgeCloseTimer!=AutoSaveTimer&&BridgeCloseTimer!=1&&BridgeCloseTimer!=2,"distinct close cleanup timer");
static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp){
 if(bridgeClosePending){
  if(msg==WM_TIMER&&wp==BridgeCloseTimer){
   reapBridges();
   if(retiredBridges.empty()){KillTimer(w,BridgeCloseTimer);DestroyWindow(w);}
   return 0;
  }
  // Retired subscriptions keep the Runtime awake; stop all new Host work
  // while the sender restores its patches and acknowledges detach.
  if(msg==WM_TIMER||msg==WM_COMMAND||msg==WM_HOTKEY||msg==FrameMessage||msg==renderupgrade::ReadyMessage||
     msg==InputMessage||msg==ControlMessage||msg==DataMessage)return 0;
#ifdef RA2_UNIFIED
  if(msg==suite::ModuleEnable||msg==suite::ApplyDisplaySetting||
     msg==suite::RefreshRateChanged||msg==suite::DisplaySettingsChanged)return 0;
#endif
 }
#ifdef RA2_UNIFIED
 if(suite::layoutMessage(w,msg,lp))return 0;
 if(msg==suite::SelectCategory){int page=std::clamp(int(wp),0,int(suite::categories::Count)-1);if(activePage!=page)scrollY=scrollX=0;activePage=page;SetWindowTextW(GetDlgItem(w,989),suite::categories::name(page));layoutPanel();return 0;}
 if(msg==suite::RefreshRateChanged){syncRefreshRate();return 0;}
 if(msg==suite::ApplyDisplaySetting){
  if(!ready||!suite::validDisplayEdit(int(wp),int(lp)))return 0;
  const auto& spec=suite::displaySpecs[int(wp)];HWND control=GetDlgItem(w,spec.id);if(!control)return 0;
  int notification=BN_CLICKED;
  {ControlUpdate guard;
   if(spec.control==suite::DisplayControl::Check)SendMessageW(control,BM_SETCHECK,lp?BST_CHECKED:BST_UNCHECKED,0);
   else if(spec.control==suite::DisplayControl::Hotkey){SendMessageW(control,HKM_SETHOTKEY,lp,0);notification=EN_CHANGE;}
   else if(spec.control==suite::DisplayControl::Choice){SendMessageW(control,CB_SETCURSEL,lp,0);notification=CBN_SELCHANGE;}
   else {SetWindowTextW(control,std::to_wstring(int(lp)).c_str());notification=EN_CHANGE;}
  }
  // The value is already assigned. Dispatch directly to its owner: the styled
  // parent subclass treats a WM_COMMAND/BN_CLICKED as a physical toggle.
  proc(w,WM_COMMAND,MAKEWPARAM(spec.id,notification),(LPARAM)control);publishSuiteDisplay();return 0;
 }

 if(msg==suite::ModuleEnable){enabled=bool(wp);if(!enabled)pauseAssistControls();else{lastFind=0;registerKeys();setStatus(L"正在重新连接…");}suite::reportEnabled(2,enabled);return 0;}
#endif
 if(msg==modernui::DpiMessage){applyPanelDpi(unsigned(wp));return 0;}
 switch(msg){case WM_CREATE:{ui=w;panelDpi=modernui::windowDpi(w);
 placedControl({-1,{20,14,280,30}},L"STATIC",L"战术控制",989,0);
 placedControl({-1,{305,20,320,22}},L"STATIC",L"设置修改后自动保存",0,0);
 {const wchar_t* titles[]={L"键盘操作",L"键鼠操作",L"自动任务",L"显示设置",L"高级设置",L"单位清单"};for(int i:{0,1,2,5})placedControl({-1,{20+(i==5?3:i)*154,46,145,26}},L"BUTTON",titles[i],910+i,BS_PUSHBUTTON|WS_TABSTOP);}
 featureSearch=placedControl({-1,{20,80,470,28}},L"EDIT",L"",990,ES_AUTOHSCROLL|WS_TABSTOP);
 SendMessageW(featureSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"搜索功能，例如：装车、铁幕、CS");
 placedControl({-1,{505,80,120,28}},L"BUTTON",L"定位功能",991,BS_PUSHBUTTON|WS_TABSTOP);
#include "tactical_controls.h"
 EnableWindow(GetDlgItem(w,420),TRUE);
 statusLabel=control(L"STATIC",L"正在寻找游戏…",20,1370,605,44,430);control(L"BUTTON",L"暂停 / 连接",20,1420,200,28,431,BS_PUSHBUTTON|WS_TABSTOP);control(L"BUTTON",L"使用说明",235,1420,185,28,432,BS_PUSHBUTTON|WS_TABSTOP);control(L"BUTTON",L"退出",435,1420,190,28,433,BS_PUSHBUTTON|WS_TABSTOP);EnableWindow(unloadKeyEdit,TRUE);EnableWindow(GetDlgItem(w,453),TRUE);EnableWindow(hookChecks[1],TRUE);SetWindowTextW(hookChecks[1],L"智能卸载");finalizeTacticalLayout();applyPanelDpi(panelDpi);ready=true;syncSettingDependencies(true);fillTech(true);fillTypeBindings(true);SetTimer(w,1,250,nullptr);SetTimer(w,2,80,nullptr);registerKeys();return 0;}
 case WM_DISPLAYCHANGE:informationMonitor.invalidate();secondaryDisplayMonitor.invalidate();minimap::invalidateDisplays();watchwindows::invalidateDisplays();syncInformationRate(true);if(enabled||secondaryDisplayRequested())paint();return 0;
 case WM_SIZE:layoutPanel();return 0;
 case WM_DPICHANGED:{applyPanelDpi(HIWORD(wp));if(!(GetWindowLongPtrW(w,GWL_STYLE)&WS_CHILD)&&!GetPropW(w,L"RA2.Suite.Managed")){auto r=(RECT*)lp;SetWindowPos(w,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);}return 0;}
 case WM_HSCROLL:{SCROLLINFO si{sizeof si,SIF_ALL};GetScrollInfo(w,SB_HORZ,&si);switch(LOWORD(wp)){case SB_LINELEFT:scrollX-=28;break;case SB_LINERIGHT:scrollX+=28;break;case SB_PAGELEFT:scrollX-=int(si.nPage);break;case SB_PAGERIGHT:scrollX+=int(si.nPage);break;case SB_THUMBTRACK:case SB_THUMBPOSITION:scrollX=si.nTrackPos;break;}layoutPanel();return 0;}
 case modernui::FocusMessage:{for(const auto&c:pageControls)if(c.hwnd==(HWND)lp&&c.page>=0){int top=c.rect.y-scrollY,bottom=top+c.visibleHeight;RECT client{};GetClientRect(w,&client);if(top<uilayout::BodyTop)scrollTo(c.rect.y-uilayout::BodyTop);else if(bottom>uilayout::bodyBottom(logical(client.bottom)))scrollTo(c.rect.y+c.visibleHeight-uilayout::bodyBottom(logical(client.bottom)));break;}return 0;}
 case WM_VSCROLL:{SCROLLINFO si{sizeof si,SIF_ALL};GetScrollInfo(w,SB_VERT,&si);int next=scrollY;switch(LOWORD(wp)){case SB_LINEUP:next-=28;break;case SB_LINEDOWN:next+=28;break;case SB_PAGEUP:next-=int(si.nPage);break;case SB_PAGEDOWN:next+=int(si.nPage);break;case SB_THUMBTRACK:next=si.nTrackPos;break;default:return 0;}scrollTo(next);return 0;}
 case WM_MOUSEWHEEL:{int amount=pagewheel::delta(w,wp,84);if(amount)scrollTo(scrollY-amount);return 0;}

 case FrameMessage:{if(enabled||secondaryDisplayRequested())paint();InterlockedExchange(&framePending,0);return 0;}
 case renderupgrade::ReadyMessage:{renderupgrade::presentCompleted();renderupgrade::readyPending=false;return 0;}
 case InputMessage:{shiftClickTick();InterlockedExchange(&inputPending,0);return 0;}
 case ControlMessage:{tick();InterlockedExchange(&controlPending,0);return 0;}
 case DataMessage:{if(shared&&wp==pid){InterlockedExchange(&shared->hostNotifyPending,0);readSnapshot();serviceSelection();result();serviceCommandQueue(GetTickCount());serviceCalculationTasks();servicePlanner();serviceCooperative();servicePointer();serviceHotkeyBatch();serviceConstruction();serviceAirPath();}return 0;}
 case WM_TIMER:if(wp==2){updateHoverHelp();return 0;}if(wp==AutoSaveTimer){flushAutoSettings();return 0;}tick();return 0;
 case WM_HOTKEY:if(!readOnlyWatchHotkey(wp,lp))handleHotkey(wp,lp);return 0;
 case WM_COMMAND:{int id=LOWORD(wp);
 DependencyCommandRefresh dependencyRefresh;
 if(ready&&!updatingControls&&rejectUnavailableSetting(id,HIWORD(wp)))return 0;
 if(id==9300||id==9301||id==9302||id==9304||id==9305||id==9306||id==9307||id==9308||id==9309||id==9310||id==9313||id==9314||(id>=9320&&id<=9327)){
  if(updatingControls)return 0;
  if(id==9302||id==9304||id==9305||id==9308||id==9309){if(HIWORD(wp)!=CBN_SELCHANGE)return 0;int choice=int(SendDlgItemMessageW(w,id,CB_GETCURSEL,0,0));if(choice<0||choice>(id==9305?3:1))return 0;if(id==9302)renderUpgradeLayer=std::clamp(choice,0,1);else if(id==9304)renderPaletteRange=std::clamp(choice,0,1);else if(id==9309)renderColorPrecision=std::clamp(choice,0,1);else if(id==9308)renderSceneDetail=std::clamp(choice,0,1);else renderPixelScale=std::clamp(choice,0,3);}
  else{if(HIWORD(wp)!=BN_CLICKED)return 0;bool on=SendDlgItemMessageW(w,id,BM_GETCHECK,0,0)==BST_CHECKED;if(id==9300)renderUpgradeEnabled=on;else if(id==9306)renderPhysicalOutput=on;else if(id==9307)renderMaterialRefresh=on;else if(id==9310)renderVulkanEnabled=on;else if(id==9313)renderProcessing=on;else if(id==9314)renderXbrzEnabled=on;else if(id==9320)renderParticleEffects=on;else if(id==9321)renderLaserEffects=on;else if(id==9322)renderVoxelLighting=on;else if(id==9323)renderSpriteLighting=on;else if(id==9324)renderHighRefresh=on;else if(id==9325)harvesterCounterEnabled=on;else if(id==9326)powerDeltaEnabled=on;else if(id==9327)buildTimeTooltipEnabled=on;else unifiedOverlayEnabled=on;}
  if(id==9327){syncAdditional();wakeGame();}
  renderupgrade::stop();++paintRevision;changedSetting();paint();
  if(!renderUpgradeEnabled)SetWindowTextW(GetDlgItem(w,9303),unifiedOverlayEnabled?L"统一覆盖层：世界信息＋主屏统计；交互窗口尚未合并":L"渲染升级：关闭");
  return 0;
 }
 if(id==862&&HIWORD(wp)==CBN_SELCHANGE){if(!updatingControls)updatePlayerTestUI(true);return 0;}
 if(id==9185&&HIWORD(wp)==BN_CLICKED){
  if(updatingControls)return 0;
  updatePlayerTestUI(true);
  int row=int(SendDlgItemMessageW(w,862,CB_GETCURSEL,0,0));
  if(!enabled||!shared||!commandFresh(GetTickCount())||incoming.chat||shared->request!=shared->ack||shared->ack!=ack||commandQueue.size){notify(L"请等待当前对局就绪及上一条指令处理完成，再提交自毁请求。");return 0;}
  if(!playerChoicesTick||DWORD(GetTickCount()-playerChoicesTick)>=500||playerChoicesPid!=pid||playerChoicesMatch!=commandMatch()||row<0||row>=playerChoiceCount){notify(L"请先从最新列表明确选择一个所属方；自毁将作用于该方全部单位和建筑。");return 0;}
  const auto selected=playerChoices[row];
  if(eventdelivery::isMultiplayer(incoming.session)&&(playerChoicesCurrent<0||(selected.index!=playerChoicesCurrent&&!networkHouseReady()))){notify(DestructForeignNotice);return 0;}
  destructPendingTarget=L"所属方 #"+std::to_wstring(selected.index)+L" · "+std::wstring((wchar_t*)selected.name);
  uint32_t before=shared->request;
  send(DestructPlayer,int(selected.pointer),int(selected.id),0,false,uint32_t(selected.index));
  if(shared&&shared->request!=before&&shared->command==DestructPlayer){
   SetWindowTextW(GetDlgItem(w,9186),(L"等待自毁请求入队回执："+destructPendingTarget).c_str());
   EnableWindow(GetDlgItem(w,9185),FALSE);EnableWindow(GetDlgItem(w,862),FALSE);
  }return 0;
 }
 if(id==863||id==864){
  if(!playertest::SwitchAvailable){notify(L"切换操控方暂不可用。");return 0;}
  if(!shared||!commandFresh(GetTickCount())||shared->request!=shared->ack){notify(L"请等待游戏就绪后再切换。");return 0;}
  if(id==864)send(RestorePlayer);
  else {int row=int(SendDlgItemMessageW(w,862,CB_GETCURSEL,0,0));
   if(!extra.switchPlayerEnabled)notify(L"请先启用切换操控方实验性功能。");
   else if(playerChoicesMatch!=commandMatch()||row<0||row>=playerChoiceCount)notify(L"玩家列表已更新，请重新选择。");
   else send(SwitchPlayer,int(playerChoices[row].pointer),int(playerChoices[row].id));
  }return 0;
 }

 if(updatingControls)return 0;
 if(id>=910&&id<=915){for(const auto&c:pageControls)if(GetDlgCtrlID(c.hwnd)==900+id-910){scrollTo(c.rect.y-uilayout::BodyTop);break;}return 0;}

 if(autoSettingChanged(id,HIWORD(wp)))return 0;
 if(id==762&&HIWORD(wp)==CBN_SELCHANGE){
  const int selected=int(SendMessageW(objectQueueCombo,CB_GETCURSEL,0,0));
  if(selected==0||selected==1){objectQueueMode=uint32_t(selected);save();
   notify(L"维修与出售兼容模式已保存，重启游戏和工具箱后生效。");}
  return 0;
 }
 if(id==761&&HIWORD(wp)==CBN_SELCHANGE){
  int selected=int(SendMessageW(megaQueueCombo,CB_GETCURSEL,0,0));
  if(selected==0||selected==1){megaQueueMode=uint32_t(selected);save();
    notify(L"移动指令兼容模式已保存，重启游戏和工具箱后生效。兼容选项为实验性功能。");}
  return 0;
 }
 if(id==760&&HIWORD(wp)==CBN_SELCHANGE){
  int v=int(SendMessageW(schedulerEntryCombo,CB_GETCURSEL,0,0));
  if(v>=0&&v<=6){schedulerTarget=uint32_t(v);save();notify(L"连接设置已保存，重启游戏和工具箱后生效。");}
  return 0;
 }
if(id==991){wchar_t query[128]{};GetWindowTextW(featureSearch,query,128);std::wstring needle=query;for(auto&ch:needle)ch=towlower(ch);
 if(needle.empty()){scrollTo(0);return 0;}
 for(const auto&c:pageControls)if(c.page==activePage){wchar_t label[512]{};GetWindowTextW(c.hwnd,label,512);std::wstring text=std::wstring(label)+L" "+c.help;for(auto&ch:text)ch=towlower(ch);if(text.find(needle)!=std::wstring::npos){scrollTo(c.rect.y-uilayout::BodyTop-12);notify(L"已找到匹配功能；悬停查看说明。");return 0;}}
 notify(L"未找到匹配功能，请换一个关键词。");return 0;}if(id==750||id==754||id==755){
  if(id==750&&!readAdditionalControls())return 0;
  extra.variantKey=SendMessageW(extraControls[VariantKey],HKM_GETHOTKEY,0,0);
  if(id==754){extra.disk=SendMessageW(extraControls[DiskEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.sellDrained=SendMessageW(extraControls[SellDrainedEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.sellVehicles=SendMessageW(extraControls[SellVehiclesEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.sellEnemy=playertest::SellAvailable&&SendMessageW(extraControls[SellEnemyEnabled],BM_GETCHECK,0,0)==BST_CHECKED;}
  bool okay=save();registerKeys();notify(okay?L"设置已保存并应用。":L"保存失败，请检查 Assist.ini 写入权限。");return 0;
 }
 if(id==702||id==705||id>=719&&id<=723){
  extra.enter=SendMessageW(extraControls[EnterEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.air=SendMessageW(extraControls[AirEnabled],BM_GETCHECK,0,0)==BST_CHECKED;
  extra.disk=SendMessageW(extraControls[DiskEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.sellDrained=SendMessageW(extraControls[SellDrainedEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.sellVehicles=SendMessageW(extraControls[SellVehiclesEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.sellEnemy=playertest::SellAvailable&&SendMessageW(extraControls[SellEnemyEnabled],BM_GETCHECK,0,0)==BST_CHECKED;extra.frozen=SendMessageW(extraControls[FrozenEnabled],BM_GETCHECK,0,0)==BST_CHECKED;
  ++paintRevision;save();return 0;
 }
 if(id==713&&HIWORD(wp)==CBN_SELCHANGE){int row=int(SendMessageW(extraControls[QuotaType],CB_GETCURSEL,0,0));if(row>=0&&row<int(quotaTypeIDs.size())){SetWindowTextW(extraControls[QuotaID],wide(quotaTypeIDs[row].c_str()).c_str());readTargetQuota();}return 0;}
 if(id==751){readTargetQuota();return 0;}
 if(id==752){auto type=quotaID();if(type.empty()){notify(L"请输入有效的对象 ID。");return 0;}cooperative::Quota q;
  if(!extraNumber(QuotaFirst,0,512,q.first)||!extraNumber(QuotaSecond,0,512,q.second)||!extraNumber(QuotaMaximum,0,512,q.maximum)){notify(L"受击数需要在 0–512 之间。");return 0;}
  if(SendMessageW(extraControls[QuotaOverride],BM_GETCHECK,0,0)==BST_CHECKED)targetQuotas[type]=q;else targetQuotas.erase(type);
  notify(save()?L"此类型的受击数已保存。":L"保存失败，请检查 Assist.ini 写入权限。");return 0;
 }
 if(id>=100&&id<112){if(id==100){save();notify(L"自动维修开关已保存，每局自动应用。");}else send(actions[id-100]);return 0;}if(id==400){BOOL ok;int nextRepair=GetDlgItemInt(w,312,&ok,FALSE);if(!ok){notify(L"请输入有效的维修间隔。");return 0;}repairMs=std::clamp(nextRepair,500,30000);SetWindowTextW(repairEdit,std::to_wstring(repairMs).c_str());for(int i=0;i<12;++i)hotkeys[i]=(WORD)SendMessageW(keys[i],HKM_GETHOTKEY,0,0);save();notify(L"快捷键已保存。");registerKeys();}
 else if(id==401){BOOL validRate,validMargin;int nextRate=GetDlgItemInt(w,310,&validRate,FALSE),nextMargin=GetDlgItemInt(w,311,&validMargin,FALSE);if(!validRate||!validMargin){notify(L"请输入有效的刷新率和上边距。");return 0;}fps=std::clamp(nextRate,5,500);topMargin=std::clamp(nextMargin,0,4096);SetWindowTextW(rateEdit,std::to_wstring(fps).c_str());SetWindowTextW(marginEdit,std::to_wstring(topMargin).c_str());++paintRevision;save();syncInformationRate(true);notify(L"显示设置已保存。");}
 else if(id>=300&&id<307&&ready){++paintRevision;save();}else if(id==412)bindSuper();else if(id==420){techMode=SendDlgItemMessageW(w,420,BM_GETCHECK,0,0)==BST_CHECKED;
  save();if(snap.status==1)send(OpenTech,techMode);}else if(id==421&&HIWORD(wp)==EN_CHANGE&&ready)fillTech();
 else if(id==896&&HIWORD(wp)==CBN_SELCHANGE&&ready){int selected=int(SendDlgItemMessageW(w,896,CB_GETCURSEL,0,0));techImplementation=selected>=0&&selected<=2?selected:0;save();fillTech(true);if(techMode&&snap.status==1)send(OpenTech,1);}
 else if(id>=424&&id<=426){if(shared&&shared->request!=shared->ack){notify(L"上一条指令处理中，请稍后修改清单。");return 0;}int pos=(int)SendMessageW(techList,LB_GETCURSEL,0,0);if(id==424){std::string typeID;
 if(pos>=0&&pos<int(techIndices.size()))typeID=savedTypes.types[techIndices[pos]].id;
 else {wchar_t raw[80]{};GetWindowTextW(techSearch,raw,80);
  for(wchar_t c:std::wstring(raw)){if(c>32&&c<127)typeID.push_back(char(c));else {typeID.clear();break;}}
  if(!typecatalog::validId(typeID)){notify(L"请选择已缓存科技，或在搜索框输入有效的游戏对象 ID 后点击切换。");return 0;}
  if(savedTypes.ensure(typeID)){++catalogueRevision;if(!saveCatalogue())notify(L"单位缓存写入失败，请检查 Assist.ini 权限。");}
 }if(techExceptions.count(typeID))techExceptions.erase(typeID);else techExceptions.insert(typeID);}else{chooseTechProfile(2);techDefault=id==425;techExceptions.clear();}bool persisted=save();fillTech(true);if(pos>=0)SendMessageW(techList,LB_SETCURSEL,pos,0);
 if(!persisted)notify(L"自定义科技保存失败，请检查 Assist.ini 写入权限。");
 else if(techEnabled)send(OpenTech,1);
 else notify(L"自定义科技清单已保存；开启面板后生效。");}
 else if(id==427||id==428){
  if(shared&&shared->request!=shared->ack){notify(L"上一条科技指令尚未处理。 ");return 0;}
  chooseTechProfile((id==427)?0:1);bool persisted=save();fillTech(true);
  if(!persisted)notify(L"方案保存失败，请检查 Assist.ini 写入权限。");
  else if(techEnabled)send(OpenTech,1);
  else notify(id==427?L"已选择 Ra2Tool 预设清单并保存。":L"已选择 Assist.ini [tech] 自定义清单并保存。");
 }
 else if(id==814){int i=int(SendMessageW(techList,LB_GETCURSEL,0,0));if(techEnabled&&i>=0&&i<int(techIndices.size())){int live=liveTypeIndex(savedTypes.types[techIndices[i]]);if(live>=0)send(CancelTechProduction,live);}}
 else if(id==423||(id==422&&HIWORD(wp)==LBN_DBLCLK)){int i=(int)SendMessageW(techList,LB_GETCURSEL,0,0);if(!techEnabled)notify(L"先开启自定义科技。");else if(i>=0&&i<(int)techIndices.size()){auto&t=savedTypes.types[techIndices[i]];int live=liveTypeIndex(t);if(!techAllowed(t))notify(L"此项目已从开启清单中排除。");else if(live<0)notify(L"此单位在当前对局不可用。");else send(Produce,live);}}
 else if(id==410&&HIWORD(wp)==CBN_SELCHANGE){ControlUpdate guard;auto type=selectedSuperID();SendMessageW(swHotkey,HKM_SETHOTKEY,type.empty()?0:binding(type.c_str()),0);}
 else if(id==449){
  wchar_t raw[64]{};GetWindowTextW(typeIdEdit,raw,64);std::string id;
  bool okay=true;for(wchar_t c:std::wstring(raw)){
   if(c<33||c>126){okay=false;break;}id.push_back(char(c));
  }
  if(!okay||!typecatalog::validId(id)){notify(L"请输入有效的英文单位 ID（最多 23 字节）。");return 0;}
  if(savedTypes.ensure(id)){
   ++catalogueRevision;
   if(!saveCatalogue())notify(L"保存单位 ID 失败：请检查 Assist.ini 写入权限。");
   else notify(L"已加入离线单位列表；进入对局后自动匹配该 ID。");
  }else notify(L"单位 ID 已存在，或缓存列表已满。");
  fillTypeBindings(true);
  int target=savedTypes.find(id);
  if(target>=0){
   int rows=int(SendMessageW(typeList,CB_GETCOUNT,0,0));
   for(int k=0;k<rows;++k)if(int(SendMessageW(typeList,CB_GETITEMDATA,k,0))==target){
    SendMessageW(typeList,CB_SETCURSEL,k,0);typeBindingFields();break;
   }
  }
  SetWindowTextW(typeIdEdit,L"");
 }
 else if(id==440&&HIWORD(wp)==CBN_SELCHANGE)typeBindingFields();
 else if(id==442||id==443)bindType(id==443);
 else if(id==445){int i=selectedTypeIndex();if(i>=0){std::string type=savedTypes.types[i].id;if(SendMessageW(typeCS,BM_GETCHECK,0,0)==BST_CHECKED)csUnits.insert(type);else csUnits.erase(type);save();fillTypeBindings(true);}else notify(L"请从已缓存列表中选择单位类型。");}
 else if(id==446){csMode=SendMessageW(csCheck,BM_GETCHECK,0,0)==BST_CHECKED;if(csMode){hookAutoCS=true;SendMessageW(hookChecks[3],BM_SETCHECK,BST_CHECKED,0);}save();registerKeys();notify(csMode?L"自动移动攻击（CS）已开启，对允许的单位生效。":L"自动移动攻击（CS）已关闭。");}
 else if(id==448){WORD next=(WORD)SendMessageW(csKeyEdit,HKM_GETHOTKEY,0,0);if(keyConflict(next,"",true)){notify(L"此快捷键已经被其他功能使用。");return 0;}csKey=next;save();registerKeys();notify(L"自动 CS 开关快捷键已保存。");}
 else if(id==452){WORD nextA=WORD(SendMessageW(advancedKeyEdit,HKM_GETHOTKEY,0,0)),nextU=WORD(SendMessageW(unloadKeyEdit,HKM_GETHOTKEY,0,0));WORD oldA=advancedKey,oldU=unloadKey;advancedKey=unloadKey=0;
 bool conflict=keyConflict(nextA,"",false)||keyConflict(nextU,"",false)||(!extra.allHotkeys&&LOBYTE(nextA)&&comparableKey(nextA)==comparableKey(nextU));advancedKey=oldA;unloadKey=oldU;
 if(conflict){notify(L"快捷键冲突，请使用不同组合。");return 0;}advancedKey=nextA;unloadKey=nextU;save();registerKeys();notify(L"原生联机指令快捷键已保存。");}
 else if(id==458||id==459||id==465||id==466){
  hookAdvanced=SendMessageW(hookChecks[0],BM_GETCHECK,0,0)==BST_CHECKED;
  hookSmartUnload=SendMessageW(hookChecks[1],BM_GETCHECK,0,0)==BST_CHECKED;
  hookEject=ejectMode;
  hookAutoCS=SendMessageW(hookChecks[3],BM_GETCHECK,0,0)==BST_CHECKED;
  save();registerKeys();diag(L"hook switches flags="+std::to_wstring(flags()));notify(L"功能开关已保存。");}
 else if(id==455){ejectMode=SendMessageW(ejectCheck,BM_GETCHECK,0,0)==BST_CHECKED;hookEject=ejectMode;if(ejectMode){hookEject=true;SendMessageW(hookChecks[2],BM_SETCHECK,BST_CHECKED,0);}save();notify(ejectMode?L"逃生助手已开启：跟踪选中的己方载具，为幸存乘员安排部署或其他后续任务。":L"逃生助手已关闭。");}
 else if(id==457){WORD next=WORD(SendMessageW(ejectKeyEdit,HKM_GETHOTKEY,0,0)),old=ejectKey;ejectKey=0;bool conflict=keyConflict(next,"",false);ejectKey=old;if(conflict){notify(L"快捷键冲突，请换一个组合。");return 0;}ejectKey=next;save();registerKeys();notify(L"幸存乘员开关快捷键已保存。");}
 else if(id==453)send(SmartUnload);
 else if(id==454)send(CancelAdvanced);
 else if(id==477){magnetronMode=SendMessageW(magnetronCheck,BM_GETCHECK,0,0)==BST_CHECKED;save();registerKeys();notify(magnetronMode?L"磁电轮换攻击已开启；按配置的增强鼠标键点地面；按住键可留空。":L"磁电轮换攻击已关闭。");}
 else if(id==479){WORD next=WORD(SendMessageW(magnetronKeyEdit,HKM_GETHOTKEY,0,0)),prior=magnetronKey;magnetronKey=0;bool conflict=keyConflict(next,"",false);magnetronKey=prior;if(conflict){notify(L"此按键与已有功能冲突。");return 0;}magnetronKey=next;save();registerKeys();notify(L"磁电轮换攻击按住键已保存。");}
 else if(id==470){oreMode=SendMessageW(oreCheck,BM_GETCHECK,0,0)==BST_CHECKED;save();notify(oreMode?L"矿车自动卸矿已开启。":L"矿车自动卸矿已关闭，矿车继续使用游戏内卸矿方式。");}
 else if(id==472){BOOL valid;int nextToast=GetDlgItemInt(w,471,&valid,FALSE);if(!valid){notify(L"请输入有效的提示时长。");return 0;}toastMs=std::clamp(nextToast,500,60000);SetWindowTextW(toastEdit,std::to_wstring(toastMs).c_str());save();notify(L"提示时长已保存。");}
 else if(id==480){BOOL valid;int next=GetDlgItemInt(w,444,&valid,FALSE);if(!valid){notify(L"请输入有效的双按间隔。");return 0;}doubleMs=std::clamp(next,20,5000);SetWindowTextW(tapEdit,std::to_wstring(doubleMs).c_str());typeTap.clear();save();notify(L"双按间隔已保存。");}
 else if(id==481){showDetails=SendMessageW(detailCheck,BM_GETCHECK,0,0)==BST_CHECKED;save();}
 else if(id==473){csHoldEnabled=SendMessageW(csHoldCheck,BM_GETCHECK,0,0)==BST_CHECKED;save();}
 else if(id==475){WORD next=WORD(SendMessageW(csHoldEdit,HKM_GETHOTKEY,0,0));WORD prior=csHoldKey;csHoldKey=0;bool conflict=keyConflict(next,"",false);csHoldKey=prior;if(conflict){notify(L"此按键与已有功能冲突。");return 0;}csHoldKey=next;save();notify(L"CS 按键已保存；留空直接按增强鼠标键，非空按住键配合点击。");}
 else if(id==476){buildAnywhereMode=SendMessageW(anywhereCheck,BM_GETCHECK,0,0)==BST_CHECKED;
  notify(buildAnywhereMode?L"高危：扩展建筑放置范围已开启，可能影响联机同步。":L"扩展建筑放置范围已关闭，等待游戏恢复默认限制。");
  save();}
 else if(id==431){enabled=!enabled;if(!enabled)pauseAssistControls();else{lastFind=0;registerKeys();setStatus(L"正在重新连接…");}
#ifdef RA2_UNIFIED
 suite::reportEnabled(2,enabled);
#endif
 }
 else if(id==432)ShellExecuteW(w,L"open",path(L"README.txt").c_str(),nullptr,root.c_str(),SW_SHOWNORMAL);else if(id==433)PostMessageW(w,WM_CLOSE,0,0);return 0;}
 case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:{HDC dc=(HDC)wp;SetBkMode(dc,OPAQUE);int id=GetDlgCtrlID((HWND)lp);SetTextColor(dc,id>=900&&id<=905?modernui::Accent:id==989?modernui::Text:modernui::Muted);SetBkColor(dc,modernui::Background);return (LRESULT)bgBrush;}
 case WM_CTLCOLOREDIT:case WM_CTLCOLORLISTBOX:{HDC dc=(HDC)wp;SetTextColor(dc,modernui::Text);SetBkColor(dc,modernui::Field);return (LRESULT)editBrush;}
 case WM_CLOSE:
#ifdef RA2_UNIFIED
 if(wp!=suite::CloseToken){suite::requestClose();return 0;}
#endif
 if(!bridgeClosePending){
  boundedshutdown::begin();bridgeClosePending=true;flushAutoSettings();stopClock();KillTimer(w,1);KillTimer(w,2);
  for(int id:registeredKeyIDs)UnregisterHotKey(w,id);registeredKeyIDs.clear();
  detach();
 }
 if(retiredBridges.empty()){DestroyWindow(w);return 0;}
 // Leave a blocked game-side gate mapped; the independent process deadline
 // bounds all restoration and worker joins without touching the game process.
 setStatus(L"正在退出；恢复最多等待 2 秒，超时跳过。");
 SetWindowTextW(GetAncestor(w,GA_ROOT),L"红警工具箱 · 正在退出（恢复最多等待 2 秒）…");
 SetTimer(w,BridgeCloseTimer,100,nullptr);return 0;
 case WM_DESTROY:renderupgrade::shutdown();worldrender::worker.shutdown();formationhost::shutdown();watchhost::shutdown();watchwindows::destroy();minimap::destroy();destroyAircraftButton();destroyConstruction();KillTimer(w,AutoSaveTimer);KillTimer(w,BridgeCloseTimer);save();stopClock();KillTimer(w,1);KillTimer(w,2);detach();for(int id:registeredKeyIDs)UnregisterHotKey(w,id);registeredKeyIDs.clear();PostQuitMessage(0);return 0;}return DefWindowProcW(w,msg,wp,lp);}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE,LPWSTR,int show){SetProcessDPIAware();HANDLE single=CreateMutexW(nullptr,FALSE,L"Local\\RA2Assist_v013_Host");if(GetLastError()==ERROR_ALREADY_EXISTS){MessageBoxW(nullptr,L"战术助手已在运行。",L"尤里战术助手",MB_OK);return 0;}
 wchar_t p[2048];GetModuleFileNameW(nullptr,p,2048);root=p;root.resize(root.find_last_of(L"\\/"));autolog::initialize(root);autolog::write(L"host-start criticalLog=automatic rotation=2MiB+2 backups; verbose Debug.Diagnostics is independent");loadAdditionalSettings();
 megaQueueMode=value(L"Orders",L"MegaMissionBackend",0)==1?1u:0u;
 objectQueueMode=value(L"Orders",L"ObjectEventBackend",0)==1?1u:0u;
 schedulerMode=0;schedulerTarget=0; // Message manager is the only entry.
 infoAdaptive=value(L"Display",L"AdaptiveFPS",0)!=0;
 int refreshRevision=value(L"Display",L"LabelRefreshVersion",0);fps=visual::initialFPS(value(L"Display",L"FPS",visual::DefaultFPS),refreshRevision);if(refreshRevision<1){write(L"Display",L"FPS",fps);write(L"Display",L"LabelRefreshVersion",1);}topMargin=std::clamp(value(L"Display",L"TopMargin",20),0,4096);
 labelScalePercent=viewport::percent(value(L"Display",L"LabelScalePercent",100));constructionScalePercent=viewport::percent(value(L"Display",L"ConstructionScalePercent",100));
 techProfile=std::clamp(value(L"Tech",L"Profile",value(L"main",L"CustomTech",0)?1:0),0,2);
 techImplementation=value(L"Tech",L"Implementation",0);if(techImplementation<0||techImplementation>2)techImplementation=0;
 // The [tech] format mirrors Ra2Tool.ini; entries are technology TYPE ARRAY indices.
 for(int kind:{7,16,40,3}){
  std::wstring key=std::to_wstring(kind);
  wchar_t valueBuf[8192];
  GetPrivateProfileStringW(L"tech",key.c_str(),L"",valueBuf,8192,path(L"Assist.ini").c_str());
  std::string values;
  for(const wchar_t* t=valueBuf;*t;++t)if(*t<=127)values.push_back(char(*t));
  ra2CustomTech.types[kind]=techprofile::parseIndexes(values);
 }
 techDirectProduce=value(L"Tech",L"DirectProduce",0)!=0;
 techDefault=value(L"Tech",L"DefaultAllow",1)!=0;wchar_t techIDs[16384];GetPrivateProfileStringW(L"Tech",L"Exceptions",L"",techIDs,16384,path(L"Assist.ini").c_str());std::string techID;for(wchar_t*c=techIDs;;++c){if(!*c||*c==L','){if(!techID.empty())techExceptions.insert(techID);techID.clear();if(!*c)break;}else if(*c>L' '&&*c<127)techID+=(char)*c;}
 profileExceptions[techProfile]=techExceptions;
 for(int p=0;p<3;++p){
  auto k=L"Exceptions"+std::to_wstring(p);wchar_t buf[16384]{};
  GetPrivateProfileStringW(L"Tech",k.c_str(),L"__NOT_SET__",buf,16384,path(L"Assist.ini").c_str());
  if(wcscmp(buf,L"__NOT_SET__"))profileExceptions[p]=parseIDs(buf);
 }
 techExceptions=profileExceptions[techProfile];
 // Technology uses sidebar entries and vanilla production; old rule-override flags are ignored.
 hookAdvanced=value(L"Hooks",L"Advanced",value(L"NativeOrders",L"EnableInputHooks",0))!=0;
 hookSmartUnload=value(L"Hooks",L"SmartUnload",0)!=0;
 hookEject=value(L"Hooks",L"Eject",0)!=0;
 hookAutoCS=featureavailability::AutoCS&&value(L"Hooks",L"AutoCS",0)!=0;
 diagEnabled=value(L"Debug",L"Diagnostics",0)!=0;
 autoRevealNewMatch=shroudpolicy::automaticReveal(extra.mapStrategy);
 techMode=value(L"Experimental",L"NativeTechProduction",0)!=0;repairMs=std::clamp(value(L"Modes",L"RepairMs",2000),500,30000);
 const WORD defaults[]={'R','Q','T','D','M','E','V','B','O','I','C','L'};static_assert(sizeof(defaults)/sizeof(defaults[0])==12);for(int i=0;i<12;++i)hotkeys[i]=(WORD)value(L"Hotkeys",wide(iniKeys[i]).c_str(),MAKEWORD(defaults[i],HOTKEYF_CONTROL|HOTKEYF_ALT));
 wchar_t section[8192];GetPrivateProfileSectionW(L"SuperHotkeys",section,8192,path(L"Assist.ini").c_str());for(wchar_t*s=section;*s;s+=wcslen(s)+1){std::wstring line=s;size_t e=line.find(L'=');if(e==std::wstring::npos)continue;std::string id;for(size_t i=0;i<e&&i<23;++i)id+=(char)line[i];swBindings.push_back({id,(WORD)_wtoi(line.c_str()+e+1)});}
 for(const wchar_t* sec:{L"TypeHotkeys",L"AutoCSUnits"}){wchar_t entries[32768];GetPrivateProfileSectionW(sec,entries,32768,path(L"Assist.ini").c_str());for(wchar_t* p=entries;*p;p+=wcslen(p)+1){std::wstring line=p;size_t e=line.find(L'=');if(e==std::wstring::npos)continue;std::string id;for(size_t j=0;j<e&&j<23;++j)if(line[j]>32&&line[j]<127)id+=char(line[j]);if(id.empty())continue;int v=_wtoi(line.c_str()+e+1);if(!wcscmp(sec,L"TypeHotkeys")){if(LOBYTE(v)&&typeBindings.size()<512)typeBindings.push_back({id,WORD(v)});}else if(v)csUnits.insert(id);}}
 loadCatalogue();
 // Upgrade existing saved bindings to visible, editable offline entries even
 // before the first game snapshot is cached by this version.
 for(const auto&b:typeBindings)if(savedTypes.ensure(b.id))++catalogueRevision;
 for(const auto&id:csUnits)if(savedTypes.ensure(id))++catalogueRevision;
 // Preserve already-saved technology exceptions in the offline list too,
 // even when upgrading from a version which did not persist the type catalogue.
 for(const auto& exceptions:profileExceptions)
  for(const auto&id:exceptions)if(savedTypes.ensure(id))++catalogueRevision;
 if(catalogueRevision&&!saveCatalogue())diag(L"Initial offline catalogue save failed");
 ejectMode=value(L"NativeOrders",L"EjectAssist",0)!=0;hookEject=ejectMode;ejectKey=WORD(value(L"NativeOrders",L"EjectToggleKey",MAKEWORD('J',HOTKEYF_CONTROL|HOTKEYF_ALT)));veteranPercent=std::clamp(value(L"NativeOrders",L"VeteranPercent",200),100,10000);elitePercent=std::clamp(value(L"NativeOrders",L"ElitePercent",300),veteranPercent,10000);
 shiftSidebarWidth=std::clamp(value(L"Click",L"SidebarWidth",GetPrivateProfileIntW(L"Click",L"SidebarWidth",160,path(L"settings.ini").c_str())),32,600);shiftSidebarTop=std::clamp(value(L"Click",L"SidebarTop",GetPrivateProfileIntW(L"Click",L"SidebarTop",200,path(L"settings.ini").c_str())),0,2000);
 shiftClickEnabled=value(L"Click",L"Enabled",GetPrivateProfileIntW(L"Click",L"Enabled",0,path(L"settings.ini").c_str()))!=0;shiftClickMode=std::clamp(value(L"Click",L"Mode",GetPrivateProfileIntW(L"Click",L"Mode",0,path(L"settings.ini").c_str())),0,2);shiftClickGap=std::clamp(value(L"Click",L"IntervalMs",GetPrivateProfileIntW(L"Click",L"IntervalMs",0,path(L"settings.ini").c_str())),0,250);
 advancedRangeCells=std::clamp(value(L"NativeOrders",L"TargetRangeExtraCells",2),0,30);advancedTargetPriority=std::clamp(value(L"NativeOrders",L"TargetPriority",0),0,1);advancedScanHz=std::clamp(value(L"NativeOrders",L"TargetScanHz",10),1,60);advancedKey=WORD(value(L"NativeOrders",L"AdvancedHoldKey",MAKEWORD('G',HOTKEYF_ALT)));unloadKey=WORD(value(L"NativeOrders",L"UnloadKey",MAKEWORD('U',HOTKEYF_CONTROL|HOTKEYF_ALT)));pathTimeout=std::clamp(value(L"NativeOrders",L"PathTimeoutFrames",90),30,900);
 doubleMs=std::clamp(value(L"TypeSelection",L"DoublePressMs",350),20,5000);csMode=featureavailability::AutoCS&&value(L"AutoCS",L"Enabled",0)!=0&&hookAutoCS;csKey=WORD(value(L"AutoCS",L"ToggleKey",MAKEWORD('A',HOTKEYF_CONTROL|HOTKEYF_ALT)));
 toastMs=std::clamp(value(L"Display",L"ToastMs",6000),500,60000);showDetails=value(L"Display",L"DetailedStatus",0)!=0;
 magnetronMode=value(L"Modes",L"MagnetronMove",0)!=0;magnetronKey=WORD(value(L"NativeOrders",L"MagnetronHoldKey",MAKEWORD('M',HOTKEYF_ALT)));
 repairUnrepairable=value(L"Modes",L"RepairUnrepairable",0)!=0;
 {wchar_t text[13000]{};GetPrivateProfileStringW(L"OreTypes",L"Miners",L"HARV,CMIN",text,13000,path(L"Assist.ini").c_str());oreMiners=text;
 GetPrivateProfileStringW(L"OreTypes",L"Refineries",L"GAREFN,NAREFN",text,13000,path(L"Assist.ini").c_str());oreRefineries=text;
 oreconfig::parse(oreMiners,oreTypes.miners,oreTypes.minerCount);oreconfig::parse(oreRefineries,oreTypes.refineries,oreTypes.refineryCount);
 oreTypes.customMiners=value(L"OreTypes",L"CustomMiners",0)!=0;oreTypes.customRefineries=value(L"OreTypes",L"CustomRefineries",0)!=0;}
 oreMode=value(L"Modes",L"AutoOreReturn",0)!=0;buildAnywhereMode=value(L"Experimental",L"BuildAnywhere",0)!=0;
 csHoldEnabled=value(L"AutoCS",L"HoldEnabled",0)!=0;csHoldKey=WORD(value(L"AutoCS",L"HoldKey",MAKEWORD('G',HOTKEYF_CONTROL)));
 INITCOMMONCONTROLSEX cc{sizeof cc,ICC_WIN95_CLASSES};InitCommonControlsEx(&cc);GdiplusStartupInput gi;ULONG_PTR token;GdiplusStartup(&token,&gi,nullptr);font=modernui::makeFont();bgBrush=CreateSolidBrush(modernui::Background);editBrush=CreateSolidBrush(modernui::Field);
 WNDCLASSW wc{};wc.hInstance=h;wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"RA2AssistOverlay010";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);RegisterClassW(&wc);overlay=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,0,0,1,1,nullptr,nullptr,h,nullptr);
 wc.lpfnWndProc=proc;wc.lpszClassName=L"RA2Assist010";wc.hbrBackground=bgBrush;wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);RegisterClassW(&wc);RECT work{};SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);ui=CreateWindowExW(0,wc.lpszClassName,L"战术助手 v" RA2_RELEASE_VERSION_W,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_THICKFRAME|WS_VSCROLL|WS_HSCROLL|WS_CLIPCHILDREN, CW_USEDEFAULT,CW_USEDEFAULT,685,std::min(1084,int(work.bottom-work.top)-20),nullptr,nullptr,h,nullptr);
#ifdef RA2_UNIFIED
 enabled=suite::enabled(2);suite::registerPane(2,ui);publishSuiteDisplay();
#else
 ShowWindow(ui,show);
#endif
 syncInformationRate(true);startClock();uimessages::Queue messages;MSG m;while(messages.get(m)>0){if(!IsDialogMessageW(ui,&m)){TranslateMessage(&m);DispatchMessageW(&m);}}DestroyWindow(overlay);surface.clear();textRasters.clear();glyphRasters.clear();labelRasters.clear();DeleteObject(font);DeleteObject(bgBrush);DeleteObject(editBrush);GdiplusShutdown(token);CloseHandle(single);return 0;}



