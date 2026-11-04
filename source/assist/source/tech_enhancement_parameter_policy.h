#pragma once
#include <stdint.h>
namespace assist { namespace techenhanceparams {
inline constexpr unsigned Rules=0,Exact=1;
inline constexpr uint32_t breakpointTags(bool compareLevel){return compareLevel?12u:4u;}
inline uint32_t compareFlags(uint32_t lhs,uint32_t rhs,uint32_t previous){
 const uint32_t result=lhs-rhs;uint32_t flags=0;
 if(lhs<rhs)flags|=1;if(!(__builtin_popcount(result&255)&1))flags|=4;
 if((lhs^rhs^result)&16)flags|=16;if(!result)flags|=64;if(result&0x80000000u)flags|=128;
 if(((lhs^rhs)&(lhs^result))&0x80000000u)flags|=0x800;
 return (previous&~0x8D5u)|flags;
}
}}
