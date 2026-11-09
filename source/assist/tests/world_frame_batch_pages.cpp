#include "../../shared/source/world_frame.h"
#include <cassert>
#include <cstdio>
using namespace worldframe;
struct Memory {
 size_t calls=0;uint32_t denied=0;std::vector<uint32_t> pages;
 bool operator()(uint32_t at,void*out,size_t n){
  ++calls;assert(range(at,n));if(denied&&at<=denied&&uint64_t(at)+n>denied){std::memset(out,0xEF,n);return false;}
  assert(!(at&4095)&&!(n&4095));for(size_t i=0;i<n;i+=PageSize){const auto page=at+uint32_t(i);assert(std::find(pages.begin(),pages.end(),page)==pages.end());pages.push_back(page);std::memset(static_cast<uint8_t*>(out)+i,uint8_t(page>>12),PageSize);}return true;
 }
};
void compare(Snapshot&a,Snapshot&b){assert(a.pages.size()==b.pages.size());for(size_t i=0;i<a.pages.size();++i){auto*p=b.find(a.pages[i].address);assert(p&&!std::memcmp(p,&a.pages[i],sizeof(Page)));}}
struct Budget {
 size_t max=0;
 static void*allocate(size_t n,void*p)noexcept{return n<=static_cast<Budget*>(p)->max?std::malloc(n):nullptr;}
 PageAllocator allocator(){return {allocate,PageAllocator::releaseDefault,this};}
};
int main(){
 std::array<uint8_t,16*PageSize>scratch{};
 {Snapshot old,current;Memory a,b;std::vector<uint32_t>list;for(unsigned i=0;i<8192;++i)list.push_back(0x10000000+i*PageSize);
  for(auto at:list)assert(old.prefetch(a,at,PageSize));assert(current.prefetchPages(b,list,scratch.data(),scratch.size()));compare(old,current);assert(a.calls==8192&&b.calls==512);
  assert(current.seal());const auto calls=b.calls;assert(!current.prefetchPages(b,list,scratch.data(),scratch.size())&&b.calls==calls);
  std::printf("PASS batch dense: pages=8192 old_calls=%zu new_calls=%zu; identical bytes, no repeated successful reads\n",a.calls,b.calls);
 }
 {Snapshot old,current;Memory a,b;a.denied=b.denied=0x12000;const std::vector<uint32_t>list{0x10000,0x11000,0x12000,0x13000};
  for(auto at:list)old.prefetch(a,at,PageSize);assert(!current.prefetchPages(b,list,scratch.data(),scratch.size()));compare(old,current);assert(b.calls==a.calls+1);
 }
 {Snapshot old,current;Memory a,b;const std::vector<uint32_t>list{0x10000,0x11000,0x12000,0x13000};
  assert(old.prefetch(a,0x11000,PageSize)&&current.prefetch(b,0x11000,PageSize));for(auto at:list)assert(old.prefetch(a,at,PageSize));assert(current.prefetchPages(b,list,scratch.data(),scratch.size()));compare(old,current);
 }
 {Snapshot current;Memory b;const std::vector<uint32_t>list{0x10000,0x11000};assert(current.prefetchPages(b,list,nullptr,0));assert(b.calls==2);}
 {Budget budget{sizeof(Page)*2};Snapshot current(budget.allocator());Memory b;const std::vector<uint32_t>list{0x10000,0x11000,0x12000,0x13000};
  assert(!current.prefetchPages(b,list,scratch.data(),scratch.size()));assert(current.overflow()&&current.pages.size()==2&&b.calls==2);assert(current.pages[0].full&&current.pages[1].full);
 }
 {Snapshot current;Memory b;const std::vector<uint32_t>list{0,0xFFFF,0x7FFFF000u,0x80000000u};assert(!current.prefetchPages(b,list,scratch.data(),scratch.size()));assert(current.pages.size()==1&&b.calls==1);}
 std::puts("PASS batch boundaries: failed span, preobserved page, sealed snapshot, no scratch, allocation rejection, address limits");
}
