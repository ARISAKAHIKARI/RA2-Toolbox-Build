#pragma once
#include <stdint.h>
namespace assist { namespace mapvisibility {
inline constexpr uint32_t Tag=2u,Address=0x6D88FC,PrefixAddress=0x6D88F4;
inline constexpr uint8_t Prefix[]={0x8B,0x80,0x2C,1,0,0,0xA8,0x18,0x75,5};
inline uint32_t visibleFlags(uint32_t flags){return flags&~0x40u;}
inline bool compatible(const volatile uint8_t*code){
 for(unsigned i=0;i<sizeof Prefix;++i)if(code[i]!=Prefix[i])return false;
 return true;
}
}}
