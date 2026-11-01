#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/process_reader.h"
#include <cassert>
#include <cstdio>
#include <chrono>
int main(){
 // Dedicated synthetic pages in this test process; never opens the game.
 constexpr uint32_t Base=0x18000000;constexpr size_t Bytes=40*1024*1024;
 auto*memory=static_cast<uint8_t*>(VirtualAlloc((void*)(uintptr_t)Base,Bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));assert(memory&&uintptr_t(memory)==Base);
 for(size_t n=0;n<Bytes;++n)memory[n]=uint8_t(n*37+n/4096);
 external::ProcessReader reader;assert(reader.open(GetCurrentProcessId()));
 std::array<uint8_t,0x148> observed{};
 std::vector<uint32_t> crossing{Base+4090,Base+8190};reader.prefetch(crossing,observed.size());assert(reader.prefetchedPages==3&&reader.reads==1);
 for(auto address:crossing){assert(reader.read(address,observed.data(),observed.size()));assert(!std::memcmp(observed.data(),memory+address-Base,observed.size()));}
 assert(reader.reads==1&&reader.pageHits==4); // no extra RPM for cross-page blocks
 const uint8_t old=memory[4090];memory[4090]^=0xFF;uint8_t current=0;
 assert(reader.read(Base+4090,&current,1)&&current==old);
 assert(reader.readFresh(Base+4090,&current,1)&&current==memory[4090]);
 reader.begin();assert(reader.read(Base+4090,&current,1)&&current==memory[4090]);
 // A failed speculative span cannot poison a readable page or fabricate a
 // complete object block; the original exact-range fallback still fails.
 reader.begin();DWORD protection=0;assert(VirtualProtect(memory+4096,4096,PAGE_NOACCESS,&protection));
 reader.prefetch({Base,Base+4096},0x148);assert(reader.prefetchMisses==1&&reader.prefetchedPages==0);
 assert(reader.read(Base,&current,1)&&current==memory[0]);
 assert(!reader.read(Base+4090,observed.data(),observed.size()));
 assert(reader.lastFailedAddress==Base+4090&&reader.lastFailedSize==observed.size()&&reader.errors==1&&reader.pageFallbacks==1);
 DWORD unused=0;assert(VirtualProtect(memory+4096,4096,protection,&unused));
 reader.begin();assert(reader.read(Base+4090,observed.data(),observed.size())&&!std::memcmp(observed.data(),memory+4090,observed.size()));
 // Packed Cell addresses share pages. The prefetch limit applies to unique
 // pages, not duplicate object addresses, so the complete chunk is covered.
 std::vector<uint32_t> cells;for(uint32_t n=0;n<8192;++n)cells.push_back(Base+n*0x150);
 const size_t pages=(cells.back()-Base+observed.size()+4095)/4096;
 reader.begin();reader.prefetch(cells,observed.size());assert(reader.prefetchedPages==pages);
 const uint32_t batchedCalls=reader.reads;
 for(auto address:cells){assert(reader.read(address,observed.data(),observed.size()));assert(!std::memcmp(observed.data(),memory+address-Base,observed.size()));}
 assert(reader.reads==batchedCalls&&batchedCalls==(pages+15)/16);
 // Storage is bounded: fetching the second page can recycle the first page's
 // node. The cross-page read must already have copied its first fragment.
 reader.close();assert(reader.open(GetCurrentProcessId()));
 assert(reader.read(Base,&current,1));
 for(uint32_t n=2;n<=8192;++n)assert(reader.read(Base+n*4096,&current,1));
 assert(reader.cachedPageStorage()==8192);
 assert(reader.read(Base+4090,observed.data(),observed.size())&&!std::memcmp(observed.data(),memory+4090,observed.size()));
 // Measure repeat full Cell sweeps, preserving begin() freshness every time.
 auto sweep=[&](bool batch){uint64_t calls=0;const auto start=std::chrono::steady_clock::now();
  for(int pass=0;pass<20;++pass){reader.begin();if(batch)reader.prefetch(cells,observed.size());
   for(auto address:cells)assert(reader.read(address,observed.data(),observed.size()));calls+=reader.reads;
  }
  double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return std::pair<uint64_t,double>{calls,ms};
 };
 auto scalar=sweep(false),batch=sweep(true);assert(batch.first*8<scalar.first);
 std::printf("PASS ProcessReader: cross-page cache/freshness/failure/eviction; 8192 Cell chunk pages=%zu RPM=%u; 20 fresh sweeps scalar RPM=%llu ms=%.3f, batch RPM=%llu ms=%.3f\n",pages,batchedCalls,(unsigned long long)scalar.first,scalar.second,(unsigned long long)batch.first,batch.second);
 reader.close();assert(VirtualFree(memory,0,MEM_RELEASE));
}
