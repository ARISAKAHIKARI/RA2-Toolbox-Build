#include "../source/../../shared/source/world_frame_page_union.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

int main(){
 worldframe::PageRequestUnion reuser;
 auto exact=[&](std::vector<uint32_t> pages){
  auto sorted=pages;
  std::sort(sorted.begin(),sorted.end());
  sorted.erase(std::unique(sorted.begin(),sorted.end()),sorted.end());
  assert(reuser.sortedUnique(pages));
  assert(pages==sorted);
 };
 exact({});
 exact({0x10000u,0x7ffff000u,0x10000u,0x12000u,0x11000u,0x7ffff000u});
 uint32_t rand=0xFA10C3u;
 auto next=[&](){rand^=rand<<13;rand^=rand>>17;rand^=rand<<5;return rand;};
 std::vector<uint32_t> large;large.reserve(32768);
 for(int trial=0;trial<24;++trial){
  large.clear();
  for(int n=0;n<32768;++n){
   // Distinct sources can repeat the same demands thousands of times.
   uint32_t page=0x10000u+(next()%65536u)*PageSize;
   large.push_back(page);
  }
  exact(large);
 }
 // Invalid data stays visible to the original strict capture validator.
 for(uint32_t bad:{0u,0x12345u,0x80000000u}){
  std::vector<uint32_t> pages{0x10000u,bad,0x12000u};
  auto copy=pages;
  assert(!reuser.sortedUnique(pages)&&pages==copy);
  exact({0x10000u,0x7ffff000u,0x10000u});
 }
 // Measure this operation only. It does not measure game-thread FPS or
 // CaptureGraph I/O, and is not used as a flaky wall-clock CI assertion.
 large.clear();
 for(int n=0;n<16384;++n)large.push_back(0x10000u+(next()%16384u)*PageSize);
 const int rounds=128;
 auto start=std::chrono::steady_clock::now();
 for(int n=0;n<rounds;++n){auto items=large;std::sort(items.begin(),items.end());items.erase(std::unique(items.begin(),items.end()),items.end());}
 auto mid=std::chrono::steady_clock::now();
 for(int n=0;n<rounds;++n){auto items=large;assert(reuser.sortedUnique(items));}
 auto end=std::chrono::steady_clock::now();
 const auto old=std::chrono::duration_cast<std::chrono::microseconds>(mid-start).count();
 const auto newer=std::chrono::duration_cast<std::chrono::microseconds>(end-mid).count();
 std::printf("PASS page union: 24x32768 exact order/duplicates, invalid fail-closed, %d-round sort %lldus bitmap %lldus (synthetic, NOT gameplay)\\n",rounds,(long long)old,(long long)newer);
}
