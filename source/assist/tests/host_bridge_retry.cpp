// Runs the complete, extracted production ensureBridge() against deterministic
// Win32/transport substitutes. No game process or Windows API is exercised.
#include "../source/protocol.h"
#include "../../shared/source/native_viewport_projection.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

using namespace assist;
using DWORD=uint32_t;
using HANDLE=void*;
using HMODULE=void*;
using HWND=void*;
constexpr DWORD PAGE_READWRITE=4,FILE_MAP_ALL_ACCESS=31,SYNCHRONIZE=0x100000;
constexpr DWORD WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,ERROR_ALREADY_EXISTS=183;
constexpr DWORD ERROR_INVALID_PARAMETER=87,ERROR_INVALID_WINDOW_HANDLE=1400;
constexpr bool FALSE=false;
static HANDLE const INVALID_HANDLE_VALUE=reinterpret_cast<HANDLE>(~uintptr_t(0));

struct Scenario {
 bool runtimeOK=true,nativeOK=true,createOK=true,mapOK=true,windowOK=true;
 bool identityOK=true,installOK=true,existing=false,ownerAlive=true;
 DWORD runtimeMs=0,nativeMs=0,createMs=0,mapMs=0,identityMs=0,installMs=0,releaseMs=0;
 DWORD error=126;
 unsigned runtimeCalls=0,nativeCalls=0,createCalls=0,mapCalls=0,installCalls=0;
 unsigned releases=0,probes=0,wakes=0,additional=0,typeConfig=0;
} fixture;
static DWORD clockNow=1000,lastError=0;
static DWORD GetTickCount(){return clockNow;}
static DWORD GetLastError(){return lastError;}
static DWORD GetCurrentProcessId(){return 201;}
static bool enabled=true,bridgeBlocked=false;
static DWORD bridgeRetryAt=0,pid=17,gameThread=19;
static int bridgeAttempts=0;
static std::wstring bridgeFailure,status;
static auto storage=std::make_unique<Shared>();
static Shared*shared=nullptr;
static HANDLE mapping=nullptr;
static HMODULE module=nullptr;
static HWND game=reinterpret_cast<HWND>(0x100),ui=reinterpret_cast<HWND>(0x200);
static DWORD ack=0,notice=0,lastData=0,attachAt=0;
static uint32_t repairMs=300,schedulerMode=0,schedulerTarget=0,megaQueueMode=0,objectQueueMode=0;
static uint32_t flags(){return AutoRepair;}
static void syncAdditional(){++fixture.additional;}
static void syncTypeConfig(){++fixture.typeConfig;}
static void wakeGame(){++fixture.wakes;}
static void critical(const std::wstring&){}
static void diag(const std::wstring&){}
static std::wstring hex32(uint32_t value){return std::to_wstring(value);}
static void setStatus(const std::wstring&text){status=text;}
static void logBridgeHealth(bool){}
static void logLoadedGameModules(){}
static void probeNativeViewportMapping(){++fixture.probes;}
static void observeNativeViewportPrior(const Shared*,bool){}
static struct Reader {int connectionIdentity(){return 1;}} externalReader;

namespace gameentry {enum class Client {Assist};}
namespace gameinput {
using Subscription=uint64_t;
struct NativeWindowProbe {bool read=false;uint32_t raw=0;DWORD owner=0,thread=0,error=0;};
struct Capacity {unsigned targets=0,subscriptions=0;};
static bool runtimeAvailable(){++fixture.runtimeCalls;clockNow+=fixture.runtimeMs;lastError=fixture.error;return fixture.runtimeOK;}
static HWND nativeWindow(DWORD,DWORD,NativeWindowProbe*p){++fixture.nativeCalls;clockNow+=fixture.nativeMs;p->error=fixture.error;return fixture.nativeOK?game:nullptr;}
static HMODULE runtimeModule(){return reinterpret_cast<HMODULE>(0x300);}
static uint32_t windowToken(HWND){return fixture.windowOK?0x200:0;}
static bool assistIdentity(HMODULE,uint32_t,uint32_t,const wchar_t*){clockNow+=fixture.identityMs;return fixture.identityOK;}
static Subscription install(gameentry::Client,DWORD,DWORD,HWND){++fixture.installCalls;clockNow+=fixture.installMs;lastError=fixture.error;return fixture.installOK?7:0;}
static const wchar_t*installationStage(){return L"x86-hook-broker";}
static Capacity capacity(){return {};}
}
static gameinput::Subscription hook=0;
struct RetiredBridge {Shared*state;HANDLE mapping;HMODULE module;gameinput::Subscription hook;HWND window;DWORD lastWake,thread;int process;};
static std::vector<RetiredBridge>retiredBridges;
static void reapBridges(){}
static void releaseBridge(){++fixture.releases;clockNow+=fixture.releaseMs;shared=nullptr;mapping=nullptr;module=nullptr;hook=0;lastData=attachAt=0;}

static HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD,size_t,const wchar_t*){
 ++fixture.createCalls;clockNow+=fixture.createMs;
 lastError=!fixture.createOK?fixture.error:fixture.existing?ERROR_ALREADY_EXISTS:0;
 return fixture.createOK?reinterpret_cast<HANDLE>(0x400):nullptr;
}
static void*MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,size_t){++fixture.mapCalls;clockNow+=fixture.mapMs;lastError=fixture.error;return fixture.mapOK?storage.get():nullptr;}
static bool UnmapViewOfFile(const void*){return true;}
static bool CloseHandle(HANDLE){return true;}
static HANDLE OpenProcess(DWORD,bool,DWORD){return reinterpret_cast<HANDLE>(0x500);}
static DWORD WaitForSingleObject(HANDLE,DWORD){return fixture.ownerAlive?WAIT_TIMEOUT:WAIT_OBJECT_0;}
static DWORD GetModuleFileNameW(HMODULE,wchar_t*out,DWORD){const wchar_t*name=L"C:\\fixture\\RA2Toolbox.exe";std::wcscpy(out,name);return DWORD(std::wcslen(name));}
namespace runtimebroker {
struct Client {uint32_t moduleBase(){return 0x400000;}uint32_t hookRva(){return 0x1200;}};
static Client&client(){static Client value;return value;}
}
namespace runtime {
constexpr uint32_t Version=4;
struct Control {char bytes[176];};
static void mapName(wchar_t*out,DWORD,DWORD){std::wcscpy(out,L"fixture-runtime");}
}

#include HOST_BRIDGE_ROUTES

