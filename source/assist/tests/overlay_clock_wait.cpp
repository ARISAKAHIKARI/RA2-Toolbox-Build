#include "../../shared/source/deadline_wake.h"
#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>
static unsigned begins=0,ends=0;static bool reject=false;
constexpr unsigned TIMERR_NOERROR=0;
unsigned timeBeginPeriod(unsigned n){assert(n==1);++begins;return reject?1:0;}
unsigned timeEndPeriod(unsigned n){assert(n==1);++ends;return 0;}
#include "../../shared/source/timer_resolution.h"
using namespace std::chrono_literals;
int main(){
 {timerresolution::Lease lease;for(int n=0;n<1000;++n)lease.state(false);assert(!begins&&!ends);
  reject=true;lease.state(true);lease.state(false);assert(begins==1&&!ends);reject=false;
  for(int n=0;n<1000;++n)lease.state(true);assert(begins==2&&!ends);
  lease.state(false);assert(ends==1);lease.state(false);assert(ends==1);lease.state(true);
 }assert(begins==3&&ends==2);
 asyncwork::DeadlineWake wake;std::mutex mutex;bool ready=false;std::atomic<bool> entered{false},returned{false};
 std::thread idle([&]{std::unique_lock<std::mutex>lock(mutex);entered=true;wake.wait(lock,[&]{return ready;});returned=true;});
 while(!entered)std::this_thread::yield();std::this_thread::sleep_for(25ms);assert(!returned);
 {std::lock_guard<std::mutex>lock(mutex);ready=true;wake.notify_one();}idle.join();assert(returned);
 ready=false;returned=false;entered=false;
 std::thread deadline([&]{std::unique_lock<std::mutex>lock(mutex);entered=true;assert(wake.wait_until(lock,std::chrono::steady_clock::now()+3s,[&]{return ready;}));returned=true;});
 while(!entered)std::this_thread::yield();{std::lock_guard<std::mutex>lock(mutex);ready=true;wake.notify_one();}deadline.join();assert(returned);
 ready=false;std::unique_lock<std::mutex>lock(mutex);assert(!wake.wait_until(lock,std::chrono::steady_clock::now()+2ms,[&]{return ready;}));
 std::puts("PASS overlay wait: idle parks until publication/stop; deadline interrupted by revision; timer expiry returns; precision acquire/release and failure balanced; no Windows timing claim");
}
