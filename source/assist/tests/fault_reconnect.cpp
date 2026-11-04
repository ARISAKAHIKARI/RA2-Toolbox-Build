// Production bridge lifecycle routes; Win32 transport and resource completion
// are bounded substitutes. This does not run a game or Windows hooks.
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include "../../shared/source/runtime_seh_scope.h"
// Portable unwind transport for the production helper. Actual Windows SEH
// forwarding/continuation is covered separately by the Windows scope fixture.
extern "C" void RuntimeScopedCall(void*context,void(*body)(void*),void(*cleanup)(void*)){
 try{body(context);}catch(...){cleanup(context);throw;}cleanup(context);
}
using DWORD=uint32_t;using LONG=int32_t;using HANDLE=void*;
constexpr uint32_t Magic=0x12345678,Version=1,WakeTag=42,WM_NULL=0,FILE_MAP_ALL_ACCESS=1;
constexpr bool FALSE=false;
struct MSG{uint32_t message=0;uintptr_t wParam=0,lParam=0;};
struct Shared{
 uint32_t magic=Magic,version=Version,pid=123,owner=456,heartbeat=5000;
 LONG stop=0,seq=0,gameWakePending=0;
 uint32_t request=9,ack=8,selectRequest=7,selectAck=6,planRequest=5,planAck=4;
 int result=0,selectResult=0,planResult=0,planAccepted=0,planAcceptedMask=0,planRejectedMask=0;
 int superweaponGrant=0,driverMode=0,driverTarget=0,driverStatus=0;
 LONG computeFrameSeq=0;uint32_t computeFrameMatch=0,computeFrameTick=0,computeFrameSerial=0;int computeFrame=-1;
 struct{int status=1,bridgeStatus=1,commandStatus=1,labels=3;}data;
};
static Shared storage,*sh,*available;static HANDLE mapping,controllerProcess;
static bool fault,commandDormant,worldCleanupPending,pendingAckReady;
static DWORD controllerPID,controllerCheckAt,lastHouse,pendingAck;
static volatile LONG schedulerBusy,hostWorkPending,messageGameThread;
static unsigned nativeMegaQueueCallDepth,nativeObjectQueueCallDepth;
struct{int phase=0,source=0,target=0;}faultContext;
struct{int frame=0;}snap;
struct{void update(bool,int,int,int){}}sessionTracker;
struct{void reset(){}}hookWorld;
static int missileHoldCount;static bool missileCleanup;
static unsigned ordinaryCalls,pollCalls,resetCalls,opens,views,unmaps,closes,guardedCalls;
static unsigned pendingPolls;static bool directOwner,unsafeHandoff,controllerDead,entryAllowed;
static unsigned entryDepth;
static uint32_t inputMatch;static int inputFrame;static bool externalInputFrame;
static bool throwTick,throwFilter,callbackOwnsLease,sampleInput;
static unsigned filterCalls;
static LONG InterlockedExchange(volatile LONG*p,LONG v){LONG old=*p;*p=v;return old;}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG expected){LONG old=*p;if(old==expected)*p=v;return old;}
static LONG InterlockedIncrement(volatile LONG*p){return ++*p;}
static void MemoryBarrier(){}
static DWORD GetCurrentProcessId(){return 123;}
static DWORD GetCurrentThreadId(){return 777;}
static DWORD sampleClock(Shared*){return 5000;}
static void mapName(wchar_t*,DWORD){}
static HANDLE OpenFileMappingW(unsigned,bool,const wchar_t*){++opens;return available?reinterpret_cast<HANDLE>(1):nullptr;}
static void*MapViewOfFile(HANDLE,unsigned,unsigned,unsigned,size_t){++views;return available;}
static bool featureRestorationComplete(){return !pendingPolls&&!directOwner;}
static bool UnmapViewOfFile(void*){if(!featureRestorationComplete())unsafeHandoff=true;++unmaps;return true;}
static bool CloseHandle(HANDLE){++closes;return true;}
static bool controllerExited(DWORD){return controllerDead;}
static void maintainTechParameters(){++pollCalls;if((fault||(sh&&sh->stop))&&pendingPolls)--pendingPolls;}
static void playerMaintenance(){}static void controlHouseFlagMaintenance(){}
static void reset(){++resetCalls;}
static void indexWorld(int){}static bool releaseMissileHolds(uint32_t,int,DWORD){return true;}
template<class T>static T get(uint32_t){return T();}
static void stickyTick(bool){}static void notifyHost(){}
#include BRIDGE_UNWIND_HELPERS
#define NOOP(name) static void name(){}
NOOP(superweaponGrantReset) NOOP(restoreMapVisibilityParameters) NOOP(restoreControlParameters)
NOOP(restoreTechEnhanceParameters) NOOP(restoreTechParameters) NOOP(restoreSidebarView)
NOOP(nativePlacementReset) NOOP(nativePlacementCompletionReset) NOOP(restorePauseView)
NOOP(restoreCrcMapShadow) NOOP(restoreBuildTimeTooltip) NOOP(restoreNativeSellCursor)
NOOP(restoreNativeMegaGateway) NOOP(restoreControlObjectOwner) NOOP(restoreControlCommandOwner)
NOOP(restoreNetworkSender) NOOP(restoreTechEnhancement) NOOP(restorePlayerContext)
NOOP(resetMoOrders) NOOP(restoreClickHooks) NOOP(resetCooperativeState) NOOP(resetFormation)
NOOP(resetMissileControl) NOOP(resetBurstControl) NOOP(resetDodgeControl) NOOP(clearNativeOrders)
NOOP(restorePlacement) NOOP(restoreTech) NOOP(restoreCSHook)
#undef NOOP
static void restoreFrameDriver();
namespace scheduler{constexpr int Message=1,MessageActive=1;}
namespace dispatchrouter{enum class Trigger{Message,GameFrame};}
namespace gameentry{
enum class Client{Assist};
struct Scope{
 bool accepted;
 Scope(Client,bool=false):accepted(entryAllowed){if(accepted)++entryDepth;}
 void release(){if(accepted){accepted=false;--entryDepth;}}
 ~Scope(){release();}
 explicit operator bool()const{return accepted;}
};
static bool active(Client){return entryDepth!=0;}
#include GAMEENTRY_SCOPED_ROUTE
}
extern "C" void GuardedAssistTick();
#include FAULT_RECONNECT_ROUTES
extern "C" void GuardedAssistTick(){
 ++guardedCalls;callbackOwnsLease=entryDepth!=0&&schedulerBusy==1;
 if(throwTick)throw 7;AssistTick();
}
static DWORD GetTickCount(){return 5000;}
static bool superweaponGrantWantsInputSample(const MSG*){return sampleInput;}
static bool nativePlacementWantsInputSample(const MSG*){return false;}
extern "C" int GuardedAssistFilter(void*){++filterCalls;if(throwFilter)throw 8;return 0;}
#include INPUT_FILTER_ROUTE

