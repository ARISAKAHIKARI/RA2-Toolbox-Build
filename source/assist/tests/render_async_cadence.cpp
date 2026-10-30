#include "../../shared/source/recurring_worker.h"
#include "../source/render_presentation.h"
#include <atomic>
#include <cassert>
#include <cstdio>
template<class F>void eventually(F f){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);while(!f()){assert(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
int main(){
 assert(renderpresentation::ObservationRate==1000);
 std::atomic<int> reads{0},renders{0};std::mutex mutex;std::condition_variable cv;bool entered=false,release=false;
 asyncwork::Recurring<int,int> sampler([&](const int&){++reads;return std::make_shared<const int>(7);});
 asyncwork::Recurring<int,int> renderer([&](const int&){
  auto state=sampler.get();if(!state)return std::shared_ptr<const int>{};
  if(renders.fetch_add(1)==0){std::unique_lock<std::mutex>lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});}
  return state;
 });
 sampler.submit(0,1000);eventually([&]{return bool(sampler.get());});renderer.submit(0,500);
 {std::unique_lock<std::mutex>lock(mutex);assert(cv.wait_for(lock,std::chrono::seconds(2),[&]{return entered;}));}
 const int before=reads.load();eventually([&]{return reads.load()>=before+12;});assert(!renderer.get());
 {std::lock_guard<std::mutex>lock(mutex);release=true;cv.notify_all();}
 eventually([&]{return renders.load()>=5&&bool(renderer.get());});assert(*renderer.get()==7);
 // Repeated draws need neither a changed value nor a simulated game-frame
 // event. A slow renderer cannot block the independent memory reader.
 renderer.invalidate();assert(!renderer.get());renderer.shutdown();sampler.shutdown();
 asyncwork::Recurring<int,int> lease([](const int&i){return std::make_shared<const int>(i);});
 lease.submit(9,5,std::chrono::milliseconds(30));eventually([&]{return bool(lease.get());});
 eventually([&]{return !lease.get();});lease.shutdown();
 std::puts("PASS independent sampling during blocked rendering, repeated unchanged-state draws, invalidation and lease expiry; scheduling contract, not Windows frequency measurement");
}