static unsigned checks=0,failures=0;
static void check(bool yes,const char*message){++checks;if(!yes){++failures;std::printf("FAIL: %s\n",message);}}
static void reset(){
 fixture={};clockNow=1000;lastError=0;enabled=true;bridgeBlocked=false;
 bridgeRetryAt=0;bridgeAttempts=0;bridgeFailure.clear();status.clear();
 shared=nullptr;mapping=nullptr;module=nullptr;hook=0;ack=notice=lastData=attachAt=0;
 game=reinterpret_cast<HWND>(0x100);retiredBridges.clear();std::memset(storage.get(),0,sizeof(Shared));
}
static void failedAfterWork(const char*message){
 check(!ensureBridge(),message);
 check(bridgeAttempts==1,"failed first attempt counted once");
 check(bridgeRetryAt==clockNow+2000,"failure backoff starts after all blocking work");
 const unsigned before=fixture.runtimeCalls;clockNow+=1999;
 check(!ensureBridge()&&fixture.runtimeCalls==before,"retry delay does not enter runtimeAvailable");
}
int main(){
 reset();enabled=false;check(!ensureBridge()&&!fixture.runtimeCalls,"disabled module does not start transport");
 reset();shared=storage.get();check(ensureBridge()&&!fixture.runtimeCalls,"mapped bridge is preserved without transport restart");
 reset();game=nullptr;check(!ensureBridge()&&!fixture.runtimeCalls,"absent window does not start transport");
 reset();bridgeBlocked=true;check(!ensureBridge()&&!fixture.runtimeCalls,"blocked bridge must not enter runtimeAvailable");
 reset();bridgeRetryAt=1100;check(!ensureBridge()&&!fixture.runtimeCalls,"future retry must not enter runtimeAvailable");
 clockNow=1100;check(ensureBridge()&&fixture.runtimeCalls==1,"attempt at exact deadline succeeds");

 reset();fixture.runtimeOK=false;fixture.runtimeMs=3000;
 failedAfterWork("runtime startup failure returns without mapping");
 check(!fixture.nativeCalls&&!fixture.createCalls,"failed transport cannot reach native window or mapping");
 check(status.find(L"126")!=std::wstring::npos,"transport Win32 error is preserved");
 // Each failure starts a complete quiet period after its own 3-second work.
 for(unsigned attempt=2;attempt<=4;++attempt){
  clockNow=bridgeRetryAt;const unsigned before=fixture.runtimeCalls;
  check(!ensureBridge()&&fixture.runtimeCalls==before+1,"failed transport retries once at deadline");
  check(bridgeAttempts==int(attempt),"transport failure participates in attempt backoff");
  const DWORD delay=attempt>=3?30000:2000;
  check(bridgeRetryAt==clockNow+delay,"2s/30s failure deadline uses completion clock");
  clockNow=bridgeRetryAt-1;
  check(!ensureBridge()&&fixture.runtimeCalls==before+1,"not one millisecond before retry deadline");
 }
 fixture.runtimeOK=true;clockNow=bridgeRetryAt;
 check(ensureBridge()&&shared&&hook,"successful retry establishes the same command bridge");

 reset();clockNow=0xfffffff0u;fixture.runtimeOK=false;fixture.runtimeMs=40;
 check(!ensureBridge()&&bridgeRetryAt==clockNow+2000,"transport backoff crosses DWORD clock wrap");
 auto calls=fixture.runtimeCalls;clockNow=bridgeRetryAt-1;
 check(!ensureBridge()&&fixture.runtimeCalls==calls,"wrapped future deadline remains blocked");
 clockNow=bridgeRetryAt;fixture.runtimeOK=true;
 check(ensureBridge()&&fixture.runtimeCalls==calls+1,"wrapped deadline permits recovery");

 reset();fixture.runtimeMs=3000;fixture.nativeMs=70;fixture.nativeOK=false;
 check(!ensureBridge()&&bridgeAttempts==0,"native-window readiness does not consume installation attempts");
 check(!fixture.createCalls&&bridgeRetryAt==clockNow+250,"native-window wait remains 250ms after completed probe");
 calls=fixture.runtimeCalls;clockNow=bridgeRetryAt-1;
 check(!ensureBridge()&&fixture.runtimeCalls==calls,"native-window cooldown gates transport too");
 clockNow=bridgeRetryAt;fixture.nativeOK=true;
 check(ensureBridge()&&bridgeAttempts==1,"native-window recovery preserves first installation attempt");

 reset();fixture.createOK=false;fixture.createMs=2500;failedAfterWork("mapping creation failure");
 reset();fixture.mapOK=false;fixture.mapMs=2500;fixture.releaseMs=800;failedAfterWork("mapping view failure");
 reset();fixture.windowOK=false;fixture.releaseMs=2500;failedAfterWork("host window token failure");
 reset();fixture.installOK=false;fixture.installMs=2500;fixture.releaseMs=800;failedAfterWork("hook installation failure");
 check(status.find(L"x86-hook-broker")!=std::wstring::npos,"installation stage is retained in failure status");

 reset();fixture.identityOK=false;fixture.identityMs=2100;fixture.releaseMs=400;
 check(!ensureBridge()&&bridgeBlocked&&!shared,"protocol mismatch still blocks and releases mapping");
 check(bridgeRetryAt==clockNow+2000,"protocol rejection records completed failure time");
 calls=fixture.runtimeCalls;clockNow+=30000;
 check(!ensureBridge()&&fixture.runtimeCalls==calls,"protocol block is not bypassed at retry deadline");

 reset();fixture.existing=true;fixture.mapMs=2400;
 failedAfterWork("existing incompatible mapping remains rejected");
 reset();fixture.existing=true;storage->magic=Magic;storage->version=Version;storage->pid=pid;storage->owner=999;
 check(!ensureBridge()&&!fixture.installCalls&&!shared,"another live owner cannot be replaced");
 check(bridgeRetryAt==clockNow+2000,"existing live owner also receives retry deadline");

 reset();check(ensureBridge(),"fresh connection succeeds");
 check(shared==storage.get()&&shared->magic==Magic&&shared->version==Version&&shared->pid==pid&&shared->owner==GetCurrentProcessId(),"normal shared identity is initialized");
 check(shared->flags==AutoRepair&&fixture.additional==1&&fixture.typeConfig==1,"normal settings publication is unchanged");
 check(hook==7&&attachAt==1000&&fixture.wakes==1&&bridgeFailure.empty(),"normal subscription, heartbeat and wake are unchanged");
 calls=fixture.runtimeCalls;check(ensureBridge()&&fixture.runtimeCalls==calls,"connected fast path stays cheap");
 reset();fixture.existing=true;storage->magic=Magic;storage->version=Version;storage->pid=pid;storage->owner=GetCurrentProcessId();storage->stop=2;
 check(ensureBridge()&&fixture.mapCalls==2&&shared->stop==0,"acknowledged prior owner cleanup remains reusable");

 std::printf("{\"checks\":%u,\"failures\":%u,\"passed\":%s,\"scope\":\"complete production ensureBridge with deterministic Win32/transport substitutes; no Windows/game validation\"}\n",checks,failures,failures?"false":"true");
 return failures?1:0;
}
