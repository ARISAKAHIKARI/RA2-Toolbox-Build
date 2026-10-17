#pragma once
#include <stdint.h>
namespace nativecache {
// Original groups include 20-frame loops at rate=4. At high refresh their
// entire visible cycle exceeds 512 handles, so retain a full cycle safely.
constexpr int Slots=2048;
constexpr uint32_t PixelBudget=32u*1024*1024;
struct Tag {int w,h,hx,hy;uint64_t hash;};
inline bool same(const Tag& a,const Tag& b){return a.w==b.w&&a.h==b.h&&a.hx==b.hx&&a.hy==b.hy&&a.hash==b.hash;}
inline uint64_t hash(const uint32_t* pixels,unsigned count){
 uint64_t out=1469598103934665603ull;
 for(unsigned i=0;i<count;i++){out^=pixels[i];out*=1099511628211ull;}
 return out;
}
struct Usage {bool occupied;uint32_t used;};
inline int victim(const Usage* entries,int protectedSlot,uint32_t clock){
 int choice=-1;uint32_t oldest=0;
 for(int i=0;i<Slots;i++){
  if(i==protectedSlot)continue;
  if(!entries[i].occupied)return i;
  uint32_t age=clock-entries[i].used;
  if(choice<0||age>oldest){choice=i;oldest=age;}
 }
 return choice;
}
}
