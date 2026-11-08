// Source-bound lifecycle regression: complete production Client, Host pulse,
// clear/reap and (current only) install/release/rebind. Win32 IPC/process results
// are controlled; real threads verify that a returned pulse releases Host::mutex.
#include <thread>
#define main retirementRegressionMain
#include "runtime_broker_retirement.cpp"
#undef main

#ifndef BROKER_BASELINE
namespace gameentry{enum Client:uint32_t{Assist=1,Cursor=2};}
namespace attachfixture{
struct Window{DWORD pid,thread;};
static std::map<uintptr_t,Window>windows;static std::map<DWORD,HWND>native;static std::map<DWORD,bool>deadThreads;static bool timerOK=true;
static HWND window(DWORD pid,DWORD thread,uint32_t raw){auto h=reinterpret_cast<HWND>(uintptr_t(raw));windows[raw]={pid,thread};native[pid]=h;return h;}
static void reset(){resetPulse();windows.clear();native.clear();deadThreads.clear();timerOK=true;gameinput::host().timer=nullptr;}
}
static DWORD GetWindowThreadProcessId(HWND h,DWORD*pid){auto i=attachfixture::windows.find(uintptr_t(h));if(i==attachfixture::windows.end()){*pid=0;return 0;}*pid=i->second.pid;return i->second.thread;}
static HANDLE OpenProcess(DWORD,BOOL,DWORD pid){return brokerfixture::make(brokerfixture::Kind::Game,pid);}
static HANDLE OpenThread(DWORD,BOOL,DWORD tid){auto h=brokerfixture::make(brokerfixture::Kind::Thread,tid);brokerfixture::object(h).exited=attachfixture::deadThreads[tid];return h;}
static LONG InterlockedOr(volatile LONG*p,LONG bits){const LONG before=*p;*p|=bits;return before;}
constexpr DWORD WT_EXECUTEDEFAULT=0;
static BOOL CreateTimerQueueTimer(HANDLE*out,HANDLE,void(*)(void*,BOOLEAN),void*,DWORD,DWORD,DWORD){if(!attachfixture::timerOK){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}*out=brokerfixture::make(brokerfixture::Kind::Event);return TRUE;}
namespace runtime{constexpr uint32_t Magic=0x1234,Version=4;static void mapName(wchar_t*out,DWORD pid,DWORD owner){std::swprintf(out,96,L"Fixture.Control.%u.%u",pid,owner);}}
namespace gameinput{
using Subscription=uint64_t;
static bool runtimeAvailable(){return runtimebroker::client().transport(4,176,5,64);}
static HMODULE runtimeModule(){return nullptr;}
static HWND nativeWindow(DWORD pid,DWORD thread){auto i=attachfixture::native.find(pid);if(i==attachfixture::native.end()){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}DWORD actual=0;if(GetWindowThreadProcessId(i->second,&actual)!=thread||actual!=pid){SetLastError(ERROR_INVALID_PARAMETER);return nullptr;}return i->second;}
static void disarmPointer(Target::Pointer&q){q.enabled=q.focused=false;}
#include BROKER_ATTACH
}
#endif

