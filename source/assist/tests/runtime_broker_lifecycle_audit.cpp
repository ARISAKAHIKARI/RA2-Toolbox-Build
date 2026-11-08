// Executes the production Client and pulse through the existing deterministic
// Win32 boundary. Virtual delay is evidence of lock scope, not measured FPS.
#include <thread>
#define main retirementRegressionMain
#include "runtime_broker_retirement.cpp"
#undef main

int main(){
 using namespace brokerfixture;
 resetPulse();bind(0,3);
 const auto healthyWaitStart=multipleWaits.size();
 for(unsigned i=0;i<100;++i){now+=4;gameinput::pulse(nullptr,0);}
 const auto healthyWaits=multipleWaits.size()-healthyWaitStart;
 check(healthyWaits==0,"healthy pulses never enter startup/exchange waits");

 object(activeBroker).exited=true;
 startupDelay=2990;exchangeDelay=1990;
 unsigned busyProbes=0,freeProbes=0;
 std::vector<DWORD> observedDeadlines;
 onMultipleWait=[&](DWORD deadline){
  observedDeadlines.push_back(deadline);
  // The real mutex is checked from another thread, without touching any of
  // the fixture's process/mailbox state from that thread.
  bool available=false;
  std::thread probe([&]{available=gameinput::host().mutex.try_lock();if(available)gameinput::host().mutex.unlock();});
  probe.join();if(available)++freeProbes;else ++busyProbes;
 };
 const DWORD begin=now;
 gameinput::pulse(nullptr,0);
 const DWORD delayedRecovery=now-begin;
 onMultipleWait={};
 check(observedDeadlines==std::vector<DWORD>({3000,2000}),"recovery reaches both bounded production waits");
 check(busyProbes==2&&freeProbes==0,"shared host mutex remains held at both waits");
 check(delayedRecovery==4980,"controlled delayed acknowledgements advance virtual time by 4980ms");
 check(controls[0].enabled==3&&!gameinput::host().pulseBusy,"successful recovery restores both subscribers and releases latch");
 const auto afterRecovery=multipleWaits.size();
 for(unsigned i=0;i<100;++i){now+=4;gameinput::pulse(nullptr,0);}
 check(multipleWaits.size()==afterRecovery,"positive waits stop once recovery succeeds");

 resetPulse();auto&t=bind(0,3);object(activeBroker).exited=true;startup=Reply::Timeout;
 const DWORD beforeFailure=now;gameinput::pulse(nullptr,0);
 const DWORD timeoutDelay=now-beforeFailure;
 check(timeoutDelay==3000&&t.lastPostError==ERROR_TIMEOUT,"startup timeout retains its actual error and bounded duration");
 const auto failureWaits=multipleWaits.size();
 for(unsigned i=0;i<249;++i){now+=4;gameinput::pulse(nullptr,0);}
 check(multipleWaits.size()==failureWaits,"completion-based quiet interval prevents a wait on every pulse");
 const unsigned quietWaits=unsigned(multipleWaits.size()-failureWaits);
 finish(runtimebroker::client());
 std::printf("{\"checks\":%u,\"failures\":%u,\"healthy_pulses\":100,\"healthy_positive_waits\":%zu,\"recovery_virtual_ms\":%u,\"host_mutex_busy_probes\":%u,\"host_mutex_free_probes\":%u,\"startup_timeout_virtual_ms\":%u,\"quiet_interval_pulses\":249,\"quiet_interval_positive_waits\":%u}\n",checks,failures,healthyWaits,delayedRecovery,busyProbes,freeProbes,timeoutDelay,quietWaits);
 return failures?1:0;
}
