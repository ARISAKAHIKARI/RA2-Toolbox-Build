#pragma once
#include <stdint.h>
#include "planner_types.h"
#include "ore_type_config.h"
#include "feature_type_config.h"
#include "plan_freshness.h"
#include "command_gateway.h"
#include "mega_queue_backend.h"
#include "cooperative_types.h"
#include "pointer_intent.h"
namespace assist {
constexpr uint32_t Magic=0x41533130, Version=46;
constexpr uint32_t DataMessage=0x8002, WakeTag=0x41533230;
constexpr int MaxLabels=2048, MaxSW=32, MaxTypes=512, MaxThreats=128, MaxCrates=256, MaxCrateResults=16;
enum Command { None, Repair, CombatSelection, EmptyTransports, DamagedSelection, IdleHarvester, IdleEngineer, IdleTransport, ClearBeacons, RevealMap, OpenTech, SelectSuper, Produce, SelectIron, SelectChrono, AutoBoard, SelectType, SmartUnload, CancelAdvanced, SelectVariant, ProduceReserved, AirPathMove, CancelTechProduction };
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
struct Snapshot {
 int status,frame,session,width,height,labels,swCount,typeCount,selected,transports,used,capacity;
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
 int renderRevealStatus=0; // 1=render-only test installed; -2=signature/install/restore conflict
 int lastSubmitError=0; // 0=queued; 1=epoch; 2=session; 3=inactive; 4=chat; 5=thread; 6=room; 7/8=ctor; 9=timer

};
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
 volatile long worldSeq;planner::World world;
 volatile uint32_t planRequest,planAck;uint32_t planTick,planMatch;
 int planCount,planAccepted,planResult;planner::Order plan[planner::MaxOrders];volatile uint32_t plannerTracking;
 uint32_t hostWindow;volatile long hostNotifyPending,gameWakePending;uint32_t magnetronKey,planAcceptedMask,planRejectedMask;
 uint32_t externalDisplay;
 int loadMode,loadRadius;
 uint32_t enterKey,airKey1,airKey2;
 volatile long cooperativeSeq;cooperative::Request cooperativeRequest;
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
 volatile uint32_t renderReveal; // independent default-off visual experiment; never RevealMap

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

