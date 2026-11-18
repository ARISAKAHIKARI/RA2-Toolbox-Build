#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "world_frame.h"

namespace worldframe {
// The native producer used to sort as many as 32K dependency addresses on the
// game thread on every frame. This is a fixed-memory, linear bitmap union for
// large *validated* request sets; it emits exactly the same ascending,
// deduplicated list as std::sort + std::unique. No OS read or allocation here.
// Small sets retain std::sort in the caller to avoid the bitmap scan.
class PageRequestUnion {
 static constexpr size_t Words=(0x80000000ull/PageSize+63u)/64u;
 std::array<uint64_t,Words> bits_{};
 std::array<uint32_t,Words> generations_{};
 uint32_t generation_=0;
public:
 static constexpr size_t BitmapWords=Words;
 bool sortedUnique(std::vector<uint32_t>&pages){
  // An invalid/misaligned request must NOT silently disappear. Let the
  // caller retain its original producer-validation failure behavior.
  for(uint32_t page:pages)
   if((page&(PageSize-1))||!range(page,PageSize))return false;
  if(++generation_==0){generations_.fill(0);generation_=1;}
  size_t first=Words,last=0;
  for(uint32_t page:pages){
   const size_t word=page>>18;
   if(generations_[word]!=generation_){generations_[word]=generation_;bits_[word]=0;}
   bits_[word]|=uint64_t(1)<<((page>>12)&63u);
   if(word<first)first=word;
   if(word>last)last=word;
  }
  pages.clear(); // Keep capacity; unique count can never exceed input count.
  if(first==Words)return true;
  for(size_t word=first;word<=last;++word){
   if(generations_[word]!=generation_)continue;
   uint64_t mask=bits_[word];
   while(mask){
    const unsigned bit=unsigned(__builtin_ctzll(mask));
    pages.push_back(uint32_t((word*64u+bit)*PageSize));
    mask&=mask-1;
   }
  }
  return true;
 }
};
static_assert(PageRequestUnion::BitmapWords==8192,"bounded 2 GiB page bitmap");
} // namespace worldframe
