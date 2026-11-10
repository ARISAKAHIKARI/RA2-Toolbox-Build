// Controlled x86 ABI fixture. Win32 delivery, object lookup and transport are
// replaced; interception stubs, capture descriptors, queue and sender are the
// production headers. This is not a live game/platform compatibility test.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cstring>
#include "../source/private_outlist.h"
#include "../source/event_delivery.h"
using namespace assist;using Event=vanilla::Packet;
struct Shared {uint32_t privateOutMode=1,stop=0,controlCommandOwner=0,heartbeat=1000;uint32_t privateOutPending=0,privateOutAccepted=0,privateOutSent=0,privateOutRejected=0,privateOutCancelled=0;int privateOutStatus=0;};
static Shared data,*sh=&data;
static bool fault=false;static uint32_t match=1,lastHouse=0x20001000;static int outgoingBudget=64;
static unsigned nativeMegaQueueCallDepth=0,nativeObjectQueueCallDepth=0;
static struct {int session=3;bool chat=false;} snap;
static struct {bool active=false;} readOnlyState;
namespace readonly {static bool commands(Shared*){return true;}}
static uint32_t clockNow=1000;static DWORD sampleClock(Shared*){return clockNow;}
template<class T>static T get(uint32_t p){return *(T*)(uintptr_t)p;}
template<class T>static void put(uint32_t p,T v){*(T*)(uintptr_t)p=v;}
static bool read(uint32_t p,void*out,size_t n){memcpy(out,(void*)(uintptr_t)p,n);return true;}
static bool riskReady(bool=false){return true;}static bool simulationReady(){return true;}
static bool live(uint32_t p){return p!=0;}static void indexWorld(int){}
static uint32_t worldByID(uint32_t id){return id?0x20002000:0;}
static bool sellHumanHouse(uint32_t){return true;}static bool controlCommandOwnerReady(uint32_t){return true;}
static void cancelNative(uint32_t,bool){}
static struct {uint32_t get(uint32_t){return 0;}} manualVersions;
static int outgoingRoom(){return outgoingBudget;}
static bool gameSendTiming(uint32_t&v){v=5678;return true;}
static bool nativeExecutableRange(uint32_t,size_t){return true;}
template<class R,class...Args>static R call(uint32_t at,uint32_t self,Args...args){return ((R(__thiscall*)(void*,Args...))(uintptr_t)at)((void*)(uintptr_t)self,args...);}
struct RiskSite {uint32_t address;unsigned size;uint8_t before[16],after[16];};
static bool restoreRiskSite(RiskSite&){return true;}static bool installRiskSite(RiskSite&){return true;}
static bool directSellPending(){return false;}
static struct {bool active(uint32_t){return false;}} codeHookRegistry;
extern "C" int __cdecl GuardedRiskAction(unsigned,unsigned,unsigned);
#include "../source/private_outlist_bridge.h"
static bool privateBpRequest(bool){return true;}static bool privateBpRetired(){return true;}static int privateBpStatus(){return 3;}
extern "C" int __cdecl GuardedRiskAction(unsigned op,unsigned frame,unsigned source){return privateOutAction(op,frame,source);}
extern "C" __declspec(dllexport) uint32_t Entry(unsigned tag){return privateEntry(tag);}
extern "C" __declspec(dllexport) void Reset(unsigned active){privateQueue.clear();privateReturnDepth=0;clockNow=1000;privateDispatchFrame=-1;privateDispatchBudget=0;privateMode=1;data=Shared{};privateEnabled=active!=0;privateOwner=42;privateRetry=0;outgoingBudget=64;}
extern "C" __declspec(dllexport) uint32_t Packet(unsigned i){return uint32_t(uintptr_t(&privateQueue.at(i).event));}
extern "C" __declspec(dllexport) unsigned Count(){return privateQueue.count;}
extern "C" __declspec(dllexport) unsigned Depth(){return privateReturnDepth;}
extern "C" __declspec(dllexport) int Submit(Event*p){return privateOutQueue(*p,false);}
extern "C" __declspec(dllexport) void Send(){servicePrivateOut();}
extern "C" __declspec(dllexport) unsigned Sent(){return sh->privateOutSent;}
extern "C" __declspec(dllexport) int Status(){return sh->privateOutStatus;}
extern "C" __declspec(dllexport) void Stop(){restorePrivateOut();}

extern "C" __declspec(dllexport) void Advance(unsigned n){clockNow+=n;}

extern "C" __declspec(dllexport) bool Compatible(){return privateCaptureCompatible();}
