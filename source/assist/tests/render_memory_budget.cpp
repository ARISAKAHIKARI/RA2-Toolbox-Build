#include "../../external_reader/source/native_canvas.h"
#include "../../external_reader/source/native_image_bank.h"
#include <cassert>
#include <cstdio>
#include <thread>
int main(){
 using namespace external;static_assert(rendermemory::TotalBytes==(sizeof(void*)>=8?8ull*1024*1024*1024:768ull*1024*1024),"host memory target");
 auto pool=camera::NativeCanvas::sharedBudget();size_t before=pool->pages;
 {camera::NativeCanvas a,b;assert(a.reset(128,128)&&b.reset(128,128));lighting::State light;
  a.diamond(64,64,1,1,0,0xff123456,light);b.diamond(64,64,1,1,0,0xffabcdef,light);
  assert(a.residentPages()==before+2&&b.residentPages()==before+2&&pool->maximum==rendermemory::NativePages);
 }
 assert(pool->pages==before);camera::NativeCanvas::Budget limited;limited.maximum=8;std::atomic<unsigned>peak{0};std::vector<std::thread>threads;
 for(unsigned n=0;n<16;++n)threads.emplace_back([&]{for(unsigned i=0;i<1000;++i)if(limited.acquire()){auto now=limited.pages.load();assert(now<=8);auto old=peak.load();while(old<now&&!peak.compare_exchange_weak(old,unsigned(now))){}--limited.pages;}});
 for(auto&t:threads)t.join();assert(limited.pages==0&&peak>0&&peak<=8);
 std::printf("PASS architecture-aware CPU cache targets, on-demand shared native page accounting,16 concurrent admission lanes, exact release and cap; target_MiB=%llu native_pool_pages=%zu\n",(unsigned long long)(rendermemory::TotalBytes/(1024*1024)),rendermemory::NativePages);
}
