#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../shared/source/parallel_rows.h"
#include <array>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <stdexcept>
int main(){
 parallelrows::Pool team(2);std::mutex mutex;std::condition_variable changed;bool release=false;std::array<bool,2>assisted{{false,false}};
 std::atomic<int>ready{0};auto owner=[&](int id){const auto caller=std::this_thread::get_id();++ready;while(ready.load()!=2)std::this_thread::yield();
  team.rows(4096,1024*1024,[&](size_t,size_t){std::unique_lock<std::mutex>lock(mutex);
   if(std::this_thread::get_id()==caller)changed.wait(lock,[&]{return release;});else{assisted[id]=true;changed.notify_all();}
  });
 };
 std::thread a(owner,0),b(owner,1);bool both=false;{
  std::unique_lock<std::mutex>lock(mutex);both=changed.wait_for(lock,std::chrono::seconds(5),[&]{return assisted[0]&&assisted[1];});release=true;changed.notify_all();
 }a.join();b.join();assert(both);
 std::atomic<unsigned>completed{0};std::vector<std::thread>callers;
 for(unsigned id=0;id<6;++id)callers.emplace_back([&,id]{for(unsigned frame=0;frame<100;++frame){std::array<unsigned,512>rows{};
  team.rows(rows.size(),1024*1024,[&](size_t first,size_t last){for(size_t y=first;y<last;++y)rows[y]=unsigned(y)^id^frame;
   team.rows(64,1024*1024,[](size_t,size_t){});
  });for(size_t y=0;y<rows.size();++y)assert(rows[y]==(unsigned(y)^id^frame));++completed;
 }});for(auto&t:callers)t.join();assert(completed==600);
 std::atomic<int>active{0};for(int pass=0;pass<40;++pass){bool caught=false;try{
  team.rows(256,1024*1024,[&](size_t first,size_t){struct Guard{std::atomic<int>&n;Guard(std::atomic<int>&x):n(x){++n;}~Guard(){--n;}}guard(active);if(first==0)throw std::runtime_error("injected callback failure");});
 }catch(const std::runtime_error&){caught=true;}assert(caught&&active==0);
 std::array<unsigned,128>rows{};team.rows(rows.size(),1024*1024,[&](size_t first,size_t last){for(size_t y=first;y<last;++y)++rows[y];});for(auto n:rows)assert(n==1);
 }
 std::puts("PASS shared row team: both owners receive worker assistance,600 concurrent exact submissions,nested calls and40 callback failures fully retired before reuse");
}
