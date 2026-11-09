#include "clock_events_windows.h"
#include "../../shared/source/recurring_worker.h"
#include <cstdio>
#include <stdexcept>
using namespace std::chrono_literals;
template<class F>void eventually(F f){const auto end=std::chrono::steady_clock::now()+3s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
auto event(){return std::shared_ptr<void>(CreateEventW(nullptr,FALSE,FALSE,nullptr),[](void*h){CloseHandle(h);});}
void cadence(int mode){
 winwait::mode=mode;auto signal=event();std::atomic<int> latest{0},seen{-1},calls{0};
 asyncwork::Recurring<int,int> worker([&](const int&){++calls;seen=latest.load();return std::make_shared<const int>(seen.load());},
  {},[&]{return asyncwork::SignalWait{signal,50ms};},[](int a,int b){return a!=b;});
 worker.submit(0,250,5s);eventually([&]{return seen==0;});
 // Every publication must be consumed before the next one. A retained
 // auto-reset event also catches publication immediately before entering wait.
 for(int i=1;i<=60;++i){std::this_thread::sleep_for(16ms);latest=i;SetEvent(signal.get());eventually([&]{return seen==i;});}
 const int moving=calls.load();assert(moving>=61&&moving<90);
 const int before=calls.load();std::this_thread::sleep_for(210ms);const int paused=calls.load()-before;
 assert(paused>=3&&paused<=6);
 // Identical UI renewals never execute work at the UI rate.
 const int renew=calls.load();for(int i=0;i<1000;++i)worker.submit(0,250,5s);
 assert(calls.load()-renew<=1);
 latest=99;worker.submit(1,250,5s);eventually([&]{return seen==99;});
 worker.invalidate();assert(!worker.get());latest=100;worker.submit(2,250,5s);eventually([&]{return seen==100;});
 worker.submit(2,250,1ms);eventually([&]{return !worker.get();});
 worker.shutdown();
 std::printf("{\"case\":\"signal_cadence\",\"timer_mode\":%d,\"publications\":60,\"moving_jobs\":%d,\"paused_210ms_jobs\":%d}\n",mode,moving,paused);
}
void races(){
 winwait::mode=0;auto first=event(),second=event();std::atomic<int> phase{0},calls{0},seen{-1};
 std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
 asyncwork::Recurring<int,int> worker([&](const int&i){++calls;
  if(i==0){std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});}
  seen=i;return std::make_shared<const int>(i);
 },{},[&]{return asyncwork::SignalWait{phase?second:first,50ms};},[](int a,int b){return a!=b;});
 worker.submit(0,250,5s);{std::unique_lock<std::mutex> lock(mutex);assert(cv.wait_for(lock,2s,[&]{return entered;}));}
 for(int i=0;i<100;++i)SetEvent(first.get()); // one latest-state notification
 worker.invalidate();phase=1;worker.submit(1,250,5s);
 {std::lock_guard<std::mutex> lock(mutex);release=true;cv.notify_all();}
 eventually([&]{auto value=worker.get();return value&&*value==1;});assert(calls<=3);
 winwait::failedWait=1;SetEvent(second.get());const int before=calls.load();eventually([&]{return calls>before;});
 worker.shutdown();assert(!worker.get());
 std::puts("PASS event coalescing, in-flight invalidation, new-source replacement and failed wait recovery");
}
void fallbackAndFailure(){
 std::atomic<int> jobs{0};auto signal=event();std::atomic<bool> available{false};
 asyncwork::Recurring<int,int> fallback([&](const int&i){++jobs;return std::make_shared<const int>(i);},{},[&]{return asyncwork::SignalWait{available?signal:nullptr,50ms};});
 fallback.submit(1,250,5s);eventually([&]{return jobs>=5;});available=true;SetEvent(signal.get());
 std::this_thread::sleep_for(20ms);const int before=jobs.load();std::this_thread::sleep_for(110ms);assert(jobs-before<=3);
 fallback.shutdown();
 std::atomic<int> failures{0};asyncwork::Recurring<int,int> failing([&](const int&i)->std::shared_ptr<const int>{if(i){++failures;throw std::runtime_error("injected");}return std::make_shared<const int>(0);},
  {},[&]{return asyncwork::SignalWait{signal,50ms};},[](int a,int b){return a!=b;});
 failing.submit(0,250,5s);eventually([&]{return bool(failing.get());});failing.submit(1,250,5s);eventually([&]{return failures==1;});
 for(unsigned i=0;i<30;++i){SetEvent(signal.get());std::this_thread::sleep_for(1ms);}assert(failures==1);
 eventually([&]{return failures>=2;});eventually([&]{return failing.failures().work>=2;});failing.shutdown();
 std::puts("PASS polling fallback, event recovery and notification storm respects exception backoff");
}
int main(){for(int mode=0;mode<3;++mode){cadence(mode);assert(!winwait::live);}races();assert(!winwait::live);fallbackAndFailure();assert(!winwait::live);}