static bool mutexAvailable(){bool available=false;std::thread probe([&]{available=gameinput::host().mutex.try_lock();if(available)gameinput::host().mutex.unlock();});probe.join();return available;}
static unsigned positiveWaits(){unsigned count=0;for(auto value:brokerfixture::multipleWaits)count+=value!=0;for(auto value:brokerfixture::singleWaits)count+=value.milliseconds!=0;return count;}
static unsigned recoveryBusyProbes=0;static DWORD recoveryCallElapsed=0;static unsigned healthyPulses=0;
static void delayedRecovery(){using namespace brokerfixture;
 resetPulse();auto&t=bind(0,3);const auto first=multipleWaits.size();
 for(unsigned i=0;i<100;++i){now+=4;gameinput::pulse(nullptr,0);++healthyPulses;}
 check(multipleWaits.size()==first,"100 healthy production pulses issue no broker exchanges");
 object(activeBroker).exited=true;startupDelay=2990;exchangeDelay=1990;
 onMultipleWait=[&](DWORD milliseconds){if(milliseconds&&!mutexAvailable())++recoveryBusyProbes;};
 const DWORD began=now;gameinput::pulse(nullptr,0);recoveryCallElapsed=now-began;
#ifdef BROKER_BASELINE
 check(recoveryCallElapsed==4980,"baseline reproduces one 4980ms recovery pulse");
 check(recoveryBusyProbes==2,"baseline holds Host mutex in both positive-time waits");
 check(t.control->enabled==3,"baseline eventually reconnects after both waits");
#else
 check(recoveryCallElapsed==0&&t.brokerPending,"startup returns pending without advancing controlled time");
 check(t.control->enabled==0&&mutexAvailable(),"pending startup leaves target disabled and Host mutex available");
 now=began+2990;const DWORD ackBegin=now;gameinput::pulse(nullptr,0);
 check(now==ackBegin&&t.brokerPending&&exchanges.back().operation==runtimebroker::Install,"startup ACK starts one nonblocking Install request");
 check(mutexAvailable()&&t.control->enabled==0,"pending Install leaves Host mutex available and Control disabled");
 now=ackBegin+1990;gameinput::pulse(nullptr,0);
 check(!t.brokerPending&&t.control->enabled==3,"later Install ACK restores the exact subscription mask");
 check(positiveWaits()==0&&mailboxOverwrites==0,"delayed recovery uses only zero waits and never overwrites mailbox");
 const auto after=multipleWaits.size();for(unsigned i=0;i<100;++i){now+=4;gameinput::pulse(nullptr,0);++healthyPulses;}
 check(multipleWaits.size()==after,"100 recovered healthy pulses issue no broker exchanges");
#endif
 onMultipleWait={};finish(runtimebroker::client());
}
#ifndef BROKER_BASELINE
static void mailboxCases(){using namespace brokerfixture;
 {
  reset();runtimebroker::Client c;startupDelay=100;uint32_t token=0,generation=0;
  check(!c.install(42,52,62,token,generation,1)&&GetLastError()==ERROR_IO_PENDING,"initial helper startup reports pending");
  check(!token&&!generation&&now==1000,"pending startup publishes no token or generation and consumes no wait time");
  now+=50;check(!c.ensure()&&GetLastError()==ERROR_IO_PENDING&&creates==1,"repeated startup probe does not spawn another helper");
  now+=50;exchangeDelay=100;check(!c.install(42,52,62,token,generation,1)&&GetLastError()==ERROR_IO_PENDING,"startup ready advances into Install ACK wait");
  const auto sequence=packet(activeBroker)->request;const auto written=exchanges.size();uint32_t otherToken=0,otherGeneration=0;
  check(!c.install(43,53,63,otherToken,otherGeneration,2)&&GetLastError()==ERROR_IO_PENDING,"different target is queued behind current mailbox owner");
  check(exchanges.size()==written&&packet(activeBroker)->request==sequence,"competing target cannot replace the outstanding request");
  now+=100;check(!c.install(43,53,63,otherToken,otherGeneration,2)&&GetLastError()==ERROR_IO_PENDING,"another target can observe ACK without stealing its result");
  check(c.install(42,52,62,token,generation,1)&&token&&generation,"original target receives its own ACK and identity");
  check(!c.install(43,53,63,otherToken,otherGeneration,2)&&exchanges.size()==written+1,"next target advances after original owner completes");
  now+=100;check(c.install(43,53,63,otherToken,otherGeneration,2)&&otherToken!=token,"next target receives a distinct hook token");
  check(mailboxOverwrites==0&&positiveWaits()==0,"multi-target sequence contains no overwrite or positive wait");assertIdentity(activeBroker);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;c.ensure();exchangeDelay=100;
  check(!c.install(42,52,62,token,generation,101),"cancel fixture starts pending install");const auto written=exchanges.size();
  check(!c.remove(42,token,generation,101)&&GetLastError()==ERROR_IO_PENDING,"cancel retains in-flight install until ACK");
  check(exchanges.size()==written&&allOpen(activeBroker),"cancel does not overwrite or close pending mailbox");
  now+=100;check(!c.remove(42,0,0,101)&&GetLastError()==ERROR_IO_PENDING,"late cancelled ACK starts exact-token Remove");
  check(!token&&!generation&&exchanges.back().operation==runtimebroker::Remove&&exchanges.back().token==object(activeBroker).hooks[42],"cancelled install never publishes a lease and removes returned token");
  now+=100;check(c.remove(42,0,0,101)&&object(activeBroker).hooks.empty(),"cancel finishes only after Remove ACK");
  check(positiveWaits()==0&&mailboxOverwrites==0,"cancel path is nonblocking and serialized");finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;c.ensure();exchangeDelay=100;
  c.install(42,52,62,token,generation,201);now+=100;
  check(!c.install(42,54,64,token,generation,201)&&GetLastError()==ERROR_IO_PENDING,"changed HWND and thread drain previous Install before replacement");
  check(!token&&!generation&&exchanges.back().operation==runtimebroker::Remove,"old tuple ACK cannot be published for new tuple");
  now+=100;check(!c.install(42,54,64,token,generation,201)&&exchanges.back().thread==54&&exchanges.back().window==64,"replacement starts only after old token removal ACK");
  now+=100;check(c.install(42,54,64,token,generation,201)&&token==2,"replacement publishes its own new token");
  const auto oldToken=token,oldGeneration=generation;object(activeBroker).exited=true;exchangeDelay=0;check(c.install(42,54,64,token,generation),"default owner reconnects after helper exit");
  exchangeDelay=100;check(!c.install(42,54,64,token,generation),"new default-owner request can be pending");const auto requests=exchanges.size();
  check(c.remove(42,oldToken,oldGeneration)&&exchanges.size()==requests,"stale generation removal does not cancel new helper request");
  now+=100;check(c.install(42,54,64,token,generation),"new generation request survives stale removal");assertIdentity(activeBroker);finish(c);
 }
 {
  reset();runtimebroker::Client c;startup=Reply::Timeout;terminateOK=false;exitOnTerminate=false;
  check(!c.ensure()&&GetLastError()==ERROR_IO_PENDING,"hung startup initially returns pending");const DWORD began=now;HANDLE old=activeBroker;
  for(unsigned i=1;i<750;++i){now=began+i*4;c.ensure();}
  check(terminations.empty()&&creates==1&&allOpen(old),"startup keeps original 3000ms deadline across probes");
  now=began+3000;check(!c.ensure()&&GetLastError()==ERROR_TIMEOUT&&terminations.size()==1,"startup expires once at its deadline");
  const DWORD completed=now;for(unsigned i=1;i<250;++i){now=completed+i*4;c.ensure();}
  check(terminations.size()==1&&allOpen(old),"failed retirement retains exact resources for full quiet interval");
  now=completed+1000;startup=Reply::Ack;terminateOK=exitOnTerminate=true;check(c.ensure()&&creates==2,"confirmed retirement allows one fresh helper");
  uint32_t token=0,generation=0;reply=Reply::Timeout;
  check(!c.install(42,52,62,token,generation,301)&&GetLastError()==ERROR_IO_PENDING,"hung Install ACK starts pending");const DWORD ackAt=now;
  now=ackAt+1999;check(!c.install(42,52,62,token,generation,301)&&GetLastError()==ERROR_IO_PENDING,"repeated ACK probe preserves original 2000ms deadline");
  now=ackAt+2000;check(!c.install(42,52,62,token,generation,301)&&GetLastError()==ERROR_TIMEOUT,"missing ACK expires at original deadline");
  check(positiveWaits()==0&&mailboxOverwrites==0,"timeout and retirement use only zero waits");assertIdentity(old);finish(c);
 }
 {
  reset();runtimebroker::Client c;uint32_t token=0,generation=0;startupDelay=10;c.ensure();now+=4000;
  check(c.ensure(),"late host poll accepts an already-ready startup ACK");exchangeDelay=10;c.install(42,52,62,token,generation,401);now+=4000;
  check(c.install(42,52,62,token,generation,401),"late host poll accepts an already-ready matching request ACK");
  exchangeDelay=100;c.install(43,53,63,token,generation,402);
  check(!c.shutdown()&&GetLastError()==ERROR_IO_PENDING,"shutdown claims pending request without waiting");
  now+=100;uint32_t otherToken=0,otherGeneration=0;
  check(!c.install(43,53,63,otherToken,otherGeneration,402)&&GetLastError()==ERROR_IO_PENDING&&!otherToken,"shutdown intent prevents late install ACK from escaping");
  check(!c.shutdown()&&GetLastError()==ERROR_IO_PENDING,"shutdown drains outstanding request before its own ACK");
  now+=100;check(c.shutdown(),"shutdown completes after the helper exits");check(positiveWaits()==0,"shutdown performs no positive wait");assertIdentity(activeBroker);
 }
}
static void attachCases(){using namespace brokerfixture;using gameentry::Assist;using gameentry::Cursor;
 {
  attachfixture::reset();HWND w=attachfixture::window(42,52,62);exchangeDelay=100;
  auto id=gameinput::install(Assist,52,42,w);auto&t=gameinput::host().targets[0];HANDLE mapping=t.mapping,game=t.process;
  check(id&&t.brokerPending&&t.control&&t.control->enabled==0,"production install retains a real lease and disabled mapping during ACK");
  check(gameinput::host().timer&&t.brokerIntent&&mutexAvailable(),"pending attach has a pulse owner, unique intent and released mutex");
  check(gameinput::release(Assist,id)&&t.pid==42&&!object(mapping).closed&&!object(game).closed,"release during Install ACK retains both target resources");
  now+=100;gameinput::pulse(nullptr,0);check(t.pid==42&&t.control->enabled==0&&exchanges.back().operation==runtimebroker::Remove,"unused target drains late ACK into Remove without enabling");
  now+=100;gameinput::pulse(nullptr,0);check(!t.pid&&object(mapping).closed&&object(game).closed&&!object(game).exited,"Remove ACK clears retained mappings without terminating game");
  check(object(activeBroker).hooks.empty(),"cancelled attach leaves no helper hook token");assertIdentity(activeBroker);
 }
 {
  attachfixture::reset();HWND w=attachfixture::window(42,52,62);exchangeDelay=100;auto id=gameinput::install(Assist,52,42,w);auto&t=gameinput::host().targets[0];
  HWND replacement=attachfixture::window(42,52,63);check(gameinput::rebindWindow(id,replacement),"production rebind accepts current native HWND on same thread");
  now+=100;gameinput::pulse(nullptr,0);check(t.control->enabled==0&&exchanges.back().operation==runtimebroker::Remove,"rebind while pending cannot enable old-window ACK");
  now+=100;gameinput::pulse(nullptr,0);check(t.control->enabled==0&&exchanges.back().window==63,"replacement native window is sent only after Remove ACK");
  now+=100;gameinput::pulse(nullptr,0);check(!t.brokerPending&&t.control->enabled==1,"rebound window receives its own successful ACK");
  auto second=gameinput::install(Cursor,52,42,replacement);check(second&&t.control->enabled==3,"healthy second subscriber reuses installed hook pair");
  gameinput::release(Assist,id);check(t.control->enabled==2,"release preserves remaining healthy subscriber mask");
  finish(runtimebroker::client());
 }
 {
  attachfixture::reset();HWND w=attachfixture::window(42,52,62);auto oldLease=gameinput::install(Assist,52,42,w);auto&t=gameinput::host().targets[0];HANDLE oldGame=t.process,oldMap=t.mapping;const auto oldIntent=t.brokerIntent;
  object(oldGame).exited=true;exchangeDelay=100;HWND samePid=attachfixture::window(42,52,63);
  auto rejected=gameinput::install(Cursor,52,42,samePid);
  check(!rejected&&GetLastError()==ERROR_IO_PENDING,"reused PID cannot acquire a lease on a retired process handle");
  check(t.process==oldGame&&t.brokerIntent==oldIntent&&!object(oldMap).closed,"PID reuse retains old identity until asynchronous Remove ACK");
  check(!gameinput::host().leases[0].id&&oldLease,"exited game's old lease is revoked before new admission");
  now+=100;auto fresh=gameinput::install(Cursor,52,42,samePid);
  check(fresh&&t.process!=oldGame&&t.brokerIntent!=oldIntent&&object(oldMap).closed,"post-retirement PID reuse receives fresh process handle and intent");
  now+=100;gameinput::pulse(nullptr,0);check(t.control->enabled==2&&gameinput::host().leases[0].id==fresh,"new PID lease survives subsequent process-reaping pulse");
  check(!wrongTerminates&&!mailboxOverwrites&&positiveWaits()==0,"PID replacement is serialized without killing game or waiting");finish(runtimebroker::client());
 }
 {
  attachfixture::reset();HWND w=attachfixture::window(42,52,62);auto oldLease=gameinput::install(Assist,52,42,w);auto&t=gameinput::host().targets[0];const auto oldIntent=t.brokerIntent;const auto oldMap=t.mapping;exchangeDelay=100;
  check(gameinput::release(Assist,oldLease)&&t.pid&&t.control->enabled==0,"last release retains target while Remove ACK is pending");
  auto early=gameinput::install(Assist,52,42,w);check(!early&&GetLastError()==ERROR_IO_PENDING&&t.brokerIntent==oldIntent,"quick same-process reattach cannot revive retiring token");
  const auto requests=exchanges.size();gameinput::pulse(nullptr,0);check(exchanges.size()==requests&&t.control->enabled==0,"retiring target neither overwrites mailbox nor republishes work");
  now+=100;auto fresh=gameinput::install(Assist,52,42,w);check(fresh&&t.brokerIntent!=oldIntent&&object(oldMap).closed&&t.control->enabled==0,"quick reattach gets new target identity after Remove ACK");
  now+=100;gameinput::pulse(nullptr,0);check(t.control->enabled==1&&!t.brokerPending,"reattached subscription consumes its own new Install ACK");
  HWND other=attachfixture::window(43,53,63);auto otherLease=gameinput::install(Cursor,53,43,other);now+=100;gameinput::pulse(nullptr,0);check(otherLease&&gameinput::host().targets[1].control->enabled==2,"completed reattach leaves mailbox available for other targets");
  check(!mailboxOverwrites&&positiveWaits()==0,"quick-toggle sequence contains no mailbox overwrite or positive wait");finish(runtimebroker::client());
 }
 {
  attachfixture::reset();HWND w=attachfixture::window(42,52,62);auto id=gameinput::install(Assist,52,42,w);auto&t=gameinput::host().targets[0];const auto intent=t.brokerIntent;
  object(t.process).waitFailed=true;check(!gameinput::install(Cursor,52,42,w)&&GetLastError()==ERROR_ACCESS_DENIED,"uncertain retained process status cannot admit another subscriber");object(t.process).waitFailed=false;
  attachfixture::deadThreads[52]=true;HWND next=attachfixture::window(42,53,63);exchangeDelay=100;auto nextLease=gameinput::install(Cursor,53,42,next);
  check(id&&nextLease&&t.thread==53&&t.brokerIntent==intent&&t.control->enabled==0,"exited thread replacement retains target while awaiting new ACK");
  now+=100;gameinput::pulse(nullptr,0);check(t.control->enabled==3&&!t.brokerPending,"new thread restores both existing subscriber masks");
  object(activeBroker).exited=true;exchangeDelay=0;reply=Reply::RemoteError;terminateOK=false;exitOnTerminate=false;
  check(!gameinput::install(Cursor,53,42,next)&&GetLastError()==ERROR_ACCESS_DENIED,"actual attach error remains a failure for new caller");
  check(t.control->enabled==0&&t.brokerRetry==now+1000,"actual attach error disables existing leases and applies completion backoff");
  const auto writes=exchanges.size();now+=4;gameinput::pulse(nullptr,0);check(exchanges.size()==writes,"next pulse respects attach failure backoff");finish(runtimebroker::client());
 }
 {
  attachfixture::reset();HWND w=attachfixture::window(42,52,62);attachfixture::timerOK=false;exchangeDelay=100;
  check(!gameinput::install(Assist,52,42,w)&&GetLastError()==ERROR_ACCESS_DENIED,"timer creation failure returns no subscription");auto&t=gameinput::host().targets[0];const auto map=t.mapping;
  check(t.pid&&t.control&&t.control->enabled==0&&!object(map).closed,"timer failure retains pending install resources for cancellation");
  attachfixture::timerOK=true;now+=100;auto id=gameinput::install(Assist,52,42,w);
  check(!id&&GetLastError()==ERROR_IO_PENDING&&t.control->enabled==0,"later attach waits for cancellation to finish before admission");
  now+=100;id=gameinput::install(Assist,52,42,w);check(id&&t.control->enabled==0&&exchanges.back().operation==runtimebroker::Install,"retry starts only after cancelled token removal ACK");
  now+=100;gameinput::pulse(nullptr,0);check(t.control->enabled==1&&!t.brokerPending,"retry recovers after original timer failure");check(mailboxOverwrites==0&&positiveWaits()==0,"failed-owner cleanup never overwrites mailbox or waits");finish(runtimebroker::client());
 }
}
#endif
int main(){delayedRecovery();
#ifndef BROKER_BASELINE
 mailboxCases();attachCases();
#endif
 std::printf("{\"checks\":%u,\"failures\":%u,\"recovery_call_virtual_ms\":%u,\"positive_wait_mutex_busy_probes\":%u,\"healthy_pulses\":%u}\n",checks,failures,recoveryCallElapsed,recoveryBusyProbes,healthyPulses);return failures?1:0;
}
