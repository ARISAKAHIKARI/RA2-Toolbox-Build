#pragma once
#include <algorithm>
#include <cstdint>
namespace assist { namespace shroudraster {
// Native alpha-buffer convention: 0 = opaque black, 127 = clear.
// Keep sentinel pixels and compose fog without repeatedly darkening overlaps.
inline uint16_t half(uint8_t mask,uint16_t destination,bool fog){
 if(mask>127)return destination;
 const uint16_t value=uint16_t((unsigned(mask)+127u+1u)/2u);
 return fog?std::min(destination,value):value;
}
}}
