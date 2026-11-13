#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
namespace assist { namespace shroudraster {
// Native alpha-buffer convention: 0 = opaque black, 127 = clear.
// Keep sentinel pixels and compose fog without repeatedly darkening overlaps.
inline uint16_t half(uint8_t mask,uint16_t destination,bool fog){
 if(mask>127)return destination;
 const uint16_t value=uint16_t((unsigned(mask)+127u+1u)/2u);
 return fog?std::min(destination,value):value;
}
// Called only after the outer draw callback validates its Runtime lease and
// buffer bounds. No manager entry, allocation or syscall belongs in this loop.
inline void halfSpan(const uint8_t*source,uint8_t*destination,size_t pixels,bool fog){
 for(size_t i=0;i<pixels;++i){const uint8_t mask=source[i];if(mask>127)continue;
  uint16_t previous;std::memcpy(&previous,destination+i*2,2);
  const uint16_t next=half(mask,previous,fog);std::memcpy(destination+i*2,&next,2);
 }
}
}}
