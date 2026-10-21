#include "pointer_batch.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include <stddef.h>
#include <utility>
#include "protocol.h"
#include "command_intent.h"
#include "runtime_allocation.h"
#include "external_planner.h"
#include "../../external_reader/source/catalogue_stamp.h"
#include "clock_ms.h"
#include "../../shared/source/precise_clock.h"
#include "../../shared/source/runtime_fault.h"
#include "signatures.h"
#include "autoload.h"
#include "outgoing_batch.h"
#include "object_index.h"
#include "hook_policy.h"
#include "event_delivery.h"
#include "mega_queue_backend.h"
#include "constructor_adapter.h"
#include "entry_gate.h"
#include "tactical_decisions.h"
#include "object_event_backend.h"
#include "repair_delivery.h"
#include "session_epoch.h"
#include "control_link.h"
#include "scheduler_policy.h"
#include "dispatch_router.h"
#include "order_policy.h"
#include "unit_task_arbiter.h"
#include "../../external_reader/source/display_state.h"
#include "../../shared/source/message_query.h"
extern "C" void* __cdecl memset(void*p,int c,size_t n){
 volatile unsigned char*d=(volatile unsigned char*)p;uint32_t word=uint32_t((unsigned char)c)*0x01010101u;
 while(n>=4){*(volatile uint32_t*)d=word;d+=4;n-=4;}while(n--)*d++=(unsigned char)c;return p;
}
extern "C" void* __cdecl memcpy(void*p,const void*q,size_t n){
 volatile unsigned char*d=(volatile unsigned char*)p;const volatile unsigned char*s=(const volatile unsigned char*)q;
 while(n>=4){*(volatile uint32_t*)d=*(const volatile uint32_t*)s;d+=4;s+=4;n-=4;}while(n--)*d++=*s++;return p;
}
extern "C" int __cdecl memcmp(const void*p,const void*q,size_t n){auto a=(const unsigned char*)p,b=(const unsigned char*)q;while(n--){if(*a!=*b)return *a-*b;++a;++b;}return 0;}
#include "input_patch.h" // Registry is needed by publishSnapshot(), defined below.
using namespace assist;
// Export concrete ABI data: an optimized mapName() may contain no contiguous
// string literal. The host validates both protocol and structure size before
// installing a hook; the release verifier can inspect these exports offline.
extern "C" __declspec(dllexport) const uint32_t AssistProtocolInfo[2]={Version,sizeof(Shared)};
extern "C" __declspec(dllexport) const wchar_t AssistMappingPrefix[]=L"Local\\RA2Assist_v059_";
static Shared*sh;static HANDLE mapping;static bool checked,good,fault;static DWORD last,slow;static uint32_t lastHouse,match,pendingAck;static int lastFrame;
static sessionepoch::Tracker sessionTracker;static Snapshot snap;
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
static volatile LONG schedulerBusy;
static volatile LONG hostWorkPending; // one coalesced grant from the Runtime host-message ingress
static volatile LONG messageGameThread; // Thread confirmed by WH_GETMESSAGE for this game window.