static void seed(bool attached=true){
 storage={};sh=attached?&storage:nullptr;available=&storage;
 mapping=attached?reinterpret_cast<HANDLE>(1):nullptr;controllerProcess=nullptr;
 fault=commandDormant=worldCleanupPending=pendingAckReady=false;
 controllerPID=controllerCheckAt=lastHouse=pendingAck=0;
 schedulerBusy=hostWorkPending=messageGameThread=0;
 nativeMegaQueueCallDepth=nativeObjectQueueCallDepth=entryDepth=0;faultContext={};
 missileHoldCount=0;missileCleanup=false;
 ordinaryCalls=pollCalls=resetCalls=opens=views=unmaps=closes=guardedCalls=0;
 pendingPolls=0;directOwner=unsafeHandoff=controllerDead=false;entryAllowed=true;
 throwTick=throwFilter=callbackOwnsLease=sampleInput=false;filterCalls=0;
 inputMatch=0;inputFrame=-1;externalInputFrame=false;
}
static void message(){hostWorkPending=1;dispatchFromTrigger(dispatchrouter::Trigger::Message,5000,nullptr);}
static bool attachedFault(){
 seed();pendingPolls=3;AssistFault();
 const bool reported=fault&&storage.data.status==-2&&storage.result==-10&&storage.ack==storage.request;
 message();const bool held=fault&&sh==&storage&&storage.stop==0&&!unmaps&&!ordinaryCalls&&!opens;
 for(unsigned n=0;fault&&n<5;++n)message();
 const bool retired=!fault&&!sh&&!mapping&&unmaps==1&&!ordinaryCalls&&pollCalls==3&&!unsafeHandoff;
 message();
 return reported&&held&&retired&&sh==&storage&&ordinaryCalls==1&&opens==1&&views==1;
}
static bool nullFault(){
 seed(false);pendingPolls=3;AssistFault();
 const bool held=!AssistRuntimeDetached();
 AssistRuntimeWatchdog();
 const bool pending=fault&&!sh&&!opens&&!ordinaryCalls&&!AssistRuntimeDetached();
 AssistRuntimeWatchdog();
 const bool retired=!fault&&!sh&&AssistRuntimeDetached()&&!opens&&!ordinaryCalls&&pollCalls==3&&!unsafeHandoff;
 message();
 return held&&pending&&retired&&sh==&storage&&ordinaryCalls==1&&opens==1;
}
static bool stopFault(){
 seed();pendingPolls=1;directOwner=true;storage.stop=1;AssistFault();
 AssistRuntimeWatchdog();
 const bool pending=fault&&sh==&storage&&storage.stop==1&&!unmaps&&!ordinaryCalls;
 directOwner=false;AssistRuntimeWatchdog();
 const bool retired=!fault&&!sh&&storage.stop==2&&unmaps==1&&!ordinaryCalls&&!unsafeHandoff;
 // A replacement Host creates a new live mailbox after the old stop ack.
 storage={};message();
 return pending&&retired&&sh==&storage&&ordinaryCalls==1;
}
static bool watchdogAdmission(){
 seed(false);AssistRuntimeWatchdog();
 const bool empty=!opens&&!guardedCalls&&!ordinaryCalls;
 seed();AssistRuntimeWatchdog();
 const bool live=!guardedCalls&&!ordinaryCalls&&!unmaps;
 seed();storage.stop=1;pendingPolls=2;AssistRuntimeWatchdog();
 const bool pending=sh==&storage&&storage.stop==1&&!unmaps&&!ordinaryCalls;
 AssistRuntimeWatchdog();
 return empty&&live&&pending&&!sh&&storage.stop==2&&!ordinaryCalls&&!unsafeHandoff;
}
static bool dispatcherAdmission(){
 seed();fault=true;pendingPolls=2;entryAllowed=false;message();
 const bool rejected=hostWorkPending==1&&!pollCalls&&!unmaps&&!ordinaryCalls;
 entryAllowed=true;schedulerBusy=1;message();
 const bool contended=hostWorkPending==1&&!pollCalls;
 schedulerBusy=0;hostWorkPending=0;dispatchFromTrigger(dispatchrouter::Trigger::Message,5000,nullptr);
 return rejected&&contended&&!pollCalls&&!guardedCalls&&!ordinaryCalls&&pendingPolls==2;
}
static bool dispatcherUnwind(){
 seed();entryDepth=3;throwTick=true;bool forwarded=false;
 try{message();}catch(int value){forwarded=value==7;}
 const bool released=forwarded&&entryDepth==3&&schedulerBusy==0&&hostWorkPending==0&&!ordinaryCalls;
 throwTick=false;message();
 return released&&entryDepth==3&&schedulerBusy==0&&ordinaryCalls==1;
}
static bool callbackLifetime(){
 seed();message();
 return callbackOwnsLease&&entryDepth==0&&schedulerBusy==0&&ordinaryCalls==1;
}
static bool inputUnwind(){
 bool okay=true;MSG msg;
 for(bool sample:{false,true}){
  seed();entryDepth=2;sampleInput=sample;throwFilter=true;bool forwarded=false;
  try{AssistRuntimeFilter(&msg);}catch(int value){forwarded=value==8;}
  okay&=forwarded&&entryDepth==2&&schedulerBusy==0&&filterCalls==1&&ordinaryCalls==unsigned(sample);
 }
 seed();entryAllowed=false;sampleInput=true;AssistRuntimeFilter(&msg);
 return okay&&!filterCalls&&!ordinaryCalls&&!entryDepth&&!hostWorkPending;
}
static bool publicationUnwind(){
 seed();storage.computeFrameSeq=8;inputMatch=42;inputFrame=99;externalInputFrame=true;
 bool heldOdd=false;
 withInputPublication(&storage,42,5000,99,true,[&]{heldOdd=storage.computeFrameSeq==9&&storage.computeFrame==-1;return;});
 const bool normal=heldOdd&&storage.computeFrameSeq==10&&storage.computeFrameSerial==1&&storage.computeFrameMatch==42&&storage.computeFrameTick==5000&&storage.computeFrame==99;
 withInputPublication(&storage,42,5001,99,false,[]{});
 const bool unchanged=storage.computeFrameSeq==12&&storage.computeFrameSerial==1&&storage.computeFrameTick==5000;
 bool forwarded=false;
 try{withInputPublication(&storage,42,5002,100,true,[]{throw 9;});}catch(int value){forwarded=value==9;}
 const bool invalid=forwarded&&storage.computeFrameSeq==14&&storage.computeFrameSerial==1&&!storage.computeFrameMatch&&!storage.computeFrameTick&&storage.computeFrame==-1&&inputMatch==0&&inputFrame==-1&&!externalInputFrame&&storage.request==9&&storage.ack==8;
 withInputPublication(&storage,42,5003,100,true,[]{});
 return normal&&unchanged&&invalid&&storage.computeFrameSeq==16&&storage.computeFrameSerial==2&&storage.computeFrame==100&&storage.computeFrameMatch==42;
}
static bool nativeDepthUnwind(){
 unsigned depth=4,other=11,entered=0;bool forwarded=false,owned=true;
 try{withNativeQueueCallDepth(depth,[&]{
  ++entered;owned&=depth==5;
  withNativeQueueCallDepth(depth,[&]{++entered;owned&=depth==6;throw 10;});
 });}catch(int value){forwarded=value==10;}
 const bool released=forwarded&&owned&&entered==2&&depth==4&&other==11;
 withNativeQueueCallDepth(depth,[&]{++entered;owned&=depth==5;});
 return released&&owned&&entered==3&&depth==4&&other==11;
}
int main(){
 const bool attached=attachedFault(),nullIPC=nullFault(),stop=stopFault(),watchdog=watchdogAdmission(),dispatch=dispatcherAdmission();
 const bool unwind=dispatcherUnwind(),lifetime=callbackLifetime(),input=inputUnwind();
 const bool publication=publicationUnwind(),depth=nativeDepthUnwind();
 std::printf("{\"pass\":%s,\"cases\":{\"attached_fault_cleanup_then_reconnect\":%s,\"null_ipc_fault_retains_ownership_then_reconnect\":%s,\"stop_fault_cleanup_then_replacement_host\":%s,\"watchdog_never_admits_live_work\":%s,\"dispatcher_preserves_entry_and_grant_gates\":%s,\"forwarded_unwind_releases_only_owned_scheduler_and_entry\":%s,\"callback_retains_lease_until_return\":%s,\"both_input_branches_release_entry_on_unwind\":%s,\"partial_publication_invalidates_and_closes_owned_sequence\":%s,\"native_depth_releases_only_owned_nested_increments\":%s}}\n",
  attached&&nullIPC&&stop&&watchdog&&dispatch&&unwind&&lifetime&&input&&publication&&depth?"true":"false",attached?"true":"false",nullIPC?"true":"false",stop?"true":"false",watchdog?"true":"false",dispatch?"true":"false",unwind?"true":"false",lifetime?"true":"false",input?"true":"false",publication?"true":"false",depth?"true":"false");
}
