#include "../../shared/source/latest_worker.h"
#include "../../shared/source/recurring_worker.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <iostream>
#include <new>
#include <stdexcept>
using namespace std::chrono_literals;
template<class F>void eventually(F f){const auto end=std::chrono::steady_clock::now()+3s;while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
struct Gate {
 std::mutex mutex;std::condition_variable cv;bool entered=false,released=false;
 void block(){std::unique_lock<std::mutex> l(mutex);entered=true;cv.notify_all();cv.wait(l,[&]{return released;});}
 void wait(){std::unique_lock<std::mutex> l(mutex);assert(cv.wait_for(l,3s,[&]{return entered;}));}
 void release(){std::lock_guard<std::mutex> l(mutex);released=true;cv.notify_all();}
};
int main(){
 { // Real std::thread entry, standard and non-standard callback exceptions.
  asyncwork::Latest<int,int> w([](const int&i)->std::shared_ptr<const int>{if(i==1)throw std::bad_alloc();if(i==2)throw 7;return std::make_shared<const int>(i);},[]{throw std::runtime_error("notification");});
  w.submit(0);eventually([&]{return w.failures().notification==1;});assert(*w.get()==0);
  w.submit(1);eventually([&]{return w.failures().work==1;});assert(*w.get()==0);
  w.submit(2);eventually([&]{return w.failures().work==2;});assert(*w.get()==0);
  w.submit(3);eventually([&]{return w.failures().notification==2;});assert(*w.get()==3);
  const auto retained=w.get();w.invalidate();assert(!w.get());w.submit(4);
  eventually([&]{auto p=w.get();return p&&*p==4;});w.shutdown();assert(*retained==3);
  assert(w.failures().work==2&&w.failures().notification==3);
 }
 { // A throwing old task must leave the newest pending submission runnable.
  Gate gate;std::atomic<unsigned> callbacks{0};
  asyncwork::Latest<int,int> w([&](const int&i)->std::shared_ptr<const int>{if(i==1){gate.block();throw std::runtime_error("old match");}return std::make_shared<const int>(i);},[&]{++callbacks;});
  w.submit(1);gate.wait();w.submit(2);w.invalidate();w.submit(3);gate.release();
  eventually([&]{auto p=w.get();return p&&*p==3;});w.shutdown();assert(w.failures().work==1&&callbacks==1);
 }
 { // Shutdown while a callback throws: no publication, no deadlock.
  Gate gate;asyncwork::Latest<int,int> w([&](const int&)->std::shared_ptr<const int>{gate.block();throw 1;});
  w.submit(1);gate.wait();std::thread close([&]{w.shutdown();});gate.release();close.join();assert(!w.get());assert(w.failures().work==1);
 }
 { // Recurring failure rate stays bounded even while UI renews the lease/rate.
  std::atomic<unsigned> calls{0};std::atomic<bool> fail{true};
  asyncwork::Recurring<int,int> w([&](const int&i)->std::shared_ptr<const int>{++calls;if(fail)throw std::bad_alloc();return std::make_shared<const int>(i);});
  w.submit(9,1000,2s);eventually([&]{return w.failures().work==1;});
  const auto start=std::chrono::steady_clock::now();
  while(std::chrono::steady_clock::now()-start<240ms){w.submit(9,(calls.load()&1)?999:1000,2s);std::this_thread::sleep_for(1ms);}
  assert(calls>=2&&calls<=4);assert(!w.get());fail=false;eventually([&]{return bool(w.get());});assert(*w.get()==9);w.shutdown();
 }
 { // Invalidation cancels retry debt, old-world failure cannot delay new work.
  Gate gate;std::atomic<unsigned> callbacks{0};
  asyncwork::Recurring<int,int> w([&](const int&i)->std::shared_ptr<const int>{if(i==1){gate.block();throw 2;}return std::make_shared<const int>(i);},[&]{++callbacks;throw 3;});
  w.submit(1,1000,2s);gate.wait();w.invalidate();w.submit(2,1000,2s);gate.release();
  eventually([&]{return callbacks>=2;});assert(*w.get()==2);assert(w.failures().work==1&&w.failures().notification>=1);w.invalidate();w.shutdown();assert(!w.get());
 }
 { // A failing recurring task still honors its lease and stops retrying.
  asyncwork::Recurring<int,int> w([](const int&)->std::shared_ptr<const int>{throw 1;});
  w.submit(1,1000,30ms);eventually([&]{return w.failures().work==1;});std::this_thread::sleep_for(140ms);assert(w.failures().work==1);assert(!w.get());w.shutdown();
 }
 std::cout<<"PASS worker exception recovery: 6 threaded scenarios; work/notification exceptions, pending replacement, epoch invalidation, shutdown, bounded recurring retry and lease expiry; no Win32/game execution\n";
}
