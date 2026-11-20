#define WORLD_FRAME_LEASE_DIAGNOSTICS 1
#include "../../shared/source/world_frame.h"
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <vector>

int main(){
 using worldframe::DemandLeases;
 constexpr uint32_t base=0x10000u;
 auto leases=std::make_unique<DemandLeases>();
 std::vector<uint32_t> pages;
 assert(!leases->retain(0,100)&&!leases->retain(0x12345,100));
 // 4,096 new subscriber pages arrive in 32 bursts across 256 ms, with
 // frequent append() calls. Their first expiry cannot be before t=1101.
 // The old implementation scanned the growing set after EACH 128-page
 // burst's append(), even while ALL entries remained unexpired. Its exact
 // redundant visit count for this fixture is 128*(1+...+32)=67,584.
 for(uint32_t i=0;i<4096;++i){
  const uint32_t now=100+i/16;
  assert(leases->retain(base+i*4096u,now));
  if((i&127u)==127u){
   pages.clear();leases->append(now,pages);
   assert(pages.size()==i+1);
  }
 }
 assert(leases->size()==4096&&leases->pruneVisits()==0);
 pages.clear();leases->append(1100,pages);
 assert(pages.size()==4096&&leases->pruneVisits()==0);
 pages.clear();leases->append(1101,pages);
 assert(pages.size()==4080&&pages.front()==base+16u*4096u);
 assert(leases->pruneVisits()==4096);
 pages.clear();leases->append(1102,pages);
 assert(pages.size()==4064&&pages.front()==base+32u*4096u);
 assert(leases->pruneVisits()==4096+4080);
 // New and renewed demand pages survive a skip and retain their SAME expiry.
 leases->clear();
 assert(leases->retain(base,100));
 assert(leases->retain(base,500));
 assert(leases->retain(base+4096,900));
 pages.clear();leases->append(1100,pages);
 assert(pages.size()==2); // first page age=600; second age=200
 pages.clear();leases->append(1500,pages);
 assert(pages.size()==2); // age=1000 is still valid; expiry is strict >1000.
 // Check expiration after the one-millisecond boundary.
 pages.clear();leases->append(1501,pages);
 assert(pages.size()==1&&pages[0]==base+4096);
 // A full table should reject thousands of new addresses without 16K-entry
 // prune scans until one of its actual pages can expire.
 auto saturated=std::make_unique<DemandLeases>();
 for(uint32_t i=0;i<worldframe::MaxPages;++i)
  assert(saturated->retain(base+i*4096u,4000));
 assert(saturated->size()==worldframe::MaxPages&&saturated->pruneVisits()==0);
 for(uint32_t i=0;i<2048;++i)
  assert(!saturated->retain(base+(worldframe::MaxPages+i)*4096u,4000));
 assert(saturated->pruneVisits()==0);
 assert(saturated->retain(base,4001));
 assert(saturated->retain(base+worldframe::MaxPages*4096u,5001));
 assert(saturated->pruneVisits()==worldframe::MaxPages);
 pages.clear();saturated->append(5001,pages);
 assert(pages.size()==2&&pages[0]==base&&pages[1]==base+worldframe::MaxPages*4096u);
 pages.clear();saturated->append(5002,pages);
 assert(pages.size()==1&&pages[0]==base+worldframe::MaxPages*4096u);
 // 32-bit GetTickCount wrap: use modular age, not wall-clock comparisons.
 auto wrapped=std::make_unique<DemandLeases>();
 assert(wrapped->retain(0x50000u,0xFFFFFFF0u));
 assert(wrapped->retain(0x60000u,5));
 pages.clear();wrapped->append(0xFFFFFFF0u+1000u,pages);
 assert(pages.size()==2);
 pages.clear();wrapped->append(0xFFFFFFF0u+1001u,pages);
 assert(pages.size()==1&&pages[0]==0x60000u);
 wrapped->clear();pages.clear();wrapped->append(42,pages);
 assert(pages.empty()&&wrapped->size()==0);
 std::printf("PASS deadline-aware leases: 4096 pages, 16K full-table rejections, exact 1000ms expiry, renewal, wrap, and clear; %llu unnecessary prune visits avoided in growing cohort\n",
             static_cast<unsigned long long>(128ull*32ull*33ull/2));
}
