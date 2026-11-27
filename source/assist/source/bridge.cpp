#include "pointer_batch.h"
#include "../../shared/source/feature_availability.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <stddef.h>
#include <utility>
#include "protocol.h"
#include "command_intent.h"
#include "command_sampling.h"
#include "runtime_allocation.h"
#include "external_planner.h"
#include "../../external_reader/source/catalogue_stamp.h"
#include "clock_ms.h"
#include "../../shared/source/precise_clock.h"
#include "../../shared/source/runtime_fault.h"
#include "signatures.h"
#include "../../shared/source/debug_entry_policy.h"
#include "../../shared/source/patch_context.h"
#include "startup_signature_compat.h"
#include "startup_dependency_policy.h"
#include "autoload.h"
#include "outgoing_batch.h"
#include "object_index.h"
#include "global_vehicle_scan_policy.h"
#include "repair_result_filter.h"
#include "hook_policy.h"
#include "activation_diagnostic.h"
#include "event_delivery.h"
#include "private_outlist.h"
#include "mega_queue_backend.h"
#include "constructor_adapter.h"
#include "entry_gate.h"
#include "tactical_decisions.h"
#include "object_event_backend.h"
#include "repair_delivery.h"
#include "session_epoch.h"
#include "world_lifecycle.h"
#include "../../shared/source/world_frame_command_worker_win.h"
#include "world_capture_admission.h"
#include "readonly_idle_retirement.h"
#include "control_link.h"
#include "control_mode_policy.h"
#include "scheduler_policy.h"
#include "dispatch_router.h"
#include "order_policy.h"
#include "unit_task_arbiter.h"
#include "execution_proof.h"
#include "../../external_reader/source/display_state.h"
#include "../../shared/source/message_query.h"
#include "../../shared/source/runtime_memory.h"
extern "C" void* __cdecl memset(void*p,int c,size_t n){return assist::runtimememory::fill(p,c,n);}
extern "C" void* __cdecl memcpy(void*p,const void*q,size_t n){return assist::runtimememory::copy(p,q,n);}
extern "C" int __cdecl memcmp(const void*p,const void*q,size_t n){auto a=(const unsigned char*)p,b=(const unsigned char*)q;while(n--){if(*a!=*b)return *a-*b;++a;++b;}return 0;}
#include "input_patch.h" // Registry is needed by publishSnapshot(), defined below.
using namespace assist;
// Export concrete ABI data: an optimized mapName() may contain no contiguous
// string literal. The host validates both protocol and structure size before
// installing a hook; the release verifier can inspect these exports offline.
extern "C" __declspec(dllexport) const uint32_t AssistProtocolInfo[2]={Version,sizeof(Shared)};
extern "C" __declspec(dllexport) const MappingPrefixValue AssistMappingPrefix{};
static Shared*sh;static HANDLE mapping;static bool checked,good,fault;static DWORD last,slow;static uint32_t lastHouse,match,pendingAck;static int lastFrame;
static sessionepoch::Tracker sessionTracker;static Snapshot snap;
static worldlife::Tracker hookWorld;static bool commandDormant=false,worldCleanupPending=false;
static readonly::RuntimeState readOnlyState;static uint32_t commandComputeEpoch=0;
static runtimefault::Context faultContext;
extern "C" void AssistReportException(unsigned boundary,unsigned code,unsigned address,unsigned operation,unsigned access){
 RuntimeReportException(boundary,code,address,operation,access,sh&&sh->request!=sh->ack?sh->command:0,
  faultContext.phase,faultContext.source,faultContext.target);
}
struct FaultPhase {
 runtimefault::Context previous=faultContext;
 FaultPhase(unsigned phase,unsigned source=0,unsigned target=0){faultContext={phase,source,target};}
 ~FaultPhase(){faultContext=previous;}
};
static unsigned nativeMegaQueueCallDepth,nativeObjectQueueCallDepth; // own leased calls skip duplicate user-input conversion
template<class Fn>static void withNativeQueueCallDepth(unsigned&depth,Fn&&work){
 ++depth;runtimeunwind::run([&]{work();},[&]{--depth;});
}
static volatile LONG schedulerBusy;
static volatile LONG hostWorkPending; // one coalesced grant from the Runtime host-message ingress
static volatile LONG messageGameThread; // Thread confirmed by WH_GETMESSAGE for this game window.

