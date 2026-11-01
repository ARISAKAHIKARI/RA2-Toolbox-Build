#pragma once
#include "player_test_policy.h"
#include "formation.h"
#include "air_path_policy.h"
#include <stdint.h>
#include <stddef.h>
#include "planner_types.h"
#include "ore_type_config.h"
#include "feature_type_config.h"
#include "plan_freshness.h"
#include "command_gateway.h"
#include "mega_queue_backend.h"
#include "cooperative_types.h"
#include "pointer_intent.h"
#include "construction_input.h"
#include "compute_policy.h"
#include "compute_tasks.h"
#include "compute_frame.h"
#include "cooperative_frame.h"
#include "unload_compute.h"
#include "missile_compute.h"
#include "burst_compute.h"
#include "dodge_compute.h"
namespace assist {
constexpr uint32_t Magic=0x41533130, Version=86;
// Both binaries derive the namespace from the protocol version. Do not maintain
// a separate literal in the host: a valid ABI with a different name never connects.
struct MappingPrefixValue {
 wchar_t text[sizeof(L"Local\\RA2Assist_v000_")/sizeof(wchar_t)]=L"Local\\RA2Assist_v000_";
 constexpr MappingPrefixValue(){static_assert(Version<1000);const unsigned n=sizeof(text)/sizeof(wchar_t)-5;text[n]=L'0'+Version/100;text[n+1]=L'0'+Version/10%10;text[n+2]=L'0'+Version%10;}
};
constexpr MappingPrefixValue MappingPrefix{};
inline void mappingName(wchar_t*out,uint32_t pid){const wchar_t*s=MappingPrefix.text;int i=0;while(*s)out[i++]=*s++;wchar_t digits[10];int n=0;do{digits[n++]=L'0'+pid%10;pid/=10;}while(pid);while(n)out[i++]=digits[--n];out[i]=0;}

constexpr uint32_t DataMessage=0x8002, WakeTag=0x41533230;
constexpr int MaxLabels=2048, MaxSW=32, MaxTypes=512, MaxThreats=128, MaxCrates=256, MaxCrateResults=16;
enum Command { None, Repair, CombatSelection, EmptyTransports, DamagedSelection, IdleHarvester, IdleEngineer, IdleTransport, ClearBeacons, RevealMap, OpenTech, SelectSuper, Produce, SelectIron, SelectChrono, AutoBoard, SelectType, SmartUnload, CancelAdvanced, SelectVariant, ProduceReserved, AirPathMove, CancelTechProduction, SwitchPlayer, RestorePlayer, Cheer, Taunt, ProduceDeferred, StickyGuard, SetSleep, SetGuard, SetHarmless, DestructPlayer };
// DestructPlayer: arg=House pointer, arg2=House UniqueID, requestToken=House
// array index. Standard requestMatch/requestTick and ack/result apply.
namespace selectedmode {
constexpr int Count=3;
constexpr int command(int index){return index>=0&&index<Count?SetSleep+index:None;}
constexpr int index(int command){return command>=SetSleep&&command<=SetHarmless?command-SetSleep:-1;}
constexpr int mission(int index){return index==0?0:index==1?5:index==2?23:-1;}
inline const wchar_t* label(int index){const wchar_t*names[]={L"不攻击模式",L"驻守模式",L"无害模式"};return index>=0&&index<Count?names[index]:L"任务模式";}
inline const wchar_t* setting(int index){const wchar_t*names[]={L"SleepMode",L"GuardMode",L"HarmlessMode"};return index>=0&&index<Count?names[index]:L"";}
}
enum Flags : uint32_t { Capacity=1, Iron=2, Production=4, AllTransports=8, Psychic=16, Crates=32, AutoRepair=64, TextPanel=128, CrateResults=256, AutoCS=512, NativeOrders=1024, EjectAssist=2048, UnsafeTech=4096, AdvancedInput=8192, SmartUnloadOrders=16384, AutoOreReturn=32768, BuildAnywhere=65536, CustomCS=131072, MagnetronInput=262144, Frozen=524288, AdvancedEnter=1048576, AircraftSpread=2097152, DiskPrimary=4194304, SellDrained=8388608, SellVehicles=16777216, SellEnemy=33554432, ExperimentalReveal=67108864, RepairUnrepairable=134217728, ReserveConstruction=268435456, AirWaypoints=536870912, AntiTheft=1073741824, NativeAirWaypoints=0x80000000u };
constexpr uint32_t CooperativeFlags=AdvancedEnter|AircraftSpread,SpecialClickFlags=CooperativeFlags|DiskPrimary|SellDrained|SellVehicles|SellEnemy;
struct Threat { uint32_t id,target; int x,y,tx,ty; };
struct CrateMark { int cellX,cellY,x,y,preview; };
enum CrateDetail { UnitCreated=1, EffectsMeasured=2, PartialEffects=4, NoVanillaEffect=8 };
struct CrateResult { uint32_t serial,tick; int cellX,cellY,reward,amount,affected,factor,detail; char unitID[24]; uint16_t unitName[40]; };
struct TaskReceiptSummary {
 uint32_t serial,match,unit,target,queuedAt,observedAt;
 uint8_t phase,failure,owner,flags;
};
constexpr int RecentReceiptCapacity=32;
struct Label { uint32_t id; int x,y,used,capacity,ironFrames,shield,advanced; uint32_t ownerColor; int frozenFrames; };
struct Super { char id[24]; uint16_t name[40]; int index,ready,frames; };
struct Type { char id[24]; uint16_t name[40]; int kind,index,naval,cost; };
inline bool typeIndexedCommand(int c,int arg){return (c==OpenTech&&arg)||c==Produce||c==CancelTechProduction||c==ProduceReserved||c==ProduceDeferred||c==SelectType;}
inline uint64_t typeListHash(const Type*types,int count){
 if(count<0||count>MaxTypes)return 0;uint64_t h=1469598103934665603ull;
 auto mix=[&](const void*p,size_t n){auto b=(const uint8_t*)p;while(n--)h=(h^*b++)*1099511628211ull;};
 mix(&count,4);for(int i=0;i<count;++i){mix(types[i].id,24);mix(&types[i].kind,4);mix(&types[i].index,4);}return h?h:1;
}
struct Snapshot {
 int status,frame,session,width,height,labels,swCount,typeCount,selected,transports,used,capacity;
 // techOpen: 0=off, 1=controlled, 2=classic compatibility, 3=direct.
 int factories,busyFactories,pausedFactories,completedFactories,credits,techOpen;
 uint32_t match,noticeSerial; int notice; uint32_t scanMicros;
 Label label[MaxLabels]; Super supers[MaxSW]; Type types[MaxTypes];
 int threatCount,crateCount; Threat threats[MaxThreats]; CrateMark crates[MaxCrates];
 int techVisible,techUnavailable,techError,autoRepair,repairTotal;
 int loadUnits,loadVehicles,loadIssued,loadWaiting,loadUnassigned;
 uint32_t sampledTick;int cameraX,cameraY;
 int crateHookStatus,crateResultCount;uint32_t crateResultSerial;CrateResult crateResults[MaxCrateResults];
 int chat,csHookStatus,csConverted;
 int nativeHookStatus,advancedUnits,unloadPending,unloadIssued,ejectWatched;
 int inputPatchMask; // 1=local outgoing mission, 2=CS key/cursor, 4=local event cancellation
 int oreQueued,oreDocking,oreIssued,placementStatus,nativeAirStatus;
 uint32_t controlMicros,worldMicros,displayMicros;
 int oreManual,oreYielded;
 uint32_t oreDelivered=0,oreUnverified=0; // Planner read-only evidence; not credit accounting.
 uint32_t magnetShots=0,magnetUnverified=0; // Read-only weapon/lift evidence, not hit attribution.
 uint32_t hookInstalled=0,hookConflicts=0; // Game-process inline patch registry only.
 uint32_t queueSent,queueFull,queueTimingFailure,planDeferred;
 int megaQueueStatus=0; // 0=direct EventClass; 1=game native accepted; negative=reason
 uint32_t megaQueueAccepted=0,megaQueueRejected=0;
 int objectQueueStatus=0; uint32_t objectQueueAccepted=0,objectQueueRejected=0;
 // Independent readiness channels. Only a command bridge can assert control.
 int readStatus,bridgeStatus,commandStatus,renderStatus;
 // Every successful enqueue increments queuedEvents, never executedEvents.
 uint32_t queuedEvents,observedEvents,commandRejected,receiptCancelled,receiptExpired,receiptDropped;
 uint32_t receiptWrite,receiptCount;TaskReceiptSummary recentReceipts[RecentReceiptCapacity];
 uint32_t entryThread=0,entryCalls=0,entryReentrant=0,entryContended=0,entryWrongThread=0;
 int c4TestStatus=0;
 int renderRevealStatus=0; // 1=active, 2=waiting for loaded materials; -2=signature/install/restore conflict
 int lastSubmitError=0; // 0=queued; 1=epoch; 2=session; 3=inactive; 4=chat; 5=thread; 6=room; 7/8=ctor; 9=timer

};
struct PlayerTestInfo {uint32_t pointer,id;int index,human;uint16_t name[21];};
struct Shared {
 uint32_t magic,version,pid,owner; volatile uint32_t heartbeat,stop,foreground,flags,scanMs;
 volatile uint32_t request,ack; int command,arg,arg2;uint32_t requestToken; uint32_t requestTick,requestMatch;
 int result,count; volatile long seq; Snapshot data;
 uint8_t techAllowed[MaxTypes];
 uint32_t repairMs;
 uint64_t clockStart,clockFrequency;uint32_t clockTick;
 volatile long csSeq;uint32_t csMatch;uint8_t csAllowed[MaxTypes];
 uint32_t advancedKey,pathTimeout,veteranPercent,elitePercent;
 // Selection is local UI state. It must never wait behind simulation commands.
 volatile uint32_t selectRequest,selectAck;
 int selectIndex,selectGlobal;uint32_t selectObjectID;uint32_t selectTick,selectMatch;
 int selectResult,selectCount;uint32_t csHoldKey;
 // One game-thread publication epoch covers all external decision inputs.
 volatile long computeFrameSeq=0;uint32_t computeFrameSerial=0,computeFrameMatch=0,computeFrameTick=0;int computeFrame=-1;
 volatile long worldSeq;planner::World world;
 volatile uint32_t planRequest,planAck;uint32_t planTick,planMatch;
 int planCount,planAccepted,planResult;planner::Order plan[planner::MaxOrders];volatile uint32_t plannerTracking;
 uint32_t hostWindow;volatile long hostNotifyPending,gameWakePending;uint32_t magnetronKey,planAcceptedMask,planRejectedMask;
 uint32_t externalDisplay;
 int loadMode,loadRadius;
 uint32_t enterKey,airKey1,airKey2;
 volatile long cooperativeSeq;cooperative::Request cooperativeRequest;
 cooperative::FrameInput cooperativeInputFrame;volatile uint32_t cooperativeFrameWanted=0;
 volatile uint32_t cooperativePlan,cooperativeAck;uint32_t cooperativeSerial,cooperativeTick;
 int cooperativeCount,cooperativeAccepted,cooperativeRejected,cooperativeMode;
 cooperative::Assignment cooperativeOrders[cooperative::MaxSources];
 int clickHookStatus;
 // Scheduler configuration/state. One selected call site at a time.
 uint32_t driverMode,driverTarget;
 uint32_t megaQueueMode; // 0=EventClass; 1=game QueueMegaMission, MP only
 uint32_t objectQueueMode; // 0=EventClass; 1=TechnoClass::ClickedEvent, MP only
 volatile int32_t driverStatus;
 volatile uint32_t driverTicks,driverLastTick;
 volatile uint32_t productionRequest,productionAck;uint32_t productionMatch,productionTick;int productionCount,productionTypes[4];
 // Source stamps survive calculation/IPC. Existing tick fields now carry
 // acquisition time, not submission time. Both peers require protocol 33.
 int planFrame,cooperativeFrame;uint32_t cooperativeMatch;
 int cooperativeProcessed,cooperativeResult;uint32_t cooperativeAcceptedBits[16];
 pointerinput::Mailbox pointer;
 volatile long pointerPolicySeq;pointerinput::NativePolicy pointerPolicy;pointerinput::SuppressedClick suppressedClick;
 // Immutable until selectAck; -3 selects the union of this match-bound mask.
 uint8_t selectTypes[MaxTypes];
 volatile long oreConfigSeq;oreconfig::Config oreConfig;
 volatile long featureConfigSeq;featuretypes::Config featureConfig;
 volatile uint32_t c4Test;
 volatile uint32_t advancedRangeCells; // 0..30 extra acquisition cells, host-owned
 volatile uint32_t renderReveal; // 0=native, 1/2=hook, 3=direct, 4/5=clear/half material; never RevealMap
 // Local UI feedback, consumed by the existing Runtime game-thread dispatcher.
 // Immutable until ack; short lived and never an EventClass / OutList command.
 volatile uint32_t uiSoundRequest,uiSoundAck;uint32_t uiSoundTick,uiSoundMatch;
 volatile long reservePolicySeq;construction::InputPolicy reservePolicy;
 volatile uint32_t reserveInput,reserveConsumed;construction::InputClick reserveClick;
 // Requested mask is host-owned; Runtime latches it only at a new match.
 // This prevents pending state/receipts from being executed by two planners.
 volatile uint32_t computeRequested=compute::Default,computeApplied=compute::Default;
 uint32_t advancedScanHz=10,advancedPriority=0;
 volatile uint32_t runtimeTracking=0,runtimePlannerMicros=0;
 // Runtime produces raw immutable tasks; host publishes pure results and the
 // computed serial last. Neither field is a command/event receipt.
 volatile long loadResultSeq=0,repairResultSeq=0;
 volatile uint32_t loadTask=0,loadComputed=0;compute::LoadInput loadInput;compute::LoadResult loadResult;
 volatile uint32_t repairTask=0,repairComputed=0;compute::RepairInput repairInput;compute::RepairResult repairResult;
 volatile long quotaConfigSeq=0;compute::QuotaConfig quotaConfig;
 volatile uint32_t runtimeCoopDone=0;int runtimeCoopAccepted=0,runtimeCoopRejected=0;
 // Coherent immutable unload observations and host-only calculation result.
 volatile long unloadInputSeq=0,unloadResultSeq=0;
 volatile uint32_t unloadTask=0,unloadComputed=0;
 unloadcompute::Input unloadInput;unloadcompute::Result unloadResult;
 volatile uint32_t controlAll=0,switchPlayerEnabled=0; // session-only opt-ins, host owned
 int playerCount=0,playerCurrent=-1,playerOriginal=-1,playerControlStatus=0;
 volatile uint32_t missileEnabled=0,missileScanHz=10;
 volatile long missileInputSeq=0,missileResultSeq=0;
 volatile uint32_t missileTask=0,missileComputed=0;
 missilecompute::Input missileInput;missilecompute::Result missileResult;
 uint32_t missileSources=0,missileTargets=0,missileIssued=0;
 volatile uint32_t burstEnabled=0,burstHoldEnabled=0,burstHeld=0;
 volatile long burstInputSeq=0,burstResultSeq=0;volatile uint32_t burstTask=0,burstComputed=0;
 burstcompute::Input burstInput;burstcompute::Result burstResult;uint32_t burstActive=0,burstIssued=0;
 volatile uint32_t dodgeEnabled=0;volatile long dodgeInputSeq=0,dodgeResultSeq=0;volatile uint32_t dodgeTask=0,dodgeComputed=0;
 dodgecompute::Input dodgeInput;dodgecompute::Result dodgeResult;uint32_t dodgeActive=0,dodgeIssued=0;
 PlayerTestInfo players[32]{}; // Runtime writer, protected by seq
 volatile uint32_t scrollRefreshMode=0,scrollRefreshHz=0; // 0 cursor, 1 scroll, 2 scene, 3 forced redraw; zero Hz disables
 int scrollRefreshStatus=0;uint32_t scrollRefreshDraws=0,scrollRefreshRateMilli=0,scrollRefreshMaxGapUs=0; // Runtime diagnostics
 volatile uint32_t viewZoomPercent=100;volatile long viewZoomBusy=0;
 int viewZoomStatus=0,viewZoomApplied=100,viewZoomWidth=0,viewZoomHeight=0;
 volatile uint32_t magnetronDrop=0;
 volatile uint32_t missileScope=0,missileAllocation=0,magnetronGlobal=0,missileHeld=0;
 volatile long formationPolicySeq=0,formationSeq=0;formation::Policy formationPolicy;formation::Preview formationPreview;
 volatile uint32_t formationKey=0,formationKeyAck=0,formationKeyCode=0,formationConsumedUntil=0;
 uint32_t formationAccepted=0,formationRejected=0,formationJobs=0;
 volatile long cameraSeq=0;volatile uint32_t cameraAck=0,cameraTick=0,cameraMatch=0;int cameraTargetX=0,cameraTargetY=0;
 volatile uint32_t dodgeMode=0; // 0 reverse-flight move, 1 existing native Scatter
 volatile uint32_t placementImplementation=0,techEnhance=0;
 uint64_t requestTypeHash=0;
 int techEnhanceStatus=0;uint32_t techEnhanceCount=0;
 volatile uint32_t nativeAirNetwork=0;
 volatile uint32_t missileReserve=1,lowDamageFilter=0,lowDamageNetwork=0;
 volatile int lowDamageStatus=0; // 0 off, 1 native+tool, 2 MP tool-only, -2 conflict
 volatile uint32_t networkHousePreserve=0,nativeSellCursor=0; // explicit outgoing/native-cursor experiments
 volatile int networkHouseStatus=0,nativeSellCursorStatus=0; // 0 off, 1 ready, 2 draining; negative unavailable
 uint32_t networkHousePackets=0,networkHouseDropped=0,nativeSellCursorPasses=0,nativeSellOwnerAdjusted=0;
 volatile uint32_t nativeMegaGateway=0;volatile int nativeMegaGatewayStatus=0;
 uint32_t nativeMegaGatewayAccepted=0,nativeMegaGatewaySkipped=0;
 volatile uint32_t controlCommandOwner=0;volatile int controlCommandOwnerStatus=0;
 uint32_t controlCommandOwnerAccepted=0,controlCommandOwnerRejected=0;
 volatile uint32_t buildTimeTooltip=0;volatile int32_t buildTimeTooltipStatus=0; // native sidebar box, local UI only
 volatile uint32_t crcMapShadow=0;volatile int32_t crcMapShadowStatus=0;
 uint32_t crcMapShadowTracked=0,crcMapShadowSubstitutions=0,crcMapShadowFaults=0;
 volatile uint32_t aircraftAllocationPolicy=0; // 0 quotas, 1 five-cell damage, 2 single-target damage/deselect
 volatile uint32_t nativeCombatQueries=0; // explicit experimental engine weapon/range queries; default raw observation

};
inline int remaining(int start,int duration,int frame){
 if(duration<=0)return 0; if(start==-1)return duration;
 int64_t n=int64_t(duration)-(int64_t(frame)-start);return n<=0?0:n>duration?duration:int(n);
}
inline bool idleMission(int mission,int queued,bool target,bool deactivated){return !deactivated&&!target&&(queued==-1||queued==mission)&&(mission==0||mission==5||mission==11||mission==13);}
inline bool canRepair(int hp,int max,bool active,bool repairing){return active&&!repairing&&hp>0&&max>hp;}
inline bool queueAvailable(int count,int tail){return count>=0&&count<112&&tail>=0&&tail<128;}
inline bool validType(int level,bool insignificant,bool spawned,uint32_t owners){return level>=0&&level<=100&&!insignificant&&!spawned&&owners!=0;}
}



