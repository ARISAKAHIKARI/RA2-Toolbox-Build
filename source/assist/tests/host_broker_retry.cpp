// Executes the complete production pulse() with deterministic host/Win32 and
// broker substitutes. This is scheduling evidence, not a Windows/game run.
#include <cstdint>
#include <cstdio>
#include <memory>
#include <mutex>

using DWORD=uint32_t;
using LONG=int32_t;
using BOOLEAN=unsigned char;
using HWND=void*;
#define CALLBACK
struct LARGE_INTEGER {int64_t QuadPart=0;};
static DWORD clockNow=1000,lastError=0;
static DWORD GetTickCount(){return clockNow;}
static DWORD GetLastError(){return lastError;}
static LONG InterlockedExchange(volatile LONG*p,LONG value){const LONG old=*p;*p=value;return old;}
static LONG InterlockedCompareExchange(volatile LONG*p,LONG value,LONG expected){const LONG old=*p;if(old==expected)*p=value;return old;}
static bool QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=uint64_t(clockNow)*1000;return true;}
namespace preciseclock {static uint64_t micros(){return uint64_t(clockNow)*1000;}}
namespace runtime {
constexpr uint32_t Tactical=1,Cursor=2;
struct Control {volatile LONG enabled=0;DWORD heartbeat=0;uint32_t ready=0;};
}
namespace framesignal {
constexpr uint32_t Compute=1,ExplicitDiagnostic=2,Native=1;
struct Stamp {uint32_t state=0,serial=0;int64_t frequency=0,qpc=0;};
struct Client {bool open(DWORD,uint32_t){return false;}void heartbeat(){}bool current(Stamp&){return false;}};
}
namespace runtimebroker {
struct Client {
 bool installOK=false;
 DWORD delay=3000,error=126;
 uint32_t healthyGeneration=9,nextToken=77,nextGeneration=10;
 unsigned installs=0;
 bool alive(uint32_t generation){return generation==healthyGeneration;}
 bool install(DWORD,DWORD,uint32_t,uint32_t&token,uint32_t&generation){
  ++installs;clockNow+=delay;lastError=error;
  if(!installOK)return false;
  token=nextToken;generation=nextGeneration;healthyGeneration=nextGeneration;return true;
 }
};
static Client&client(){static Client value;return value;}
}
namespace gameinput {
struct Target {
 DWORD pid=0,thread=0;HWND window=nullptr;runtime::Control*control=nullptr;
 uint32_t brokerToken=0,brokerGeneration=0;DWORD brokerRetry=0,lastPostError=0;
 bool frameSampling=false,diagnosticFrame=false,fastTactical=false;
 struct Pointer {bool enabled=false,focused=false;}pointer;
 uint64_t cursorAt=0,tacticalAt=0;
 std::shared_ptr<framesignal::Client>frameSignal;
};
struct Lease {uint64_t id=0;uint32_t client=0;unsigned target=0;};
struct Host {std::recursive_mutex mutex;Target targets[32]{};Lease leases[256]{};volatile LONG pulseBusy=0;};
static Host&host(){static Host value;return value;}
static unsigned posts=0,clears=0,polls=0,timerUpdates=0;
static uint32_t mask(uint32_t client){return client;}
static void reapTargets(){}
static void clear(Target&t){++clears;t={};}
static void pollPointer(Target&,DWORD){++polls;}
static void post(Target&t,uint32_t bits){++posts;t.control->ready|=bits;}
static void updateTimer(){++timerUpdates;}

#include HOST_BROKER_PULSE
}