static bool playableHouse(uint32_t);static bool playerContextChanged;static void playerMaintenance();static void restorePlayerContext();static void publishPlayers();
enum class OrderAuthority { Local, PlayerTest, SellPlayerTest };
static bool authorizePlayerOrder(const intent::EventOrder&,OrderAuthority);
static void resetRuntimePlanner();static void resetLoadCalculation();
static uint64_t catalogueHash;static DWORD catalogueAt;static bool catalogueReady,catalogueDirty;
static void restoreFrameDriver();
static uint32_t repairIDs[2048];static int repairCount,repairAt,repairDone;
// Request serial zero is valid after wrap; pending state is explicit.
static bool pendingAckReady,repairAckPending;
static uint32_t repairAckRequest;static DWORD repairAckStarted;
static void resetMissileControl();
static int dodgeCount;static void resetDodgeControl();static bool dodgeOwns(uint32_t id);static void cancelDodge(uint32_t id);
static int burstCount;static void resetBurstControl();static bool burstOwns(uint32_t id);static void cancelBurst(uint32_t id);
static bool repairCalculationPending;static DWORD repairCalculationStarted;static uint32_t repairTaskSeen;
static uint32_t lastIdle[3];static int prevBusy,prevPaused,prevCompleted;static bool productionBaseline;
static uint8_t techAllowed[MaxTypes];
static uint32_t typePointers[MaxTypes];
static ManualVersions manualVersions;static ObjectIndex worldObjects;static int indexedFrame=-1,indexedCount=-1;static uint32_t indexedItems;
static ObjectIndex advancedObjects;
static int outgoingFrame=-1,outgoingBudget;
static void recordReceipt(const gateway::Receipt&r){
 auto &s=snap.recentReceipts[snap.receiptWrite%RecentReceiptCapacity];
 s={r.serial,r.match,r.unit,r.target,r.queuedAt,r.observedAt,
    uint8_t(r.phase),uint8_t(r.failure),uint8_t(r.owner),0};
 ++snap.receiptWrite;
 if(snap.receiptCount<RecentReceiptCapacity)++snap.receiptCount;
}
static gateway::MissionObserver<128> missionObserver;
static taskarbiter::FrameArbiter<2048> actorFrameArbiter;
static taskarbiter::LeaseArbiter<2048> actorLeaseArbiter;
static bool bridgePinned,autoWas;static DWORD lastAuto,techRefresh;
static repairdelivery::Pending<> pendingRepairs;
static void resetPointerInput();static void restoreClickHooks();static void resetCooperativeState();static void clickHooksTick(uint32_t);static void cooperativeTick(uint32_t,int,DWORD);
static void clearNativeOrders();static void restoreCSHook();static void restoreTech();static void clearFlashes();static void resetProduction();
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
template<class T> static T get(uint32_t p){return *(T*)(uintptr_t)p;}
template<class T> static void put(uint32_t p,T v){if(gameentry::active(gameentry::Client::Assist))*(T*)(uintptr_t)p=v;}
static bool read(uint32_t p,void*out,size_t n){SIZE_T got=0;return p>=0x10000&&uint64_t(p)+n<0x80000000ull&&ReadProcessMemory(GetCurrentProcess(),(void*)(uintptr_t)p,out,n,&got)&&got==n;}
template<class R,class...Args> static R call(uint32_t address,uint32_t self,Args...args){if(!gameentry::active(gameentry::Client::Assist))return R();return ((R(__thiscall*)(void*,Args...))(uintptr_t)address)((void*)(uintptr_t)self,args...);}
template<class R,class...Args> static R callFast(uint32_t address,Args...args){
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
static bool sameid(const char*a,const char*b){for(int i=0;i<24;++i){if(a[i]!=b[i])return false;if(!a[i])return true;}return true;}
static bool own(uint32_t p,uint32_t house){return live(p)&&get<uint32_t>(p+0x21c)==house;}
static void indexWorld(int frame){Vec all=vec(0xA8EC78);if(indexedFrame==frame&&indexedCount==all.count&&indexedItems==all.items)return;
 worldObjects.clear();for(int i=0;i<all.count;++i){uint32_t p=item(all,i);if(p>=0x10000){uint32_t id=0;if(read(p+0x10,&id,sizeof id)&&id)worldObjects.add(p,id);}}indexedFrame=frame;indexedCount=all.count;indexedItems=all.items;
 static int prunedFrame=-100000;if(manualVersions.used>=ManualVersions::Size/2&&(frame<prunedFrame||frame-prunedFrame>=300)){
  static ManualVersions liveVersions;liveVersions.copyLive(manualVersions,[](uint32_t id){uint32_t p=worldObjects.byId(id);uint32_t found=0;return p&&read(p+0x10,&found,sizeof found)&&found==id;});manualVersions=liveVersions;prunedFrame=frame;
 }
}
static void reset(){resetLoadCalculation();resetRuntimePlanner();catalogueReady=false;catalogueHash=0;catalogueAt=0;catalogueDirty=true;resetFeatureTypes();resetPointerInput();if(sh&&sh->pointer.request!=sh->pointer.ack){sh->pointer.result=-7;sh->pointer.accepted=0;sh->pointer.rejected=sh->pointer.intent.count;MemoryBarrier();sh->pointer.ack=sh->pointer.request;}nativeMegaQueueCallDepth=nativeObjectQueueCallDepth=0;snap.lastSubmitError=0;missionObserver.clear();actorFrameArbiter.clear();actorLeaseArbiter.clear();snap.megaQueueStatus=0;snap.megaQueueAccepted=snap.megaQueueRejected=0;snap.objectQueueStatus=0;snap.objectQueueAccepted=snap.objectQueueRejected=0;
 snap.queueSent=snap.queueFull=snap.queueTimingFailure=snap.planDeferred=snap.queuedEvents=snap.observedEvents=snap.commandRejected=snap.receiptCancelled=snap.receiptExpired=snap.receiptDropped=0;snap.receiptWrite=snap.receiptCount=0;restoreClickHooks();resetCooperativeState();resetMissileControl();resetBurstControl();resetDodgeControl();if(sh)sh->clickHookStatus=0;clearOreOrders();restorePlacement();indexedFrame=indexedCount=-1;outgoingFrame=-1;clearNativeOrders();snap.labels=snap.selected=snap.transports=snap.used=snap.capacity=snap.swCount=0;snap.nativeHookStatus=snap.unloadIssued=0;snap.techOpen=0;restoreTech();restoreCSHook();clearFlashes();resetProduction();snap.inputPatchMask=0;repairCount=repairAt=repairDone=0;repairAckRequest=0;repairAckPending=false;repairCalculationPending=false;for(auto&v:lastIdle)v=0;productionBaseline=false;snap.typeCount=0;snap.threatCount=snap.crateCount=crateCount=0;cratesOn=false;autoWas=false;snap.autoRepair=0;snap.repairTotal=0;snap.techVisible=snap.techUnavailable=snap.techError=0;snap.crateHookStatus=snap.crateResultCount=0;snap.crateResultSerial=0;sceneResultSerial=0;snap.csHookStatus=snap.csConverted=0;loadLedger.clear();snap.loadUnits=snap.loadVehicles=snap.loadIssued=snap.loadWaiting=snap.loadUnassigned=0;memset(techAllowed,0,sizeof techAllowed);pendingRepairs.clear();}
static void mapName(wchar_t*out,DWORD p){const wchar_t*s=AssistMappingPrefix;int i=0;while(*s)out[i++]=*s++;wchar_t d[16];int n=0;do{d[n++]=L'0'+p%10;p/=10;}while(p);while(n)out[i++]=d[--n];out[i]=0;}
static HANDLE controllerProcess;static DWORD controllerPID,controllerCheckAt;
static void disconnect(){restorePlayerContext();InterlockedExchange(&hostWorkPending,0);if(controllerProcess)CloseHandle(controllerProcess);controllerProcess=nullptr;controllerPID=controllerCheckAt=0;restoreFrameDriver();InterlockedExchange(&messageGameThread,0);reset();sessionTracker.update(false,0,-1,-1);if(sh)UnmapViewOfFile(sh);sh=nullptr;if(mapping)CloseHandle(mapping);mapping=nullptr;lastHouse=pendingAck=0;pendingAckReady=false;}
static bool signatures(){for(auto&s:Signatures){unsigned char b[12];if(!read(s.address,b,s.length)||memcmp(b,s.bytes,s.length))return false;}return true;}
static void notifyHost(){
 if(!sh||!sh->hostWindow)return;
 if(!InterlockedCompareExchange(&sh->hostNotifyPending,1,0)&&!PostMessageW((HWND)(uintptr_t)sh->hostWindow,DataMessage,sh->pid,0))InterlockedExchange(&sh->hostNotifyPending,0);
}
static uint32_t micros(LARGE_INTEGER from,LARGE_INTEGER to,LARGE_INTEGER frequency){return frequency.QuadPart>0?uint32_t((to.QuadPart-from.QuadPart)*1000000/frequency.QuadPart):0;}
static void publishSnapshot(DWORD now){
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
 int frame=get<int>(0xA8ED84);if(frame!=outgoingFrame){outgoingFrame=frame;outgoingBudget=24;}
 const int mode=get<int>(0xA8B238);
 if(outgoingBudget<=0||!eventdelivery::mayQueue(mode))return 0;
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
static bool simulationReady(){
 if(!gameentry::active(gameentry::Client::Assist)||!sh||sh->stop||fault||
    snap.status!=1||snap.commandStatus!=1||!match||!lastHouse||
    DWORD(sampleClock(sh)-sh->heartbeat)>=3000||!get<bool>(0xA8E9A0))return false;
 const uint32_t house=get<uint32_t>(0xA83D4C);const int frame=get<int>(0xA8ED84),session=get<int>(0xA8B238);
 if(house!=lastHouse||house==get<uint32_t>(0xAC1198)||get<bool>(house+0x1F5)||
    get<bool>(house+0x1F6)||get<bool>(house+0x1F8)||frame<0||frame>=INT32_MAX||
    frame!=snap.frame||session!=snap.session||!eventdelivery::mayQueue(session))return false;
 return external::display::inspect([](uint32_t at,void*out,size_t size){
  memcpy(out,(const void*)(uintptr_t)at,size);return true;
 }).visible();
}
static bool enqueue(Event&e){
 if(!simulationReady()||get<bool>(0xA8BC60+25)){++snap.commandRejected;return false;}
 if(!eventdelivery::mayQueue(get<int>(0xA8B238))){++snap.commandRejected;return false;}
 if(!outgoingRoom()){snap.lastSubmitError=6;++snap.queueFull;return false;}
 // Fail closed if the game callback cannot be resolved; never fabricate its
 // output using a Windows wall-clock timer.
 uint32_t timing=0;
 if(!gameSendTiming(timing)){snap.lastSubmitError=9;++snap.queueTimingFailure;return false;}
 // The native timer is an external call boundary. Recheck identity/frame before
 // copying the packet; never route a constructed event into a changed session.
 if(!simulationReady()||get<bool>(0xA8BC60+25)){++snap.commandRejected;return false;}
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
static bool submitConstructed(const intent::EventOrder& order,OrderAuthority authority=OrderAuthority::Local){
 if(!authorizePlayerOrder(order,authority)){++snap.commandRejected;return false;}
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
 Event packet{};
 const auto state=constructors::fill(packet,order,
  [](uint32_t address,uint32_t self,auto... args){call<void>(address,self,args...);});
 if(state!=constructors::Verdict::Ready){snap.lastSubmitError=8;++snap.commandRejected;return false;}
 return enqueue(packet); // SP DoList / MP OutList, timing and receipt centrally.
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
 if(selected==megaqueue::Backend::ConstructAndEnqueue||
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
 ++nativeMegaQueueCallDepth;
 bool success=((Native)(uintptr_t)0x646E90)(mission,&targetArg,id,0x34,&destinationArg);
 --nativeMegaQueueCallDepth;
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
// Constructors/ABI are verified against the supported game build at startup.
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
                          int targetTag=-1,OrderAuthority authority=OrderAuthority::Local,bool appendWaypoint=false){
 const bool foreign=authority==OrderAuthority::PlayerTest&&sh&&sh->controlAll&&playableHouse(house);
 if(!id||!house||(house!=lastHouse&&!foreign)||!eventdelivery::mayQueue(get<int>(0xA8B238))||!loadInterfaces())return false;
 const uint8_t resolvedTargetTag=targetTag>=0?uint8_t(targetTag):(target?0x34:0);
 const uint32_t submittedAt=sampleClock(sh);
 const gateway::Context ctx{match,submittedAt,sh&&!sh->stop&&!fault&&snap.status==1,
                            get<bool>(0xA8BC60+25)};
 const gateway::Intent intent{match,submittedAt,id,target,manualVersions.get(id)};
 if(gateway::validate(ctx,intent,3500)!=gateway::Failure::None){++snap.commandRejected;return false;}
 // The command gateway, not just the planner, owns the final game-thread
 // actor/ownership preflight. Never serialize a vanished/foreign actor ID.
 uint32_t object=worldByID(id);
 uint32_t targetObject=(target&&resolvedTargetTag==0x34)?worldByID(target):0;
 const bool targetCellValid=(resolvedTargetTag==11)&&vanilla::validTargetRef(int(target),vanilla::TargetTag::Cell);
 uint32_t destObject=(destTag==0x34&&dest)?worldByID(dest):0;
 auto reject=orderpolicy::missionWithTargetTag(object!=0,object&&own(object,house),object&&live(object),
  mission,resolvedTargetTag,target,targetObject!=0,targetCellValid,destTag==0x34,destObject!=0);
 if(reject!=orderpolicy::Reject::None){++snap.commandRejected;return false;}
 const int frame=get<int>(0xA8ED84);
 actorFrameArbiter.begin(match,frame);
 if(!actorFrameArbiter.canIssue(id,manualVersions.get(id))||
    !actorLeaseArbiter.canIssue(match,frame,id,object,manualVersions.get(id),owner)){
  ++snap.commandRejected;return false;
 }
 gateway::MissionState before{};
 before.valid=true;before.mission=get<int>(object+0xAC);
 before.queued=get<int>(object+0xB4);before.carrier=get<uint32_t>(object+0x11C);
 if(foreign||appendWaypoint){auto order=intent::mega(get<int>(house+0x30),id,mission,target,resolvedTargetTag,dest,uint8_t(destTag),nearest,uint8_t(nearestTag));order.appendWaypoint=appendWaypoint;if(!submitConstructed(order,authority))return false;}
 else if(!submitMegaMission(house,id,mission,target,resolvedTargetTag,
                       dest,uint8_t(destTag),nearest,uint8_t(nearestTag)))return false;
 showOrderLines(object);
 actorFrameArbiter.record(id,manualVersions.get(id),owner);
 // A queued EventClass is not an executed mission. This read-only tracker
 // records the later state transition; unsupported events stay unconfirmed.
 uint32_t ticket=missionObserver.record(match,id,destTag==0x34?dest:target,mission,
                             frame,submittedAt,before,owner);
 taskarbiter::Lease superseded{};
 actorLeaseArbiter.record(match,frame,id,object,manualVersions.get(id),owner,ticket,&superseded);
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
static bool nativeObjectEvent(int houseIndex,int eventType,uint32_t objectId,OrderAuthority authority=OrderAuthority::Local,gateway::Receipt* receipt=nullptr){
 if(houseIndex<0||houseIndex>=32||!objectId||!lastHouse){++snap.commandRejected;return false;}
 uint32_t object=worldByID(objectId);
 uint32_t owner=object?get<uint32_t>(object+0x21C):0;
 uint32_t localIndex=get<uint32_t>(lastHouse+0x30);
 uint32_t ownerIndex=owner>=0x10000?get<uint32_t>(owner+0x30):0xFFFFFFFFu;
 const int mode=get<int>(0xA8B238);
 if(!eventdelivery::mayQueue(mode)){++snap.commandRejected;return false;}
 const bool mp=eventdelivery::isMultiplayer(mode);
 auto reject=orderpolicy::objectEvent(eventType,object!=0,object&&live(object),
  uint32_t(houseIndex)==localIndex||(authority==OrderAuthority::SellPlayerTest&&sh&&(sh->flags&SellEnemy)&&eventType==0x16&&object&&kind(object)==3&&playableHouse(owner)),uint32_t(houseIndex)==ownerIndex,mp);
 if(reject!=orderpolicy::Reject::None||(eventType==0x07&&kind(object)!=1&&kind(object)!=2)){++snap.commandRejected;return false;}
 const uint32_t submittedAt=sampleClock(sh);
 const gateway::Context ctx{match,submittedAt,sh&&!sh->stop&&!fault&&snap.status==1,
                            get<bool>(0xA8BC60+25)};
 const uint32_t rev=manualVersions.get(objectId);
 const gateway::Intent objIntent{match,submittedAt,objectId,0,rev};
 if(gateway::validate(ctx,objIntent,3500)!=gateway::Failure::None){++snap.commandRejected;return false;}
 const int frame=get<int>(0xA8ED84);
 const auto objOwner=eventType==0x07?taskarbiter::Owner::Dodge:eventType==0x16?taskarbiter::Owner::Sell:taskarbiter::Owner::Repair;
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
 if(authority!=OrderAuthority::Local){if(!submitConstructed(intent::object(houseIndex,eventType,objectId),authority))return false;return acceptedObject();}
 if(eventType==0x07||!sh||sh->objectQueueMode!=1||eventdelivery::isSingleplayer(mode)){
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
 ++nativeObjectQueueCallDepth;
 const bool accepted=((NativeClicked)(uintptr_t)method)((void*)(uintptr_t)object,uint32_t(eventType));
 --nativeObjectQueueCallDepth;
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
 const gateway::Context ctx{match,submittedAt,sh&&!sh->stop&&!fault&&snap.status==1,get<bool>(0xA8BC60+25)};
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
static bool selection(uint32_t p){return call<bool>(0x6FBFA0,p);}
static void unselect(){if(gameentry::active(gameentry::Client::Assist))((void(__cdecl*)())0x48DC90)();}
static int selectFilter(uint32_t house,int action){
 static uint32_t chosen[4096];int n=0;Vec v=vec(action==EmptyTransports?0xA8EC78:0xA8ECB8);
 for(int i=0;i<v.count&&n<4096;++i){uint32_t p=item(v,i);if(!own(p,house))continue;int k=kind(p);if(k<0||k==3)continue;uint32_t t=typeOf(p,k);if(!t)continue;
 bool harv=k==2&&(get<bool>(t+0xE0E)||get<bool>(t+0xE0F)),eng=k==1&&get<bool>(t+0xEC3);
 bool ok=action==CombatSelection?(!harv&&!eng&&get<bool>(t+0xDBC)):action==EmptyTransports?(get<int>(t+0x5E0)>0&&get<int>(p+0x114)==0):get<int>(p+0x6c)<get<int>(t+0xA0);
 if(ok)chosen[n++]=p;
 }if(!n)return 0;unselect();for(int i=0;i<n;++i)selection(chosen[i]);return n;
}
static int findIdle(uint32_t house,int mode){Vec v=vec(0xA8EC78);uint32_t next=0,first=0,nextid=0xffffffff,firstid=0xffffffff;int slot=mode-IdleHarvester;
 for(int i=0;i<v.count;++i){uint32_t p=item(v,i);if(!own(p,house))continue;int k=kind(p);if(k<0||k==3)continue;uint32_t t=typeOf(p,k);if(!t)continue;
 bool category=slot==0?(k==2&&(get<bool>(t+0xE0E)||get<bool>(t+0xE0F))):slot==1?(k==1&&get<bool>(t+0xEC3)):(get<int>(t+0x5E0)>0&&get<int>(p+0x114)==0);
 if(!category||!idleMission(get<int>(p+0xAC),get<int>(p+0xB4),get<uint32_t>(p+0x2B4)!=0,get<bool>(p+0x1C8)))continue;
 uint32_t id=get<uint32_t>(p+0x10);if(id<firstid){firstid=id;first=p;}if(id>lastIdle[slot]&&id<nextid){nextid=id;next=p;}
 }if(!next){next=first;nextid=firstid;}if(!next)return 0;lastIdle[slot]=nextid;unselect();selection(next);
 uint32_t tactical=get<uint32_t>(0x887324);if(tactical)call<void>(0x6D2420,tactical,(void*)(next+0x9C),0);return 1;
}
static void scanTypes(){catalogueDirty=true;snap.typeCount=0;const uint32_t arrays[]={0xA8B218,0xA8E348,0xA83CE0,0xA83C68};const int kinds[]={3,16,40,7};
 for(int k=0;k<4;++k){Vec v=vec(arrays[k],4096);for(int i=0;i<v.count&&snap.typeCount<MaxTypes;++i){uint32_t t=item(v,i);if(!t||!validType(get<int>(t+0x634),get<bool>(t+0x232),get<bool>(t+0xD54),get<uint32_t>(t+0x6CC)))continue;
 typePointers[snap.typeCount]=t;auto&d=snap.types[snap.typeCount++];memset(&d,0,sizeof d);memcpy(d.id,(void*)(t+0x24),23);name(t,d.name,40);d.kind=kinds[k];d.index=i;d.naval=get<bool>(t+0xCCE);d.cost=get<int>(t+0x610);
 }}
}
#include "sidebar.h"
#include "type_select_bridge.h"
static uint32_t rawObjectID(uint32_t pointer);
#include "raw_combat.h"
static bool rawArmed(uint32_t p){int k=kind(p);uint32_t type=typeOf(p,k);if(!type)return false;int slots[2],count=rawWeaponSlots(p,type,slots);return count&&rawWeapon(type,get<float>(p+0x150),slots[0])>=0x10000;}
#include "native_orders_bridge.h"
#include "auto_cs_bridge.h"
static bool nativeMegaOriginalOrOwned(){static const uint8_t original[]={0x83,0xEC,0x70,0x32,0xC0};return memcmp((const void*)0x646E90,original,sizeof original)==0;}
#include "native_input_hooks.h"
static bool nativeObjectUnhooked(){return nativeObjectSignature();}
#include "anti_theft_bridge.h"
#include "external_orders_bridge.h"
#include "player_test_bridge.h"
#include "placement_bridge.h"
#include "small_map_camera_bridge.h"
#include "scroll_refresh_bridge.h"
#include "cooperative_bridge.h"
#include "pointer_bridge.h"
// Reserve a share for other active task families, then let a lone family use
// the remaining capacity. All sends still obey the shared 24-event frame cap.
static int recurringBudget(EventLane lane){
 bool external=sh->dodgeEnabled||dodgeCount||sh->burstEnabled||sh->burstHoldEnabled||burstCount||sh->missileEnabled||nativeCount||runtimeCoopPending||sh->plannerTracking||sh->runtimeTracking||sh->planRequest!=sh->planAck||sh->cooperativePlan!=sh->cooperativeAck;
 bool repairs=repairAt<repairCount,loading=loadLedger.count!=0,unloading=unloadCount!=0;
 if(lane==EventLane::Planning)return repairs||(sh->flags&AutoRepair)||loading||unloading?12:24;
 if(external)return 4;
 return 24-4*((lane!=EventLane::Repair&&repairs)+(lane!=EventLane::Boarding&&loading)+(lane!=EventLane::Unload&&unloading));
}
#include "runtime_planner_bridge.h"
#include "planning_budget.h"
static planningbudget::Budget sharedPlanningBudget;
static int planningRoom(int channel){
 sharedPlanningBudget.begin(match,get<int>(0xA8ED84),recurringBudget(EventLane::Planning));
 uint32_t active=0;
 if(sh->planRequest!=sh->planAck||sh->runtimeTracking||nativeCount||(sh->flags&AntiTheft))active|=1;
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
#include "../../external_reader/source/idle_aircraft.h"
static int selectIdleAircraft(uint32_t house,uint32_t id){
 uint32_t p=worldByID(id);auto reader=[](uint32_t at,void*out,size_t n){return read(at,out,n);};external::aircraft::Inspect<decltype(reader)> planes{reader};
 if(!p||!planes.idle(p,house)||!call<bool>(get<uint32_t>(get<uint32_t>(p)+0x13C),p))return 0;unselect();if(!selection(p))return 0;
 uint32_t tactical=get<uint32_t>(0x887324);if(tactical)call<void>(0x6D2420,tactical,(void*)(p+0x9C),0);return 1;
}
static void selectionCommand(uint32_t house,DWORD now){
 uint32_t request=sh->selectRequest;if(request==sh->selectAck)return;MemoryBarrier();
 int result=0,count=0;
 if(sh->selectMatch!=match||DWORD(now-sh->selectTick)>1500)result=-7;
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
static int collectRepairs(uint32_t house,DWORD now){
 if(repairCalculationPending)return -6;
 bool internal=compute::inside(sh->computeApplied,compute::Repair);
 if(!internal&&sh->repairTask!=sh->repairComputed)return -6;
 Vec v=vec(0xA8EB40);repairCount=repairAt=repairDone=0;auto&in=sh->repairInput;
 if(!internal){in.match=match;in.tick=now;in.frame=get<int>(0xA8ED84);in.count=0;}
 for(int i=0;i<v.count&&repairCount<2048;++i){uint32_t p=item(v,i);if(!p||!own(p,house))continue;uint32_t t=typeOf(p,3),id=get<uint32_t>(p+0x10);
  if(!t||!featureTypeAllowed(featuretypes::RepairBuilding,t)||(!(sh->flags&RepairUnrepairable)&&!get<uint8_t>(t+0x157A)))continue;
  decisions::RepairView view{get<int>(p+0x6C),get<int>(t+0xA0),true,get<bool>(p+0x6E8),pendingRepairs.contains(id,p)};
  if(internal){if(decisions::shouldRepair(view))repairIDs[repairCount++]=id;}
  else if(in.count<2048)in.items[in.count++]={id,view};
 }
 if(internal)return repairCount;
 repairCalculationPending=true;repairCalculationStarted=now;MemoryBarrier();InterlockedIncrement((volatile LONG*)&sh->repairTask);notifyHost();return -30;
}
static void repairCalculationTick(DWORD now){
 if(!repairCalculationPending)return;
 bool timeout=DWORD(now-repairCalculationStarted)>1000;
 if(!timeout&&(sh->repairComputed!=sh->repairTask||sh->repairTask==repairTaskSeen))return;
 MemoryBarrier();repairTaskSeen=sh->repairTask;repairCalculationPending=false;
 const auto&in=sh->repairInput;const auto&r=sh->repairResult;
 bool valid=!timeout&&r.count>=0&&r.count<=2048&&r.match==in.match&&r.tick==in.tick&&r.frame==in.frame&&
  freshness::valid(in.match,match,in.tick,in.frame,now,get<int>(0xA8ED84),freshness::Cooperative)&&!get<bool>(0xA8BC60+25);
 repairCount=repairAt=repairDone=0;
 if(valid){for(int i=0;i<r.count;++i){bool known=false;for(int j=0;j<in.count;++j)if(in.items[j].id==r.ids[i]){known=true;break;}if(known)repairIDs[repairCount++]=r.ids[i];}}
 else if(repairAckPending){sh->result=-7;sh->count=0;pendingAck=repairAckRequest;pendingAckReady=true;repairAckPending=false;}
}

static int superSelect(uint32_t house,int index){Vec v=vec(house+0x254,256);uint32_t p=item(v,index);if(!p||get<uint32_t>(p+0x2C)!=house||!get<bool>(p+0x6D))return -4;
 if(!call<char>(0x6CC360,p))return -5;uint32_t t=get<uint32_t>(p+0x28);if(!t)return -4;
 if(get<int>(t+0xBC)==0){
  if(orderpolicy::superWeaponEvent(true,true,true,0)!=orderpolicy::Reject::None)return -4;
  const uint32_t submittedAt=sampleClock(sh);
  const uint32_t synthActor=taskarbiter::syntheticSuperWeaponActor(index);
  const gateway::Context ctx{match,submittedAt,sh&&!sh->stop&&!fault&&snap.status==1,get<bool>(0xA8BC60+25)};
  const gateway::Intent swIntent{match,submittedAt,synthActor,uint32_t(get<int>(t+0x98)),0};
  if(gateway::validate(ctx,swIntent,3500)!=gateway::Failure::None){++snap.commandRejected;return -6;}
  const int frame=get<int>(0xA8ED84);
  actorFrameArbiter.begin(match,frame);
  if(!actorFrameArbiter.canIssue(synthActor,0)){++snap.commandRejected;return -6;}
  if(!submitConstructed(intent::superWeapon(
   get<int>(house+0x30),0x12,get<int>(t+0x98),vanilla::Cell{0,0})))return -6;
  actorFrameArbiter.record(synthActor,0,taskarbiter::Owner::SuperWeapon);
  recordReceipt(gateway::makeQueuedReceipt(missionObserver.nextSerial,swIntent,submittedAt,taskarbiter::Owner::SuperWeapon));
  return 1;
 }
 put<uint32_t>(0x88098C,0);put<uint32_t>(0x880990,0);put<int>(0x880994,-1);call<void>(0x4A8BF0,0x87F7E8,(void*)nullptr);call<void>(0x4AC8C0,0x87F7E8,0);call<void>(0x4AC660,0x87F7E8,0);
 put<unsigned char>(0x88099A,0);put<unsigned char>(0x88099B,0);put<unsigned char>(0x88099C,0);put<int>(0x8809A0,index);unselect();return 1;
}
static int namedSuper(uint32_t house,int command){const char* id=command==SelectIron?"IronCurtainSpecial":"ChronoSphereSpecial";Vec v=vec(house+0x254,256);
 for(int i=0;i<v.count;++i){uint32_t p=item(v,i);if(!p)continue;uint32_t t=get<uint32_t>(p+0x28);if(t&&sameid((const char*)(t+0x24),id))return superSelect(house,i);}return -4;
}
#include "social_bridge.h"
static void command(uint32_t house,DWORD now){uint32_t req=sh->request;if(req==sh->ack||(repairAckPending&&req==repairAckRequest)||(loadAckPending&&req==loadAckRequest))return;MemoryBarrier();int c=sh->command,arg=sh->arg;int result=1,n=0;
 if(sh->requestMatch!=match||DWORD(now-sh->requestTick)>3500){result=-7;}
 else if(get<bool>(0xA8BC60+25)){result=-8;}
 else if(c==Repair){
  n=collectRepairs(house,now);if(n<0&&n!=-30){result=n;n=0;}
  if(n>0||n==-30){repairAckRequest=req;repairAckPending=true;repairAckStarted=now;sh->result=1;sh->count=0;return;}
 }
 else if(c==Cheer||c==Taunt){result=socialCommand(house,c,arg,now);n=result>0;}
 else if(c==SwitchPlayer||c==RestorePlayer){result=-4;n=0;} // unavailable in this release, including stale commands
 else if(c==SelectType){n=selectType(house,arg,sh->arg2!=0);if(n<0){result=n;n=0;}}
 else if(c==SelectVariant){n=selectVariant(house,sh->arg2!=0);if(n<0){result=n;n=0;}}
 else if(c==SmartUnload){int sent=smartUnload(house);if(sent<0)result=sent;else n=sent;}
 else if(c==CancelAdvanced){Vec v=vec(0xA8ECB8);for(int i=0;i<v.count;++i){uint32_t p=item(v,i);if(p&&own(p,house)){cancelAutomatic(p);++n;}}}
 else if(c==AutoBoard){result=autoLoad(house);n=snap.loadIssued;if(result==-30){loadAckRequest=req;loadAckPending=true;return;}}
 else if(c>=CombatSelection&&c<=DamagedSelection)n=selectFilter(house,c);
 else if(c>=IdleHarvester&&c<=IdleTransport)n=findIdle(house,c);
 else if(c==ClearBeacons){call<void>(0x431410,0x89C3B0,get<int>(house+0x30));n=1;}
 else if(c==RevealMap&&(!(sh->flags&ExperimentalReveal)||sh->renderReveal)){result=-21;}
 else if(c==RevealMap){
  // Early in a restarted match, House can be valid before its map-cell array.
  // A void reveal call against that transitional state silently does nothing.
  uint32_t cells=get<uint32_t>(0x87F7E8+316);
  int capacity=get<int>(0x87F7E8+320);
  if(cells<0x10000||capacity<=0||capacity>512*512)result=-14;
  else {call<void>(0x577D90,0x87F7E8,(void*)house);n=1;}
 }
 else if(c==OpenTech){
  // Enable paired sidebar eligibility gates before adding native cameos.
  {snap.techOpen=arg!=0;memcpy(techAllowed,sh->techAllowed,sizeof techAllowed);snap.techError=0;if(snap.techOpen){requestTechRefresh();refreshTech(house,now);}else restoreTech();n=snap.techVisible;if(snap.techError<0)result=snap.techError;}}
 else if(c==SelectSuper){result=superSelect(house,arg);n=result>0;}
 else if(c==SelectIron||c==SelectChrono){result=namedSuper(house,c);n=result>0;}
 else if(c==AirPathMove){uint32_t id=uint32_t(arg),p=worldByID(id),cell=uint32_t(sh->arg2);
  if(!airpathpolicy::events(get<int>(0xA8B238),(sh->flags&AirWaypoints)!=0,(sh->flags&NativeAirWaypoints)!=0)||!p||!own(p,house)||kind(p)!=0||manualVersions.get(id)!=sh->requestToken||get<bool>(p+0x81)||get<uint32_t>(p+0x11C)||!readMapCell(int(cell%1000),int(cell/1000)))result=-4;
  else if(nativeMission(house,id,2,0,cell,11,0,0,taskarbiter::Owner::Generic))n=1;else result=-6;
 }
 else if(c==ProduceReserved){
  uint32_t origin=worldByID(uint32_t(sh->arg2));
  if(!(sh->flags&ReserveConstruction)||arg<0||arg>=snap.typeCount||snap.types[arg].kind!=7||!origin||!own(origin,house)||kind(origin)!=3||get<bool>(origin+0x81)||!get<bool>(origin+0x90))result=-4;
  else {const auto&t=snap.types[arg];int tab=typeTab(arg);uint32_t f=tab==0||tab==1?get<uint32_t>(house+(tab?0x53CC:0x53BC)):0;
   if(tab<0||tab>1||!hasCameo(7,t.index)||!typePointers[arg]||call<int>(0x4F7870,house,(void*)(uintptr_t)typePointers[arg],false,true)<=0)result=-9;
   else if(f>=0x10000&&(get<uint32_t>(f+0x58)||get<int>(f+0x50)>0))result=-9;
   else if(nativeProduceEvent(house,t,true)){n=1;watchReservedSidebar(house,t.index,tab,uint32_t(sh->arg2),now);}else result=-6;
  }
 }
 else if(c==ProduceDeferred){
  if(!(sh->flags&ReserveConstruction)||arg<0||arg>=snap.typeCount||!typePointers[arg])result=-4;
  else {const auto&t=snap.types[arg];int tab=typeTab(arg);uint32_t offset=t.kind==3?0x53AC:t.kind==16?0x53B0:t.kind==40?(t.naval?0x53B8:0x53B4):t.kind==7?(tab==1?0x53CC:0x53BC):0;
   uint32_t factory=offset?get<uint32_t>(house+offset):0;
   if(!offset||tab<0||tab>3||!hasCameo(t.kind,t.index)||call<int>(0x4F7870,house,(void*)(uintptr_t)typePointers[arg],false,true)<=0)result=-9;
   else if(factory>=0x10000&&(get<uint32_t>(factory+0x6C)!=house||get<uint32_t>(factory+0x58)||get<int>(factory+0x50)>0))result=-9;
   else if(!techFactory(typePointers[arg],house))result=-9;
   else if(nativeProduceEvent(house,t,true)){n=1;if(t.kind==7)watchReservedSidebar(house,t.index,tab,0,now);}else result=-6;
  }
 }
 else if(c==Produce||c==CancelTechProduction){
  if(!(sh->flags&UnsafeTech)||!snap.techOpen||arg<0||arg>=snap.typeCount||!techAllowed[arg]||!typePointers[arg] )result=-4;
  else if(c==CancelTechProduction){auto&t=snap.types[arg];if(nativeProduceEvent(house,t,false,0x10))n=1;else result=-6;}
  else {const uint32_t type=typePointers[arg];uint32_t table=get<uint32_t>(type),method=table?get<uint32_t>(table+0x94):0;
   if(method<0x10000||call<int>(0x4F7870,house,(void*)(uintptr_t)type,true,true)<=0||!call<uint32_t>(method,type,true,true,true,(void*)(uintptr_t)house))result=-9;
   else{auto&t=snap.types[arg];if(!ensureTechCameo(arg,house))result=-9;else{result=nativeProduceEvent(house,t)?1:-6;n=result>0;if(n&&t.kind==7)watchReservedSidebar(house,t.index,typeTab(arg),0,now);}}
  }
 }
 else result=-4;
 sh->result=result;sh->count=n;pendingAck=req;pendingAckReady=true;
}
static void repairTick(uint32_t house,DWORD now){
 repairCalculationTick(now);
 pendingRepairs.observe([&](uint32_t id,uint32_t pointer){
  uint32_t p=worldByID(id);bool valid=p==pointer&&p&&kind(p)==3&&own(p,house);
  if(!valid)return repairdelivery::Observation{};
  uint32_t t=typeOf(p,3);int maximum=t?get<int>(t+0xA0):0;
  return repairdelivery::Observation{true,get<bool>(p+0x6E8),maximum>0&&get<int>(p+0x6C)>=maximum};
 });
 bool automatic=(sh->flags&AutoRepair)!=0;DWORD interval=clampedRepairInterval(sh->repairMs);
 if(!repairCalculationPending&&!repairAckPending&&automatic&&(!autoWas||(DWORD(now-lastAuto)>=interval&&repairAt>=repairCount))){collectRepairs(house,now);lastAuto=now;}
 if(!repairAckPending&&!automatic&&autoWas)repairCount=repairAt=0;autoWas=automatic;snap.autoRepair=automatic;
 static int budgetFrame=-1,budget;static uint32_t budgetMatch;
 int frame=get<int>(0xA8ED84);if(budgetFrame!=frame||budgetMatch!=match){budgetFrame=frame;budgetMatch=match;budget=recurringBudget(EventLane::Repair);}
 if(repairAt<repairCount&&budget>0){
  indexWorld(frame);
  while(repairAt<repairCount&&budget>0){uint32_t id=repairIDs[repairAt],p=worldByID(id);
  const bool recent=pendingRepairs.contains(id,p);
  if(p&&kind(p)==3&&own(p,house)){uint32_t t=typeOf(p,3);if(t&&featureTypeAllowed(featuretypes::RepairBuilding,t)&&((sh->flags&RepairUnrepairable)||get<uint8_t>(t+0x157A))&&decisions::shouldRepair({get<int>(p+0x6c),get<int>(t+0xA0),true,
     get<bool>(p+0x6E8),recent})){if(!pendingRepairs.room(id)||!repairEvent(house,id))break;
     pendingRepairs.queued(id,p);--budget;++repairDone;++snap.repairTotal;}}
  ++repairAt;
  }
 }
 // Completion reports events actually queued, not the number of candidates.
 // A congested native queue must also produce a terminal ACK, not wait forever.
 if(repairAckPending&&!repairCalculationPending&&(repairAt>=repairCount||DWORD(now-repairAckStarted)>3000)){
  sh->result=repairDone?1:-6;sh->count=repairDone;
  pendingAck=repairAckRequest;pendingAckReady=true;repairAckPending=false;repairAckRequest=0;
  repairAt=repairCount; // Drop unsent candidates on timeout; no surprise later repair.
 }
}
static void scanCrates(uint32_t tactical,int frame,DWORD now,uint32_t flags){
 snap.crateCount=0;if(!(flags&Crates)){cratesOn=false;crateCount=0;return;}
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
 snap.labels=0;snap.selected=0;snap.transports=snap.used=snap.capacity=0;
 uint32_t tactical=get<uint32_t>(0x887324),flags=sh->flags;
 bool global=(flags&(Iron|Psychic))||((flags&Capacity)&&(flags&AllTransports))||nativeCount;
 Vec v=vec(global?0xA8EC78:0xA8ECB8);
 snap.threatCount=0;uint32_t allies=get<uint32_t>(house+0x5788);
 for(int i=0;i<v.count;++i){uint32_t p=item(v,i);if(!p||!live(p))continue;int k=kind(p);if(k<0)continue;
 bool local=get<uint32_t>(p+0x21C)==house;
 if(!local){
  if((flags&Psychic)&&tactical&&snap.threatCount<MaxThreats){uint32_t owner=get<uint32_t>(p+0x21C),target=get<uint32_t>(p+0x2B4);
   int ownerIndex=owner>=0x10000?get<int>(owner+0x30):-1;
   if(ownerIndex>=0&&ownerIndex<32&&!(allies&(1u<<ownerIndex))&&target>=0x10000&&kind(target)>=0&&own(target,house)){
    POINT a{},b{};if(call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&a)&&call<bool>(0x6D2140,tactical,(const void*)(target+0x9C),&b))snap.threats[snap.threatCount++]={get<uint32_t>(p+0x10),get<uint32_t>(target+0x10),a.x,a.y,b.x,b.y};
   }
  }
 }
 bool selected=local&&get<bool>(p+0x83);if(selected)++snap.selected;
 int iron=(flags&Iron)?remaining(get<int>(p+0x18C),get<int>(p+0x194),frame):0;uint32_t t=typeOf(p,k);int capacity=local&&t&&k!=3&&(flags&Capacity)?get<int>(t+0x5E0):0;bool capacityShow=capacity>0&&(selected||(flags&AllTransports));
 int used=0;if(capacityShow){used=call<int>(0x473460,p+0x114);if(selected){++snap.transports;snap.used+=used;snap.capacity+=capacity;}}
 int advancedIndex=local&&nativeCount?orderIndex(p):-1;bool advanced=advancedIndex>=0&&nativeOrders[advancedIndex].stopped;
 if((!capacityShow&&!iron&&!advanced)||!tactical||snap.labels>=MaxLabels)continue;
 POINT pt{};if(!call<bool>(0x6D2140,tactical,(const void*)(p+0x9C),&pt))continue;
 auto&l=snap.label[snap.labels++];l={get<uint32_t>(p+0x10),pt.x,pt.y,used,capacityShow?capacity:0,iron,get<int>(p+0x1C4)!=0,advanced};
 }
 snap.frame=frame;snap.width=get<int>(0xA8EB84);snap.height=get<int>(0xA8EB88);snap.credits=get<int>(house+0x30C);
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
 snap.swCount=0;Vec sw=vec(house+0x254,256);for(int i=0;i<sw.count&&snap.swCount<MaxSW;++i){uint32_t p=item(sw,i);if(!p||get<uint32_t>(p+0x2C)!=house||!get<bool>(p+0x6D))continue;uint32_t t=get<uint32_t>(p+0x28);if(!t)continue;auto&d=snap.supers[snap.swCount++];memset(&d,0,sizeof d);memcpy(d.id,(void*)(t+0x24),23);name(t,d.name,40);d.index=i;d.ready=call<char>(0x6CC360,p);d.frames=remaining(get<int>(p+0x30),get<int>(p+0x38),frame);}
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
 selectionCommand(house,now);uiSoundCommand(now);
 // Preserve the ordering of the known-good path: input observers first,
 // then command / planner / cooperative requests.
 uint32_t inputFlags=sh->flags|(unloadCount?NativeOrders:0);
 csHooksTick(inputFlags);nativeHooksTick(inputFlags);clickHooksTick(inputFlags);
 snap.inputPatchMask=0;pointerTick();
 command(house,now);if(playerContextChanged)return;reservedSidebarTick(house,now);externalOrdersTick(house,frame,now);cooperativeTick(house,frame,now);placementTick();
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
static void tick(){
 if(!sh){wchar_t n[80];mapName(n,GetCurrentProcessId());mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,n);if(!mapping)return;sh=(Shared*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Shared));if(!sh||sh->magic!=Magic||sh->version!=Version||sh->pid!=GetCurrentProcessId()){disconnect();return;}}
 playerMaintenance();DWORD now=sampleClock(sh);if(sh->stop||(DWORD(now-sh->heartbeat)>3000&&controllerExited(now))){restoreFrameDriver();Shared* old=sh;reset();if(old->stop)old->stop=2;disconnect();return;}if(!checked){good=signatures();checked=true;}if(!good){if(sh->data.status!=-1){InterlockedIncrement(&sh->seq);sh->data.status=-1;sh->data.labels=0;MemoryBarrier();InterlockedIncrement(&sh->seq);}return;}
 DWORD period=sh->foreground?sh->scanMs:((sh->flags&(AdvancedInput|MagnetronInput|AutoOreReturn|AirWaypoints|AntiTheft))?64:500);if(period<2)period=2;if(period>1000)period=1000;
 // An active refresh adapter must also wake promptly to restore after its
 // setting is disabled or focus is lost, before returning to idle cadence.
 if(scrollRefreshActive||(sh->foreground&&(sh->burstEnabled||sh->burstHeld||burstCount||sh->scrollRefreshHz)))period=4;
 bool selectionPending=sh->selectRequest!=sh->selectAck;bool planPending=sh->planRequest!=sh->planAck;bool cooperativePending=sh->cooperativePlan!=sh->cooperativeAck;bool cmd=sh->request!=sh->ack||selectionPending||planPending||cooperativePending||sh->pointer.request!=sh->pointer.ack;// A message wake observes the current simulation frame; pending work bypasses
 // display cadence. This is not a frame hook and does not advance the game.
 bool frameDue=get<int>(0xA8ED84)!=lastFrame&&
   (sh->dodgeEnabled||dodgeCount||sh->burstEnabled||sh->burstHoldEnabled||burstCount||sh->missileEnabled||nativeCount||unloadCount||sh->runtimeTracking||runtimeCoopPending||loadCalculationPending||repairCalculationPending||(sh->flags&(EjectAssist|AntiTheft))||
    (sh->plannerTracking&&(sh->flags&(AdvancedInput|MagnetronInput|AutoOreReturn)))||
    sh->planRequest!=sh->planAck||sh->cooperativePlan!=sh->cooperativeAck);
 bool soundPending=sh->uiSoundRequest!=sh->uiSoundAck;
 const uint64_t preciseNow=preciseclock::micros();static uint64_t fastSampleAt=0;
 bool refreshDue=(sh->foreground&&sh->scrollRefreshMode&&sh->scrollRefreshHz&&scrollCadence.ready(preciseNow,sh->scrollRefreshHz))||
   (scrollRefreshActive&&(!sh->foreground||!sh->scrollRefreshMode||!sh->scrollRefreshHz));
 bool fastDue=sh->foreground&&(sh->dodgeEnabled||dodgeCount||sh->burstEnabled||sh->burstHeld||burstCount)&&preciseNow-fastSampleAt>=2000;
 if(!cmd&&!soundPending&&!frameDue&&!refreshDue&&!fastDue&&DWORD(now-last)<period)return;last=now;fastSampleAt=preciseNow;
 LARGE_INTEGER start,end,controlEnd,worldEnd;static LARGE_INTEGER frequency{};if(!frequency.QuadPart)QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&start);
 uint32_t house=get<uint32_t>(0xA83D4C);int frame=get<int>(0xA8ED84);bool running=get<bool>(0xA8E9A0);bool active=house>=0x10000&&house!=get<uint32_t>(0xAC1198)&&!get<bool>(house+0x1F5)&&!get<bool>(house+0x1F6)&&!get<bool>(house+0x1F8);
 if(active){Vec hv=vec(0xA80228,32);bool found=false;for(int i=0;i<hv.count;++i)if(item(hv,i)==house)found=true;active=found;}
 // A false engine run flag with the SAME valid local House is a suspension
 // (e.g. save/load/modal processing), not proof that the round ended.
 if(controllink::transientPause(running,house==lastHouse,active,lastHouse!=0)){
  if(riskRenderActive)riskRenderRedrawPending=true;
  restoreHighScroll();
  snap.status=1;snap.bridgeStatus=1;snap.commandStatus=0;snap.match=match;
  snap.frame=frame;pointerTick();publishSnapshot(now);return;
 }
 // A House and run flag can precede the playable tactical view. Keep the
 // bridge connected but do not initialize features, call game functions or
 // consume queued commands in loading/menu/dialog/invalid-frame boundaries.
 if(active&&running&&(!eventdelivery::mayQueue(get<int>(0xA8B238))||frame<0||frame>=INT32_MAX||
    !external::display::inspect([](uint32_t at,void*out,size_t n){memcpy(out,(void*)(uintptr_t)at,n);return true;}).visible())){
  if(riskRenderActive)riskRenderRedrawPending=true;
  restoreHighScroll();
  snap.status=1;snap.bridgeStatus=1;snap.commandStatus=0;snap.match=match;
  snap.frame=frame;snap.chat=get<bool>(0xA8BC60+25);pointerTick();publishSnapshot(now);return;
 }
 active=active&&running;
 if(!active){if(soundPending){MemoryBarrier();sh->uiSoundAck=sh->uiSoundRequest;}cancelCooperativePlan(-3);if(lastHouse||snap.status!=0)reset();sessionTracker.update(false,0,-1,-1);lastHouse=0;lastFrame=-1;snap.status=0;snap.bridgeStatus=0;snap.commandStatus=0;snap.labels=0;snap.swCount=0;if(sh->request!=sh->ack){sh->result=-3;sh->count=0;pendingAck=sh->request;pendingAckReady=true;}if(selectionPending){sh->selectResult=-3;sh->selectCount=0;MemoryBarrier();sh->selectAck=sh->selectRequest;}if(planPending){sh->planResult=-3;sh->planAccepted=0;sh->planAcceptedMask=sh->planRejectedMask=0;MemoryBarrier();sh->planAck=sh->planRequest;}}
 else {int sessionMode=get<int>(0xA8B238);
 // A mode switch while the House address is reused invalidates outstanding
 // requests and all actor leases; do not allow previous-lane events to leak.
 bool newMatch=sessionTracker.update(true,house,sessionMode,frame)!=sessionepoch::Transition::None;
 if(newMatch){if(lastHouse)cancelCooperativePlan(-3);reset();match=sessionTracker.epoch;lastHouse=house;sh->computeApplied=sh->computeRequested&compute::All;scanTypes();slow=now-100;sceneFrame=-2147483647;sceneTick=0;}
 if(newMatch||DWORD(now-catalogueAt)>=DWORD(catalogueReady?500:100)){
  external::catalogue::Stamp stamp;catalogueAt=now;
  if(external::catalogue::capture([](uint32_t a,void*p,size_t n){return read(a,p,n);},stamp)){
   if(!catalogueReady||catalogueHash!=stamp.hash)scanTypes();catalogueHash=stamp.hash;catalogueReady=true;
  }else{catalogueReady=false;scanTypes();}
 }
 // Control time belongs to the dispatcher, never to the optional display scan.
 rebuildPlayerSidebar(house);lastFrame=frame;snap.frame=frame;snap.match=match;snap.session=sessionMode;snap.status=1;snap.bridgeStatus=1;snap.commandStatus=1;
 snap.chat=get<bool>(0xA8BC60+25);
 bool needsIndex=hookpolicy::needsWorldIndex(cmd,sh->dodgeEnabled||dodgeCount||sh->burstEnabled||sh->burstHoldEnabled||burstCount||sh->missileEnabled||missionObserver.pending()||pendingRepairs.count||actorLeaseArbiter.count||loadLedger.count||loadCalculationPending||repairCalculationPending||runtimeCoopPending||nativeCount||unloadCount||repairAt<repairCount||entryReservationCount||cooperativeEffectCount||sh->cooperativePlan!=sh->cooperativeAck,
  sh->flags|sh->plannerTracking|((sh->cooperativePlan!=sh->cooperativeAck)?AutoOreReturn:0),(sh->flags&AutoRepair)&&(!autoWas||DWORD(now-lastAuto)>=clampedRepairInterval(sh->repairMs)));
 if(needsIndex)indexWorld(frame);
 // Release stale leases before any feature sends a new game order. No game
 // state mutation occurs here: only indexed actor identities are inspected.
 if(actorLeaseArbiter.count)actorLeaseArbiter.sweep(match,frame,[](uint32_t id){
  uint32_t p=worldByID(id);return std::pair<uint32_t,uint32_t>{p,manualVersions.get(id)};
 });
 observePausedOre(house,frame);
 observeUnloadRoster(house,frame,now);
 smallMapCameraTick();highScrollTick(preciseNow);
 // Only tick consumes feature requests; neither Hook callback can bypass
 // the shared game-thread intent/validation/event pipeline.
 consumeSubmittedIntents(house,frame,now);
 if(playerContextChanged){playerContextChanged=false;snap.commandStatus=0;publishSnapshot(now);return;}
 QueryPerformanceCounter(&controlEnd);snap.controlMicros=micros(start,controlEnd,frequency);
 // Release receipts before any recurring work or overlay projection. In a busy
 // scene the next user command can now be staged while rendering catches up.
 if(cmd){snap.scanMicros=snap.controlMicros;publishSnapshot(now);}
 loadingCalculationTick(house,now);repairTick(house,now);autoLoadTick(house,frame);nativeOrdersTick(house,frame);
 // Read-only observation, bounded to 32 outstanding orders per game frame.
 if(missionObserver.pending())snap.observedEvents+=missionObserver.pollWithReceipts(match,frame,now,[](uint32_t id){
  gateway::MissionState current{};uint32_t p=worldByID(id);
  if(p){current.valid=true;current.mission=get<int>(p+0xAC);
   current.queued=get<int>(p+0xB4);current.carrier=get<uint32_t>(p+0x11C);}
  return current;
 },[](const gateway::Receipt& receipt){
  recordReceipt(receipt);
  actorLeaseArbiter.release(receipt.match,receipt.unit,receipt.serial);
  if(receipt.phase==gateway::Phase::Cancelled)++snap.receiptCancelled;
  else if(receipt.phase==gateway::Phase::Expired)++snap.receiptExpired;
 });
 const bool dodgeChanged=dodgeControlTick(house,frame,now);
 const bool burstChanged=burstControlTick(house,frame,now);
 const bool planningChanged=publishPlanningWorld(house,frame,now);
 runtimePlanningTick(house,frame,now);
 const bool missileChanged=missileControlTick(house,frame,now);
 QueryPerformanceCounter(&worldEnd);snap.worldMicros=micros(controlEnd,worldEnd,frequency);
 uint32_t tactical=get<uint32_t>(0x887324),flags=sh->flags;int cx=tactical?get<int>(tactical+0xB0):0,cy=tactical?get<int>(tactical+0xB4):0;
 double zoom=tactical?get<double>(tactical+0xB8):0;int width=get<int>(0xA8EB84),height=get<int>(0xA8EB88);
 bool cameraMoved=cx!=cameraX||cy!=cameraY;
 // Render cadence is independent of world scans. Scan at most about 60 Hz;
 // unchanged frames reuse their scene, and camera-only changes shift it.
 static uint32_t selectedStamp;uint32_t stamp=2166136261u;Vec selected=vec(0xA8ECB8);for(int i=0;i<selected.count;++i)stamp=(stamp^item(selected,i))*16777619u;
 bool selectionChanged=stamp!=selectedStamp;selectedStamp=stamp;
 bool force=newMatch||selectionPending||selectionChanged||flags!=sceneFlags||tactical!=sceneTactical||width!=sceneWidth||height!=sceneHeight||zoom!=sceneZoom||snap.crateResultSerial!=sceneResultSerial;
 bool full=force||(DWORD(now-sceneTick)>=16&&(frame!=sceneFrame||(cameraMoved&&DWORD(now-sceneTick)>=32)||DWORD(now-sceneTick)>=100));
 bool summaryChanged=scanSummary(house,frame,now,flags);
 if(snap.techOpen&&((techPending&&DWORD(now-techRefresh)>=16)||DWORD(now-techRefresh)>=1000))refreshTech(house,now);
 if(full){if(!sh->externalDisplay)scan(house,frame,now);sceneFrame=frame;sceneTick=now;sceneFlags=flags;sceneTactical=tactical;sceneWidth=width;sceneHeight=height;sceneZoom=zoom;sceneResultSerial=snap.crateResultSerial;}
 else if(cameraMoved)shiftScene(cx-cameraX,cy-cameraY);
 else if(!cmd&&!summaryChanged&&!planningChanged&&!missileChanged&&!burstChanged&&!dodgeChanged&&DWORD(now-snap.sampledTick)<250)return;
 cameraX=cx;cameraY=cy;snap.cameraX=cx;snap.cameraY=cy;
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
static void restoreFrameDriver(){}
static void maintainScheduler(){if(sh){sh->driverMode=scheduler::Message;sh->driverTarget=0;sh->driverStatus=scheduler::MessageActive;}}
// One place owns provider choice, fallback and the execution lock.
// This adapter does not contain feature decisions or EventClass operations.
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
extern "C" unsigned BeginAssistCapture(){
 gameentry::Request r{};return gameentry::capture(gameentry::Client::Assist,r)?r.token:0;
}
extern "C" void EndAssistCapture(unsigned token){
 gameentry::Request r{};r.module=gameentry::moduleIdentity();r.client=gameentry::Client::Assist;r.token=token;gameentry::leave(r);
}
extern "C" void AssistTick(){if(gameentry::active(gameentry::Client::Assist))tick();}
extern "C" void AssistFault(){restorePlayerContext();faultContext={};nativeMegaQueueCallDepth=nativeObjectQueueCallDepth=0;fault=true;restoreClickHooks();resetCooperativeState();resetMissileControl();resetBurstControl();resetDodgeControl();clearNativeOrders();restorePlacement();restoreTech();restoreCSHook();if(sh){sh->result=-10;InterlockedIncrement(&sh->seq);sh->data.status=-2;sh->data.bridgeStatus=-2;sh->data.commandStatus=0;sh->data.labels=0;MemoryBarrier();InterlockedIncrement(&sh->seq);sh->ack=sh->request;sh->selectResult=sh->planResult=-10;sh->selectAck=sh->selectRequest;sh->planAccepted=0;sh->planAcceptedMask=sh->planRejectedMask=0;sh->planAck=sh->planRequest;notifyHost();}}
extern "C" void GuardedAssistTick();
#include "native_message_policy.h"
extern "C" void AssistRuntimeMessage(int code,WPARAM wp,LPARAM lp){
 if(code>=0){
  InterlockedExchange(&hostWorkPending,1);
  dispatchFromTrigger(dispatchrouter::Trigger::Message,GetTickCount(),wp==PM_REMOVE?reinterpret_cast<const MSG*>(lp):nullptr);
 }
}
extern "C" void AssistRuntimeWatchdog(){
 if(!sh)return;
 const DWORD now=sampleClock(sh);
 if(!sh->stop&&!(DWORD(now-sh->heartbeat)>3000&&controllerExited(now)))return;
 gameentry::Scope entry(gameentry::Client::Assist);if(!entry)return;
 GuardedAssistTick(); // stop/dead-controller branch only; never normal work
}