static bool playableHouse(uint32_t);static bool enemyBuildingSellAllowed(uint32_t);static bool playerContextChanged;static void playerMaintenance();static void restorePlayerContext();static void publishPlayers();
enum class OrderAuthority { Local, PlayerTest, SellPlayerTest, DestructPlayerTest };
static bool authorizePlayerOrder(const intent::EventOrder&,OrderAuthority);
struct NativePlacementSubmission;
static bool validateNativePlacement(const intent::EventOrder&,const NativePlacementSubmission*);
static bool nativePlacementPacket(const vanilla::Packet&,const intent::EventOrder&);
static void nativePlacementOffers(DWORD);
static bool networkHousePreserveReady();static bool networkSenderHouse(int);static void networkSenderTick(bool);static bool restoreNetworkSender();
static void crcMapShadowTick();static void crcMapShadowRewind(uint32_t);static bool restoreCrcMapShadow();static bool crcMapToolBegin(uint32_t);static void crcMapToolEnd();
static bool controlCommandOwnerReady(uint32_t);
static void controlCommandOwnerTick();static void restoreControlCommandOwner();static void controlObjectOwnerTick();static void restoreControlObjectOwner();
static void nativeSellCursorTick();static void restoreNativeSellCursor();static void nativeMegaGatewayTick();static void restoreNativeMegaGateway();
static void resetRuntimePlanner();static void resetLoadCalculation();
static uint64_t catalogueHash;static DWORD catalogueAt,catalogueRequestedAt;static bool catalogueReady,catalogueDirty;
static external::catalogue::ProbeRetryGate nativeCatalogueProbe;
static void restoreFrameDriver();
static void resetDirectSell();static bool directSellPending();
static bool privateOutActive();static bool privateOutPending();static int privateOutRoom();
static bool privateOutQueue(const vanilla::Packet&,bool);static void privateOutTick();static void servicePrivateOut();static void restorePrivateOut();static bool privateOutRetired();
static void resetBombBoard();static bool validateBombBoardSubmission(const intent::EventOrder&);
static bool validateDirectAirSubmission(const intent::EventOrder&);
static void resetDirectAir();static bool directAirPending();
static void resetMoOrders();static bool moOrdersPending();static bool validateMoSubmission(const intent::EventOrder&);
static void restoreBuildTimeTooltip();
static void restoreViewZoom();static void restorePauseView();static void pauseViewTick();
static bool restoreSidebarView();static bool sidebarViewRendering();static void sidebarViewTick();
static void superweaponGrantReset();
static void nativePlacementReset();static void nativePlacementTick(DWORD);static void nativePlacementCompletionReset();
static repairdelivery::Source repairOrders[8192];
static int repairCount,repairAt,repairCursor;
static bool manualRepairPending,manualRepairNeedsInput;
static DWORD manualRepairStarted;
static void resetRepairBatch();
static commandsampling::SelectionReceipt selectionReceipt;
// Request serial zero is valid after wrap; pending state is explicit.
static bool pendingAckReady;
static bool releaseSticky(uint32_t);static bool stickyOwns(uint32_t);static void stickyTick(bool);static int stickyCount;
static int formationJobCount;static void resetFormation();static bool formationOwns(uint32_t);
static void pauseMissileForManual(uint32_t,uint32_t);static bool missileManualOverflowed();
static int missileHoldCount;static bool missileCleanup;static void resetMissileControl();static bool releaseMissileHolds(uint32_t,int,DWORD);
static int dodgeCount;static void resetDodgeControl();static bool dodgeOwns(uint32_t id);static void cancelDodge(uint32_t id);
static int burstCount;static void resetBurstControl();static bool burstOwns(uint32_t id);static void cancelBurst(uint32_t id);
static bool minerDefensePending(),minerDefenseOwns(uint32_t);static void minerDefenseReset(),minerDefenseForget(uint32_t);
static void minerDefenseRememberOre(const planner::Order&,uint32_t);
static void minerDefenseReindexManual(const ManualVersions&);
static bool repairCalculationPending;static DWORD repairCalculationStarted;static uint32_t repairTaskSeen;
static uint32_t lastIdle[3];static int prevBusy,prevPaused,prevCompleted;static bool productionBaseline;
static uint8_t techAllowed[MaxTypes];static bool enhancementChangedCatalogue=false;
static uint32_t typePointers[MaxTypes];
static ManualVersions manualVersions;static ObjectIndexView worldObjects;static int indexedFrame=-1,indexedCount=-1;static uint32_t indexedItems;
// Reuse the mandatory world-index traversal for global magnetron discovery.
// Input and planning still revalidate identities and native eligibility.
static int globalVehicleCandidateCount=0;static uint32_t globalVehicleCandidates[16384]{};static bool indexedGlobalVehicles=false;static uint32_t indexedVehicleHouse=0;
// The candidate list itself is worker-owned in async mode, not copied into
// another 64 KiB main-thread array on every accepted command snapshot.
static globalvehicle::Cursor globalVehicleScan;
static ObjectIndex advancedObjects;
static int outgoingFrame=-1,outgoingBudget;
static void recordReceipt(const gateway::Receipt&r){
 auto &s=snap.recentReceipts[snap.receiptWrite%RecentReceiptCapacity];
 s={r.serial,r.match,r.unit,r.target,r.queuedAt,r.observedAt,
    uint8_t(r.phase),uint8_t(r.failure),uint8_t(r.owner),0};
 ++snap.receiptWrite;
 if(snap.receiptCount<RecentReceiptCapacity)++snap.receiptCount;
}
static gateway::MissionObserver<1024> missionObserver;
static bool externalInputFrame=false;static int inputFrame=-1;static uint32_t inputMatch=0;
static int indexedInputFrame=-1;static uint32_t indexedInputMatch=0;
static bool indexedWorkDeferred=false;
static taskarbiter::FrameArbiter<8192> actorFrameArbiter;
static taskarbiter::LeaseArbiter<8192> actorLeaseArbiter;
static bool bridgePinned,autoWas;static DWORD lastAuto,techRefresh;
static repairdelivery::Pending<> pendingRepairs;
static void resetPointerInput();static void restoreClickHooks();static void resetCooperativeState();static void clickHooksTick(uint32_t);static void cooperativeTick(uint32_t,int,DWORD);
static void clearNativeOrders(bool retireHooks=true);static void restoreCSHook();static void restoreTech();static void clearFlashes();static void resetProduction();
static bool techLegacyEnhancementOwns(uint32_t),techLegacyEnhancementProductionPending(uint32_t);static void deferTechEnhancementParameters();
static bool restoreTechEnhancement();static bool restoreTechParameters(),techParametersRetired();static void maintainTechParameters();
static bool techDataPending();static int techEnhancementCatalogueLevel(uint32_t,int);static bool techEnhancementCatalogueIncludesInsignificant(uint32_t,uint32_t,int,int);
static bool restoreControlParameters(),controlParametersRetired();static int requestControlParameters();
static bool restoreTechEnhanceParameters(),techEnhanceParametersRetired();static void techEnhanceParametersTick(uint32_t,DWORD);
static bool featureRestorationComplete();
static void resetOwnGround();
static bool restoreMapVisibilityParameters(),mapVisibilityParametersRetired();
static bool restoreSinglePlayerFPS(),singlePlayerFPSRetired();static bool restoreTriggerInput();
static bool redrawRevealTest();
static void resetFeatureTypes();static void clearOreOrders();static void cancelOreJob(uint32_t p);static void restorePlacement();
static uint32_t worldByID(uint32_t id);static bool rawOreCompatible(uint32_t unit,uint32_t refinery);
static void cancelUnloadPassenger(uint32_t p);
enum class EventLane {Repair,Boarding,Unload,Planning};
static int recurringBudget(EventLane lane);
static int planningRoom(int channel);
static void consumePlanning(int channel);
struct CrateCache {int cellX,cellY,coords[3],preview;};
static CrateCache crateCache[MaxCrates];static int crateCount;static DWORD crateTick;static bool cratesOn;
static loading::Ledger<> loadLedger;
static int sceneFrame=-2147483647,sceneWidth,sceneHeight;static uint32_t sceneFlags,sceneTactical;
static DWORD sceneTick;static int cameraX,cameraY;static double sceneZoom;static uint32_t sceneResultSerial;
#include "world_frame_runtime.h"
template<class T> static T get(uint32_t p){if(sampleReadDepth){T v{};readWorldSample(p,&v,sizeof v);return v;}return *(T*)(uintptr_t)p;}
template<class T> static void put(uint32_t p,T v){if(sampleReadDepth){sampleReadComplete=false;return;}if(gameentry::active(gameentry::Client::Assist))*(T*)(uintptr_t)p=v;}
static bool readLiveWorld(uint32_t p,void*out,size_t n){SIZE_T got=0;return p>=0x10000&&uint64_t(p)+n<0x80000000ull&&ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)p,out,n,&got)&&got==n;}
static bool read(uint32_t p,void*out,size_t n){return sampleReadDepth?readWorldSample(p,out,n):readLiveWorld(p,out,n);}
template<class R,class...Args> static R call(uint32_t address,uint32_t self,Args...args){if(sampleReadDepth){sampleReadComplete=false;return R();}if(!gameentry::active(gameentry::Client::Assist))return R();return ((R(__thiscall*)(void*,Args...))(uintptr_t)address)((void*)(uintptr_t)self,args...);}
template<class R,class...Args> static R callFast(uint32_t address,Args...args){
 if(sampleReadDepth){sampleReadComplete=false;return R();}
 if(!gameentry::active(gameentry::Client::Assist))return R();
 return ((R(__fastcall*)(Args...))(uintptr_t)address)(args...);
}
struct Vec {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static Vec vec(uint32_t p,int limit=16384){Vec v{};if(!read(p,&v,sizeof v)||v.count<0||v.count>limit||v.count>v.capacity||v.items<0x10000)return {};return v;}
static uint32_t item(Vec v,int i){return i>=0&&i<v.count?get<uint32_t>(v.items+i*4):0;}
static bool live(uint32_t p){return p>=0x10000&&get<int>(p+0x6c)>0&&get<bool>(p+0x74)&&!get<bool>(p+0x81)&&get<bool>(p+0x90);}
static int kind(uint32_t p){uint32_t v=get<uint32_t>(p);return v==0x7F5C70?2:v==0x7EB058?1:v==0x7E3EBC?3:v==0x7E22A4?0:-1;}
static uint32_t typeOf(uint32_t p,int k){return k>=0?get<uint32_t>(p+(k==3?0x520:k==1?0x6C0:0x6C4)):0;}
static void name(uint32_t t,uint16_t*out,int n){uint32_t p=get<uint32_t>(t+0x60);int i=0;for(;i<n-1;++i){uint16_t c;if(!read(p+i*2,&c,2)||!c)break;out[i]=c;}out[i]=0;if(i==0)for(i=0;i<n-1&&i<23;++i){out[i]=get<unsigned char>(t+0x24+i);if(!out[i])break;}}
#include "hook_readiness.h"
#include "single_player_fps_runtime.h"
#include "build_time_tooltip.h"
static bool sameid(const char*a,const char*b){for(int i=0;i<24;++i){if(a[i]!=b[i])return false;if(!a[i])return true;}return true;}
static bool own(uint32_t p,uint32_t house){return live(p)&&get<uint32_t>(p+0x21c)==house;}
// An incomplete worker identity projection is UNKNOWN, not an empty world.
// No execution guard may rebuild the full live roster as a fallback.
// The worker-built identity projection is not a native EventClass grant.
// Its only lifetime is the currently sealed exact game logic frame. Every
// native operation still verifies live UID, House, mission and eligibility.
static bool commandRosterReady(){
 if(worldFrameCommandWorker.enabled())
  return worldFrameReady&&!worldFrameBoundaryOnly&&
   worldFrame.key.match==match&&worldFrame.key.frame==snap.frame&&
   worldFrameCommandRoster.current(worldFrame.key)&&
   worldFrameCommandRoster.global==bool(sh&&sh->magnetronGlobal)&&
   worldFrame.key.house==lastHouse;
 // The synchronous Runtime index is built in this exact game callback.
 // It proves membership only; every action still re-reads live UID, House,
 // mission and current eligibility immediately before native execution.
 return indexedFrame==snap.frame&&indexedCount>=0&&
  indexedVehicleHouse==lastHouse&&
  indexedGlobalVehicles==bool(sh&&sh->magnetronGlobal);
}

// The production background worker builds a full world index off-thread.
// Strict same-frame identity is still required for command SNAPSHOT authority,
// but delayed complete results may be used as candidates for UID discovery.
// Every candidate lookup is checked against the current live UID before any
// native command can be submitted. This is a bounded read, not a 16K scan.
static bool candidateRosterReady(){
 if(!worldFrameCandidateAvailable||!sh||!worldFrameCandidateRoster.objects)return false;
 const auto&source=worldFrameCandidateRoster.key;
 worldframe::Key current=source;
 current.pid=GetCurrentProcessId();current.owner=sh->owner;
 current.match=match;current.house=lastHouse;current.frame=snap.frame;
 current.connectionLow=uint32_t(worldFrameServer.connection());
 current.connectionHigh=uint32_t(worldFrameServer.connection()>>32);
 current.generationLow=uint32_t(worldProducerGeneration);
 current.generationHigh=uint32_t(worldProducerGeneration>>32);
 current.session=snap.session;
 return worldframe::acceptObservationCandidate(source,current,worldframe::Base,
          worldframe::Base,GetTickCount())&&
        worldFrameCandidateRoster.current(source)&&
        worldFrameCandidateRoster.global==bool(sh->magnetronGlobal);
}
static bool verifyCandidateID(uint32_t id,uint32_t pointer){
 if(sampleReadDepth){
  // Pure candidate conversion never calls the original native executor.
  // Avoid one live game-thread RPM for every lookup during large-world scans.
  return sampleWorldSource==&worldFrameCandidate&&
         worldFrameCandidateRoster.objects&&
         worldFrameCandidateRoster.objects->byId(id)==pointer;
 }
 uint32_t observed=0;
 // Outside the sealed read scope, including every final native order path,
 // re-read the live UID to reject despawn/reuse of a stale candidate address.
 return pointer>=0x10000&&readLiveWorld(pointer+0x10,&observed,sizeof observed)&&
        id&&observed==id;
}
static bool verifyCandidatePointer(uint32_t pointer){
 if(sampleReadDepth){
  // A sealed observation is not command authority. Preserve its immutable
  // membership without triggering an O(N) stream of live UID probes.
  return sampleWorldSource==&worldFrameCandidate&&
         worldFrameCandidateRoster.objects&&
         worldFrameCandidateRoster.objects->contains(pointer);
 }
 uint32_t observed=0;
 return pointer>=0x10000&&readLiveWorld(pointer+0x10,&observed,sizeof observed)&&
        observed&&worldFrameCandidateRoster.objects&&
        worldFrameCandidateRoster.objects->byId(observed)==pointer;
}
static void indexWorld(int frame){
 Vec all{};
 const auto unknown=[](){
  worldObjects.unbind();worldObjects.clear();indexedFrame=indexedCount=-1;
  indexedItems=0;globalVehicleCandidateCount=0;indexedGlobalVehicles=false;indexedVehicleHouse=0;
 };
 if(!read(0xA8EC78,&all,sizeof all)||all.count<0||all.count>16384||
    all.capacity<all.count||all.capacity>262144||(all.count&&all.items<0x10000)){
  unknown();return;
 }
 if(indexedFrame==frame&&indexedCount==all.count&&indexedItems==all.items&&
    indexedGlobalVehicles==bool(sh&&sh->magnetronGlobal)&&indexedVehicleHouse==lastHouse)return;
 // One bounded roster read. A failed header, item span or UID read is UNKNOWN,
 // never an empty world and never permission to use a partial index.
 static uint32_t pointers[16384];
 // CaptureGraph already decoded this exact pointer vector. Borrow it only
 // inside its original sealed read scope; never substitute it for live
 // native membership or for a different observation candidate.
 const uint32_t* roster=sampleReadDepth&&sampleWorldSource==&worldFrame?
  worldFrameCaptureScratch.capturedTechnoRoster(worldFrame.key,all.items,all.count):nullptr;
 if(!roster){
  if(all.count&&!read(all.items,pointers,size_t(all.count)*4)){unknown();return;}
  roster=pointers;
 }
 worldObjects.unbind();worldObjects.clear();globalVehicleCandidateCount=0;
 indexedGlobalVehicles=bool(sh&&sh->magnetronGlobal);indexedVehicleHouse=lastHouse;
 bool complete=true;
 for(int i=0;i<all.count;++i){
  const uint32_t p=roster[i];if(p<0x10000)continue;
  const bool native=runtimeDirectActions&&!sampleReadDepth;
  if(native&&p>0x7ffffddfu){complete=false;continue;}
  uint32_t id=0;
  if(native)id=get<uint32_t>(p+0x10);
  else if(!read(p+0x10,&id,4)){complete=false;continue;}
  if(id)worldObjects.add(p,id);
  if(id&&indexedGlobalVehicles&&kind(p)==2){
   uint32_t owner=0;
   if(native)owner=get<uint32_t>(p+0x21C);
   else if(!read(p+0x21C,&owner,4)){complete=false;continue;}
   if(owner==lastHouse)globalVehicleCandidates[globalVehicleCandidateCount++]=p;
  }
 }
 if(!complete||(sampleReadDepth&&!sampleReadComplete)){unknown();return;}
 indexedFrame=frame;indexedCount=all.count;indexedItems=all.items;
 static int prunedFrame=-100000;
 if((!sampleReadDepth||sampleReadComplete)&&manualVersions.used>=ManualVersions::Size/2&&
    (frame<prunedFrame||frame-prunedFrame>=300)){
  static ManualVersions liveVersions;
  liveVersions.copyLive(manualVersions,[](uint32_t id){return worldObjects.byId(id)!=0;});
  minerDefenseReindexManual(liveVersions);manualVersions=liveVersions;prunedFrame=frame;
 }
}
static bool refreshWorldIndex(int frame,bool executionGuards=true){
 static uint32_t failedMatch=0,failedAt=0;static int failedFrame=-1;
 if(worldFrameCommandWorker.enabled()){
  const bool strict=commandRosterReady()&&frame==worldFrame.key.frame;
  const bool candidate=!strict&&candidateRosterReady();
  if(!strict&&!candidate){
   worldObjects.clear();indexedFrame=indexedCount=-1;indexedItems=0;
   globalVehicleCandidateCount=0;return false;
  }
  static uint32_t adoptedSerial=0;static bool previouslyCandidate=false;
  const uint32_t version=candidate?worldFrameCandidateVersion:worldFrame.key.serial;
  if(indexedFrame!=frame||adoptedSerial!=version||previouslyCandidate!=candidate){
   if(candidate)
    worldObjects.bindCandidate(&worldFrameCandidateRoster.objects,candidateRosterReady,
                              verifyCandidateID,verifyCandidatePointer);
   else worldObjects.bind(&worldFrameCommandRoster.objects,commandRosterReady);
   const auto&roster=candidate?worldFrameCandidateRoster:worldFrameCommandRoster;
   indexedCount=roster.count;indexedItems=roster.items;
   indexedGlobalVehicles=roster.global;indexedVehicleHouse=lastHouse;
   globalVehicleCandidateCount=int(std::min<size_t>(16384,roster.globalVehicles.size()));
   indexedFrame=frame;adoptedSerial=version;previouslyCandidate=candidate;
  }
  static int prunedFrame=-100000;
  if(!candidate&&manualVersions.used>=ManualVersions::Size/2&&
     (frame<prunedFrame||frame-prunedFrame>=300)){
   static ManualVersions liveVersions;
   liveVersions.copyLive(manualVersions,[](uint32_t id){return worldObjects.byId(id)!=0;});
   minerDefenseReindexManual(liveVersions);manualVersions=liveVersions;prunedFrame=frame;
  }
  return true;
 }
 if(runtimeDirectActions){
  // A current native roster can be reused within the same frame; no world
  // graph or page lease is needed to resolve the IDs of live command actors.
  if(!executionGuards)return false;
  indexWorld(frame);return indexedFrame==frame&&commandRosterReady();
 }
 const uint32_t now=sampleClock(sh);
 // Prefer a newly sealed same-frame Runtime sample even if a prior direct
 // fallback failed less than 16ms ago. The throttle applies only to another
 // expensive live-roster fallback, never to fresh authoritative evidence.
 if(worldFrameReady&&!worldFrameBoundaryOnly){
  bool complete=false;
  {SampleReadScope sample;indexWorld(frame);complete=sample.complete();}
  if(complete&&indexedFrame==frame)return true;
 }
 if(failedMatch==match&&failedFrame==frame&&uint32_t(now-failedAt)<16)return false;
 worldObjects.unbind();worldObjects.clear();indexedFrame=indexedCount=-1;
 indexedItems=0;globalVehicleCandidateCount=0;
 if(!executionGuards)return false;
 indexWorld(frame);
 if(indexedFrame==frame)return true;
 failedMatch=match;failedFrame=frame;failedAt=now;return false;
}

// Identity-index publication already checked every UID. Avoid a second
// fallible live UID read when deciding whether an automatic lease still exists.
static void sweepActiveLeases(int frame){
 if(indexedFrame!=frame||!commandRosterReady()||!actorLeaseArbiter.count)return;
 // Tiny cohorts are deliberately rechecked on each callback: a manual
 // revision or a newly complete identity index can change within the SAME
 // game frame. The large-cohort budget must not regress that native priority.
 const auto liveLease=[&](uint32_t id){
  return std::pair<uint32_t,uint32_t>{worldObjects.byId(id),manualVersions.get(id)};
 };
 if(actorLeaseArbiter.count<=8)actorLeaseArbiter.sweep(match,frame,liveLease);
 else actorLeaseArbiter.sweepBudgeted(match,frame,liveLease);
}
// Mission receipts must not turn a failed mission/queue read into a valid
// default-zero observation (e.g. a spurious mission 0 acknowledgement).
static gateway::MissionState observeMissionReceipt(uint32_t id){
 if(indexedFrame!=snap.frame||!commandRosterReady())return {};
 const uint32_t p=worldObjects.byId(id);if(!p)return {};
 uint32_t currentID=0,carrier=0;int mission=-1,queued=-1;
 if(!read(p+0x10,&currentID,sizeof currentID)||!read(p+0xAC,&mission,sizeof mission)||
    !read(p+0xB4,&queued,sizeof queued)||!read(p+0x11C,&carrier,sizeof carrier)||currentID!=id)return {};
 return {true,mission,queued,carrier};
}

// Never dereference an old/stale worker roster from a native command path.
static uint32_t globalVehicleCandidateAt(int index){
 if(index<0||index>=globalVehicleCandidateCount)return 0;
 if(!worldFrameCommandWorker.enabled())return indexedFrame==snap.frame?globalVehicleCandidates[index]:0;
 const bool strict=commandRosterReady();
 if(!strict&&!candidateRosterReady())return 0;
 const auto&roster=strict?worldFrameCommandRoster:worldFrameCandidateRoster;
 if(size_t(index)>=roster.globalVehicles.size())return 0;
 const uint32_t pointer=roster.globalVehicles[size_t(index)];
 return !strict&&!worldObjects.contains(pointer)?0:pointer;
}

// Native hook maintenance is still permitted to use exact per-object
// admission, but must never rebuild a 16K-object roster when the dedicated
// command capture worker owns all graph reads. A missing worker index is
// UNKNOWN: caller skips this observation; native game events stay untouched.
static bool prepareWorldIndex(int frame){
 if(worldFrameCommandWorker.enabled())return refreshWorldIndex(frame,false);
 indexWorld(frame);
 return indexedFrame==frame;
}

#include "../../shared/source/key_gesture.h"
static keygesture::Tracker runtimeKeys;
static void reset(bool retireHooks=true,bool preserveMissile=false){retireWorldFrame();restorePrivateOut();minerDefenseReset();restoreTriggerInput();restoreSinglePlayerFPS();resetOwnGround();superweaponGrantReset();restoreMapVisibilityParameters();restoreControlParameters();restoreTechEnhanceParameters();restoreTechParameters();nativePlacementReset();if(retireHooks)nativePlacementCompletionReset();restorePauseView();resetMoOrders();resetDirectAir();resetDirectSell();resetBombBoard();if(retireHooks){restoreSidebarView();resetHookReadiness();restoreCrcMapShadow();restoreBuildTimeTooltip();restorePrivateOut();restoreNativeSellCursor();restoreNativeMegaGateway();restoreControlObjectOwner();restoreControlCommandOwner();restoreNetworkSender();restoreTechEnhancement();enhancementChangedCatalogue=false;stickyTick(true);restoreViewZoom();}stickyCount=0;runtimeKeys.clear();resetLoadCalculation();resetRuntimePlanner();nativeCatalogueProbe.reset();catalogueReady=false;catalogueHash=0;catalogueAt=0;catalogueDirty=true;resetFeatureTypes();resetPointerInput();if(sh&&sh->pointer.request!=sh->pointer.ack){sh->pointer.result=-7;sh->pointer.accepted=0;sh->pointer.rejected=sh->pointer.intent.count;MemoryBarrier();sh->pointer.ack=sh->pointer.request;}nativeMegaQueueCallDepth=nativeObjectQueueCallDepth=0;snap.lastSubmitError=0;missionObserver.clear();actorFrameArbiter.clear();actorLeaseArbiter.clear();snap.megaQueueStatus=0;snap.megaQueueAccepted=snap.megaQueueRejected=0;snap.objectQueueStatus=0;snap.objectQueueAccepted=snap.objectQueueRejected=0;
 snap.queueSent=snap.queueFull=snap.queueTimingFailure=snap.planDeferred=snap.queuedEvents=snap.observedEvents=snap.commandRejected=snap.receiptCancelled=snap.receiptExpired=snap.receiptDropped=0;snap.receiptWrite=snap.receiptCount=0;if(retireHooks)restoreClickHooks();resetCooperativeState();resetFormation();if(!preserveMissile)resetMissileControl();resetBurstControl();resetDodgeControl();if(retireHooks&&sh)sh->clickHookStatus=0;clearOreOrders();if(retireHooks)restorePlacement();indexedFrame=indexedCount=-1;globalVehicleScan.reset();outgoingFrame=-1;clearNativeOrders(retireHooks);snap.labels=snap.selected=snap.transports=snap.used=snap.capacity=snap.swCount=0;snap.unloadIssued=0;if(retireHooks){snap.nativeHookStatus=0;snap.techOpen=0;restoreTech();restoreCSHook();}clearFlashes();resetProduction();if(retireHooks)snap.inputPatchMask=0;resetRepairBatch();manualRepairPending=manualRepairNeedsInput=false;selectionReceipt.clear();for(auto&v:lastIdle)v=0;productionBaseline=false;snap.typeCount=0;snap.threatCount=snap.crateCount=crateCount=0;cratesOn=false;autoWas=false;snap.autoRepair=0;snap.repairTotal=0;if(retireHooks){snap.techVisible=snap.techUnavailable=0;snap.techError=techDataPending()?-44:0;}snap.crateHookStatus=snap.crateResultCount=0;snap.crateResultSerial=0;sceneResultSerial=0;if(retireHooks)snap.csHookStatus=0;snap.csConverted=0;loadLedger.clear();snap.loadUnits=snap.loadVehicles=snap.loadIssued=snap.loadWaiting=snap.loadUnassigned=0;if(retireHooks)memset(techAllowed,0,sizeof techAllowed);pendingRepairs.clear();}
static void mapName(wchar_t*out,DWORD p){mappingName(out,p);}
static HANDLE controllerProcess;static DWORD controllerPID,controllerCheckAt;
static void disconnect(){
 // reset attempts EVERY restoration even if one group must defer. Keep each
 // group's world identity and the transport until all ownership has retired.
 worldFrameServer.disconnect();restorePlayerContext();reset();
 // In particular, raw direct patches have no disabled callback to fall back
 // through. Do not acknowledge stop/unmap IPC while any feature still owns one.
 if(!featureRestorationComplete()||!worldFramePublication.stop()||!worldFrameShadow.stop()||!worldFrameCommandWorker.stop())return;
 if(sh&&sh->magic==Magic&&sh->version==Version&&sh->pid==GetCurrentProcessId()&&sh->stop){MemoryBarrier();sh->stop=2;}
 InterlockedExchange(&hostWorkPending,0);if(controllerProcess)CloseHandle(controllerProcess);controllerProcess=nullptr;controllerPID=controllerCheckAt=0;restoreFrameDriver();InterlockedExchange(&messageGameThread,0);sessionTracker.update(false,0,-1,-1);hookWorld.reset();commandDormant=worldCleanupPending=false;readOnlyState={};commandComputeEpoch=0;if(sh)UnmapViewOfFile(sh);sh=nullptr;if(mapping)CloseHandle(mapping);mapping=nullptr;lastHouse=pendingAck=0;pendingAckReady=false;
 // The pinned DLL outlives a Host connection. Only completed retirement and
 // transport teardown release its legacy fault latch for a fresh connection.
 fault=false;
}
#include "native_entry_runtime.h"
static_assert(Signatures[8].address==startupcompat::BeaconEntry,"ClearBeacons signature index");
static_assert(Signatures[9].address==startupdependency::RevealEntry,"Map::Reveal signature index");
static bool startupSignatureStrict(const Signature&s){
 unsigned char b[12];if(s.length>sizeof b||!read(s.address,b,s.length))return false;
 if(!memcmp(b,s.bytes,s.length))return true;
 // Keep the original native entry and its verified platform dispatch.
 // Raw instruction/DR consumers do not inherit ordinary-call profiles.
 if(nativeOrdinaryEntryKnown(s.address))return nativeOrdinaryEntryCallable(s.address,true);
 if(s.address!=startupcompat::BeaconEntry)return false;
 const uint32_t module=uint32_t(uintptr_t(GetModuleHandleW(L"MRGA1.DLL")));uint32_t target=0;
 if(!startupcompat::beaconTarget(b,s.length,module,target))return false;
 IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS32 nt{};
 if(!read(module,&dos,sizeof dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<64||dos.e_lfanew>4096||
    !read(module+uint32_t(dos.e_lfanew),&nt,sizeof nt)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_I386||
    nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC||nt.OptionalHeader.SizeOfImage!=startupcompat::ModuleBytes)return false;
 MEMORY_BASIC_INFORMATION region{};const size_t bytes=startupcompat::BeaconBytes;
 if(VirtualQuery((void*)(uintptr_t)target,&region,sizeof region)!=sizeof region||region.State!=MEM_COMMIT||region.Type!=MEM_IMAGE||
    uintptr_t(region.AllocationBase)!=module||(region.Protect&PAGE_GUARD)||
    !(region.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||
    uint64_t(target)+bytes>uint64_t(uintptr_t(region.BaseAddress))+region.RegionSize)return false;
 unsigned char body[startupcompat::BeaconBytes];
 return read(target,body,sizeof body)&&startupcompat::beaconBody(body,sizeof body,module);
}
static bool nativeVerifiedCanBuildDependency(uint32_t address){
 if(address!=nativeentry::CanBuild)return false;
 for(const auto&signature:Signatures)if(signature.address==address)return startupSignatureStrict(signature);
 return false;
}
// Ordinary calls have their own setting; DR consumers still use the strict
// nativeEntryCallable() evidence and their separate Breakpoint identity.
static bool startupSignature(const Signature&s){
 uint8_t observed[12]{};
 const bool readable=s.length<=sizeof observed&&nativeExecutableRange(s.address,s.length)&&read(s.address,observed,s.length);
 if(!readable){const bool accepted=debugentries::allow(debugentries::Kind::Call,s.address,false,false);nativeOrdinaryReport(s.address,accepted,false);return accepted;}
 if(!debugentries::signatureRequired(debugentries::Kind::Call,s.address)){
  const bool accepted=debugentries::allow(debugentries::Kind::Call,s.address,true,false);nativeOrdinaryReport(s.address,accepted,false);return accepted;
 }
 const bool exact=!memcmp(observed,s.bytes,s.length),compatible=exact||startupSignatureStrict(s);
 const bool accepted=debugentries::allow(debugentries::Kind::Call,s.address,true,compatible,exact?debugentries::Signature::Match:debugentries::Signature::Platform);
 nativeOrdinaryReport(s.address,accepted,!exact);return accepted;
}
// Cold feature/command admission. Recheck failures as well as successes so
// transient platform loading never latches a local feature off forever.
static nativeentry::TimedResult nativeDependencyCache[sizeof(Signatures)/sizeof(Signatures[0])];
static bool nativeDependencyReady(uint32_t address,bool refresh=false){
 for(size_t i=0;i<sizeof(Signatures)/sizeof(Signatures[0]);++i)if(Signatures[i].address==address){
  static uint32_t revision=0;if(revision!=debugentries::policyRevision){revision=debugentries::policyRevision;for(auto&value:nativeDependencyCache)value={};}
 const DWORD now=GetTickCount();auto&cache=nativeDependencyCache[i];
  if(!refresh&&cache.fresh(now))return cache.value;
  return cache.set(now,startupSignature(Signatures[i]));
 }
 return false;
}
template<class... More>static bool nativeDependenciesReady(uint32_t first,More... rest){
 return nativeDependencyReady(first)&&(...&&nativeDependencyReady(uint32_t(rest)));
}
static bool revealSignature(){return startupSignature(Signatures[9]);}
static bool signatures(){return startupdependency::coreReady(Signatures,[](const Signature&s){return startupSignature(s);});}
static void notifyHost(){
 if(!sh||!sh->hostWindow)return;
 if(!InterlockedCompareExchange(&sh->hostNotifyPending,1,0)&&!PostMessageW((HWND)(uintptr_t)sh->hostWindow,DataMessage,sh->pid,0))InterlockedExchange(&sh->hostNotifyPending,0);
}
static uint32_t micros(LARGE_INTEGER from,LARGE_INTEGER to,LARGE_INTEGER frequency){return frequency.QuadPart>0?uint32_t((to.QuadPart-from.QuadPart)*1000000/frequency.QuadPart):0;}
static void syncDebugPolicy(DWORD now){
 patchcontext::verifiedCallDependency=nativeVerifiedCanBuildDependency;
 debugentries::executableProbe=nativeExecutableRange;debugentries::clockTick=now;
 if(!sh)return;
 for(unsigned attempt=0;attempt<3;++attempt){
  const LONG before=InterlockedCompareExchange(&sh->debugPolicySeq,0,0);if(before&1)continue;
  debugentries::Config next;memcpy(&next,&sh->debugPolicy,sizeof next);MemoryBarrier();
  if(before==InterlockedCompareExchange(&sh->debugPolicySeq,0,0)){debugentries::applyPolicy(next);break;}
 }
}
static void publishDebugEntries(DWORD now){
 static DWORD lastAt=0;static uint32_t revision=0;
 if(lastAt&&DWORD(now-lastAt)<500&&revision==debugentries::policyRevision)return;
 lastAt=now;revision=debugentries::policyRevision;debugentries::clockTick=now;
 if(sh->debugVisible){
  // Probes only read instructions. Opening Debug never installs an optional Hook.
  for(const auto&s:Signatures)startupSignature(s);
  for(const auto&e:debugentries::Entries)if(e.kind==debugentries::Kind::Patch)gameentry::inspect(e.address,true);
 }
 InterlockedIncrement(&sh->debugSnapshotSeq);
 debugentries::snapshot(sh->debugSnapshot,now);MemoryBarrier();InterlockedIncrement(&sh->debugSnapshotSeq);
}
static void publishSnapshot(DWORD now){
 sh->missileManualOverflow=missileManualOverflowed()?1u:0u;
 publishDebugEntries(now);
 if(readonly::commands(sh)&&!readOnlyState.active)nativePlacementOffers(now);else snap.placementOfferCount=0;
 bool copyTypes=catalogueDirty||sh->seq==0||sh->data.match!=snap.match||sh->data.typeCount!=snap.typeCount;
 const auto entryState=gameentry::inspect();
 snap.hookConflicts=int(entryState.conflicts);snap.hookInstalled=int(entryState.installed);
 snap.entryThread=entryState.gameThread;snap.entryCalls=entryState.entries;
 snap.entryReentrant=entryState.reentrant;snap.entryContended=entryState.contended;snap.entryWrongThread=entryState.wrongThread;
 snap.sampledTick=now;InterlockedIncrement(&sh->seq);publishPlayers();
 // Only copy populated arrays. The live type table is static within a match.
 memcpy(&sh->data,&snap,offsetof(Snapshot,label));
 memcpy(sh->data.label,snap.label,size_t(snap.labels)*sizeof(Label));
 memcpy(sh->data.supers,snap.supers,size_t(snap.swCount)*sizeof(Super));
 if(copyTypes){memcpy(sh->data.types,snap.types,size_t(snap.typeCount)*sizeof(Type));catalogueDirty=false;}
 sh->data.threatCount=snap.threatCount;sh->data.crateCount=snap.crateCount;
 memcpy(sh->data.threats,snap.threats,size_t(snap.threatCount)*sizeof(Threat));memcpy(sh->data.crates,snap.crates,size_t(snap.crateCount)*sizeof(CrateMark));
 memcpy(&sh->data.techVisible,&snap.techVisible,sizeof(Snapshot)-offsetof(Snapshot,techVisible));
 MemoryBarrier();InterlockedIncrement(&sh->seq);
 if(pendingAckReady){MemoryBarrier();sh->ack=pendingAck;pendingAckReady=false;}
 notifyHost();
}
using Event=vanilla::Packet;
static_assert(sizeof(Event)==111,"YRpp NetworkEvent ABI mismatch");
static int outgoingRoom(){
 int frame=get<int>(0xA8ED84);if(frame!=outgoingFrame){outgoingFrame=frame;outgoingBudget=64;}
 const int mode=get<int>(0xA8B238);
 if(outgoingBudget<=0||!eventdelivery::mayQueue(mode))return 0;
 if(privateOutActive()){int room=privateOutRoom();return room<outgoingBudget?room:outgoingBudget;}
 // One interface, session-specific transport: MO OrderUtil uses DoList for
 // SP and OutList for MP. Features cannot choose or write a queue themselves.
 if(eventdelivery::isSingleplayer(mode)){
  int count=get<int>(0x8B41F8),next=get<int>(0x8B4200);
  if(!eventdelivery::singleRoom(count,next))return 0;
  int room=eventdelivery::SingleCapacity-count;return room<outgoingBudget?room:outgoingBudget;
 }
 int count=get<int>(0xA802C8),tail=get<int>(0xA802D0);
 if(!queueAvailable(count,tail))return 0;
 int room=112-count;return room<outgoingBudget?room:outgoingBudget;
}
// YRpp Imports::TimeGetTime / MO OrderUtil::OU_SndTimings, IAT 0x7E1530.
// A real C call boundary tells the compiler that EAX/ECX/EDX and flags are
// volatile. Keep legacy nonvolatile-register protection inside this thunk;
// never hold a C++ reference/register allocation across an opaque inline call.
__declspec(naked) static uint32_t __cdecl invokeGameClock(uint32_t callback){
 __asm {
  mov eax,[esp+4]
  push ebx
  push esi
  push edi
  push ebp
  call eax
  pop ebp
  pop edi
  pop esi
  pop ebx
  ret
 }
}
static bool gameSendTiming(uint32_t& result){
 uint32_t callback=get<uint32_t>(0x7E1530u);
 if(callback<0x10000u)return false;
 MEMORY_BASIC_INFORMATION mbi{};
 if(!VirtualQuery((const void*)(uintptr_t)callback,&mbi,sizeof(mbi))||mbi.State!=MEM_COMMIT||
    (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 DWORD protection=mbi.Protect&0xFF;
 if(protection!=PAGE_EXECUTE&&protection!=PAGE_EXECUTE_READ&&
    protection!=PAGE_EXECUTE_READWRITE&&protection!=PAGE_EXECUTE_WRITECOPY)return false;
 result=invokeGameClock(callback);
 return true;
}
static bool simulationReady(bool allowNewerFrame=false,bool allowDisplayHidden=false){
 const bool hidden=allowDisplayHidden&&snap.commandStatus==activation::DisplayHidden;
 if(!gameentry::active(gameentry::Client::Assist)||!sh||((sh->stop||!readonly::commands(sh)||readOnlyState.active)&&!missileCleanup)||fault||
    snap.status!=1||(snap.commandStatus!=1&&!hidden&&!missileCleanup)||!match||!lastHouse||
    (DWORD(sampleClock(sh)-sh->heartbeat)>=3000&&!missileCleanup)||!get<bool>(0xA8E9A0))return false;
 const uint32_t house=get<uint32_t>(0xA83D4C);const int frame=get<int>(0xA8ED84),session=get<int>(0xA8B238);
 if(house!=lastHouse||house==get<uint32_t>(0xAC1198)||get<bool>(house+0x1F5)||
    get<bool>(house+0x1F6)||get<bool>(house+0x1F8)||frame<0||frame>=INT32_MAX||
    (allowNewerFrame?frame<snap.frame:frame!=snap.frame)||session!=snap.session||!eventdelivery::mayQueue(session))return false;
 return hidden||external::display::inspect([](uint32_t at,void*out,size_t size){
  memcpy(out,(const void*)(uintptr_t)at,size);return true;
 }).visible();
}
#include "sell_single_bridge.h"
static bool enqueue(Event&e,const intent::EventOrder& order,OrderAuthority authority,const NativePlacementSubmission* placement=nullptr,bool* responded=nullptr){
 if(!simulationReady()||get<bool>(0xA8BC60+25)){++snap.commandRejected;return false;}
 if(!validateMoSubmission(order)||!validateBombBoardSubmission(order)||!validateDirectAirSubmission(order)){snap.lastSubmitError=7;++snap.commandRejected;return false;}
 if(!validateNativePlacement(order,placement)||!nativePlacementPacket(e,order)){
  snap.lastSubmitError=43;++snap.commandRejected;return false;
 }
 if(e.kind==0x16&&authority==OrderAuthority::SellPlayerTest&&eventdelivery::isSingleplayer(get<int>(0xA8B238))){
  const uint32_t object=worldByID(uint32_t(order.a));
  if(object&&!sellHumanHouse(get<uint32_t>(object+0x21C))){
   if(!authorizePlayerOrder(order,authority)||!executeSinglePlayerAISell(e,object)){++snap.commandRejected;return false;}
   snap.lastSubmitError=0;++snap.queuedEvents;return true;
  }
 }
 if(!eventdelivery::mayQueue(get<int>(0xA8B238))){++snap.commandRejected;return false;}
 if(!outgoingRoom()){snap.lastSubmitError=6;++snap.queueFull;return false;}
 // Fail closed if the game callback cannot be resolved; never fabricate its
 // output using a Windows wall-clock timer.
 uint32_t timing=0;
 if(!gameSendTiming(timing)){snap.lastSubmitError=9;++snap.queueTimingFailure;return false;}
 // The native timer is an external call boundary. Recheck identity/frame before
 // copying the packet; never route a constructed event into a changed session.
 if(!simulationReady()||get<bool>(0xA8BC60+25)){++snap.commandRejected;return false;}
 if(!validateMoSubmission(order)||!validateBombBoardSubmission(order)||!validateDirectAirSubmission(order)){snap.lastSubmitError=7;++snap.commandRejected;return false;}
 if(!validateNativePlacement(order,placement)||!nativePlacementPacket(e,order)){
  snap.lastSubmitError=43;++snap.commandRejected;return false;
 }
 if(authority==OrderAuthority::PlayerTest&&!authorizePlayerOrder(order,authority)){
  // The original clock is an external boundary: the actor may have changed
  // Owner, or the explicit control/transport option may have been disabled.
  ++snap.commandRejected;return false;
 }
 if(e.kind==vanilla::EventType::Destruct){
  // NoOptions carries only a House index. Keep the explicit pointer/ID/index
  // request valid across the external clock callback before final delivery.
  intent::EventOrder verify{};verify.domain=vanilla::Domain::NoOptions;
  verify.eventKind=vanilla::EventType::Destruct;verify.player=e.house;
  if(!authorizePlayerOrder(verify,OrderAuthority::DestructPlayerTest)){
   if(snap.lastSubmitError!=42&&snap.lastSubmitError!=7)snap.lastSubmitError=1;
   ++snap.commandRejected;return false;
  }
 }
 if(!privateOutActive()&&eventdelivery::isMultiplayer(get<int>(0xA8B238))&&sh&&sh->networkHousePreserve&&
    int(e.house)!=get<int>(lastHouse+0x30)&&
    (!networkHousePreserveReady()||!networkSenderHouse(int(e.house)))){
  snap.lastSubmitError=42;++snap.commandRejected;return false;
 }
 // The original DoList scheduler visits only current/human Houses. A valid
 // singleplayer AI Destruct would otherwise remain unexecuted indefinitely.
 // Keep its original packet and receiver; only this explicit SP operation may
 // use immediate response on the already leased simulation thread.
 if(e.kind==vanilla::EventType::Destruct&&authority==OrderAuthority::DestructPlayerTest&&
    eventdelivery::isSingleplayer(get<int>(0xA8B238))){
  Vec houses=vec(0xA80228,32);uint32_t target=item(houses,int(e.house));uint8_t current=0,human=0;
  if(!read(target+0x1EC,&current,1)||!read(target+0x1ED,&human,1)){snap.lastSubmitError=7;++snap.commandRejected;return false;}
  if(!current&&!human){
   static const uint8_t receiver[]={0x81,0xEC,0x70,0x03,0x00,0x00,0x33,0xC0,0x53,0x55,0x56,0x8B,0xF1};
   if(!debugentries::pattern(debugentries::Kind::Call,0x4C6CB0,receiver,sizeof receiver,read)){snap.lastSubmitError=12;++snap.commandRejected;return false;}
   call<void>(0x4C6CB0,uint32_t(uintptr_t(&e)));
   if(!get<bool>(target+0x1F6)){snap.lastSubmitError=6;++snap.commandRejected;return false;}
   if(responded)*responded=true;
   snap.lastSubmitError=0;--outgoingBudget;return true;
  }
 }
 if(privateOutActive()){if(!privateOutQueue(e,false))return false;--outgoingBudget;++snap.queueSent;++snap.queuedEvents;return true;}
 constexpr uint32_t p=0xA802C8;
 int count=get<int>(p),tail=get<int>(p+8);
 if(eventdelivery::isMultiplayer(get<int>(0xA8B238))&&queueAvailable(count,tail)){
  // MP copies the original constructor packet unchanged into OutList.
  memcpy((void*)(p+12+tail*sizeof(Event)),&e,sizeof e);
  put<uint32_t>(p+12+128*sizeof(Event)+tail*4,timing);
  put<int>(p+8,(tail+1)&127);put<int>(p,count+1);
 }else if(eventdelivery::isSingleplayer(get<int>(0xA8B238))){
  constexpr uint32_t countAddr=0x8B41F8,nextAddr=0x8B4200;
  int spCount=get<int>(countAddr),next=get<int>(nextAddr);
  if(!eventdelivery::singleRoom(spCount,next)){++snap.queueFull;return false;}
  next&=eventdelivery::SingleMask;
  Event local=e;
  uint32_t hp=get<uint32_t>(0xA83D4C);
  if(hp<0x10000u){++snap.queueTimingFailure;return false;}
  eventdelivery::prepareSingle(local.executed,local.house,local.frame,
   e.house,get<int>(0xA8ED84)); // retain validated event owner, including explicit player tests
  memcpy((void*)(0x8B4204u+unsigned(next)*sizeof(Event)),&local,sizeof local);
  put<uint32_t>(0xA70204u+unsigned(next)*4u,timing);
  put<int>(nextAddr,(next+1)&eventdelivery::SingleMask);
  put<int>(countAddr,spCount+1);
 }else{++snap.queueFull;return false;}
 if(authority==OrderAuthority::PlayerTest&&sh->controlCommandOwner)++sh->controlCommandOwnerAccepted;
 snap.lastSubmitError=0;--outgoingBudget;++snap.queueSent;++snap.queuedEvents;return true;
}
static entrygate::Snapshot currentEntryBoundary(){
 const DWORD confirmedGameThread=DWORD(InterlockedCompareExchange(&messageGameThread,0,0));
 const int session=get<int>(0xA8B238);
 return entrygate::Snapshot{match,match,session,
  simulationReady(),
  get<bool>(0xA8BC60+25),
  confirmedGameThread&&GetCurrentThreadId()==confirmedGameThread&&gameentry::active(gameentry::Client::Assist),
  outgoingRoom()>0};
}
// ENTRY: the only ordinary EventClass construction-and-submit channel.
// Caller / planner supplies a semantic intent, never a raw address or packet.
// This does not replace the two separately verified high-level append backends.
static bool submitConstructed(const intent::EventOrder& order,OrderAuthority authority=OrderAuthority::Local,const NativePlacementSubmission* placement=nullptr,bool* responded=nullptr){
 if(!validateMoSubmission(order)||!validateBombBoardSubmission(order)||!validateDirectAirSubmission(order)){snap.lastSubmitError=7;++snap.commandRejected;return false;}
 if(!authorizePlayerOrder(order,authority)){++snap.commandRejected;return false;}
 if(!validateNativePlacement(order,placement)){snap.lastSubmitError=43;++snap.commandRejected;return false;}
 const entrygate::Snapshot boundary=currentEntryBoundary();
 const auto gate=entrygate::admit(boundary);
 if(gate!=entrygate::Outcome::Pass){
  snap.lastSubmitError=gate==entrygate::Outcome::Stale?1:
    gate==entrygate::Outcome::UnsupportedSession?2:
    gate==entrygate::Outcome::NoRoom?6:!boundary.active?3:boundary.chat?4:5;
  ++snap.commandRejected;return false;
 }
 // No speculative UI/probe operations: only previously supported game orders
 // reach an engine call. The other YRpp constructors are exposed as ABI
 // descriptors for offline tests, not remotely callable game commands.
 if(constructors::admit(order)!=constructors::Verdict::Ready){snap.lastSubmitError=7;++snap.commandRejected;return false;}
 // These four historical startup probes belong to the selected packet ABI.
 // Other domains retain their own consumer guards; no unknown entry is
 // admitted through nativeDependencyReady or this classification.
 const uint32_t constructor=vanilla::address(order.domain);
 if(order.eventKind!=0x16&&startupdependency::eventConstructor(constructor)&&!nativeDependencyReady(constructor)&&!(constructor==0x4C6860&&privateOutActive())){
  snap.lastSubmitError=8;++snap.commandRejected;return false;
 }
 Event packet{};
 if(order.domain==vanilla::Domain::Object&&order.eventKind==0x16){
  if(!sellpacket::event(packet,order.player,uint32_t(order.a),uint32_t(snap.frame)))return false;
  return enqueue(packet,order,authority,placement,responded);
 }
 const auto state=constructors::fill(packet,order,
  [](uint32_t address,uint32_t self,auto... args){call<void>(address,self,args...);});
 if(state!=constructors::Verdict::Ready){snap.lastSubmitError=8;++snap.commandRejected;return false;}
 return enqueue(packet,order,authority,placement,responded); // SP DoList / MP OutList, timing and receipt centrally.
}
// Original game QueueMegaMission verified in two user-provided 32-bit EXEs.
// x86 __fastcall ABI (NOT the EventClass ctor):
// ECX=mission, EDX=pointer to a 5-byte abstract target,
// stack=(sourceID, sourceKind, pointer to abstract destination), ret 0x0C.
// It calls 0x4C6860, copies 111 bytes to the *game's own* OutList,
// obtains native send timing, and updates ring pointers itself.
// NEVER hand-write OutList after a successful native call.
static bool nativeMegaSignature(){
 static const uint8_t prologue[]={0x56,0x57,0x8B,0x7C,0x24,0x7C,0x83,0xEC,0x08,0x8B,0xF4,
  0x83,0xEC,0x08,0x89,0x3E,0x88,0x46,0x04};
 static const uint8_t ctorCall[]={0xE8,0x6B,0xF9,0xE7,0xFF}; // call 0x4C6860
 static const uint8_t queueCopy[]={0xB9,0x1B,0x00,0x00,0x00,0xF3,0xA5};
 static const uint8_t nativeTimer[]={0xFF,0x15,0x30,0x15,0x7E,0x00};
 static const uint8_t nativeReturn[]={0xC2,0x0C,0x00};
 return memcmp((const void*)0x646E95,prologue,sizeof prologue)==0
  && memcmp((const void*)0x646EF0,ctorCall,sizeof ctorCall)==0
  && memcmp((const void*)0x646F16,queueCopy,sizeof queueCopy)==0
  && memcmp((const void*)0x646F20,nativeTimer,sizeof nativeTimer)==0
  && memcmp((const void*)0x646F55,nativeReturn,sizeof nativeReturn)==0;
}
static bool nativeMegaOriginalOrOwned(); // implemented after the CS hook definitions
static bool nativeMegaTimingAvailable(){
 uint32_t callback=get<uint32_t>(0x7E1530u);if(callback<0x10000u)return false;
 MEMORY_BASIC_INFORMATION mbi{};
 if(!VirtualQuery((const void*)(uintptr_t)callback,&mbi,sizeof mbi)||mbi.State!=MEM_COMMIT||
    (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
 DWORD protection=mbi.Protect&0xFF;
 return protection==PAGE_EXECUTE||protection==PAGE_EXECUTE_READ||
        protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
}
static bool submitMegaMission(uint32_t house,uint32_t id,int mission,
 uint32_t target,uint8_t targetTag,uint32_t destination,uint8_t destinationTag,
 uint32_t nearest,uint8_t nearestTag){
 const auto selected=sh&&sh->megaQueueMode==1 ? megaqueue::Backend::GameQueueMegaMission
                                            : megaqueue::Backend::ConstructAndEnqueue;
 if(privateOutActive()||selected==megaqueue::Backend::ConstructAndEnqueue||
    eventdelivery::isSingleplayer(get<int>(0xA8B238))){
  // Default/SP backend: original constructor, then the central session transport.
  snap.megaQueueStatus=int(megaqueue::Result::DefaultEventClass);
  return submitConstructed(intent::mega(get<int>(house+0x30),id,mission,target,targetTag,
    destination,destinationTag,nearest,nearestTag));
 }
 const int mode=get<int>(0xA8B238);
 megaqueue::Capability cap{};
 cap.isMultiplayer=eventdelivery::isMultiplayer(mode);
 cap.isSingleplayer=eventdelivery::isSingleplayer(mode);
 // Own CS Hook can be chained; unknown detours are always rejected.
 cap.engineSignature=nativeMegaSignature();
 cap.knownOriginalOrOwnedHook=nativeMegaOriginalOrOwned();
 cap.timingCallbackExecutable=nativeMegaTimingAvailable();
 auto result=megaqueue::admit(selected,cap,nearest,nearestTag);
 if(result!=megaqueue::Result::Queued){snap.megaQueueStatus=int(megaqueue::Result::DefaultEventClass);
   return submitConstructed(intent::mega(get<int>(house+0x30),id,mission,target,targetTag,
     destination,destinationTag,nearest,nearestTag));}
 const entrygate::Outcome gateOutcome=entrygate::admit(currentEntryBoundary());
 if(gateOutcome!=entrygate::Outcome::Pass){
  if(gateOutcome==entrygate::Outcome::NoRoom){
   snap.megaQueueStatus=int(megaqueue::Result::QueueFull);++snap.queueFull;
  }else{
   snap.megaQueueStatus=int(megaqueue::Result::NotReady);
  }
  ++snap.megaQueueRejected;++snap.commandRejected;return false;
 }
 const megaqueue::Target targetArg{target,targetTag,{0,0,0}};
 const megaqueue::Target destinationArg{destination,destinationTag,{0,0,0}};
 using Native=bool (__fastcall*)(int,const megaqueue::Target*,uint32_t,uint32_t,const megaqueue::Target*);
 const int beforeCount=get<int>(0xA802C8),beforeTail=get<int>(0xA802D0);
 bool success=false;withNativeQueueCallDepth(nativeMegaQueueCallDepth,[&]{
  success=((Native)(uintptr_t)0x646E90)(mission,&targetArg,id,0x34,&destinationArg);
 });
 const int afterCount=get<int>(0xA802C8),afterTail=get<int>(0xA802D0);
 if(!megaqueue::appended(beforeCount,beforeTail,afterCount,afterTail)){
  snap.megaQueueStatus=int(megaqueue::Result::NoAppend);
  ++snap.megaQueueRejected;++snap.commandRejected;return false;
 }
 // A physical append is the only delivery claim. Treat a surprising
 // false return plus a proven append as queued, never retry the same order.
 (void)success;
 --outgoingBudget;++snap.queueSent;++snap.queuedEvents;++snap.megaQueueAccepted;
 snap.megaQueueStatus=int(megaqueue::Result::Queued);
 return true;
}
// The high-level virtual call is guarded at invocation. We never jump through
// a foreign detour or chain our own 0x6FFE00 input hook from this path.
static bool nativeObjectUnhooked(); // definition follows input-hook state
static bool nativeObjectSignature(){
 static const unsigned char original[]={0x83,0xEC,0x78,0x56,0x57};
 // This original prologue is also captured in our input-hook module.
 return memcmp((const void*)0x6FFE00,original,sizeof original)==0;
}
static bool loadInterfaces();
static void cancelReceiptTicket(uint32_t session,uint32_t ticket){
 missionObserver.cancelTicket(session,ticket,gateway::Failure::Superseded,
  [](const gateway::Receipt& receipt){recordReceipt(receipt);++snap.receiptCancelled;});
}
static void releaseAutoOwner(uint32_t actor){
 int index=actorLeaseArbiter.find(actor);
 if(index>=0){auto old=actorLeaseArbiter.leases[index];cancelReceiptTicket(old.match,old.serial);}
 actorLeaseArbiter.manual(actor);
}
// Unified game-thread command gateway. All existing features submit normal
// EventClass orders here; no event is executed directly on just one peer.
// The selected native constructor is verified before filling its packet.
// Original display-only action-line timer (25 simulation frames). Verified
// against both supported EXEs: no unit mission/target writes and no event.
// Let the original renderer draw later; never draw into game surfaces here.
static void showOrderLines(uint32_t object){
 if(!gameentry::active(gameentry::Client::Assist)||!get<bool>(object+0x83))return;
 static const uint8_t code[]={0xA1,0x84,0xED,0xA8,0x00,0xB9,0x19,0,0,0,
  0xA3,0x80,0xEA,0xB0,0,0x8B,0x44,0x24,0xF8,0x83,0xEC,0x0C,
  0xA3,0x84,0xEA,0xB0,0,0x89,0x0D,0x88,0xEA,0xB0,0,0x83,0xC4,0x0C,0xC3};
 uint8_t actual[sizeof code];
 if(read(0x70D150,actual,sizeof actual)&&!memcmp(actual,code,sizeof code))
  call<void>(0x70D150,object);
}
static bool nativeMission(uint32_t house,uint32_t id,int mission,uint32_t target,
                          uint32_t dest,int destTag,uint32_t nearest=0,int nearestTag=0,
                          taskarbiter::Owner owner=taskarbiter::Owner::Generic,uint32_t* receiptID=nullptr,
                          int targetTag=-1,OrderAuthority authority=OrderAuthority::Local,bool appendWaypoint=false,bool cooperativeTakeover=false,bool forceConstructed=false,
                          const executionproof::Ticket* proof=nullptr){
 const bool foreign=authority==OrderAuthority::PlayerTest&&sh&&controlmode::proxyOrders(sh->controlAll)&&playableHouse(house);
 if(!id||!house||(house!=lastHouse&&!foreign)||!eventdelivery::mayQueue(get<int>(0xA8B238))||!loadInterfaces())return false;
 const uint8_t resolvedTargetTag=targetTag>=0?uint8_t(targetTag):(target?0x34:0);
 const uint32_t submittedAt=sampleClock(sh);
 const gateway::Context ctx{match,submittedAt,sh&&((!sh->stop&&readonly::commands(sh)&&!readOnlyState.active)||missileCleanup)&&!fault&&snap.status==1,
                            get<bool>(0xA8BC60+25)};
 const gateway::Intent intent{match,submittedAt,id,target,manualVersions.get(id)};
 if(gateway::validate(ctx,intent,3500)!=gateway::Failure::None){++snap.commandRejected;return false;}
 // The command gateway, not just the planner, owns the final game-thread
 // actor/ownership preflight. Never serialize a vanished/foreign actor ID.
 uint32_t object=worldByID(id);
 if(stickyOwns(object))return false; // Explicit held units belong to the player.
 if(minerDefenseOwns(id)&&owner!=taskarbiter::Owner::HarvesterDefense&&taskarbiter::priority(owner)<=taskarbiter::priority(taskarbiter::Owner::HarvesterDefense))return false;
 uint32_t targetObject=(target&&resolvedTargetTag==0x34)?worldByID(target):0;
 const bool targetCellValid=(resolvedTargetTag==11)&&vanilla::validTargetRef(int(target),vanilla::TargetTag::Cell);
 uint32_t destObject=(destTag==0x34&&dest)?worldByID(dest):0;
 auto reject=orderpolicy::missionWithTargetTag(object!=0,object&&own(object,house),object&&live(object),
  mission,resolvedTargetTag,target,targetObject!=0,targetCellValid,destTag==0x34,destObject!=0);
 if(reject!=orderpolicy::Reject::None){++snap.commandRejected;return false;}
 // The original planner's proof is optional on legacy/manual callers, but
 // mandatory for migrated delayed planners. A candidate never grants authority.
 // Verify UID->current pointer, original manual token, House, object-bound
 // target identity and the actual requested event parameters on Game thread.
 if(proof){
  const auto orderFailure=executionproof::ordered(*proof,id,target,resolvedTargetTag,dest,uint8_t(destTag));
  const auto liveFailure=orderFailure==executionproof::Failure::None?
   executionproof::verify(*proof,{match,house,submittedAt},
      [](uint32_t uid){return worldByID(uid);},
      [](uint32_t uid){return manualVersions.get(uid);},
      [](uint32_t p,uint32_t h){return own(p,h);}):orderFailure;
  if(liveFailure!=executionproof::Failure::None){
   snap.lastSubmitError=7;++snap.commandRejected;return false;
  }
 }
 const int frame=get<int>(0xA8ED84);
 actorFrameArbiter.begin(match,frame);
 // An explicitly captured limited aircraft click must leave failed/unused
 // actors alone. Replace old autonomous ownership only AFTER the new event
 // physically queues; early cancelNative would change every candidate.
 const bool takeover=cooperativeTakeover&&owner==taskarbiter::Owner::Cooperative&&authority==OrderAuthority::Local&&house==lastHouse&&mission==1&&!appendWaypoint;
 if(!actorFrameArbiter.canIssue(id,manualVersions.get(id))||
    (!actorLeaseArbiter.canIssue(match,frame,id,object,manualVersions.get(id),owner)&&!(takeover&&actorLeaseArbiter.find(id)>=0))){
  ++snap.commandRejected;return false;
 }
 gateway::MissionState before{};
 before.valid=true;before.mission=get<int>(object+0xAC);
 before.queued=get<int>(object+0xB4);before.carrier=get<uint32_t>(object+0x11C);
 if(foreign||appendWaypoint||forceConstructed){auto order=intent::mega(get<int>(house+0x30),id,mission,target,resolvedTargetTag,dest,uint8_t(destTag),nearest,uint8_t(nearestTag));order.appendWaypoint=appendWaypoint;if(!submitConstructed(order,authority))return false;}
 else if(!submitMegaMission(house,id,mission,target,resolvedTargetTag,
                       dest,uint8_t(destTag),nearest,uint8_t(nearestTag)))return false;
 showOrderLines(object);
 actorFrameArbiter.record(id,manualVersions.get(id),owner);
 // A queued EventClass is not an executed mission. This read-only tracker
 // records the later state transition; unsupported events stay unconfirmed.
 uint32_t ticket=missionObserver.record(match,id,destTag==0x34?dest:target,mission,
                             frame,submittedAt,before,owner);
 taskarbiter::Lease superseded{};
 if(takeover){int old=actorLeaseArbiter.find(id);if(old>=0){superseded=actorLeaseArbiter.leases[old];actorLeaseArbiter.remove(old);}
  actorLeaseArbiter.record(match,frame,id,object,manualVersions.get(id),owner,ticket);
 }else actorLeaseArbiter.record(match,frame,id,object,manualVersions.get(id),owner,ticket,&superseded);
 if(superseded.serial)cancelReceiptTicket(superseded.match,superseded.serial);
 if(receiptID)*receiptID=ticket;
 if(!ticket)++snap.receiptDropped;
 else {gateway::Receipt queued{};
  gateway::Intent ticketIntent{match,submittedAt,id,destTag==0x34?dest:target,manualVersions.get(id)};
  gateway::planned(queued,ticket,ticketIntent);gateway::submitted(queued,submittedAt);
  queued.owner=owner;recordReceipt(queued);
 }
 return true;
}
// Typed execution path for delayed planners. Native callers still use the
// unchanged vanilla EventClass backend. Every proof is checked only inside
// nativeMission before an event is constructed or enqueued.
static bool submitProvenMission(uint32_t house,uint32_t id,int mission,uint32_t target,
  uint32_t destination,int destinationTag,taskarbiter::Owner owner,
  const executionproof::Ticket&proof,uint32_t*receipt=nullptr,
  uint32_t nearest=0,int nearestTag=0){
 return nativeMission(house,id,mission,target,destination,destinationTag,nearest,nearestTag,
   owner,receipt,-1,OrderAuthority::Local,false,false,false,&proof);
}
static bool sellAllowed(uint32_t);
#include "sell_direct_bridge.h"
static bool nativeObjectEvent(int houseIndex,int eventType,uint32_t objectId,OrderAuthority authority=OrderAuthority::Local,gateway::Receipt* receipt=nullptr,bool hazardDrop=false){
 if(houseIndex<0||houseIndex>=32||!objectId||!lastHouse){++snap.commandRejected;return false;}
 uint32_t object=worldByID(objectId);
 uint32_t owner=object?get<uint32_t>(object+0x21C):0;
 uint32_t localIndex=get<uint32_t>(lastHouse+0x30);
 uint32_t ownerIndex=owner>=0x10000?get<uint32_t>(owner+0x30):0xFFFFFFFFu;
 const int mode=get<int>(0xA8B238);
 if(!eventdelivery::mayQueue(mode)){++snap.commandRejected;return false;}
 const bool mp=eventdelivery::isMultiplayer(mode);
 auto reject=orderpolicy::objectEvent(eventType,object!=0,object&&live(object),
  uint32_t(houseIndex)==localIndex||(authority==OrderAuthority::SellPlayerTest&&eventType==0x16&&enemyBuildingSellAllowed(object)),uint32_t(houseIndex)==ownerIndex,mp);
 if(reject!=orderpolicy::Reject::None||(eventType==0x07&&kind(object)!=1&&kind(object)!=2)){++snap.commandRejected;return false;}
 const uint32_t submittedAt=sampleClock(sh);
 const gateway::Context ctx{match,submittedAt,sh&&((!sh->stop&&readonly::commands(sh)&&!readOnlyState.active)||missileCleanup)&&!fault&&snap.status==1,
                            get<bool>(0xA8BC60+25)};
 const uint32_t rev=manualVersions.get(objectId);
 const gateway::Intent objIntent{match,submittedAt,objectId,0,rev};
 if(gateway::validate(ctx,objIntent,3500)!=gateway::Failure::None){++snap.commandRejected;return false;}
 const int frame=get<int>(0xA8ED84);
 const auto objOwner=eventType==0x06?(hazardDrop?taskarbiter::Owner::MagnetronDrop:taskarbiter::Owner::Magnetron):eventType==0x07?taskarbiter::Owner::Dodge:eventType==0x16?taskarbiter::Owner::Sell:taskarbiter::Owner::Repair;
 actorFrameArbiter.begin(match,frame);
 if(!actorFrameArbiter.canIssue(objectId,rev)||
    !actorLeaseArbiter.canIssue(match,frame,objectId,object,rev,objOwner)){
  ++snap.commandRejected;return false;
 }
 auto acceptedObject=[&](){
  actorFrameArbiter.record(objectId,rev,objOwner);
  gateway::Receipt queued=gateway::makeQueuedReceipt(missionObserver.nextSerial,objIntent,submittedAt,objOwner);
  taskarbiter::Lease superseded{};
  actorLeaseArbiter.record(match,frame,objectId,object,rev,objOwner,queued.serial,&superseded);
  if(superseded.serial)cancelReceiptTicket(superseded.match,superseded.serial);
  recordReceipt(queued);if(receipt)*receipt=queued;
  return true;
 };
 if(eventType==0x16&&mp&&!privateOutActive()){
  if(!queueDirectSell(intent::object(houseIndex,eventType,objectId),authority,object))return false;
  return acceptedObject();
 }
 if(authority!=OrderAuthority::Local){if(!submitConstructed(intent::object(houseIndex,eventType,objectId),authority))return false;return acceptedObject();}
 if(privateOutActive()||eventType==0x06||eventType==0x07||eventType==0x16||!sh||sh->objectQueueMode!=1||eventdelivery::isSingleplayer(mode)){
  snap.objectQueueStatus=int(objectqueue::Result::DefaultEventClass);
  if(!submitConstructed(intent::object(houseIndex,eventType,objectId)))return false;
  return acceptedObject();
 }
 const uint32_t vtable=object?get<uint32_t>(object):0;
 uint32_t method=0;
 const bool tableReadable=vtable>=0x10000u&&
   read(vtable+objectqueue::ClickedEventVtableOffset,&method,sizeof method);
 const entrygate::Snapshot boundary=currentEntryBoundary();
 objectqueue::Capability cap{};
 cap.multiplayer=mp;
 cap.validEvent=eventType==0x15||eventType==0x16;
 cap.actorOwnedAlive=object&&live(object)&&kind(object)==3&&own(object,lastHouse);
 cap.signature=nativeObjectSignature()||nativeObjectUnhooked();
 cap.unhooked=nativeObjectUnhooked();
 cap.vtableValid=tableReadable&&method==objectqueue::ClickedEventVanilla;
 cap.sameGameThread=boundary.gameThread;
 cap.ready=boundary.active&&!boundary.chat&&loadInterfaces();
 cap.hasRoom=boundary.queueRoom;
 auto decision=objectqueue::admit(objectqueue::Backend::ClickedEvent,cap);
 if(decision!=objectqueue::Result::Appended){
  if(decision==objectqueue::Result::HookConflict||decision==objectqueue::Result::UnsupportedSession||decision==objectqueue::Result::SignatureMismatch){
   snap.objectQueueStatus=int(objectqueue::Result::DefaultEventClass);
   if(!submitConstructed(intent::object(houseIndex,eventType,objectId)))return false;
   return acceptedObject();
  }
  snap.objectQueueStatus=int(decision);
  ++snap.objectQueueRejected;if(decision==objectqueue::Result::QueueFull)++snap.queueFull;
  else ++snap.commandRejected;return false;}
 // Use the YRpp virtual API, not an arbitrary raw event buffer. In these
 // supported builds the vtable slot +0x374 resolves to vanilla 0x6FFE00.
 // NetworkEvents is unsigned char; x86 thiscall passes a 32-bit stack slot.
 const int preCount=get<int>(0xA802C8),preTail=get<int>(0xA802D0);
 using NativeClicked=bool (__thiscall*)(void*,uint32_t);
 bool accepted=false;withNativeQueueCallDepth(nativeObjectQueueCallDepth,[&]{
  accepted=((NativeClicked)(uintptr_t)method)((void*)(uintptr_t)object,uint32_t(eventType));
 });
 const int postCount=get<int>(0xA802C8),postTail=get<int>(0xA802D0);
 if(!objectqueue::appended(preCount,preTail,postCount,postTail)){
  // DO NOT retry through handcrafted enqueue! A return of true can hide
  // a full native queue, or there may be an incompatible runtime detour.
  snap.objectQueueStatus=int(objectqueue::Result::NoAppend);
  ++snap.objectQueueRejected;++snap.commandRejected;
  return false;
 }
 (void)accepted; // Real append outranks the engine's unreliable bool.
 --outgoingBudget;++snap.queueSent;++snap.queuedEvents;++snap.objectQueueAccepted;
 snap.objectQueueStatus=int(objectqueue::Result::Appended);
 return acceptedObject();
}
#include "construction_sidebar_bridge.h"
static bool nativeProduceEvent(uint32_t house,const Type&t,bool nativeCameo=false,int eventKind=0x0E){
 const int mode=get<int>(0xA8B238);
 if(!house||!eventdelivery::mayQueue(mode))return false;
 // Technology Beta uses native cameos and UI-only eligibility gates.
 // Event execution and factory allocation stay native on every peer.
 const bool mp=eventdelivery::isMultiplayer(mode);
 const bool unsafeAllowed=(sh&&sh->flags&UnsafeTech)!=0;
 if(mp&&!unsafeAllowed&&!nativeCameo)return false;
 intent::EventOrder order{};
 const decisions::ProductionView view{get<int>(house+0x30),t.kind,t.index,t.naval,true,mp,unsafeAllowed,nativeCameo};
 const bool planned=decisions::planProduction(view,order)&&(eventKind==0x0E||eventKind==0x10);order.eventKind=eventKind;
 if(orderpolicy::productionEvent(planned,mp,unsafeAllowed,nativeCameo)!=orderpolicy::Reject::None){++snap.commandRejected;return false;}
 const uint32_t submittedAt=sampleClock(sh);
 const uint32_t synthActor=taskarbiter::syntheticProductionActor(t.kind,t.index);
 const gateway::Context ctx{match,submittedAt,sh&&((!sh->stop&&readonly::commands(sh)&&!readOnlyState.active)||missileCleanup)&&!fault&&snap.status==1,get<bool>(0xA8BC60+25)};
 const gateway::Intent prodIntent{match,submittedAt,synthActor,uint32_t(t.index),0};
 if(gateway::validate(ctx,prodIntent,3500)!=gateway::Failure::None){++snap.commandRejected;return false;}
 const int frame=get<int>(0xA8ED84);
 actorFrameArbiter.begin(match,frame);
 if(!actorFrameArbiter.canIssue(synthActor,0)){++snap.commandRejected;return false;}
 if(!submitConstructed(order))return false;
 actorFrameArbiter.record(synthActor,0,taskarbiter::Owner::Production);
 recordReceipt(gateway::makeQueuedReceipt(missionObserver.nextSerial,prodIntent,submittedAt,taskarbiter::Owner::Production));
 return true;
}
static bool repairEvent(uint32_t house,uint32_t id){return nativeObjectEvent(get<int>(house+0x30),0x15,id);}
#include "feature_type_bridge.h"
#include "autoload_bridge.h"
static bool selection(uint32_t p){return nativeDependencyReady(0x6FBFA0)&&call<bool>(0x6FBFA0,p);}
static void unselect(){if(gameentry::active(gameentry::Client::Assist)&&nativeDependencyReady(0x48DC90))((void(__cdecl*)())0x48DC90)();}
static int selectFilter(uint32_t house,int action){
 static uint32_t chosen[4096],ids[4096];int n=0;
 if(!commandRosterReady())return -12;
 const auto collect=[&]()->bool{
 Vec v{};const uint32_t source=action==EmptyTransports?0xA8EC78:0xA8ECB8;
 if(!sampleRoster(source,v,source==0xA8EC78?16384:4096))return false;
 for(int i=0;i<v.count&&n<4096;++i){
  const uint32_t p=item(v,i);if(!p||!own(p,house))continue;
  const int k=kind(p);if(k<0||k==3)continue;const uint32_t t=typeOf(p,k);if(!t)continue;
  const bool harv=k==2&&(get<bool>(t+0xE0E)||get<bool>(t+0xE0F));
  const bool eng=k==1&&get<bool>(t+0xEC3);
  const bool ok=action==CombatSelection?(!harv&&!eng&&get<bool>(t+0xDBC)):
   action==EmptyTransports?(get<int>(t+0x5E0)>0&&get<int>(p+0x114)==0):
   get<int>(p+0x6c)<get<int>(t+0xA0);
  if(ok){const uint32_t id=get<uint32_t>(p+0x10);if(id){chosen[n]=p;ids[n++]=id;}}
 }
 return true;
 };
 if(runtimeDirectActions){if(!collect())return -12;}
 else {SampleReadScope sample;if(!sample||!collect()||!sample.complete())return -12;}
 if(!n)return 0;if(!nativeDependenciesReady(0x48DC90,0x6FBFA0))return -12;
 unselect();int selected=0;
 for(int i=0;i<n;++i)if(worldByID(ids[i])==chosen[i]&&own(chosen[i],house)&&selection(chosen[i]))++selected;
 return selected;
}
static int findIdle(uint32_t house,int mode){
 static uint32_t lastIdle[3],firstPointer[3],firstID[3],nextPointer[3],nextID[3];
 const int slot=mode-IdleHarvester;if(slot<0||slot>=3)return -4;
 firstPointer[slot]=nextPointer[slot]=0;firstID[slot]=0xffffffffu;nextID[slot]=0xffffffffu;
 if(!commandRosterReady())return -12;
 const auto collect=[&]()->bool{
 Vec all{};if(!sampleRoster(0xA8EC78,all))return false;
 for(int i=0;i<all.count;++i){
  const uint32_t p=item(all,i);if(!p||!own(p,house))continue;
  const int k=kind(p);if(k<0||k==3)continue;const uint32_t t=typeOf(p,k);if(!t)continue;
  const bool category=slot==0?(k==2&&(get<bool>(t+0xE0E)||get<bool>(t+0xE0F))):
   slot==1?(k==1&&get<bool>(t+0xEC3)):(get<int>(t+0x5E0)>0&&get<int>(p+0x114)==0);
  if(!category||!idleMission(get<int>(p+0xAC),get<int>(p+0xB4),get<uint32_t>(p+0x2B4)!=0,get<bool>(p+0x1C8)))continue;
  const uint32_t id=get<uint32_t>(p+0x10);if(!id)continue;
  if(id<firstID[slot]){firstID[slot]=id;firstPointer[slot]=p;}
  if(id>lastIdle[slot]&&id<nextID[slot]){nextID[slot]=id;nextPointer[slot]=p;}
 }
 return true;
 };
 if(runtimeDirectActions){if(!collect())return -12;}
 else {SampleReadScope sample;if(!sample||!collect()||!sample.complete())return -12;}
 uint32_t next=nextPointer[slot],nextid=nextID[slot];
 auto stillIdle=[&](uint32_t p,uint32_t id){
  if(!p||worldByID(id)!=p||!own(p,house))return false;
  return idleMission(get<int>(p+0xAC),get<int>(p+0xB4),get<uint32_t>(p+0x2B4)!=0,get<bool>(p+0x1C8));
 };
 if(!stillIdle(next,nextid)){next=firstPointer[slot];nextid=firstID[slot];if(!stillIdle(next,nextid))return 0;}
 const uint32_t tactical=get<uint32_t>(0x887324);
 if(!nativeDependenciesReady(0x48DC90,0x6FBFA0)||(tactical&&!nativeDependencyReady(0x6D2420)))return -12;
 unselect();if(!selection(next))return 0;lastIdle[slot]=nextid;
 if(tactical)call<void>(0x6D2420,tactical,(void*)(next+0x9C),0);return 1;
}
static void requestTypeCatalogue(){
 worldFramePublication.invalidateCatalogue();
 nativeCatalogueProbe.reset();catalogueReady=false;catalogueRequestedAt=GetTickCount();worldFrameCatalogueProbeAt=0;
}
static bool scanTypes(bool direct=false,const std::array<std::vector<external::catalogue::TypeProbe>,4>* probes=nullptr){
 if(probes&&!direct)return false;
 if(direct?sampleReadDepth!=0:(!sampleReadDepth||!sampleReadComplete))return false;
 struct Projection {Type types[MaxTypes];uint32_t pointers[MaxTypes];uint8_t allowed[MaxTypes];
  std::array<std::pair<uint32_t,int>,MaxTypes> priorByPointer;};
 static Projection next;
 // Preserve the first previous entry for a duplicate pointer, exactly like
 // the old linear scan. Build the lookup once per catalogue projection.
 const int priorCount=std::min<int>(MaxTypes,std::max<int>(0,snap.typeCount));
 auto& priorByPointer=next.priorByPointer;
 for(int prior=0;prior<priorCount;++prior)priorByPointer[prior]={typePointers[prior],prior};
 std::sort(priorByPointer.begin(),priorByPointer.begin()+priorCount);
 int count=0;
 const auto&arrays=external::catalogue::TypeTables;
 const int kinds[]={3,16,40,7};
 static uint32_t nativeTypes[4096];
 for(int k=0;k<4;++k){
  Vec types{};
  if(direct){
   // A failed header is not an empty type vector. Retain the previous
   // published catalogue until the full native pointer list is available.
   if(!read(arrays[k],&types,sizeof types)||types.count<0||types.count>4096||
      types.capacity<types.count||types.capacity>65536||
      (types.count&&(types.items<0x10000||uint64_t(types.items)+uint64_t(types.count)*4>=0x80000000ull)))return false;
   if(probes){
    const auto&batch=(*probes)[k];
    if(batch.size()!=size_t(types.count)||(!batch.empty()&&batch.front().items!=types.items))return false;
   }else if(count<MaxTypes&&types.count&&!read(types.items,nativeTypes,size_t(types.count)*4))return false;
  }else types=vec(arrays[k],4096);
  for(int i=0;i<types.count&&count<MaxTypes;++i){
   const auto*probe=probes?&(*probes)[k][i]:nullptr;
   const uint32_t type=probe?probe->pointer:direct?nativeTypes[i]:item(types,i);
   if(type<0x10000)return false;
   int level=0,cost=0;uint8_t insignificantRaw=0,spawned=0,naval=0;uint32_t owners=0,localizedName=0;
   if(probe){
    level=probe->level;owners=probe->owners;insignificantRaw=probe->insignificant;spawned=probe->spawned;
   }else if(direct){
    // A failed metadata read is not a false filter bit or a zero price.
    // Read the nearby cost/level fields together and stage all results.
    uint32_t costLevel[(0x634-0x610)/4+1]{};
    if(!read(type+0x610,costLevel,sizeof costLevel)||
       !read(type+0x232,&insignificantRaw,1)||!read(type+0xD54,&spawned,1)||
       !read(type+0x6CC,&owners,4))return false;
    memcpy(&cost,costLevel,4);memcpy(&level,costLevel+(0x634-0x610)/4,4);
   }else{
    level=get<int>(type+0x634);insignificantRaw=get<bool>(type+0x232);
    spawned=get<bool>(type+0xD54);owners=get<uint32_t>(type+0x6CC);
   }
   const bool insignificant=insignificantRaw&&!techEnhancementCatalogueIncludesInsignificant(type,arrays[k],i,level);
   if(!validType(techEnhancementCatalogueLevel(type,level),insignificant,spawned!=0,owners))continue;
   Type& out=next.types[count];out={};
   if(probe){
    memcpy(out.id,probe->id,23);
    if(!read(type+0x610,&cost,4))return false;
   }else if(!read(type+0x24,out.id,23))return false;
   if(direct){
    if(!read(type+0x60,&localizedName,4)||!read(type+0xCCE,&naval,1))return false;
   }else{
    localizedName=get<uint32_t>(type+0x60);naval=get<bool>(type+0xCCE);cost=get<int>(type+0x610);
   }
   const bool isolated=!direct&&sampleWorldSource->catalogue.isolated;
   if(localizedName>=0x10000&&(direct||isolated)){
    // Reuse the already read name pointer. Most names fit in a single
    // contiguous read. Fall back to the original bounded character walk
    // when the speculative suffix crosses an unreadable region.
    uint16_t text[39]{};
    const auto readName=[&](uint32_t at,void*out,size_t size){
     return isolated?sampleWorldSource->read(at,out,size):read(at,out,size);
    };
    int n=0;
    if(readName(localizedName,text,sizeof text)){
     while(n<39&&text[n]){out.name[n]=text[n];++n;}
    }else{
     for(;n<39;++n){uint16_t ch=0;if(!readName(localizedName+uint32_t(n)*2,&ch,2)||!ch)break;out.name[n]=ch;}
    }
    if(!n)for(int i=0;i<23&&out.id[i];++i)out.name[i]=uint8_t(out.id[i]);
   }else if(localizedName>=0x10000)name(type,out.name,40);
   else for(int n=0;n<23&&out.id[n];++n)out.name[n]=uint8_t(out.id[n]);
   out.kind=kinds[k];out.index=i;out.naval=naval!=0;out.cost=cost;
   next.pointers[count]=type;next.allowed[count]=0;
   const auto prior=std::lower_bound(priorByPointer.begin(),priorByPointer.begin()+priorCount,std::make_pair(type,0));
   if(prior!=priorByPointer.begin()+priorCount&&prior->first==type)next.allowed[count]=techAllowed[prior->second];
   ++count;
  }
  if(direct){
   Vec after{};
   if(!read(arrays[k],&after,sizeof after)||after.items!=types.items||
      after.count!=types.count||after.capacity!=types.capacity)return false;
  }
 }
 if(!direct&&!sampleReadComplete)return false;
 std::copy_n(next.types,count,snap.types);std::copy_n(next.pointers,count,typePointers);
 std::fill(std::begin(techAllowed),std::end(techAllowed),0);std::copy_n(next.allowed,count,techAllowed);
 snap.typeCount=count;catalogueDirty=true;return true;
}
#include "sidebar.h"
#include "tech_enhancement_bridge.h"
#include "tech_data_bridge.h"
#include "type_select_bridge.h"
static uint32_t rawObjectID(uint32_t pointer);
#include "raw_combat.h"
#include "combat_query_bridge.h"
static bool rawArmed(uint32_t p){int k=kind(p);uint32_t type=typeOf(p,k);if(!type)return false;int slots[2],count=rawWeaponSlots(p,type,slots);return count&&rawWeapon(type,get<float>(p+0x150),slots[0])>=0x10000;}
#include "native_orders_bridge.h"
#include "auto_cs_bridge.h"
static bool nativeMegaOriginalOrOwned(){static const uint8_t original[]={0x83,0xEC,0x70,0x32,0xC0};return memcmp((const void*)0x646E90,original,sizeof original)==0;}
#include "native_input_hooks.h"
static bool nativeObjectUnhooked(){return nativeObjectSignature();}
#include "anti_theft_bridge.h"
#include "harvester_defense_raw.h"
#include "external_orders_bridge.h"
#include "harvester_defense_bridge.h"
#include "player_test_bridge.h"
#include "control_house_flags_bridge.h"
#include "input_lock_bridge.h"
#include "placement_bridge.h"
#include "network_sender_bridge.h"
#include "crc_shadow_bridge.h"
#include "native_sell_cursor_bridge.h"
#include "control_command_owner_bridge.h"
#include "control_object_owner_bridge.h"
#include "native_mega_gateway_bridge.h"
#include "private_outlist_bridge.h"
#include "small_map_camera_bridge.h"
#include "radar_navigation_bridge.h"
#include "scroll_refresh_bridge.h"
#include "sidebar_view_bridge.h"
#include "view_zoom_bridge.h"
#include "native_pip_bridge.h"
#include "pause_view_bridge.h"
#include "tech_ui_breakpoint_bridge.h"
#include "cooperative_bridge.h"
#include "pointer_bridge.h"
#include "sticky_bridge.h"
#include "attack_ground_bridge.h"
#include "selected_mode_bridge.h"
static bool controlCompanionsRetired(){
 for(const auto&s:controlCommandOwnerSites)if(codeHookRegistry.active(s.address))return false;
 if(codeHookRegistry.active(controlObjectOwnerSite.address)||networkSenderInstalled||networkSenderPacking||networkSenderCacheDirty)return false;
 for(const auto&s:networksender::Sites)if(codeHookRegistry.active(s.address))return false;
 return true;
}
static bool featureRestorationComplete(){
 // 55DE81 belongs to Runtime's independent read-only frame notification
 // channel. It has its own cleanup and must not deadlock Assist detachment.
 const unsigned runtimeOnly=(codeHookRegistry.active(0x55DE81)?1u:0u)+
  (!buildTimeTooltipActive&&codeHookRegistry.active(0x7F74D8)?1u:0u)+
  (codeHookRegistry.active(0x7F74C8)?1u:0u);
 // A released build-time claim may leave the shared tooltip slot owned by
 // Cursor, which also owns the tooltip layout slot. Runtime separately
 // retires those slots and verifies NativeTooltipDetached before handoff.
 return codeHookRegistry.installed()==runtimeOnly&&!crcMapInstalled&&!networkSenderInstalled&&
  privateOutRetired()&&!shroudMaterials.active()&&!enhancementRestoring&&!techDataPending()&&mapVisibilityParametersRetired()&&techParametersRetired()&&techEnhanceParametersRetired()&&controlParametersRetired()&&singlePlayerFPSRetired()&&techparameters::groupOff(triggerinput::Tag)&&nativeViewRetired()&&!controlHouseFlagsPending();
}
// Reserve a share for other active task families, then let a lone family use
// the remaining capacity. All sends still obey the shared 64-event frame cap.
static int recurringBudget(EventLane lane){
 bool external=sh->bombBoardEnabled||sh->minerDefenseEnabled||minerDefensePending()||(sh->moFeatures&7u)||moOrdersPending()||directAirPending()||sh->formationPolicy.enabled||formationJobCount||stickyCount||sh->magnetronGlobal||sh->magnetronDrop||sh->dodgeEnabled||dodgeCount||burstCount||sh->missileEnabled||missileHoldCount||nativeCount||runtimeCoopPending||sh->plannerTracking||sh->runtimeTracking||sh->planRequest!=sh->planAck||sh->cooperativePlan!=sh->cooperativeAck;
 bool repairs=repairAt<repairCount,loading=loadLedger.count!=0,unloading=unloadCount!=0||queuedUnload.count!=0;
 if(lane==EventLane::Planning)return repairs||(sh->flags&AutoRepair)||loading||unloading?32:64;
 if(external)return 16;
 return 64-16*((lane!=EventLane::Repair&&repairs)+(lane!=EventLane::Boarding&&loading)+(lane!=EventLane::Unload&&unloading));
}
#include "runtime_planner_bridge.h"
#include "planning_budget.h"
static planningbudget::Budget sharedPlanningBudget;
static int planningRoom(int channel){
 sharedPlanningBudget.begin(match,get<int>(0xA8ED84),recurringBudget(EventLane::Planning));
 uint32_t active=0;
 if(sh->minerDefenseEnabled||minerDefensePending()||sh->planRequest!=sh->planAck||sh->runtimeTracking||nativeCount||(sh->flags&AntiTheft))active|=1;
 if(sh->cooperativePlan!=sh->cooperativeAck||runtimeCoopPending)active|=2;
 if(sh->missileEnabled&&sh->missileTargets)active|=4;
 if(burstCount||sh->burstTask!=sh->burstComputed)active|=8;
 if(dodgeCount||sh->dodgeTask!=sh->dodgeComputed)active|=16;
 return sharedPlanningBudget.roomMask(channel,active);
}
static void consumePlanning(int channel){sharedPlanningBudget.consume(channel);}
#include "missile_bridge.h"
#include "burst_bridge.h"
#include "dodge_bridge.h"
#include "formation_bridge.h"
#include "mo_orders_bridge.h"
#include "air_path_events_bridge.h"
#include "bomb_board_bridge.h"
#include "../../external_reader/source/idle_aircraft.h"
static int selectIdleAircraft(uint32_t house,uint32_t id){
 uint32_t p=worldByID(id);auto reader=[](uint32_t at,void*out,size_t n){return read(at,out,n);};external::aircraft::Inspect<decltype(reader)> planes{reader};
 if(!p||!planes.idle(p,house)||!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x13C),p))return 0;
 if(!nativeDependenciesReady(0x48DC90,0x6FBFA0))return -12;
 unselect();if(!selection(p))return 0;
 return 1; // Selecting an idle aircraft must not move the tactical camera.
}
static void selectionCommand(uint32_t house,DWORD now){
 uint32_t request=sh->selectRequest;if(request==sh->selectAck)return;MemoryBarrier();
 int result=0,count=0;
 if(sh->selectMatch!=match||DWORD(now-sh->selectTick)>1500)result=-7;
 else if((sh->selectIndex>=0||sh->selectIndex==-3)&&sh->selectTypeHash!=typeListHash(snap.types,snap.typeCount))result=-7;
 else if(get<bool>(0xA8BC60+25))result=-8;
 else {count=sh->selectIndex==-4?selectIdleAircraft(house,sh->selectObjectID):sh->selectIndex==-2?selectVariant(house,sh->selectGlobal!=0):sh->selectIndex==-3?selectTypes(house,sh->selectTypes,sh->selectGlobal!=0):selectType(house,sh->selectIndex,sh->selectGlobal!=0);if(count<0){result=count;count=0;}else result=1;}
 sh->selectResult=result;sh->selectCount=count;MemoryBarrier();sh->selectAck=request;
}
// Native sidebar GUIBuildSound: RulesClass +0x18C, VocClass::PlayGlobal.
// See YRpp VocClass.h and native StripClass click sites 0x6AAFE1..0x6AAFF8.
static void uiSoundCommand(DWORD now){
 uint32_t request=sh->uiSoundRequest;if(request==sh->uiSoundAck)return;MemoryBarrier();
 const bool allowed=sh->uiSoundMatch==match&&DWORD(now-sh->uiSoundTick)<=300&&
  sh->foreground&&(sh->flags&ReserveConstruction)&&!get<bool>(0xA8BC60+25);
 // Ack before calling an optional device API: a fault must not replay feedback.
 MemoryBarrier();sh->uiSoundAck=request;
 if(!allowed)return;
 uint32_t rules=0;int sound=-1;uint8_t code[10]{};
 static const uint8_t expected[]={0x51,0xA0,0xAC,0x64,0x84,0x00,0x89,0x54,0x24,0x00};
 if(!read(0x8871E0,&rules,4)||rules<0x10000||!read(rules+0x18C,&sound,4)||sound<0||
    !read(0x750920,code,sizeof code)||memcmp(code,expected,sizeof code))return;
 callFast<void>(0x750920,sound,0x2000,1.0f,(void*)nullptr);
}
static DWORD clampedRepairInterval(uint32_t ms){
 return ms<500?500:ms>30000?30000:ms;
}
struct RepairCapture {
 compute::RepairInput input;
 repairdelivery::Source identities[8192];
 uint32_t sourceSerial=0;
};
static compute::FrameHistory<RepairCapture> repairFrames;
static compute::RepairResult repairResultCopy;
// Input membership proof is staged across callbacks, never 8K-sort on Game.
static repairfilter::Filter repairResultGate;
static uint32_t repairValidatedSerial=0;static unsigned repairStagedCount=0;
static void resetRepairBatch(){
 repairCount=repairAt=repairCursor=0;
 repairCalculationPending=false;repairCalculationStarted=0;
 repairResultGate.reset();repairStagedCount=0;repairValidatedSerial=0;
 repairFrames.clear();
 if(sh)repairTaskSeen=sh->repairComputed;
}
static void cancelAutomaticRepairWork(){
 // Disabling automation cannot retract an already queued toggle, and a
 // manual request owns its own pending input and candidates.
 if(!manualRepairPending&&(repairCalculationPending||repairAt<repairCount))resetRepairBatch();
}
static void stageManualRepair(DWORD now){
 // New explicit input supersedes only the unsubmitted work.
 resetRepairBatch();
 manualRepairPending=manualRepairNeedsInput=true;manualRepairStarted=now;
}
static int collectRepairs(uint32_t house,DWORD now,bool refresh=false){
 if(repairCalculationPending&&!refresh)return -6;
 const bool internal=compute::inside(sh->computeApplied,compute::Repair);
 static RepairCapture candidate;int count=0;
 // Native synchronous collectors use current fields, not a copied graph.
 // The legacy Host lane keeps immutable provenance for asynchronous results.
 auto& in=candidate.input;
 const auto collect=[&](const worldframe::Key&key)->bool{
 if(refresh){
  const auto*last=repairFrames.find(sh->repairTask);
  if(last&&last->sourceSerial==key.serial&&last->input.frame==key.frame)return false;
 }
 Vec v{};if(!sampleRoster(0xA8EB40,v))return false;
 candidate.sourceSerial=key.serial;
 in.match=match;in.tick=now;in.frame=key.frame;in.count=0;
 for(int i=0;i<v.count&&count<8192;++i){
  const uint32_t p=item(v,i);if(!p||!own(p,house))continue;
  const uint32_t t=typeOf(p,3),id=get<uint32_t>(p+0x10);
  if(!id||!t||!featureTypeAllowed(featuretypes::RepairBuilding,t)||
     (!(sh->flags&RepairUnrepairable)&&!get<uint8_t>(t+0x157A)))continue;
  const decisions::RepairView view{get<int>(p+0x6C),get<int>(t+0xA0),true,get<bool>(p+0x6E8),pendingRepairs.contains(id,p)};
  if(internal){
   if(decisions::shouldRepair(view))candidate.identities[count++]={id,p,manualVersions.get(id)};
  }else if(in.count<8192){
   in.items[in.count]={id,view};
   candidate.identities[in.count++]={id,p,manualVersions.get(id)};
  }
 }
 return true;
 };
 if(internal&&runtimeDirectActions){
  worldframe::Key key{};key.match=match;key.frame=get<int>(0xA8ED84);key.tick=now;
  if(!collect(key))return -6;
 }else{
  ObservationReadScope source;if(!source)return -6;
  const auto&key=(sampleWorldSource?sampleWorldSource:&worldFrame)->key;
  if(!collect(key)||!source.complete())return -6;
 }
 repairCount=repairAt=repairCursor=0;
 if(internal){
  repairCalculationPending=false;
  repairCount=count;std::copy_n(candidate.identities,count,repairOrders);return count;
 }
 if(!refresh){repairFrames.clear();repairTaskSeen=sh->repairComputed;}
 sh->repairInput=in;
 repairCalculationPending=true;if(!refresh)repairCalculationStarted=now;
 const uint32_t serial=compute::nextTask(sh->repairTask);
 repairFrames.retain(serial,candidate);MemoryBarrier();sh->repairTask=serial;notifyHost();
 return -30;
}
static void repairCalculationTick(DWORD now){
 if(!repairCalculationPending)return;
 if(DWORD(now-repairCalculationStarted)>1000){resetRepairBatch();return;}
 const uint32_t completed=sh->repairComputed;
 if(!repairResultGate.running&&compute::newer(completed,repairTaskSeen)){
  const long seq=sh->repairResultSeq;
  if(!(seq&1)){
   MemoryBarrier();repairResultCopy=sh->repairResult;MemoryBarrier();
   if(seq==sh->repairResultSeq&&completed==sh->repairComputed){
    repairTaskSeen=completed;
    const auto*capture=repairFrames.find(completed);const auto&r=repairResultCopy;
    if(capture){
     const auto&input=capture->input;
     if(input.count>=0&&input.count<=8192&&r.count>=0&&r.count<=8192&&
        r.match==input.match&&r.tick==input.tick&&r.frame==input.frame&&
        freshness::valid(input.match,match,input.tick,input.frame,now,get<int>(0xA8ED84),freshness::Cooperative)&&
        !get<bool>(0xA8BC60+25)){
      repairCount=repairAt=repairCursor=0;repairStagedCount=0;repairValidatedSerial=completed;
      if(r.count==0){repairCalculationPending=false;return;}
      if(!repairResultGate.begin(input.count,r.count))repairValidatedSerial=0;
     }
    }
   }
  }
 }
 if(repairResultGate.running){
  const auto*capture=repairFrames.find(repairValidatedSerial);
  const auto&r=repairResultCopy;
  const auto*input=capture?&capture->input:nullptr;
  if(!input||input->count<0||input->count>8192||r.count<0||r.count>8192||
     r.match!=input->match||r.tick!=input->tick||r.frame!=input->frame||
     !freshness::valid(input->match,match,input->tick,input->frame,now,get<int>(0xA8ED84),freshness::Cooperative)||
     get<bool>(0xA8BC60+25)){
   repairResultGate.reset();repairStagedCount=0;repairValidatedSerial=0;
  }else{
   const bool finished=repairResultGate.step(unsigned(input->count),unsigned(r.count),
    [&](unsigned i){return input->items[i].id;},
    [&](unsigned i){return r.ids[i];},
    [&](uint32_t,unsigned index){repairOrders[repairStagedCount++]=capture->identities[index];});
   if(repairResultGate.failed){
    repairResultGate.reset();repairStagedCount=0;repairValidatedSerial=0;
   }else if(finished){
    repairCount=int(repairStagedCount);repairAt=repairCursor=0;
    repairResultGate.reset();repairValidatedSerial=0;repairCalculationPending=false;
    return;
   }else return;
  }
 }
 if(externalInputFrame&&!get<bool>(0xA8BC60+25))collectRepairs(lastHouse,now,true);
}
static int superSelect(uint32_t house,int index){Vec v=vec(house+0x254,256);uint32_t p=item(v,index);if(!p||get<uint32_t>(p+0x2C)!=house||!get<bool>(p+0x6D))return -4;
 if(!nativeDependencyReady(0x6CC360))return -12;
 if(!call<char>(0x6CC360,p))return -5;uint32_t t=get<uint32_t>(p+0x28);if(!t)return -4;
 if(get<int>(t+0xBC)==0){
  if(orderpolicy::superWeaponEvent(true,true,true,0)!=orderpolicy::Reject::None)return -4;
  const uint32_t submittedAt=sampleClock(sh);
  const uint32_t synthActor=taskarbiter::syntheticSuperWeaponActor(index);
  const gateway::Context ctx{match,submittedAt,sh&&((!sh->stop&&readonly::commands(sh)&&!readOnlyState.active)||missileCleanup)&&!fault&&snap.status==1,get<bool>(0xA8BC60+25)};
  const gateway::Intent swIntent{match,submittedAt,synthActor,uint32_t(get<int>(t+0x98)),0};
  if(gateway::validate(ctx,swIntent,3500)!=gateway::Failure::None){++snap.commandRejected;return -6;}
  const int frame=get<int>(0xA8ED84);
  actorFrameArbiter.begin(match,frame);
  if(!actorFrameArbiter.canIssue(synthActor,0)){++snap.commandRejected;return -6;}
  if(!submitConstructed(intent::superWeapon(
   get<int>(house+0x30),0x12,get<int>(t+0x98),vanilla::Cell{0,0})))return -6;
  actorFrameArbiter.record(synthActor,0,taskarbiter::Owner::SuperWeapon);
  recordReceipt(gateway::makeQueuedReceipt(missionObserver.nextSerial,swIntent,submittedAt,taskarbiter::Owner::SuperWeapon));
  return 2; // Immediate event accepted; no targeting cursor remains.
 }
 // Admit the whole local cursor transaction before clearing any selection
 // or targeting state. A missing cursor helper only rejects this command.
 if(!nativeDependenciesReady(0x4A8BF0,0x4AC8C0,0x4AC660,0x48DC90))return -12;
 put<uint32_t>(0x88098C,0);put<uint32_t>(0x880990,0);put<int>(0x880994,-1);call<void>(0x4A8BF0,0x87F7E8,(void*)nullptr);call<void>(0x4AC8C0,0x87F7E8,0);call<void>(0x4AC660,0x87F7E8,0);
 put<unsigned char>(0x88099A,0);put<unsigned char>(0x88099B,0);put<unsigned char>(0x88099C,0);put<int>(0x8809A0,index);unselect();return 1;
}
static int namedSuper(uint32_t house,int command){const char* id=command==SelectIron?"IronCurtainSpecial":"ChronoSphereSpecial";Vec v=vec(house+0x254,256);
 for(int i=0;i<v.count;++i){uint32_t p=item(v,i);if(!p)continue;uint32_t t=get<uint32_t>(p+0x28);if(t&&sameid((const char*)(t+0x24),id))return superSelect(house,i);}return -4;
}
#include "social_bridge.h"
#include "native_placement_bridge.h"
#include "superweapon_sidebar_bridge.h"
#include "superweapon_grant_bridge.h"
static int readSelectionReceipt(commandsampling::SelectionMember* selected){
 Vec v{};
 if(!read(0xA8ECB8,&v,sizeof v)||v.count<0||v.count>commandsampling::SelectionReceipt::Capacity||
    v.count>v.capacity||(v.count&&v.items<0x10000))return -1;
 for(int i=0;i<v.count;++i){
  uint32_t pointer=0,id=0;
  if(!read(v.items+uint32_t(i)*4,&pointer,4)||!pointer||
     !read(pointer+0x10,&id,4)||!id||!manualVersions.trackable(id))return -1;
  selected[i]={pointer,id,manualVersions.get(id)};
 }
 return v.count;
}
static bool selectionReceiptMatches(uint32_t receipt){
 if(receipt!=selectionReceipt.serial)return false;
 commandsampling::SelectionMember live[commandsampling::SelectionReceipt::Capacity]{};
 return selectionReceipt.matches(live,readSelectionReceipt(live));
}
static int awaitCommandSample(int command,int& receipt){
 if(commandsampling::selectionBound(command)){
  if(sh->requestToken)receipt=int(sh->requestToken);
  else {
   commandsampling::SelectionMember live[commandsampling::SelectionReceipt::Capacity]{};
   receipt=int(selectionReceipt.capture(live,readSelectionReceipt(live)));
  }
 }
 return CommandAwaitingSample;
}
static void command(uint32_t house,DWORD now,commandsampling::Sample sample=commandsampling::Sample::Exact){
 const uint32_t req=sh->request;
 if(req==sh->ack||(pendingAckReady&&req==pendingAck))return;
 MemoryBarrier();
 const int c=sh->command,arg=sh->arg;
 // A command posted after direct-lane admission must wait for its own full
 // graph; never acknowledge an unmigrated command against a zero-page source.
 if(runtimeDirectActions&&worldFrameBoundaryOnly&&c!=Repair&&((c!=AutoBoard&&c!=SmartUnload&&(c<CombatSelection||c>IdleTransport))||!commandRosterReady()))return;
 const bool candidateOnly=sample!=commandsampling::Sample::Exact;
 int result=1,n=0;
 if(sh->requestMatch!=match||DWORD(now-sh->requestTick)>3500){result=-7;}
 else if(c==AttackOwnGround&&DWORD(now-sh->requestTick)>150){result=-7;}
 else if((enhancementChangedCatalogue||sh->requestTypeHash)&&typeIndexedCommand(c,arg)&&sh->requestTypeHash!=typeListHash(snap.types,snap.typeCount)){result=-7;}
 else if(get<bool>(0xA8BC60+25)){result=-8;}
 else if(commandsampling::selectionBound(c)&&sh->requestToken&&!selectionReceiptMatches(sh->requestToken)){
  result=CommandSelectionChanged;
 }
 else if(!commandsampling::canAttempt(c,sample)){result=awaitCommandSample(c,n);}
 else if(c==Repair){stageManualRepair(now);result=RepairStaged;}
 else if(c==Cheer||c==Taunt){result=socialCommand(house,c,arg,now);n=result>0;}
 else if(c==DestructPlayer){result=destructPlayerCommand(uint32_t(arg),uint32_t(sh->arg2),sh->requestToken);n=result>0?1:0;}
 else if(c==PlaceCompletedBuilding){result=nativePlacementCommand(house,now);n=result>0?1:0;}
 else if(c==SwitchPlayer||c==RestorePlayer){result=-4;n=0;} // unavailable in this release, including stale commands
 else if(c==SelectType){n=selectType(house,arg,sh->arg2!=0);if(n<0){result=n;n=0;}}
 else if(c==SelectVariant){n=selectVariant(house,sh->arg2!=0);if(n<0){result=n;n=0;}}
 else if(c==SmartUnload){
  const int sent=smartUnload(house,candidateOnly);
  if(sent==-30)result=awaitCommandSample(c,n);
  else if(sent<0)result=sent;
  else n=sent;
 }
 else if(c==StickyGuard){n=stickyCommand(house);if(n<0){result=n;n=0;}}
 else if(c==AttackOwnGround||selectedmode::index(c)>=0){
  if(ownground::job.pending)result=-6;
  else{
   if(c==AttackOwnGround){attackOwnGroundCommand(house,now,req);if(!ownground::job.pending)return;}
   else n=selectedModeCommand(house,selectedmode::mission(selectedmode::index(c)));
   if(ownground::job.pending){
    ownground::job.detached=true;result=SelectionBatchStaged;n=ownground::job.sent;
   }else if(n<0){result=n;n=0;}
  }
 }
 else if(c==CancelAdvanced){Vec v=vec(0xA8ECB8);for(int i=0;i<v.count;++i){uint32_t p=item(v,i);if(p&&own(p,house)){cancelAutomatic(p);++n;}}}
 else if(c==AutoBoard){result=autoLoad(house,nullptr,candidateOnly);n=snap.loadIssued;
  if(result==-30){result=AutoBoardStaged;n=0;}
 }
 else if(c>=CombatSelection&&c<=DamagedSelection){n=selectFilter(house,c);if(n<0){result=n;n=0;}}
 else if(c>=IdleHarvester&&c<=IdleTransport){n=findIdle(house,c);if(n<0){result=n;n=0;}}
 else if(c==ClearBeacons){if(!startupSignature(Signatures[8]))result=-12;else {call<void>(0x431410,0x89C3B0,get<int>(house+0x30));n=1;}}
 else if(c==RevealMap&&(!(sh->flags&ExperimentalReveal)||sh->renderReveal)){result=-21;}
 else if(c==RevealMap){
  // Early in a restarted match, House can be valid before its map-cell array.
  // A void reveal call against that transitional state silently does nothing.
  uint32_t cells=get<uint32_t>(0x87F7E8+316);
  int capacity=get<int>(0x87F7E8+320);
  if(cells<0x10000||capacity<=0||capacity>512*512)result=-14;
  else if(!revealSignature())result=-12;
  else if(!crcMapToolBegin(house))result=-22;
  else {call<void>(0x577D90,0x87F7E8,(void*)house);crcMapToolEnd();n=1;}
 }
 else if(c==OpenTech){
  if(arg&&!techmode::valid(sh->arg2))result=-4;
  else if(arg&&!techNativeDependencies(sh->arg2)){snap.techError=-12;result=-12;}
  else if(arg&&sh->arg2!=techImplementation&&(techmode::data(sh->arg2)||techmode::parameters(sh->arg2)||techmode::data(techImplementation)||techmode::parameters(techImplementation))&&techTransitionProductionPending(house)){snap.techError=-43;result=-43;}
  else {techImplementation=arg?sh->arg2:0;snap.techOpen=arg?int(techImplementation)+1:0;memcpy(techAllowed,sh->techAllowed,sizeof techAllowed);snap.techError=0;
   if(techmode::data(techImplementation))for(auto&w:reservedSidebar)w={};
   if(snap.techOpen){requestTechRefresh();refreshTech(house,now);}else restoreTech();n=snap.techVisible;if(snap.techError<0)result=snap.techError;}}
 else if(c==SelectSuper){
  const int arg2=sh->arg2;
  if(arg==-1&&!arg2&&!sh->requestToken)result=cancelSuperSidebarSelection();
  else if(arg2&&(!sh->requestToken||item(vec(house+0x254,256),arg)!=uint32_t(arg2)||get<uint32_t>(uint32_t(arg2)+0x28)!=sh->requestToken))result=-4;
  else result=superSelect(house,arg);n=result>0;
 }
 else if(c==SelectIron||c==SelectChrono){result=namedSuper(house,c);n=result>0;}
 else if(c==AirPathMove){uint32_t id=uint32_t(arg),p=worldByID(id),cell=uint32_t(sh->arg2);
  if(candidateOnly&&!p)result=CommandAwaitingSample;
  else if(!airpathpolicy::events(get<int>(0xA8B238),(sh->flags&AirWaypoints)!=0,(sh->flags&NativeAirWaypoints)!=0,sh->nativeAirNetwork)||!p||!own(p,house)||kind(p)!=0||manualVersions.get(id)!=sh->requestToken||get<bool>(p+0x81)||get<uint32_t>(p+0x11C)||!readMapCell(int(cell%1000),int(cell/1000)))result=-4;
  else if(nativeMission(house,id,2,0,cell,11,0,0,taskarbiter::Owner::Generic))n=1;else result=-6;
 }
 else if(c==ProduceReserved){
  uint32_t origin=worldByID(uint32_t(sh->arg2));
  if(candidateOnly&&!origin)result=CommandAwaitingSample;
  else if(!(sh->flags&ReserveConstruction)||arg<0||arg>=snap.typeCount||snap.types[arg].kind!=7||!origin||!own(origin,house)||kind(origin)!=3||get<bool>(origin+0x81)||!get<bool>(origin+0x90))result=-4;
  else if(!nativeDependenciesReady(0x6ABC60,0x4F7870,0x5F7900))result=-12;
  else {const auto&t=snap.types[arg];int tab=typeTab(arg);uint32_t f=tab==0||tab==1?get<uint32_t>(house+(tab?0x53CC:0x53BC)):0;
   if(tab<0||tab>1||!hasCameo(7,t.index)||!typePointers[arg]||!reservationCanBuild(typePointers[arg],house))result=-9;
   else if(f>=0x10000&&(get<uint32_t>(f+0x58)||get<int>(f+0x50)>0))result=-9;
   else if(!techFactory(typePointers[arg],house))result=-9;
   else if(nativeProduceEvent(house,t,true)){n=1;watchReservedSidebar(house,t.index,tab,uint32_t(sh->arg2),now);}else result=-6;
  }
 }
 else if(c==ProduceDeferred){
  if(!(sh->flags&ReserveConstruction)||arg<0||arg>=snap.typeCount||!typePointers[arg])result=-4;
  else if(!nativeDependenciesReady(0x6ABC60,0x4F7870,0x5F7900))result=-12;
  else {const auto&t=snap.types[arg];int tab=typeTab(arg);uint32_t offset=t.kind==3?0x53AC:t.kind==16?0x53B0:t.kind==40?(t.naval?0x53B8:0x53B4):t.kind==7?(tab==1?0x53CC:0x53BC):0;
   uint32_t factory=offset?get<uint32_t>(house+offset):0;
   if(!offset||tab<0||tab>3||!hasCameo(t.kind,t.index)||!reservationCanBuild(typePointers[arg],house))result=-9;
   else if(factory>=0x10000&&(get<uint32_t>(factory+0x6C)!=house||
      (t.kind==7&&(get<uint32_t>(factory+0x58)||get<int>(factory+0x50)>0))))result=-9;
   // FactoryClass +0x50 contains QueuedObjects.Count and +0x58 the active
   // Object. Units may join the native queue while a product is in progress;
   // buildings still wait for an empty factory and placement.
   else if(!techFactory(typePointers[arg],house))result=-9;
   else if(nativeProduceEvent(house,t,true)){n=1;if(t.kind==7)watchReservedSidebar(house,t.index,tab,0,now);}else result=-6;
  }
 }
 else if(c==Produce||c==CancelTechProduction){
  if(!(sh->flags&UnsafeTech)||!snap.techOpen||arg<0||arg>=snap.typeCount||!techAllowed[arg]||!typePointers[arg] )result=-4;
  // Data mode is operated through the game's native sidebar. Do not retain
  // the unpublished toolbox event-only route under a renamed mode.
  else if(techmode::data(techImplementation)||techmode::parameters(techImplementation))result=-4;
  else if(c==CancelTechProduction){auto&t=snap.types[arg];if(nativeProduceEvent(house,t,false,0x10))n=1;else result=-6;}
  else if(!nativeDependenciesReady(0x4F7870,0x5F7900,0x6ABC60,0x6A6300))result=-12;
  else {const uint32_t type=typePointers[arg];uint32_t table=get<uint32_t>(type),method=table?get<uint32_t>(table+0x94):0;
   if(method<0x10000||call<int>(0x4F7870,house,(void*)(uintptr_t)type,true,true)<=0||!call<uint32_t>(method,type,techImplementation!=1,true,true,(void*)(uintptr_t)house))result=-9;
   else{auto&t=snap.types[arg];
    if(!ensureTechCameo(arg,house))result=-9;
    else{result=nativeProduceEvent(house,t)?1:-6;n=result>0;if(n&&t.kind==7)watchReservedSidebar(house,t.index,typeTab(arg),0,now);}}
  }
 }
 else result=-4;
 sh->result=result;sh->count=n;pendingAck=req;pendingAckReady=true;
}
static repairdelivery::Observation observePendingRepair(uint32_t id,uint32_t pointer,uint32_t house,bool exact){
 const uint32_t p=worldObjects.byId(id);
 if(!p||p!=pointer)return {false,false,false,!exact}; // Candidate absence is unknown.
 uint32_t liveID=0,vt=0,owner=0,type=0;int hp=0,maxHP=0;
 uint8_t alive=0,onMap=0,limbo=1,repairing=0;
 // A failed live field does not prove that the toggle was consumed.
 if(!read(p+0x10,&liveID,sizeof liveID)||!read(p,&vt,sizeof vt)||
    !read(p+0x21C,&owner,sizeof owner)||!read(p+0x6C,&hp,sizeof hp)||
    !read(p+0x90,&alive,sizeof alive)||!read(p+0x74,&onMap,sizeof onMap)||
    !read(p+0x81,&limbo,sizeof limbo)||!read(p+0x520,&type,sizeof type)||
    !read(p+0x6E8,&repairing,sizeof repairing))return {false,false,false,true};
 if(liveID!=id||vt!=0x7E3EBCu||owner!=house||hp<=0||!alive||!onMap||limbo)return {};
 if(type<0x10000||!read(type+0xA0,&maxHP,sizeof maxHP))return {false,false,false,true};
 return {true,repairing!=0,maxHP>0&&hp>=maxHP};
}
// The batch cursor only advances after a complete admission observation.
// A second live UID or building field read can fail after indexWorld succeeds;
// treating the default-zero result as a rejected building silently loses Repair.
enum class RepairAdmission { Unknown, Skip, Issue };
static RepairAdmission checkedRepairAdmission(const repairdelivery::Source&source,uint32_t house,bool exact){
 const uint32_t id=source.id,pointer=worldObjects.byId(id);
 if(!pointer)return exact?RepairAdmission::Skip:RepairAdmission::Unknown;
 if(pointer!=source.pointer||manualVersions.get(id)!=source.manual||!manualVersions.trackable(id))return RepairAdmission::Skip;
 uint32_t uid=0,vt=0,owner=0,type=0;int hp=0,maximum=0;
 uint8_t alive=0,onMap=0,limbo=1,repairing=0,repairable=0;
 if(!read(pointer+0x10,&uid,sizeof uid)||!read(pointer,&vt,sizeof vt)||
    !read(pointer+0x21C,&owner,sizeof owner)||!read(pointer+0x6C,&hp,sizeof hp)||
    !read(pointer+0x90,&alive,sizeof alive)||!read(pointer+0x74,&onMap,sizeof onMap)||
    !read(pointer+0x81,&limbo,sizeof limbo)||!read(pointer+0x520,&type,sizeof type)||
    !read(pointer+0x6E8,&repairing,sizeof repairing))return RepairAdmission::Unknown;
 if(uid!=id||vt!=0x7E3EBCu||owner!=house||hp<=0||!alive||!onMap||limbo)return RepairAdmission::Skip;
 if(type<0x10000)return RepairAdmission::Skip;
 if(!read(type+0xA0,&maximum,sizeof maximum)||!read(type+0x157A,&repairable,sizeof repairable))return RepairAdmission::Unknown;
 if(!refreshFeatureTypes())return RepairAdmission::Unknown;
 // When customized, the TypeID source itself must be readable; a failed
 // read is not proof that the building was explicitly filtered out.
 if(featureConfig.filters[featuretypes::RepairBuilding].custom){
  char typeName[24]{};
  if(!read(type+0x24,typeName,23))return RepairAdmission::Unknown;
 }
 if(!featureTypeAllowed(featuretypes::RepairBuilding,type)||
    (!((sh->flags&RepairUnrepairable)!=0)&&!repairable))return RepairAdmission::Skip;
 const bool recent=pendingRepairs.contains(id,pointer);
 return decisions::shouldRepair({hp,maximum,true,repairing!=0,recent})?
   RepairAdmission::Issue:RepairAdmission::Skip;
}
static void repairTick(uint32_t house,DWORD now,bool localCandidate){
 const int frame=get<int>(0xA8ED84);
 const bool currentRoster=indexedFrame==frame&&commandRosterReady();
 const bool automatic=(sh->flags&AutoRepair)!=0;
 snap.autoRepair=automatic;
 if(manualRepairPending&&DWORD(now-manualRepairStarted)>3000){
  resetRepairBatch();manualRepairPending=manualRepairNeedsInput=false;
 }
 // Cancel before polling so a late Host result cannot revive disabled work.
 if(!automatic)cancelAutomaticRepairWork();
 if(get<bool>(0xA8BC60+25))return;
 if(manualRepairNeedsInput&&collectRepairs(house,now)!=-6)manualRepairNeedsInput=false;
 repairCalculationTick(now);
 if(currentRoster||localCandidate)pendingRepairs.observeBudgeted([&](uint32_t id,uint32_t pointer){
  return observePendingRepair(id,pointer,house,currentRoster);
 });
 const DWORD interval=clampedRepairInterval(sh->repairMs);
 bool deferred=false;
 if(!repairCalculationPending&&!manualRepairPending&&automatic&&
    (!autoWas||(DWORD(now-lastAuto)>=interval&&repairAt>=repairCount))){
  deferred=collectRepairs(house,now)==-6;if(!deferred)lastAuto=now;
 }
 autoWas=automatic&&!deferred;
 static int budgetFrame=-1,budget;static uint32_t budgetMatch;
 if(budgetFrame!=frame||budgetMatch!=match){budgetFrame=frame;budgetMatch=match;budget=recurringBudget(EventLane::Repair);}
 if(repairAt<repairCount&&budget>0&&(currentRoster||localCandidate)){
  // In a cross-frame candidate a single missing UID need not hold the rest
  // of the batch. Keep that UID pending. Bound the number of checks per pass.
  unsigned remaining=unsigned(repairCount-repairAt);
  if(!currentRoster)remaining=std::min(remaining,64u);
  while(repairAt<repairCount&&budget>0&&remaining--){
   if(repairCursor<repairAt||repairCursor>=repairCount)repairCursor=repairAt;
   const auto& source=repairOrders[repairCursor];
   const RepairAdmission status=checkedRepairAdmission(source,house,currentRoster);
   if(status==RepairAdmission::Unknown){
    if(currentRoster)break;
    ++repairCursor;continue;
   }
   if(status==RepairAdmission::Issue){
    if(!pendingRepairs.room(source.id)||!repairEvent(house,source.id))break;
    pendingRepairs.queued(source.id,source.pointer);--budget;++snap.repairTotal;
   }
   if(repairCursor!=repairAt)repairOrders[repairCursor]=repairOrders[repairAt];
   ++repairAt;++repairCursor;
  }
 }
 if(manualRepairPending&&!manualRepairNeedsInput&&!repairCalculationPending&&repairAt>=repairCount)manualRepairPending=false;
}
static void scanCrates(uint32_t tactical,int frame,DWORD now,uint32_t flags){
 snap.crateCount=0;if(!(flags&Crates)){cratesOn=false;crateCount=0;return;}
 if(!nativeDependenciesReady(0x578080,0x6D2140)){cratesOn=false;crateCount=0;return;}
 if(!cratesOn||DWORD(now-crateTick)>=500){cratesOn=true;crateTick=now;crateCount=0;
  uint32_t cells=get<uint32_t>(0x87F7E8+316);int capacity=get<int>(0x87F7E8+320);
  if(cells>=0x10000&&capacity>0&&capacity<=512*512)for(int i=0;i<256;++i){uint32_t c=0x87F7E8+344+i*16;int x=get<short>(c+12),y=get<short>(c+14);
   if(x<=0||y<=0||x>=512||y>=512||!remaining(get<int>(c),get<int>(c+8),frame))continue;
   int index=y*512+x;if(index>=capacity)continue;uint32_t cell=get<uint32_t>(cells+index*4);if(cell<0x10000)continue;
   int overlay=get<int>(cell+0x44);Vec overlays=vec(0xA83D80,4096);uint32_t overlayType=item(overlays,overlay);
   if(!overlayType||!get<bool>(overlayType+0x2AA))continue;
   auto&d=crateCache[crateCount++];d.cellX=x;d.cellY=y;int preset=get<unsigned char>(cell+0x11E);d.preview=preset<19?preset:-1;d.coords[0]=x*256+128;d.coords[1]=y*256+128;d.coords[2]=call<int>(0x578080,0x87F7E8,d.coords);
  }
 }
 if(tactical)for(int i=0;i<crateCount;++i){auto&c=crateCache[i];POINT pt{};if(!call<bool>(0x6D2140,tactical,c.coords,&pt))continue;snap.crates[snap.crateCount++]={c.cellX,c.cellY,pt.x,pt.y,c.preview};}
}
static void scan(uint32_t house,int frame,DWORD now){
 snap.labels=0;snap.selected=0;snap.transports=snap.used=snap.capacity=0;snap.threatCount=0;
 const uint32_t tactical=get<uint32_t>(0x887324),flags=sh->flags;
 const bool capacityReady=(flags&Capacity)&&nativeDependencyReady(0x473460);
 const bool projectionReady=tactical&&nativeDependencyReady(0x6D2140);
 // Global Iron/Psychic/AllTransports projections are suspended until the
 // Worker produces a dedicated display projection. Only the bounded current
 // selection is read from the sealed Runtime sample in this phase.
 static uint32_t selectedPointers[4096],selectedIDs[4096];int selectedCount=0;
 {
  SampleReadScope sample;
  Vec selected{};
  if(!sample||!sampleRoster(0xA8ECB8,selected,4096)){
   snap.frame=frame;snap.width=get<int>(0x8A00A4);snap.height=get<int>(0x8A00A8);
   snap.credits=get<int>(house+0x30C);scanCrates(tactical,frame,now,flags);return;
  }
  for(int i=0;i<selected.count&&selectedCount<4096;++i){
   const uint32_t p=item(selected,i);if(!p)continue;
   const uint32_t id=get<uint32_t>(p+0x10);if(!id)continue;
   selectedPointers[selectedCount]=p;selectedIDs[selectedCount++]=id;
  }
  if(!sample.complete())selectedCount=0;
 }
 for(int i=0;i<selectedCount;++i){
  const uint32_t p=selectedPointers[i];uint32_t liveID=0;
  if(!readLiveWorld(p+0x10,&liveID,sizeof liveID)||liveID!=selectedIDs[i]||!own(p,house)||!live(p))continue;
  const int k=kind(p);if(k<0)continue;
  const bool selected=get<bool>(p+0x83);if(selected)++snap.selected;
  const int iron=(flags&Iron)?remaining(get<int>(p+0x18C),get<int>(p+0x194),frame):0;
  const uint32_t t=typeOf(p,k);
  const int capacity=t&&k!=3&&capacityReady?get<int>(t+0x5E0):0;
  const bool capacityShow=capacity>0&&(selected||(flags&AllTransports));
  int used=0;
  if(capacityShow){used=call<int>(0x473460,p+0x114);if(selected){++snap.transports;snap.used+=used;snap.capacity+=capacity;}}
  const int advancedIndex=nativeCount?orderIndex(p):-1;
  const int advanced=get<int>(p+0xAC)==6?2:advancedIndex>=0&&nativeOrders[advancedIndex].stopped;
  if((!capacityShow&&!iron&&!advanced)||!projectionReady||snap.labels>=MaxLabels)continue;
  POINT pt{};if(!call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&pt))continue;
  auto&l=snap.label[snap.labels++];l={liveID,pt.x,pt.y,used,capacityShow?capacity:0,iron,get<int>(p+0x1C4)!=0,advanced};
 }
 snap.frame=frame;snap.width=get<int>(0x8A00A4);snap.height=get<int>(0x8A00A8);snap.credits=get<int>(house+0x30C);
 scanCrates(tactical,frame,now,flags);
}
static bool scanSummary(uint32_t house,int frame,DWORD now,uint32_t flags){
 if(DWORD(now-slow)<100)return false;slow=now;
 if(sh->externalDisplay){
  if(!(flags&Production))clearFlashes();
  if(sh->productionRequest!=sh->productionAck){MemoryBarrier();int count=sh->productionCount;
   if((flags&Production)&&sh->productionMatch==match&&DWORD(now-sh->productionTick)<2000&&count>=0&&count<=4)for(int i=0;i<count;++i)flashCameo(sh->productionTypes[i],frame);
   MemoryBarrier();sh->productionAck=sh->productionRequest;
  }return true;
 }
 snap.swCount=0;Vec sw=vec(house+0x254,256);if(nativeDependencyReady(0x6CC360))for(int i=0;i<sw.count&&snap.swCount<MaxSW;++i){uint32_t p=item(sw,i);if(!p||get<uint32_t>(p+0x2C)!=house||!get<bool>(p+0x6D))continue;uint32_t t=get<uint32_t>(p+0x28);if(!t)continue;auto&d=snap.supers[snap.swCount++];memset(&d,0,sizeof d);memcpy(d.id,(void*)(t+0x24),23);name(t,d.name,40);d.index=i;d.ready=call<char>(0x6CC360,p);d.frames=remaining(get<int>(p+0x30),get<int>(p+0x38),frame);}
 scanProduction(house,frame,flags);
 return true;
}
static void shiftScene(int dx,int dy){
 for(int i=0;i<snap.labels;++i){snap.label[i].x-=dx;snap.label[i].y-=dy;}
 for(int i=0;i<snap.crateCount;++i){snap.crates[i].x-=dx;snap.crates[i].y-=dy;}
 for(int i=0;i<snap.threatCount;++i){auto&t=snap.threats[i];t.x-=dx;t.y-=dy;t.tx-=dx;t.ty-=dy;}
}
// Consume one set of game-thread intents irrespective of which callback woke
// the dispatcher. Producers write Shared requests / plans; they never choose
// the callback provider or write EventClass data themselves.
static void consumeSubmittedIntents(uint32_t house,int frame,DWORD now){
 crcMapShadowTick();networkSenderTick(sh->networkHousePreserve!=0);nativeSellCursorTick();controlObjectOwnerTick();controlCommandOwnerTick();nativeMegaGatewayTick();
 if(!runtimeDirectActions||!worldFrameBoundaryOnly)selectionCommand(house,now);uiSoundCommand(now);
 // Preserve the ordering of the known-good path: input observers first,
 // then command / planner / cooperative requests.
 uint32_t inputFlags=sh->flags|((unloadCount||queuedUnload.count)?NativeOrders:0);
 csHooksTick(inputFlags);nativeHooksTick(inputFlags);clickHooksTick(inputFlags);
 snap.inputPatchMask=0;
 // Pointer intents are native work. A late mailbox publication waits for
 // the next current roster, never for a full graph or display snapshot.
 if(!runtimeDirectActions||commandRosterReady())pointerTick();
 ownGroundTick(house,frame,now);command(house,now);if(playerContextChanged)return;reservedSidebarTick(house,now);
 if(!runtimeDirectActions||!worldFrameBoundaryOnly)externalOrdersTick(house,frame,now);
 if(!runtimeDirectActions||commandRosterReady())cooperativeTick(house,frame,now);
 placementTick();
}
// A suspended command context retains selected code/vtable ownership. Only
// explicit option changes retire command sites here; no command installer or
// automatic actor job runs merely because the same world is still resident.
static void suspendedOptions(){
 restoreMapVisibilityParameters();
 // The native map zoom shares the sidebar Render dispatcher even when the
 // sidebar itself is visible. Focus loss, pause or observer mode must not
 // tear that dispatcher down while the zoom owns or is retiring its view.
 if(sh->sidebarMode==0&&!nativeViewNeedsTracking()&&!nativePipDemand())restoreSidebarView();
 if(!(sh->moFeatures&pauseview::Feature))restorePauseView();
 if(!sh->nativeSellCursor)restoreNativeSellCursor();
 if(!sh->nativeMegaGateway)restoreNativeMegaGateway();
 if(!sh->controlCommandOwner||!controlmode::native(sh->controlAll)){restoreControlObjectOwner();restoreControlCommandOwner();}
 controlSitesSuspend();
 if(!sh->networkHousePreserve||!controlmode::sender(sh->controlAll))restoreNetworkSender();
 if(!sh->buildTimeTooltip)restoreBuildTimeTooltip();
 if(!sh->crcMapShadow)restoreCrcMapShadow();
 if(sh->renderReveal==3)directMapRevealReady();
 else if(!directGroupRestore(directpatch::Group::Map))snap.renderRevealStatus=-2;
 if(!sh->renderReveal)renderRevealTick(false);
 if(!sh->techEnhance)restoreTechEnhancement();
 if(!(sh->flags&BuildAnywhere)){riskPlacementActive=false;restoreRiskSite(riskSites[0]);directGroupRestore(directpatch::Group::Placement);}
 if(!(sh->flags&NativeAirWaypoints)){riskAirActive=false;restoreRiskSite(riskSites[1]);restoreRiskSite(riskSites[2]);}
 if(!sh->lowDamageFilter||!sh->lowDamageNetwork)restoreRiskSite(riskSites[9]);
 if(!sh->scrollRefreshMode||!sh->scrollRefreshHz)restoreHighScroll();
 // Disable behavior while preserving the installed command adapters. The
 // ordinary playable maintenance re-enables them after all gates pass.
 riskControlActive=false;riskLowDamageActive=false;
}
static void suspendedSnapshot(DWORD now,bool render,int reason){
 if(sh&&sh->superweaponGrant)sh->superweaponGrantStatus=2;
 if(moOrdersPending())resetMoOrders();
 if(directAirPending())resetDirectAir();
 // Preserve manual cancellation for the same bomb across pause/focus loss.
 if(sh&&!sh->bombBoardEnabled)resetBombBoard();
 if(sh)sh->inputUnlockStatus=sh->keepInputUnlocked?inputlock::Waiting:inputlock::Off;
 suspendedOptions();if(!render){scrollRendering=false;scrollCadence={};}
 uint8_t chat=1;const bool chatKnown=read(0xA8BC79,&chat,sizeof chat);
 snap.commandStatus=reason;snap.chat=!chatKnown||chat!=0;runtimeKeys.clear();
 // Already accepted commands do not depend on the tactical window being visible.
 if(reason==activation::DisplayHidden&&privateOutPending()&&simulationReady(false,true)&&prepareWorldIndex(snap.frame))servicePrivateOut();
 if(render&&good&&chatKnown){pauseViewTick();buildTimeTooltipTick();renderRevealTick();viewZoomTick();sidebarViewTick();smallMapCameraTick();radarNavigationTick(now);highScrollTick(preciseclock::micros());}
 if(!render&&riskRenderActive)riskRenderRedrawPending=true;
 pointerTick();publishSnapshot(now);
}
static bool controllerExited(DWORD now){
 if(!sh||!sh->owner)return false;
 if(controllerPID!=sh->owner){if(controllerProcess)CloseHandle(controllerProcess);controllerProcess=nullptr;controllerPID=sh->owner;controllerCheckAt=0;}
 if(controllerCheckAt&&DWORD(now-controllerCheckAt)<250)return false;
 controllerCheckAt=now;
 if(!controllerProcess){controllerProcess=OpenProcess(SYNCHRONIZE,FALSE,controllerPID);
  if(!controllerProcess)return GetLastError()==ERROR_INVALID_PARAMETER;}
 return WaitForSingleObject(controllerProcess,0)==WAIT_OBJECT_0;
}
static bool hostFrameCaptureWanted(DWORD now){
 // An actual consumer, regardless of calculation placement, subscribes to
 // raw capture. Idle repair intervals do not add a full capture every frame.
 if((sh->flags&AutoRepair)&&(!autoWas||DWORD(now-lastAuto)>=clampedRepairInterval(sh->repairMs)))return true;
 if(sh->flags&AutoOreReturn)return true;
 if(sh->pointer.request!=sh->pointer.ack)return false;
 const auto&r=sh->cooperativeRequest;
 if(r.pointerRequest!=sh->pointer.request)return false;
 const auto&captured=sh->cooperativeInputFrame;
 const bool admitted=captured.frame>=0&&captured.request.serial==r.serial&&captured.match==r.match;
 if(compute::inside(sh->computeApplied,r.mode==cooperative::Enter?compute::Enter:compute::Aircraft)||
    !cooperative::valid(r)||r.match!=match||(!admitted&&!freshness::intentAlive(r.tick,now)))return false;
 return !admitted||sh->cooperativeFrameWanted==r.serial;
}
template<class Fn>static void withInputPublication(Shared*value,uint32_t session,DWORD now,int frame,bool advanced,Fn&&tailWork){
 const unsigned priorReadDepth=sampleReadDepth;const bool priorReadComplete=sampleReadComplete;
 const bool priorDirectActions=runtimeDirectActions;
 bool complete=false;InterlockedIncrement(&value->computeFrameSeq);
 runtimeunwind::run([&]{tailWork();complete=true;},[&]{
  sampleReadDepth=priorReadDepth;sampleReadComplete=priorReadComplete;runtimeDirectActions=priorDirectActions;
  if(complete){
   if(advanced){value->computeFrameMatch=session;value->computeFrameTick=now;value->computeFrame=frame;++value->computeFrameSerial;}
  }else{
   // A platform-owned unwind must close our sequence without presenting a
   // partial sample as complete. Recapture on the next admitted tick; events
   // and their request/ack state are left to the existing delivery guards.
   value->computeFrameMatch=value->computeFrameTick=0;value->computeFrame=-1;
   inputMatch=indexedInputMatch=0;inputFrame=indexedInputFrame=-1;externalInputFrame=false;
  }
  MemoryBarrier();InterlockedIncrement(&value->computeFrameSeq);notifyHost();
 });
}
static void cancelReadOnlyMailboxes(){
 if(!sh)return;
 // A delayed internal ACK must not overwrite the terminal pause receipt.
 pendingAckReady=false;pendingAck=sh->request;
 readonly::cancelMailboxes(*sh,[]{MemoryBarrier();});
}
static bool retireForReadOnly(){
 // First disarm every ordinary feature. Keep only identities required to
 // release a hold already owned by us, never a candidate or computed task.
 const bool held=missileHoldCount&&lastHouse&&get<uint32_t>(0xA83D4C)==lastHouse&&get<bool>(0xA8E9A0);
 const int previousOutgoingFrame=outgoingFrame,previousOutgoingBudget=outgoingBudget;
 reset(true,held);maintainTechParameters();
 if(held){
  outgoingFrame=previousOutgoingFrame;outgoingBudget=previousOutgoingBudget;
  missileFrames.clear();missileApplied=sh?sh->missileComputed:0;missilePending=missileScanned=false;missileFrame=-1;
  missileSourceCursor=missileTargetCursor=missileManagedCursor=0;
  const int frame=get<int>(0xA8ED84);snap.frame=frame;missileCleanup=true;
  const bool released=releaseMissileHolds(lastHouse,frame,sampleClock(sh));missileCleanup=false;
  if(!released)return false;
  resetMissileControl();
 }
 restorePlayerContext();return featureRestorationComplete();
}
static void applyCommandComputeEpoch(){
 if(commandComputeEpoch==match)return;
 sh->computeApplied=sh->computeRequested&compute::All;commandComputeEpoch=match;
}
// Both explicit read-only and view-only authority admit only the Host's
// zero-page display boundary. Reject stale mixed/native subscriptions.
static void captureDisplayOnlyWorld(const worldlife::Sample&world,uint32_t house,DWORD now,bool connected){
 const uint32_t demand=connected?worldFrameServer.demandMask():0u;
 if(demand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly))
  captureWorldFrame(readLiveWorld,world,house,now,demand,true);
 else retireWorldFrame();
}
// Passive callbacks have no local snapshot consumer. A live mapping or
// heartbeat alone does not establish an active world-data subscription.
static void capturePassiveWorld(const worldlife::Sample&world,uint32_t house,DWORD now,bool connected){
 if(!connected)return;
 const uint32_t demand=worldFrameServer.demandMask();
 if(demand)captureWorldFrame(readLiveWorld,world,house,now,demand,true);
}
static bool serviceReadOnlyRuntime(DWORD now){
 // Only the fully retired, explicit read-only path can suppress duplicate
 // cleanup. Never retain the marker across command authority or a capture.
 static readonly_idle_cleanup::IdleRetirement idleRetired;
 const bool wasReadOnly=readOnlyState.active;
 if(!wasReadOnly)idleRetired.valid=false;
 const auto admission=readOnlyState.service(*sh,cancelReadOnlyMailboxes,retireForReadOnly,[]{MemoryBarrier();});
 if(admission==readonly::Admission::Commands){
  idleRetired.valid=false;
  if(wasReadOnly){commandDormant=worldCleanupPending=false;lastFrame=-1;slow=now-100;sceneFrame=-2147483647;sceneTick=0;}
  return false;
 }
 runtimeKeys.clear();snap.commandStatus=admission==readonly::Admission::Retiring?activation::CleanupPending:activation::HostReadOnly;
 snap.bridgeStatus=1;snap.labels=snap.selected=snap.transports=snap.used=snap.capacity=snap.swCount=0;
 snap.readStatus=snap.renderStatus=0;snap.placementOfferCount=0;
 if(admission==readonly::Admission::Retiring){idleRetired.valid=false;retireWorldFrame();publishSnapshot(now);return true;}
 const bool wanted=worldFrameServer.wanted(sh->owner,GetTickCount());
 const auto world=worldlife::capture(readLiveWorld);const auto change=hookWorld.observe(world);
 if(change==worldlife::Change::Unknown){idleRetired.valid=false;retireWorldFrame();snap.status=hookWorld.bound?1:0;publishSnapshot(now);return true;}
 if(change==worldlife::Change::Absent){idleRetired.valid=false;retireWorldFrame();sessionTracker.update(false,0,-1,-1);lastHouse=0;lastFrame=-1;snap.status=0;snap.match=0;publishSnapshot(now);return true;}
 if(change==worldlife::Change::Start||change==worldlife::Change::Replaced||change==worldlife::Change::Rewind){
  idleRetired.valid=false;retireWorldFrame();sessionTracker.update(false,0,-1,-1);
 }
 uint32_t house=0;if(worldlife::authority(readLiveWorld,house)==worldlife::Authority::Unknown){idleRetired.valid=false;retireWorldFrame();snap.status=1;publishSnapshot(now);return true;}
 const auto transition=sessionTracker.update(true,house,world.session,world.frame);
 if(transition!=sessionepoch::Transition::None){idleRetired.valid=false;retireWorldFrame();}
 match=sessionTracker.epoch;lastHouse=house;lastFrame=world.frame;
 snap.status=1;snap.match=match;snap.frame=world.frame;snap.session=world.session;
 uint8_t chat=1;snap.chat=!readLiveWorld(0xA8BC79,&chat,1)||chat!=0;
 const uint32_t demand=wanted?worldFrameServer.demandMask():0u;
 // Admit only the same display boundary as before. Re-read inside the
 // existing helper on all nonzero demands; never skip mixed-demand cleanup.
 if(demand!=0u){
  idleRetired.valid=false;
  captureDisplayOnlyWorld(world,house,now,wanted);
 }else if(idleRetired.needsRetirement(sh->owner,match,worldFrameServer.connection(),worldProducerGeneration,
           worldFrameReady||haveCaptureAttempt||worldFrameCandidateAvailable||
           !runtimeWorldDemands.empty()||runtimeDemandOverflow)){
  retireWorldFrame();
  idleRetired={true,sh->owner,match,worldFrameServer.connection(),worldProducerGeneration};
 }
 publishSnapshot(now);return true;
}
static void tick(){
 LARGE_INTEGER start{},end{},controlEnd{},worldEnd{};static LARGE_INTEGER frequency{};if(!frequency.QuadPart)QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
 sampleReadDepth=0;sampleReadComplete=true;runtimeDirectActions=false; // A platform unwind must never leak the sample domain.
 // A detached legacy fault may still own asynchronous retirement. Finish it
 // before opening a fresh mailbox; mapped failures retain their Host below.
 if(fault&&!sh){maintainTechParameters();disconnect();return;}
 if(!sh){wchar_t n[80];mapName(n,GetCurrentProcessId());mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,n);if(!mapping)return;sh=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));}
 // A deferred disconnect must never turn an invalid mapping into a usable
 // command connection on the next callback.
 if(!sh||sh->magic!=Magic||sh->version!=Version||sh->pid!=GetCurrentProcessId()){disconnect();return;}
 syncDebugPolicy(GetTickCount()); // Before every cold feature/DR admission.
 DWORD now=sampleClock(sh);if(sh->stop||(DWORD(now-sh->heartbeat)>3000&&controllerExited(now))){
  maintainTechParameters();
  stickyTick(true);
  // Release our automatic attack/hold orders before acknowledging detach. No game
  // field is changed locally, including when the controlling EXE has exited.
  if(missileHoldCount&&lastHouse&&get<uint32_t>(0xA83D4C)==lastHouse&&get<bool>(0xA8E9A0)){
   int cleanupFrame=get<int>(0xA8ED84);snap.frame=cleanupFrame;missileCleanup=true;
   bool released=releaseMissileHolds(lastHouse,cleanupFrame,now);missileCleanup=false;if(!released)return;
  }
  // Stop admitting native input before retiring the sender. A changed world
  // or an invalid pending queue may prevent safe restoration; keep the IPC
  // and message entry alive so a later game-thread tick can finish cleanup.
  // stop=2 permits the Host to remove its last message-hook subscription.
  restorePrivateOut();restoreNativeSellCursor();restoreNativeMegaGateway();restoreControlObjectOwner();restoreControlCommandOwner();
  restoreFrameDriver();disconnect();return;
 }
 // The failed connection remains latched until explicit stop or a dead Host.
 // Retry retirement and publish failure without reopening ordinary work.
 if(fault){reset();maintainTechParameters();snap.status=snap.bridgeStatus=-2;snap.commandStatus=0;snap.labels=0;publishSnapshot(now);return;}
 if(serviceReadOnlyRuntime(now))return;
 if(!sh->superweaponGrant)superweaponGrantReset();
 maintainTechParameters();playerMaintenance();controlHouseFlagMaintenance();
 if((snap.techOpen||techPending||techDataPending())&&!(sh->flags&UnsafeTech))restoreTech();
 if(techDataRestoring&&techImplementation!=3)restoreTechData();
 const bool sharedWorldWanted=worldFrameServer.wanted(sh->owner,GetTickCount());
 const auto worldSample=worldlife::capture(readLiveWorld);const auto worldChange=hookWorld.observe(worldSample);
 if(sharedWorldWanted&&worldFrameReady&&worldSample.valid()&&worldSample.scenario==worldFrame.key.scenario&&worldSample.cells==worldFrame.key.cells&&uint32_t(worldSample.count)==worldFrame.key.cellCount&&worldSample.session==worldFrame.key.session){uint32_t currentHouse=0;if(readLiveWorld(0xA83D4C,&currentHouse,4)&&currentHouse==worldFrame.key.house){auto observed=worldFrame.key;observed.frame=worldSample.frame;worldFrameServer.observe(observed,observed.frame==worldFrame.key.frame||(worldFrameBoundaryOnly&&worldFrameServer.demandMask()==uint32_t(worldframe::Base|worldframe::HostDisplayOnly)));}}
 if(worldCleanupPending){reset();worldCleanupPending=!featureRestorationComplete();if(worldCleanupPending){snap.commandStatus=activation::CleanupPending;publishSnapshot(now);return;}}
 if(worldChange==worldlife::Change::Unknown){retireWorldFrame();snap.status=hookWorld.bound?1:0;snap.bridgeStatus=1;suspendedSnapshot(now,false,activation::WorldUnknown);return;}
 const uint32_t runtimeActionFlags=AutoRepair|EjectAssist|AdvancedInput|SmartUnloadOrders|AutoOreReturn|MagnetronInput|AdvancedEnter|AircraftSpread|AirWaypoints|AntiTheft;
 const bool runtimeActionDemand=(sh->flags&runtimeActionFlags)||sh->magnetronGlobal||sh->magnetronDrop||
  sh->missileEnabled||sh->dodgeEnabled||
  sh->minerDefenseEnabled||sh->bombBoardEnabled||sh->loadMode||sh->cooperativeFrameWanted||
  sh->formationPolicy.enabled||sh->moFeatures||sh->runtimeTracking||sh->plannerTracking||
  sh->privateOutMode||sh->keepInputUnlocked||sh->blockTriggerCamera;
 const uint32_t runtimeFrameFlags=EjectAssist|AdvancedInput|SmartUnloadOrders|AutoOreReturn|MagnetronInput|AdvancedEnter|AircraftSpread|AntiTheft;
 const bool runtimeFrameDemand=(sh->flags&runtimeFrameFlags)||sh->magnetronGlobal||sh->magnetronDrop||
  sh->missileEnabled||sh->dodgeEnabled||
  sh->minerDefenseEnabled||sh->bombBoardEnabled||sh->loadMode||sh->cooperativeFrameWanted||
  sh->formationPolicy.enabled||sh->moFeatures||sh->runtimeTracking||sh->plannerTracking;
 DWORD period=sh->foreground?sh->scanMs:((runtimeActionDemand||sh->bombBoardEnabled||sh->minerDefenseEnabled||minerDefensePending())?64:500);if(period<2)period=2;if(period>1000)period=1000;
 // Runtime-owned actions bypass the ordinary display scan cadence. The game
 // frame remains the scheduler; this only bounds waits between frame callbacks.
 if(sh->foreground&&(runtimeActionDemand||scrollRefreshActive||burstCount||sh->scrollRefreshHz))period=4;
 bool selectionPending=sh->selectRequest!=sh->selectAck;bool planPending=sh->planRequest!=sh->planAck;bool cooperativePending=sh->cooperativePlan!=sh->cooperativeAck;bool cmd=sh->request!=sh->ack||selectionPending||planPending||cooperativePending||sh->pointer.request!=sh->pointer.ack;// A message wake observes the current simulation frame; pending work bypasses
 // display cadence. This is not a frame hook and does not advance the game.
 bool frameDue=worldSample.frame!=lastFrame&&
   (sharedWorldWanted||(directSellPending()||privateOutPending()||sh->privateOutMode)||sh->keepInputUnlocked||sh->blockTriggerCamera||sh->bombBoardEnabled||sh->minerDefenseEnabled||minerDefensePending()||sh->superweaponGrant||sh->placementPreview.active||nativePlacementCompletionPending()||snap.techOpen||techPending||techDataPending()||hostFrameCaptureWanted(now)||(sh->moFeatures&7u)||moOrdersPending()||directAirPending()||sh->formationPolicy.enabled||formationJobCount||stickyCount||sh->magnetronGlobal||sh->magnetronDrop||sh->dodgeEnabled||dodgeCount||burstCount||sh->missileEnabled||missileHoldCount||nativeCount||unloadCount||queuedUnload.count||sh->runtimeTracking||runtimeCoopPending||loadCalculationPending||repairCalculationPending||loadLedger.count||repairAt<repairCount||manualRepairPending||pendingRepairs.count||entryReservationCount||cooperativeEffectCount||missionObserver.pending()||actorLeaseArbiter.count||ownground::job.pending||(sh->flags&(EjectAssist|AntiTheft))||oreHarvestNeedsWork()||
    runtimeFrameDemand||sh->planRequest!=sh->planAck||sh->cooperativePlan!=sh->cooperativeAck);
 bool resultsDue=(loadCalculationPending&&compute::newer(sh->loadComputed,loadTaskSeen))||(repairCalculationPending&&compute::newer(sh->repairComputed,repairTaskSeen))||((sh->missileEnabled||missileHoldCount)&&compute::newer(sh->missileComputed,missileApplied))||((sh->dodgeEnabled||dodgeCount)&&compute::newer(sh->dodgeComputed,dodgeApplied))||(unloadCount&&compute::newer(sh->unloadComputed,unloadApplied));
 bool soundPending=sh->uiSoundRequest!=sh->uiSoundAck;
 const uint64_t preciseNow=preciseclock::micros();static uint64_t fastSampleAt=0;
 bool refreshDue=(featureavailability::SceneRefresh&&sh->foreground&&sh->scrollRefreshMode&&sh->scrollRefreshHz&&scrollCadence.ready(preciseNow,sh->scrollRefreshHz))||
   (scrollRefreshActive&&(!sh->scrollRefreshMode||!sh->scrollRefreshHz));
 bool fastDue=sh->foreground&&(sh->dodgeEnabled||dodgeCount||burstCount)&&preciseNow-fastSampleAt>=2000;
 // Updating the display consumes its frame wake. Continue polling for a
 // pending strict capture on the existing 4 ms Host pulse.
 const bool indexRetryDue=indexedWorkDeferred&&DWORD(now-last)>=4;
 if(worldChange==worldlife::Change::Same&&!cmd&&!resultsDue&&!soundPending&&!frameDue&&!refreshDue&&!fastDue&&!indexRetryDue&&DWORD(now-last)<period)return;last=now;fastSampleAt=preciseNow;
 indexedWorkDeferred=false;
 // Include admission and maintenance in game-thread timing.
 uint32_t house=0;const auto authority=worldlife::authority(read,house);const int frame=worldSample.frame,sessionMode=worldSample.session;
 const bool worldGone=worldChange==worldlife::Change::Absent;
 bool active=false;
 if(worldGone){if(soundPending){MemoryBarrier();sh->uiSoundAck=sh->uiSoundRequest;}cancelCooperativePlan(-3);if(lastHouse||snap.status!=0){reset();worldCleanupPending=!featureRestorationComplete();}sessionTracker.update(false,0,-1,-1);lastHouse=0;lastFrame=-1;commandDormant=false;snap.status=0;snap.bridgeStatus=1;snap.commandStatus=0;snap.labels=0;snap.swCount=0;if(sh->request!=sh->ack){sh->result=-3;sh->count=0;pendingAck=sh->request;pendingAckReady=true;}if(selectionPending){sh->selectResult=-3;sh->selectCount=0;MemoryBarrier();sh->selectAck=sh->selectRequest;}if(planPending){sh->planResult=-3;sh->planAccepted=0;sh->planAcceptedMask=sh->planRejectedMask=0;MemoryBarrier();sh->planAck=sh->planRequest;}}
 else {
 const bool replaced=worldChange==worldlife::Change::Start||worldChange==worldlife::Change::Replaced||worldChange==worldlife::Change::Rewind;
 if(replaced){if(worldChange==worldlife::Change::Rewind)crcMapShadowRewind(sessionTracker.epoch+1);if(lastHouse)cancelCooperativePlan(-3);reset();worldCleanupPending=!featureRestorationComplete();sessionTracker.update(false,0,-1,-1);lastHouse=0;commandDormant=false;if(worldCleanupPending){snap.commandStatus=activation::CleanupPending;publishSnapshot(now);return;}}
 // A missing House/roster read is uncertainty within this confirmed world.
 // Keep both the command epoch and owned hooks until identity is readable.
 if(authority==worldlife::Authority::Unknown){snap.status=1;snap.bridgeStatus=1;snap.frame=frame;snap.session=sessionMode;suspendedSnapshot(now,false,activation::AuthorityUnknown);return;}
 auto matchTransition=sessionTracker.update(true,house,sessionMode,frame);
 const bool houseChanged=!replaced&&matchTransition==sessionepoch::Transition::HouseChanged;
 bool newMatch=replaced||matchTransition!=sessionepoch::Transition::None;
 if(newMatch){if(houseChanged){cancelCooperativePlan(-3);reset(false);}match=sessionTracker.epoch;lastHouse=house;sh->inputUnlockCount=0;sh->triggerInputBlocked=sh->triggerCameraBlocked=0;sh->sellDirectSent=sh->sellDirectRejected=0;sh->inputUnlockStatus=sh->keepInputUnlocked?inputlock::Waiting:inputlock::Off;snap.typeCount=0;catalogueDirty=true;requestTypeCatalogue();slow=now-100;sceneFrame=-2147483647;sceneTick=0;}
 // A read-only world can predate the first command entry of this epoch.
 // Same-match pause/resume must retain its already latched placement.
 applyCommandComputeEpoch();
 // Local playable state above is sufficient; do not wait for remote FrameInfo.
 snap.frame=frame;snap.match=match;snap.session=sessionMode;snap.status=1;snap.bridgeStatus=1;
 // Function entries are checked by their actual consumers. This startup
 // boundary only retains fail-closed handling for unclassified future probes;
 // world, protocol, authority and owned-hook cleanup gates remain independent.
 if(!checked&&(!signatureRetryAt||DWORD(now-signatureRetryAt)>=1000)){
  signatureRetryAt=now;good=signatures();checked=good;
 }
 if(!good){snap.commandStatus=3;publishSnapshot(now);return;}
 const auto display=external::display::inspect(read);
 active=authority==worldlife::Authority::Command&&worldSample.running&&display.visible();
 if(!active){
  resetOwnGround();
  if(authority==worldlife::Authority::ViewOnly&&!commandDormant){cancelCooperativePlan(-3);reset(false);commandDormant=true;}
  if(authority==worldlife::Authority::ViewOnly)
   captureDisplayOnlyWorld(worldSample,house,now,sharedWorldWanted);
  else capturePassiveWorld(worldSample,house,now,sharedWorldWanted);
  suspendedSnapshot(now,worldSample.running&&display.visible(),authority==worldlife::Authority::ViewOnly?activation::ViewOnly:!worldSample.running?activation::Paused:activation::DisplayHidden);return;
 }
 commandDormant=false;
 techEnhancementTick(now);techEnhanceParametersTick(house,now);mapVisibilityParametersTick(house);
 const bool catalogueDue=newMatch||!worldFrameCatalogueProbeAt||DWORD(now-worldFrameCatalogueProbeAt)>=DWORD(catalogueReady?500:100);
 // Type metadata is acquired independently of the tactical graph. The game
 // thread applies its current production rules to the sealed observation.
 const bool nativeCatalogue=!worldFrameCommandWorker.enabled()&&
  sh->computeApplied==compute::All&&gameentry::active(gameentry::Client::Assist);
 const bool graphCatalogueDue=catalogueDue&&!nativeCatalogue;
 if(nativeCatalogue&&catalogueDue){
  static worldframe::Snapshot catalogue;
  const auto catalogueKey=samplingKey(worldSample,house);
  bool updated=false;
  if(worldFramePublication.takeCatalogue(catalogueKey,catalogue)){
   const auto observed=catalogue.catalogue;
   SampleReadScope sample(false,&catalogue);
   if(sample&&((catalogueReady&&catalogueHash==observed.hash)||(scanTypes()&&sample.complete()))){
    catalogueHash=observed.hash;catalogueReady=true;catalogueAt=catalogue.key.tick;
    catalogueRequestedAt=0;worldFrameCatalogueProbeAt=now;updated=true;
   }
  }
  if(!updated&&nativeCatalogueProbe.begin(catalogueKey.tick))worldFramePublication.requestCatalogue(catalogueKey);
 }
 uint32_t worldDemand=worldframe::Base;
 if(graphCatalogueDue)worldDemand|=worldframe::TypeCatalogue;
 const uint32_t hostDemand=sharedWorldWanted?worldFrameServer.demandMask():0u;
 worldDemand|=hostDemand;
 if(sh->dodgeEnabled||dodgeCount)worldDemand|=worldframe::Bullets|worldframe::Combat;
 if(sh->missileEnabled||missileHoldCount||sh->minerDefenseEnabled||minerDefensePending()||sh->magnetronGlobal||sh->magnetronDrop||(sh->flags&(AdvancedInput|MagnetronInput|AircraftSpread|AdvancedEnter|AntiTheft))||nativeCount||runtimeCoopPending||unloadCount||queuedUnload.count)worldDemand|=worldframe::Combat;
 if(capturepolicy::passengerGraphNeeded(sh->flags,sh->externalDisplay!=0)||sh->bombBoardEnabled||loadLedger.count||loadCalculationPending||unloadCount||queuedUnload.count||cooperativeEffectCount)worldDemand|=worldframe::Passengers;
 if((sh->flags&AutoOreReturn)||oreHarvestNeedsWork()||sh->minerDefenseEnabled||minerDefensePending())worldDemand|=worldframe::Ore;
 // The full display graph belongs to the separate Host VisualCache. The
 // game thread may publish a small scene boundary only if ALL Host readers
 // are display-only and no local native command feature has any pending work.
 // A same-frame hotkey later upgrades this boundary to a full graph.
 // Scene refresh is native camera/composition work, not a world graph consumer.
 // Bomb boarding, formation and direct aircraft gestures use current native
 // objects. They still participate in command-index and frame scheduling.
 // An enabled gesture preference does not itself require a graph. A live
 // gesture still escalates through moOrdersPending(), including retries.
 // Input-lock and trigger-view maintenance read their own live local
 // state. They still wake the dispatcher, but do not consume CaptureGraph.
 // The native placement preview/completion reads only the current offer,
 // cursor and viewport. Superweapon maintenance reads the local House list.
 // All retain their existing wake and command guards; neither needs a
 // tactical snapshot just to service an already active local task.
 const bool noGraphJobs=
  !(sh->runtimeTracking&~uint32_t(AdvancedInput|MagnetronInput|AutoOreReturn))&&!sh->plannerTracking&&
  // Retired Burst flags are ABI compatibility fields, not graph demand.
  !stickyCount&&
  !burstCount&&!loadCalculationPending&&
  !ownground::job.pending&&
  !moOrdersPending()&&
  !directSellPending()&&!privateOutPending()&&
  !sh->privateOutMode&&!techPending&&!techDataPending()&&!snap.techOpen;
 // Match start by itself is not a command or display graph consumer.
 // Native catalogue publication above is independent; if catalogue data
 // still needs a graph, TypeCatalogue in worldDemand excludes this lane.
 const bool noCommandWork=!cmd&&!resultsDue&&noGraphJobs&&
   !sh->bombBoardEnabled&&!sh->formationPolicy.enabled&&!formationJobCount&&!directAirPending()&&
   !sh->runtimeTracking&&!sh->minerDefenseEnabled&&!minerDefensePending()&&!oreHarvestNeedsWork()&&!manualVersions.pausedCount&&!sh->magnetronGlobal&&!sh->magnetronDrop&&!sh->dodgeEnabled&&!dodgeCount&&!nativeCount&&
   !sh->missileEnabled&&!missileHoldCount&&!missileSendCount&&!loadLedger.count&&
   !unloadCount&&!queuedUnload.count&&!entryReservationCount&&!cooperativeEffectCount&&
   !runtimeCoopPending&&!sh->cooperativeFrameWanted&&
   !actorLeaseArbiter.count&&!missionObserver.pending()&&
   !capturepolicy::needsNativeFlagWork(sh->flags,sh->externalDisplay!=0)&&
   !repairCalculationPending&&!manualRepairPending&&!(repairAt<repairCount)&&!pendingRepairs.count;
 const bool hostDisplayOnly=sharedWorldWanted&&noCommandWork&&
  worldDemand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly);
 // Direct Runtime lane: native repair, selection, combat, dodge, missile defense, boarding, unload and cooperative planning.
 // Only the remaining graph-dependent collectors and command mailboxes
 // retain their old path. Pointer admission is independent of visual publication.
 const bool directCommand=sh->request==sh->ack||
  sh->command==Repair||sh->command==AutoBoard||sh->command==SmartUnload||(sh->command>=CombatSelection&&sh->command<=IdleTransport);
 // A missing Host subscription is not a graph consumer either. Preserve
 // the existing direct-command eligibility and retain pending Runtime page
 // requests when no Host display boundary is available.
 const bool directRuntime=noGraphJobs&&!newMatch&&!resultsDue&&!graphCatalogueDue&&
  !worldFrameCommandWorker.enabled()&&sh->externalDisplay&&
  !(sh->flags&~(capturepolicy::PassiveVisualFlags|capturepolicy::LocalDispatchFlags|AutoRepair|RepairUnrepairable|AdvancedInput|MagnetronInput|NativeOrders|CustomCS|SmartUnloadOrders|EjectAssist|AdvancedEnter|AircraftSpread|AutoOreReturn|AntiTheft|NativeAirWaypoints))&&
  !capturepolicy::pointerNeedsGraph(sh->flags,sh->pointer.request,sh->pointer.ack)&&
  !selectionPending&&!planPending&&!cooperativePending&&
  directCommand&&!repairCalculationPending&&
  sh->computeApplied==compute::All&&
  (hostDemand==uint32_t(worldframe::Base|worldframe::HostDisplayOnly)||
   (!hostDemand&&runtimeWorldDemands.empty()&&!runtimeDemandOverflow));
 // Display-only demand publishes a zero-page world boundary. The Host
 // VisualCache independently samples the read-only display graph; it never
 // becomes a source of Runtime commands or action-planning authority.
 // Do not mistake a connected but unsubscribed Host, a fresh match or an
 // ordinary idle wake for a graph consumer. Keep legacy rendering, pending
 // work, unknown dependencies and every existing command lane unchanged.
 // Catalogue mode itself is not a consumer between catalogue deadlines.
 // An actual due graph catalogue still excludes this idle lane.
 const bool idleWithoutGraph=noCommandWork&&sh->externalDisplay&&!graphCatalogueDue&&
  worldDemand==uint32_t(worldframe::Base)&&
  !hostDemand&&
  runtimeWorldDemands.empty()&&!runtimeDemandOverflow;
 if(hostDisplayOnly)
  captureWorldFrame(readLiveWorld,worldSample,house,now,worldDemand,true);
 else if(directRuntime&&!idleWithoutGraph)
  captureWorldFrame(readLiveWorld,worldSample,house,now,worldframe::Base|worldframe::HostDisplayOnly,true);
 else if(!idleWithoutGraph&&(sharedWorldWanted||frameDue||cmd||resultsDue||fastDue||newMatch||hostFrameCaptureWanted(now)))
  captureWorldFrame(readLiveWorld,worldSample,house,now,worldDemand,false);
 if(!nativeCatalogue&&(worldFrame.catalogue.attempted||worldFrameCandidate.catalogue.attempted)){
  ObservationReadScope sample;
  if(sample&&sampleWorldSource->catalogue.complete){
   const auto sourceTick=sampleWorldSource->key.tick;
   const auto observed=sampleWorldSource->catalogue;
   if((!catalogueRequestedAt||int32_t(sourceTick-catalogueRequestedAt)>0)&&
      (!catalogueAt||int32_t(sourceTick-catalogueAt)>0)){
    if((catalogueReady&&catalogueHash==observed.hash)||scanTypes()){
     catalogueAt=sourceTick;catalogueHash=observed.hash;catalogueReady=true;
     catalogueRequestedAt=0;
    }
   }
  }
 }
 const bool observationFrame=inputMatch!=match||inputFrame!=frame;
 if(observationFrame){inputMatch=match;inputFrame=frame;}
 bool finishSnapshot=false;
 withInputPublication(sh,match,now,frame,observationFrame,[&]{
 // Native execution and snapshot publication are independent. A legacy
 // collector or catalogue refresh may still require a graph, but that must
 // never force migrated synchronous consumers back into SampleReadScope.
 runtimeDirectActions=!worldFrameCommandWorker.enabled()&&sh->computeApplied==compute::All&&gameentry::active(gameentry::Client::Assist);
 // Control time belongs to the dispatcher, never to the optional display scan.
 rebuildPlayerSidebar(house);lastFrame=frame;snap.frame=frame;snap.match=match;snap.session=sessionMode;snap.status=1;snap.bridgeStatus=1;snap.commandStatus=1;pauseViewTick();buildTimeTooltipTick();
 snap.chat=get<bool>(0xA8BC60+25);if(!sh->foreground||snap.chat)runtimeKeys.clear();inputLockTick();
 offlineToolPreview(house,frame,now);
 if(worldChange==worldlife::Change::Same&&lastHouse)oreHarvestObserve(lastHouse,frame,now);
 bool needsIndex=hookpolicy::needsWorldIndex(cmd,(directSellPending()||privateOutPending()||sh->privateOutMode)||sh->bombBoardEnabled||sh->minerDefenseEnabled||minerDefensePending()||(sh->moFeatures&7u)||moOrdersPending()||directAirPending()||sh->formationPolicy.enabled||formationJobCount||stickyCount||sh->magnetronGlobal||sh->magnetronDrop||sh->planRequest!=sh->planAck||sh->dodgeEnabled||dodgeCount||burstCount||sh->missileEnabled||missileHoldCount||missileSendCount||missionObserver.pending()||pendingRepairs.count||actorLeaseArbiter.count||loadLedger.count||loadCalculationPending||repairCalculationPending||manualVersions.pausedCount||oreHarvestNeedsWork()||runtimeCoopPending||sh->cooperativeFrameWanted||(cooperative::valid(sh->cooperativeRequest)&&sh->cooperativeRequest.serial!=runtimeCoopRequest.serial)||nativeCount||unloadCount||queuedUnload.count||repairAt<repairCount||manualRepairPending||ownground::job.pending||entryReservationCount||cooperativeEffectCount||sh->cooperativePlan!=sh->cooperativeAck,
  sh->flags|sh->plannerTracking|((sh->cooperativePlan!=sh->cooperativeAck)?AutoOreReturn:0),(sh->flags&AutoRepair)&&(!autoWas||DWORD(now-lastAuto)>=clampedRepairInterval(sh->repairMs)));
 const bool deferIndexedWork=needsIndex&&(!refreshWorldIndex(frame)||!commandRosterReady());
 indexedWorkDeferred=deferIndexedWork;
 if(deferIndexedWork){++sh->worldSampleFailures;++sh->sampling.indexUnavailable;}
 const bool localDeferred=deferIndexedWork&&eventdelivery::isSingleplayer(sessionMode);
 const bool localCandidate=localDeferred&&candidateRosterReady();
 // Service delayed planning once per current frame, with a wall-clock floor
 // when an uncapped local match advances multiple frames per callback burst.
 static uint32_t candidateWorkMatch=0,candidateWorkVersion=0;static int candidateWorkFrame=-1;
 static uint64_t candidateWorkAt=0;
 const bool candidateInputFrame=localCandidate&&
  (candidateWorkMatch!=match||(candidateWorkFrame!=frame&&candidateWorkVersion!=worldFrameCandidateVersion&&preciseNow-candidateWorkAt>=4000));
 if(candidateInputFrame){candidateWorkMatch=match;candidateWorkFrame=frame;candidateWorkVersion=worldFrameCandidateVersion;candidateWorkAt=preciseNow;}
 // The first strict pass may follow an observation in the same game frame.
 // Keep the frame-driven command inputs pending until that pass can run.
 externalInputFrame=!deferIndexedWork&&(indexedInputMatch!=match||indexedInputFrame!=frame);
 if(externalInputFrame){indexedInputMatch=match;indexedInputFrame=frame;}
 if(needsIndex&&!deferIndexedWork){
  privateOutTick();servicePrivateOut();serviceDirectSell();
  // Release stale leases before any feature sends a new game order.
  sweepActiveLeases(frame);
 }
 if(!deferIndexedWork){stickyTick(false);observePausedOre(house,frame);observeUnloadRoster(house,frame,now);}
 viewZoomTick();sidebarViewTick();smallMapCameraTick();radarNavigationTick(now);if(nativePipJumpPending&&nativePipDemand())GuardedNativePipAction(4,0);highScrollTick(preciseNow);
 // Only tick consumes feature requests; neither Hook callback can bypass
 // the shared game-thread intent/validation/event pipeline.
 if(!deferIndexedWork){superweaponGrantTick(house,frame,now);consumeSubmittedIntents(house,frame,now);}
 else{
  uiSoundCommand(now);
  // Missing the exact frame at high FPS only delays work that needs a
  // complete current view. An already proposed local attack can be proved
  // against the two live objects, with the original manual revision.
  // New input and unit production use their own narrow validation even
  // before a candidate arrives. Commands that need a parent UID still wait.
  ownGroundTick(house,frame,now,false);
  command(house,now,localCandidate?commandsampling::Sample::LocalCandidate:commandsampling::Sample::Missing);
  if(localCandidate){
   if(queuedUnload.count||unloadCount){
    int budget=recurringBudget(EventLane::Unload);
    smartUnloadAdmissionTick(house,frame,budget,true);smartUnloadTick(house,frame,budget,true);
   }
   if(candidateInputFrame&&loadLedger.count)autoLoadTick(house,frame,true);
   if(sh->planRequest!=sh->planAck){
    if(snap.nativeHookStatus!=1)nativeHooksTick(sh->flags);
    externalOrdersTick(house,frame,now,true);
   }
   if(sh->pointer.request!=sh->pointer.ack)pointerTick(true);
   serviceCooperativePlan(house,frame,now,true,preciseNow);
  }
 }
 nativePlacementTick(now);
 if(playerContextChanged){playerContextChanged=false;snap.commandStatus=activation::HouseChanged;publishSnapshot(now);return;}
 QueryPerformanceCounter(&controlEnd);snap.controlMicros=micros(start,controlEnd,frequency);
 // Release receipts before any recurring work or overlay projection. In a busy
 // scene the next user command can now be staged while rendering catches up.
 if(cmd&&(!deferIndexedWork||pendingAckReady)){snap.scanMicros=snap.controlMicros;publishSnapshot(now);}
 formationTick(house,frame,now,!deferIndexedWork);moOrdersTick(house,frame,now,!deferIndexedWork);
 if(!deferIndexedWork){directair::tick(house,frame,now);bombboard::tick(house,frame);}
 if(!deferIndexedWork||candidateInputFrame)loadingCalculationTick(house,now,deferIndexedWork);
 repairTick(house,now,candidateInputFrame);
 if(!deferIndexedWork){autoLoadTick(house,frame);nativeOrdersTick(house,frame);}
 // Terminal receipts also release ownership. Observe only with a strict roster.
 if(!deferIndexedWork&&missionObserver.pending())snap.observedEvents+=missionObserver.pollWithReceipts(match,frame,now,[](uint32_t id){
  return observeMissionReceipt(id);
 },[](const gateway::Receipt& receipt){
  recordReceipt(receipt);
  actorLeaseArbiter.release(receipt.match,receipt.unit,receipt.serial);
  if(receipt.phase==gateway::Phase::Cancelled)++snap.receiptCancelled;
  else if(receipt.phase==gateway::Phase::Expired)++snap.receiptExpired;
 });
 const bool dodgeChanged=(!deferIndexedWork||candidateInputFrame)&&dodgeControlTick(house,frame,now,deferIndexedWork);
 resetBurstControl(); // Removed feature: old shared flags never produce commands.
 const bool burstChanged=false;
 captureCooperativeFrame(now,!deferIndexedWork);
 const bool planningChanged=publishPlanningWorld(house,frame,now,!deferIndexedWork);
 const bool minerDefenseChanged=!deferIndexedWork&&minerDefenseTick(house,frame,now);
 oreHarvestTick(house,frame,now,!deferIndexedWork);
 if(!deferIndexedWork)runtimePlanningTick(house,frame,now);
 const bool missileChanged=!deferIndexedWork&&missileControlTick(house,frame,now);
 QueryPerformanceCounter(&worldEnd);snap.worldMicros=micros(controlEnd,worldEnd,frequency);
 uint32_t tactical=get<uint32_t>(0x887324),flags=sh->flags;int cx=tactical?get<int>(tactical+0xB0):0,cy=tactical?get<int>(tactical+0xB4):0;
 double zoom=tactical?get<double>(tactical+0xB8):0;int width=get<int>(0x8A00A4),height=get<int>(0x8A00A8);
 bool cameraMoved=cx!=cameraX||cy!=cameraY;
 // Render cadence is independent of world scans. Scan at most about 60 Hz;
 // unchanged frames reuse their scene, and camera-only changes shift it.
 static uint32_t selectedStamp;bool selectionChanged=false;
 // Host overlays already track selection on their own input path.
 if(!sh->externalDisplay&&!deferIndexedWork){
  uint32_t stamp=2166136261u;Vec selected=vec(0xA8ECB8);for(int i=0;i<selected.count;++i)stamp=(stamp^item(selected,i))*16777619u;
  selectionChanged=stamp!=selectedStamp;selectedStamp=stamp;
 }
 bool force=newMatch||selectionPending||selectionChanged||flags!=sceneFlags||tactical!=sceneTactical||width!=sceneWidth||height!=sceneHeight||zoom!=sceneZoom||snap.crateResultSerial!=sceneResultSerial;
 bool full=force||(DWORD(now-sceneTick)>=16&&(frame!=sceneFrame||(cameraMoved&&DWORD(now-sceneTick)>=32)||DWORD(now-sceneTick)>=100));
 bool summaryChanged=(sh->externalDisplay||!deferIndexedWork)&&scanSummary(house,frame,now,flags);
 if(!deferIndexedWork){
  if((snap.techOpen||techPending)&&((techPending&&DWORD(now-techRefresh)>=16)||DWORD(now-techRefresh)>=1000))refreshTech(house,now);
  maintainTechCameos(house,frame,now);
 }
 if(full&&(sh->externalDisplay||!deferIndexedWork)){if(!sh->externalDisplay)scan(house,frame,now);sceneFrame=frame;sceneTick=now;sceneFlags=flags;sceneTactical=tactical;sceneWidth=width;sceneHeight=height;sceneZoom=zoom;sceneResultSerial=snap.crateResultSerial;}
 else if(cameraMoved)shiftScene(cx-cameraX,cy-cameraY);
 else if(!observationFrame&&!deferIndexedWork&&!cmd&&!summaryChanged&&!planningChanged&&!minerDefenseChanged&&!missileChanged&&!burstChanged&&!dodgeChanged&&DWORD(now-snap.sampledTick)<250)return;
 cameraX=cx;cameraY=cy;snap.cameraX=cx;snap.cameraY=cy;
 finishSnapshot=true;
 });
 if(!finishSnapshot)return;
 }
 QueryPerformanceCounter(&end);if(active)snap.displayMicros=micros(worldEnd,end,frequency);
 snap.scanMicros=micros(start,end,frequency);publishSnapshot(now);
}
// One message-driven dispatcher owns all game event construction, legality,
// acknowledgements and SP/MP routing. There is no simulation-frame detour.
extern "C" void GuardedAssistTick();
// Both entrypoints call this ONE game-thread dispatcher. Their only job is
// waking it; event construction / queue selection are wholly inside tick().
static void dispatchFromTrigger(dispatchrouter::Trigger trigger,DWORD now,const MSG* message=nullptr);
static void restoreFrameDriver(){restoreSinglePlayerFPS();}
static void maintainScheduler(){if(sh){sh->driverMode=scheduler::Message;sh->driverTarget=0;sh->driverStatus=scheduler::MessageActive;}}
// One place owns provider choice, fallback and the execution lock.
// This adapter does not contain feature decisions or EventClass operations.
static void dispatchFromTrigger(dispatchrouter::Trigger trigger,DWORD now,const MSG* message){
 gameentry::scoped(gameentry::Client::Assist,trigger==dispatchrouter::Trigger::Message,[&]{
  if(trigger==dispatchrouter::Trigger::Message){
   InterlockedExchange(&messageGameThread,LONG(GetCurrentThreadId()));
   if(sh&&message&&message->message==WM_NULL&&message->wParam==WakeTag&&message->lParam==sh->owner)
    InterlockedExchange(&sh->gameWakePending,0);
  }
  if(trigger!=dispatchrouter::Trigger::Message)return;
  if(InterlockedCompareExchange(&schedulerBusy,1,0)!=0)return;
  const auto previousFaultContext=faultContext;
  // Only the successful CAS owns this release, including an SEH unwind.
  runtimeunwind::run([&]{
   // A frame callback is a safe execution point, not an independent producer.
   // Preserve the grant on rejected entry/router/reentrancy; consume only here.
   if(!InterlockedExchange(&hostWorkPending,0))return;
   // Producers and event gateway do not know which provider woke this tick.
   // The shared request/ack serials and unit arbiter prevent redispatching an
   // already accepted user request when ownership changes between callbacks.
   GuardedAssistTick(); // tick owns normal admission and cleanup-only faults
   // Install / remove experimental hooks only from the original message path.
   if(trigger==dispatchrouter::Trigger::Message)maintainScheduler();
  },[&]{faultContext=previousFaultContext;InterlockedExchange(&schedulerBusy,0);});
 });
}
extern "C" unsigned BeginAssistCapture(){
 gameentry::Request r{};return gameentry::capture(gameentry::Client::Assist,r)?r.token:0;
}
extern "C" void EndAssistCapture(unsigned token){
 gameentry::Request r{};r.module=gameentry::moduleIdentity();r.client=gameentry::Client::Assist;r.token=token;gameentry::leave(r);
}
extern "C" void AssistTick(){if(gameentry::active(gameentry::Client::Assist))tick();}
extern "C" void AssistFault(){fault=true;resetOwnGround();maintainTechParameters();superweaponGrantReset();restoreMapVisibilityParameters();restoreControlParameters();restoreTechEnhanceParameters();restoreTechParameters();restoreSidebarView();nativePlacementReset();nativePlacementCompletionReset();restorePauseView();restoreCrcMapShadow();restoreBuildTimeTooltip();restorePrivateOut();restoreNativeSellCursor();restoreNativeMegaGateway();restoreControlObjectOwner();restoreControlCommandOwner();restoreNetworkSender();restoreTechEnhancement();restorePlayerContext();faultContext={};nativeMegaQueueCallDepth=nativeObjectQueueCallDepth=0;resetMoOrders();resetDirectAir();resetDirectSell();resetBombBoard();restoreClickHooks();resetCooperativeState();resetFormation();resetMissileControl();resetBurstControl();resetDodgeControl();clearNativeOrders();restorePlacement();restoreTech();restoreCSHook();if(sh){sh->result=-10;InterlockedIncrement(&sh->seq);sh->data.status=-2;sh->data.bridgeStatus=-2;sh->data.commandStatus=0;sh->data.labels=0;MemoryBarrier();InterlockedIncrement(&sh->seq);sh->ack=sh->request;sh->selectResult=sh->planResult=-10;sh->selectAck=sh->selectRequest;sh->planAccepted=0;sh->planAcceptedMask=sh->planRejectedMask=0;sh->planAck=sh->planRequest;notifyHost();}}
extern "C" void GuardedAssistTick();
#include "native_message_policy.h"
extern "C" void AssistRuntimeMessage(int code,WPARAM wp,LPARAM lp){
 if(code>=0){
  InterlockedExchange(&hostWorkPending,1);
  dispatchFromTrigger(dispatchrouter::Trigger::Message,GetTickCount(),wp==PM_REMOVE?reinterpret_cast<const MSG*>(lp):nullptr);
 }
}
extern "C" void AssistRuntimeWatchdog(){
 if(!fault){
  if(!sh)return;
  const DWORD now=sampleClock(sh);
  if(!sh->stop&&!(DWORD(now-sh->heartbeat)>3000&&controllerExited(now)))return;
 }
 gameentry::scoped(gameentry::Client::Assist,false,[]{
  GuardedAssistTick(); // fault/stop/dead-controller cleanup only; never normal work
 });
}
extern "C" bool AssistRuntimeDetached(){return sh==nullptr&&!fault;}




