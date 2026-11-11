#pragma once
#include <stdint.h>
namespace assist { namespace slotbudget {
inline constexpr uint32_t Known=0x80000000u,Full=1u,Map=2u,Enhance=4u,Compare=8u,Control=16u,FPS=32u,Trigger=64u,PrivateOut=896u;
inline unsigned count(uint32_t tags){return unsigned(__builtin_popcount(tags));}
inline uint32_t pack(unsigned free,unsigned owned,unsigned foreign,uint32_t tags){
 return free<=4&&owned<=4&&foreign<=4&&free+owned+foreign==4&&!(tags&~1023u)&&count(tags)==owned?
  Known|free|(owned<<3)|(foreign<<6)|(tags<<9):0;
}
inline bool known(uint32_t value){return (value&Known)!=0;}
inline unsigned free(uint32_t value){return value&7u;}
inline unsigned owned(uint32_t value){return (value>>3)&7u;}
inline unsigned foreign(uint32_t value){return (value>>6)&7u;}
inline uint32_t tags(uint32_t value){return (value>>9)&1023u;}
inline unsigned needed(uint32_t value,uint32_t wanted){return count(wanted&~tags(value));}
inline bool allows(uint32_t value,uint32_t wanted){return known(value)&&!(wanted&~1023u)&&needed(value,wanted)<=free(value);}
}}
