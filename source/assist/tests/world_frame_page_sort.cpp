#include "../../shared/source/world_frame_page_sort.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <vector>

static uint32_t state=0xC4EB1801u;
static uint32_t rnd(){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void check(size_t count,unsigned variant){
 std::vector<uint32_t> old,now,workspace;
 old.reserve(count);
 for(size_t i=0;i<count;++i){
  uint32_t value=rnd();
  if(variant%3==0)value&=0x1FFFF000u;
  if(variant%3==1)value=uint32_t(i%37u)*4096u;
  if(variant%3==2)value=(value&0xFFF00000u)|uint32_t(i%128u)*4096u;
  if(i%11u==0)value=0x7FFFF000u;
  if(i%17u==0)value=0u;
  if(i%43u==0)value=0xFFFFF000u;
  old.push_back(value);
 }
 now=old;
 std::sort(old.begin(),old.end());old.erase(std::unique(old.begin(),old.end()),old.end());
 worldframe::sortUniquePageAddresses(now,workspace);
 assert(now==old);
 const size_t currentCapacity=workspace.capacity();
 // No new scratch allocation after the largest cohort is reached.
 for(unsigned j=0;j<4;++j){
  now=old;std::reverse(now.begin(),now.end());
  worldframe::sortUniquePageAddresses(now,workspace);
  assert(now==old);
  assert(workspace.capacity()==currentCapacity);
 }
}
int main(){
 for(const size_t n:{0u,1u,2u,15u,127u,128u,129u,255u,256u,1024u,
                     4096u,16384u,32768u,65536u,131072u}){
  for(unsigned v=0;v<4;++v)check(n,v);
 }
 std::vector<uint32_t> alias={44u,0xFFFFF000u,44u,1u};
 worldframe::sortUniquePageAddresses(alias,alias);
 assert((alias==std::vector<uint32_t>{1u,44u,0xFFFFF000u}));
 const size_t size=32768;
 std::vector<uint32_t> seed(size),reference,scratch;
 for(size_t i=0;i<size;++i)seed[i]=(rnd()&0xFFFFF000u);
 auto begin=std::chrono::steady_clock::now();
 for(int n=0;n<20;++n){reference=seed;std::sort(reference.begin(),reference.end());reference.erase(std::unique(reference.begin(),reference.end()),reference.end());}
 const auto original=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 begin=std::chrono::steady_clock::now();
 for(int n=0;n<20;++n){reference=seed;worldframe::sortUniquePageAddresses(reference,scratch);}
 const auto improved=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 std::printf("PASS exact 32-bit page ascending/unique order, 0..131072 pages, alias, ties, rollover and retained scratch\n");
 std::printf("SYNTHETIC_SORT_MS comparator=%.3f radix=%.3f (20x32768 pages; NOT gameplay CPU/FPS)\n",original,improved);
}
