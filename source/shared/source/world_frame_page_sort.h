#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace worldframe {
// CPU-only ordering of 32-bit page addresses. The retained scratch vector
// avoids a per-frame allocation after first reaching this scene size.
// Preserve std::sort's exact ascending+unique output and small-cohort speed.
inline void sortUniquePageAddresses(std::vector<uint32_t>&pages,
                                    std::vector<uint32_t>&scratch){
 if(pages.size()<128||&pages==&scratch)std::sort(pages.begin(),pages.end());
 else {
  scratch.resize(pages.size());
  uint32_t*src=pages.data(),*dst=scratch.data();
  const size_t n=pages.size();
  for(unsigned shift=0;shift<32;shift+=8){
   size_t offsets[256]{};
   for(size_t i=0;i<n;++i)++offsets[(src[i]>>shift)&255u];
   size_t sum=0;
   for(unsigned bucket=0;bucket<256;++bucket){
    const size_t count=offsets[bucket];offsets[bucket]=sum;sum+=count;
   }
   for(size_t i=0;i<n;++i)dst[offsets[(src[i]>>shift)&255u]++]=src[i];
   uint32_t*previous=src;src=dst;dst=previous;
  }
  // Four passes are even: the result finishes in the original page buffer.
 }
 pages.erase(std::unique(pages.begin(),pages.end()),pages.end());
}
} // namespace worldframe