static runtime::Control controls[2];
static unsigned checks=0,failures=0;
static void check(bool yes,const char*message){++checks;if(!yes){++failures;std::printf("FAIL: %s\n",message);}}
static void reset(){
 clockNow=1000;lastError=0;runtimebroker::client()={};auto&h=gameinput::host();
 h.pulseBusy=0;for(auto&t:h.targets)t={};for(auto&l:h.leases)l={};
 for(auto&c:controls){c.enabled=0;c.heartbeat=0;c.ready=0;}
 gameinput::posts=gameinput::clears=gameinput::polls=gameinput::timerUpdates=0;
}
static gameinput::Target&bind(unsigned index,uint32_t generation,uint32_t clients=3){
 auto&h=gameinput::host();auto&t=h.targets[index];
 t.pid=index+10;t.thread=index+20;t.window=reinterpret_cast<HWND>(uintptr_t(index+30));
 t.control=&controls[index];t.control->enabled=LONG(clients);t.brokerToken=50+index;t.brokerGeneration=generation;
 h.leases[index]={index+1,clients,index};return t;
}
static void pulse(){gameinput::pulse(nullptr,0);}
int main(){
 reset();auto&t=bind(0,5);pulse();
 check(runtimebroker::client().installs==1,"dead broker gets one initial recovery attempt");
 check(clockNow==4000,"fixture exposes three-second synchronous startup wait");
 check(t.brokerRetry==clockNow+1000,"broker retry starts after failed installation completes");
 check(t.brokerToken==50&&t.brokerGeneration==5,"failed install retains old token and generation");
 check(t.control->enabled==0&&t.lastPostError==126,"failed recovery stays disabled and reports Win32 error");
 check(!gameinput::posts&&!gameinput::polls,"failed recovery cannot publish work");
 check(gameinput::host().pulseBusy==0,"failed recovery releases pulse-busy latch");
 const DWORD failedAt=clockNow;
 clockNow=failedAt+4;pulse();
 check(runtimebroker::client().installs==1,"next four-millisecond pulse must not retry timed-out startup");
 check(clockNow==failedAt+4,"quiet recovery pulse does not wait on broker again");
 clockNow=failedAt+999;pulse();
 check(runtimebroker::client().installs==1,"recovery stays quiet until completion plus one second");
 clockNow=failedAt+1000;pulse();
 check(runtimebroker::client().installs==2,"retry is admitted exactly one second after failed completion");
 check(t.brokerRetry==clockNow+1000,"every failed retry gets its own post-completion interval");

 reset();auto&pending=bind(0,5);pending.brokerRetry=1500;pulse();
 check(!runtimebroker::client().installs&&pending.control->enabled==0,"existing future deadline preserves disabled state");
 clockNow=1500;runtimebroker::client().delay=0;pulse();
 check(runtimebroker::client().installs==1&&pending.brokerRetry==2500,"short failure also receives one-second retry");

 reset();auto&recovering=bind(0,5);pulse();const auto retry=recovering.brokerRetry;
 clockNow=retry;runtimebroker::client().installOK=true;runtimebroker::client().delay=1500;pulse();
 check(runtimebroker::client().installs==2,"recovery succeeds on next admitted attempt");
 check(recovering.brokerToken==77&&recovering.brokerGeneration==10,"successful install publishes new transport identity");
 check(recovering.control->enabled==3&&recovering.lastPostError==0,"successful recovery restores subscribed masks and clears error");
 check(recovering.control->ready==3&&gameinput::posts==1&&gameinput::polls==1,"successful recovery resumes cursor and tactical scheduling");
 const auto installed=runtimebroker::client().installs;clockNow+=4;pulse();
 check(runtimebroker::client().installs==installed,"healthy recovered broker is not reinstalled");

 reset();auto&healthy=bind(0,9);pulse();
 check(!runtimebroker::client().installs&&healthy.control->ready==3,"healthy broker keeps the normal pulse route");
 check(gameinput::host().pulseBusy==0&&gameinput::timerUpdates==1,"normal pulse cleanup and timer update remain intact");
 reset();auto&dead=bind(0,5);auto&other=bind(1,9,runtime::Cursor);pulse();
 check(dead.control->enabled==0&&other.control->enabled==2,"failed target cannot disable another healthy target");
 check(other.control->ready==2&&gameinput::posts==1,"another target is still serviced after bounded failed wait");
 const DWORD multiFailedAt=clockNow;clockNow=multiFailedAt+4;pulse();
 check(runtimebroker::client().installs==1,"another target does not erase failed-target retry backoff");

 reset();bind(0,5);gameinput::host().pulseBusy=1;pulse();
 check(!runtimebroker::client().installs&&gameinput::host().pulseBusy==1,"reentrant pulse does not reset another owner's busy latch");
 reset();auto&unused=bind(0,5);gameinput::host().leases[0]={};pulse();
 check(!runtimebroker::client().installs&&!unused.pid&&gameinput::clears==1,"unsubscribed target remains on cleanup route");
 reset();auto&unmapped=bind(0,5);unmapped.control=nullptr;pulse();
 check(!runtimebroker::client().installs,"unmapped target does not attempt broker recovery");

 reset();auto&wrapped=bind(0,5);clockNow=0xffffff00u;runtimebroker::client().delay=600;pulse();
 check(wrapped.brokerRetry==clockNow+1000,"retry completion deadline crosses DWORD wrap");
 const DWORD wrapFailedAt=clockNow;clockNow=wrapFailedAt+999;pulse();
 check(runtimebroker::client().installs==1,"wrapped retry stays quiet for a full second");
 clockNow=wrapFailedAt+1000;pulse();
 check(runtimebroker::client().installs==2,"wrapped deadline admits exactly one next attempt");

 std::printf("{\"checks\":%u,\"failures\":%u,\"passed\":%s,\"scope\":\"complete production pulse with deterministic host/Win32/broker substitutes; one bounded synchronous wait remains; no Windows/game validation\"}\n",checks,failures,failures?"false":"true");
 return failures?1:0;
}
