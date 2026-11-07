// The real Client and full production pulse/clear/reap functions execute above
// deterministic Win32 and presentation boundaries. No operating-system process
// is accessed; the broker and game are distinct tracked fixture identities.
#include "runtime_broker_retirement_fixture.h"
#include <mutex>
#include <cstdio>
#include BROKER_CLIENT

namespace preciseclock{static uint64_t micros(){return uint64_t(brokerfixture::now)*1000;}}
namespace runtime{constexpr uint32_t Tactical=1,Cursor=2;struct Control{volatile LONG enabled=0;DWORD heartbeat=0;uint32_t ready=0;};}
namespace framesignal{constexpr uint32_t Compute=1,ExplicitDiagnostic=2,Native=1;struct Stamp{uint32_t state=0,serial=0;int64_t frequency=0,qpc=0;};struct Client{bool open(DWORD,uint32_t){return false;}void heartbeat(){}bool current(Stamp&){return false;}};}
namespace gameinput{
struct Target{DWORD pid=0,thread=0;HWND window=nullptr;runtime::Control*control=nullptr;HANDLE mapping=nullptr,process=nullptr;HHOOK post=nullptr,message=nullptr;
 uint32_t brokerToken=0,brokerGeneration=0;DWORD brokerRetry=0,lastPostError=0;bool frameSampling=false,diagnosticFrame=false,fastTactical=false;
 struct Pointer{bool enabled=false,focused=false;}pointer;uint64_t cursorAt=0,tacticalAt=0;std::shared_ptr<framesignal::Client>frameSignal;};
struct Lease{uint64_t id=0;uint32_t client=0;unsigned target=0;};struct Host{std::recursive_mutex mutex;Target targets[32]{};Lease leases[256]{};volatile LONG pulseBusy=0;};
static Host&host(){static Host h;return h;}static unsigned posts=0,polls=0,timerUpdates=0;
static uint32_t mask(uint32_t client){return client;}static bool releaseHook(HHOOK&hook){hook=nullptr;return true;}
static void pollPointer(Target&,DWORD){++polls;}static void post(Target&t,uint32_t bits){++posts;t.control->ready|=bits;}static void updateTimer(){++timerUpdates;}
#include BROKER_HOST
}
static unsigned checks=0,failures=0,clientChecks=0,pulseChecks=0;
static void check(bool ok,const char*text){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",text);}}
static void finish(runtimebroker::Client&c){for(HANDLE h:brokerfixture::brokers)brokerfixture::object(h).exited=true;brokerfixture::reply=brokerfixture::Reply::Ack;for(HANDLE h:brokerfixture::brokers)brokerfixture::object(h).waitFailed=false;c.shutdown();}
static bool install(runtimebroker::Client&c,uint32_t&token,uint32_t&generation,DWORD pid=42){return c.install(pid,pid+1,pid+2,token,generation);}
static void assertIdentity(HANDLE old){using namespace brokerfixture;check(!wrongTerminates,"only the retained child broker HANDLE is terminated");check(!ownerMismatches,"owner HANDLE and packet retain the real host identity");check(!overlappingCreates,"no replacement broker exists before confirmed old exit");check(!liveBrokerCloses,"no live or unknown broker process HANDLE is closed");if(old)for(const auto&t:terminations)check(object(t.handle).kind==Kind::Broker,"each termination targets a created helper, never a game/owner");}
static void clientCases(){using namespace brokerfixture;
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;check(install(c,token,generation),"initial install succeeds");HANDLE old=activeBroker;auto*mailbox=packet(old);const auto original=inheritance.front();const auto oldGeneration=generation,oldToken=token;
  check(mailbox->owner==hostPID&&GetProcessId(original[0])==hostPID,"inherited owner process and mailbox use host identity");
  terminateOK=false;exitOnTerminate=false;terminateDelay=250;reply=Reply::Timeout;
  check(!install(c,token,generation),"timeout rejects the in-flight operation");check(GetLastError()==ERROR_TIMEOUT,"first abort reports original exchange timeout");check(terminations.size()==1,"abort attempts one initial retirement");check(!c.alive(oldGeneration),"poisoned broker is never alive for a subscription");check(token==oldToken&&generation==oldGeneration,"failed install leaves subscription token and generation unchanged");
  check(allOpen(old),"all old mailbox and process handles remain retained while live");check(!c.hookRva()&&!c.moduleBase(),"poisoned metadata is not exposed as usable runtime");const auto written=exchanges.size();const DWORD completed=now;reply=Reply::Ack;
  for(DWORD delta:{0u,4u,500u,999u}){now=completed+delta;check(!c.ensure(),"live poisoned helper blocks replacement during retirement interval");check(GetLastError()==ERROR_ACCESS_DENIED,"retirement preserves actual termination failure");check(terminations.size()==1,"retirement attempts are throttled from completion time");check(exchanges.size()==written,"poisoned mailbox is never reused");check(allOpen(old),"throttled failure keeps old ownership resources");}
  now=completed+1000;terminateOK=exitOnTerminate=true;check(install(c,token,generation),"a later termination retry recovers from permanent busy");check(terminations.size()==2&&terminations.back().handle==old,"retry targets the exact original retained HANDLE");check(creates==2&&generation!=oldGeneration,"new generation starts only after old helper exited");check(inheritance.back()!=original&&packet(activeBroker)!=mailbox,"replacement uses a fresh private mailbox and handles");check(c.alive(generation)&&!c.alive(oldGeneration),"old generation remains invalid after replacement");check(c.remove(42,oldToken,oldGeneration),"old-generation removal cannot affect new helper");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;check(install(c,token,generation),"query-failure fixture starts healthy");HANDLE old=activeBroker;const auto written=exchanges.size();object(old).waitFailed=true;object(old).waitError=ERROR_ACCESS_DENIED;
  check(!c.ensure(),"WAIT_FAILED is not treated as confirmed process exit");check(GetLastError()==ERROR_ACCESS_DENIED,"ensure reports the actual failed-wait error");check(creates==1&&allOpen(old),"failed wait does not release handles or create a second helper");
  check(!c.remove(42,token,generation),"failed wait cannot falsely complete hook removal");check(GetLastError()==ERROR_ACCESS_DENIED,"remove preserves failed-wait diagnostics");check(!c.shutdown(),"failed wait cannot falsely complete shutdown");check(exchanges.size()==written&&terminations.empty(),"unknown status neither reuses mailbox nor terminates another process");
  object(old).waitFailed=false;check(c.ensure()&&c.alive(generation),"transient healthy status-query failure can recover in place");check(creates==1,"query recovery does not replace a healthy helper");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;
  reply=Reply::Timeout;terminateOK=true;exitOnTerminate=false;check(!install(c,token,generation),"asynchronous termination can remain pending");check(terminations.size()==1,"pending exit gets initial termination");const DWORD start=now;const auto written=exchanges.size(),waits=singleWaits.size();reply=Reply::Ack;
  for(unsigned i=1;i<=250;++i){now=start+i*4;c.ensure();}check(terminations.size()==2,"250 four-millisecond calls make only one admitted retry");check(GetLastError()==ERROR_BUSY,"successful termination request with pending exit reports busy");check(creates==1&&allOpen(old)&&exchanges.size()==written,"late exit retains identity and never starts a parallel helper");
  bool zero=true;for(size_t i=waits;i<singleWaits.size();++i)zero&=singleWaits[i].milliseconds==0;check(zero,"retirement never waits one second inside the host mutex");
  object(old).waitFailed=true;now+=1000;const auto attempts=terminations.size();check(!c.ensure(),"poisoned WAIT_FAILED retains uncertainty");check(!c.remove(42,token,generation),"poisoned failed wait does not pretend old hooks are gone");check(terminations.size()==attempts&&allOpen(old),"failed wait preserves handle without unsafe retirement attempt");
  object(old).waitFailed=false;object(old).exited=true;check(install(c,token,generation),"later confirmed exit permits clean rebuild");check(creates==2,"observed exit produces exactly one replacement");
  reply=Reply::Timeout;exitOnTerminate=false;const auto before=terminations.size();check(!install(c,token,generation),"fresh generation can independently fail");check(terminations.size()==before+1,"confirmed exit resets retirement throttle for the replacement");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;now=0xfffffff0u;reply=Reply::BadSequence;terminateOK=false;exitOnTerminate=false;terminateDelay=25;
  check(!install(c,token,generation),"bad sequence poisons current mailbox across clock rollover");check(GetLastError()==ERROR_INVALID_DATA,"bad sequence retains protocol diagnostic");const DWORD done=now;reply=Reply::Ack;now=done+999;check(!c.ensure()&&terminations.size()==1,"rollover retains complete 999ms quiet interval");now=done+1000;terminateOK=exitOnTerminate=true;check(c.ensure()&&terminations.size()==2,"rollover allows retry at exactly one second");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;reply=Reply::Timeout;terminateOK=false;exitOnTerminate=false;
  bool nestedShutdown=true,nestedEnsure=true,nestedRemove=true;onTerminate=[&]{now+=1200;nestedShutdown=c.shutdown();nestedEnsure=c.ensure();nestedRemove=c.remove(42,token,generation);};
  check(!install(c,token,generation),"reentrant abort still reports original failed operation");check(!nestedShutdown&&!nestedEnsure&&!nestedRemove,"reentrant shutdown ensure and remove cannot close or reuse in-flight handle");check(terminations.size()==1&&allOpen(old),"reentrant retirement cannot issue another termination");const DWORD completed=now;const auto written=exchanges.size();reply=Reply::Ack;
  check(!c.shutdown()&&!c.shutdown(),"repeated poisoned shutdown reports pending cleanup");check(terminations.size()==1&&exchanges.size()==written,"repeated shutdown shares retirement throttle and never exchanges old mailbox");now=completed+1000;terminateOK=exitOnTerminate=true;check(c.shutdown(),"later shutdown retry completes retirement");check(c.shutdown(),"completed shutdown is idempotent");check(creates==1,"shutdown never spawns a replacement");check(install(c,token,generation),"new explicit install can start after completed shutdown");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;shutdownDelay=500;const DWORD began=now;check(c.shutdown(),"healthy shutdown preserves one bounded graceful wait");check(now-began==500&&terminations.empty(),"graceful helper exit avoids forced retirement");check(c.shutdown(),"healthy close can be repeated safely");assertIdentity(old);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;shutdownNeverExits=true;terminateOK=false;exitOnTerminate=false;
  check(!c.shutdown(),"graceful shutdown timeout enters retirement");const auto waits=singleWaits.size(),written=exchanges.size();for(unsigned i=0;i<20;++i){now+=4;c.shutdown();}
  bool zero=true;for(size_t i=waits;i<singleWaits.size();++i)zero&=singleWaits[i].milliseconds==0;check(zero,"repeated failed shutdown does not repeat a one-second wait");check(exchanges.size()==written&&terminations.size()==1,"shutdown retries cannot resend poisoned mailbox or flood termination");object(old).exited=true;check(c.shutdown(),"natural exit completes previously timed-out shutdown");assertIdentity(old);
 }
 {
  reset();runtimebroker::Client c;startup=Reply::Timeout;terminateOK=false;exitOnTerminate=false;check(!c.ensure(),"startup timeout retains failed helper for retirement");HANDLE old=activeBroker;check(allOpen(old)&&creates==1,"failed startup retains exact process and inherited ownership");check(!c.ensure()&&terminations.size()==1,"abort and ensure in the same cycle do not repeat termination");startup=Reply::Ack;now+=1000;terminateOK=exitOnTerminate=true;check(c.ensure(),"startup failure is recoverable by throttled retirement");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;HANDLE unrelated=make(Kind::Game,GetProcessId(old));reply=Reply::Timeout;terminateOK=false;exitOnTerminate=false;install(c,token,generation);reply=Reply::Ack;now+=1000;terminateOK=exitOnTerminate=true;
  check(c.ensure(),"retirement survives unrelated process PID reuse");check(terminations.back().handle==old&&!object(unrelated).closed&&!object(unrelated).exited,"retained HANDLE protects game and unrelated same-PID object");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;install(c,token,generation);HANDLE old=activeBroker;reply=Reply::Failed;terminateOK=false;exitOnTerminate=false;check(!install(c,token,generation),"failed exchange wait enters retirement");check(GetLastError()==ERROR_ACCESS_DENIED,"exchange WAIT_FAILED keeps system error rather than broken pipe");reply=Reply::Ack;now+=1000;exitDespiteFailure=true;check(c.ensure(),"confirmed exit wins even if TerminateProcess reports failure");assertIdentity(old);finish(c);
 }
 clientChecks=checks;
}
static runtime::Control controls[2];
static void resetPulse(){using namespace brokerfixture;finish(runtimebroker::client());reset();auto&h=gameinput::host();h.pulseBusy=0;for(auto&t:h.targets)t={};for(auto&l:h.leases)l={};for(auto&c:controls)c={};gameinput::posts=gameinput::polls=gameinput::timerUpdates=0;}
static gameinput::Target&bind(unsigned index,uint32_t clients){using namespace brokerfixture;auto&h=gameinput::host();auto&t=h.targets[index];t.pid=42+index;t.thread=52+index;t.window=reinterpret_cast<HWND>(uintptr_t(62+index));t.process=make(Kind::Game,t.pid);t.control=&controls[index];t.control->enabled=LONG(clients);runtimebroker::client().install(t.pid,t.thread,uint32_t(uintptr_t(t.window)),t.brokerToken,t.brokerGeneration);h.leases[index]={index+1,clients,index};return t;}
static void pulseCases(){using namespace brokerfixture;
 resetPulse();auto&a=bind(0,3);auto&b=bind(1,2);HANDLE old=activeBroker;const auto ga=a.brokerGeneration,gb=b.brokerGeneration,ta=a.brokerToken,tb=b.brokerToken;uint32_t token=ta,generation=ga;
 reply=Reply::Timeout;terminateOK=false;exitOnTerminate=false;install(runtimebroker::client(),token,generation);reply=Reply::Ack;const DWORD completed=now;const auto written=exchanges.size(),waits=singleWaits.size();
 gameinput::pulse(nullptr,0);check(a.control->enabled==0&&b.control->enabled==0,"actual pulse disables all subscriptions while old broker is poisoned");check(a.brokerToken==ta&&a.brokerGeneration==ga&&b.brokerToken==tb&&b.brokerGeneration==gb,"failed pulse recovery retains each old subscription identity");check(a.lastPostError==ERROR_ACCESS_DENIED&&b.lastPostError==ERROR_ACCESS_DENIED,"actual pulse retains retirement diagnostic for each target");check(a.brokerRetry==completed+1000&&b.brokerRetry==completed+1000,"pulse keeps completion-based one-second retry");
 for(unsigned i=1;i<250;++i){now=completed+i*4;gameinput::pulse(nullptr,0);}
 check(terminations.size()==1&&creates==1&&exchanges.size()==written,"250 actual pulses do not retry termination or reuse poisoned mailbox");check(!gameinput::posts&&!gameinput::polls,"blocked subscriptions publish no stale work or pointer samples");bool zero=true;for(size_t i=waits;i<singleWaits.size();++i)zero&=singleWaits[i].milliseconds==0;check(zero,"poisoned actual pulse performs no positive-time process waits");
 now=completed+1000;terminateDelay=80;gameinput::pulse(nullptr,0);check(terminations.size()==2,"multiple subscribers share one global retirement retry");check(a.brokerRetry==now+1000,"pulse retry starts after termination attempt completes");
 terminateOK=exitOnTerminate=true;terminateDelay=0;now=a.brokerRetry;gameinput::pulse(nullptr,0);check(creates==2,"actual pulse starts exactly one replacement after exit");check(a.brokerGeneration!=ga&&b.brokerGeneration!=gb,"both subscriptions rebind to the new broker generation");check(a.control->enabled==3&&b.control->enabled==2&&a.lastPostError==0&&b.lastPostError==0,"subscription masks and clear diagnostics are restored");check(gameinput::posts==2&&gameinput::polls==1,"only restored subscriptions resume their requested cadence");check(!gameinput::host().pulseBusy,"actual pulse always releases its reentry latch");assertIdentity(old);
 // Last-lease cleanup must retain a target until the poisoned broker is gone.
 resetPulse();auto&idle=bind(0,1);old=activeBroker;const auto savedGame=idle.process;token=idle.brokerToken;generation=idle.brokerGeneration;reply=Reply::Timeout;terminateOK=false;exitOnTerminate=false;install(runtimebroker::client(),token,generation);reply=Reply::Ack;gameinput::host().leases[0]={};
 gameinput::pulse(nullptr,0);check(idle.pid==42&&idle.process==savedGame&&idle.control->enabled==0,"actual clear/reap retain target ownership until retirement is confirmed");check(!object(savedGame).closed,"pending clear retains game process reference without killing it");check(terminations.size()==1,"reap and pulse cleanup share the same retirement interval");now+=1000;terminateOK=exitOnTerminate=true;gameinput::pulse(nullptr,0);check(idle.pid==0&&object(savedGame).closed&&!object(savedGame).exited,"confirmed retirement releases only target handles, not the game process");check(creates==1&&!gameinput::posts,"cleanup does not start a replacement without subscribers");assertIdentity(old);finish(runtimebroker::client());pulseChecks=checks-clientChecks;
}
int main(){clientCases();pulseCases();std::printf("{\"checks\":%u,\"failures\":%u,\"client_checks\":%u,\"host_pulse_checks\":%u,\"actual_pulses_in_quiet_window\":250}\n",checks,failures,clientChecks,pulseChecks);return failures?1:0;}
