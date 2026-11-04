#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "patch_context.h"
// Fixed in-image control semantics; these are not arbitrary game-code targets.
// Owner calls remain strict before the common predicate is made unconditional.
namespace mastercontrol {
struct Site {uint32_t address;uint8_t before[5],after[5];};
static constexpr Site Sites[]={
 {0x6339BA,{0xE8,0x31,0x7D,0xED,0xFF},{0xE8,0x57,0x7D,0xED,0xFF}},
 {0x637E40,{0xE8,0xAB,0x38,0xED,0xFF},{0xE8,0xD1,0x38,0xED,0xFF}},
 {0x50B6F0,{0xA1,0x38,0xB2,0xA8,0},{0xE9,0x1B,0,0,0}}
};
inline const Site* find(uint32_t at){for(const auto&s:Sites)if(s.address==at)return &s;return nullptr;}
inline bool installed(uint32_t at,const uint8_t*bytes,unsigned n){auto s=find(at);return s&&bytes&&n==5&&!memcmp(bytes,s->after,5);}
inline bool original(uint32_t at,const uint8_t*bytes,unsigned n){
 const auto*s=find(at);if(!s||!bytes||n!=5)return false;
 if(!memcmp(bytes,s->before,5))return true;
 static const uint8_t forward[][5]={{0xE9,0xFB,0x2A,0x67,0},{0xE9,0x8B,0x2C,0x67,0}};
 return at==0x50B6F0&&(!memcmp(bytes,forward[0],5)||!memcmp(bytes,forward[1],5));
}
template<class Read> bool compatible(Read read){
 for(const auto&s:Sites)if(!patchcontext::compatible(s.address,read))return false;
 uint8_t entry[5]{},prefixTail[4]{};
 if(!read(0x50B6F0,entry,5)||!read(0x50B6F5,prefixTail,4))return false;
 if(!installed(0x50B6F0,entry,5)){
  if(!original(0x50B6F0,entry,5))return false;
  const uint8_t native[]={0x85,0xC0,0x75,0x1D},forward[]={0xCC,0xCC,0xCC,0xCC};
  if(memcmp(prefixTail,entry[0]==0xE9?forward:native,4))return false;
 }
 // Both branch destinations must remain the exact verified native leaves.
 // The strict leaf is thiscall House==CurrentPlayer, NOT a two-arg fastcall.
 static const uint8_t tail[]={0xB8,1,0,0,0,0xC3,0x8B,0x15,0x4C,0x3D,0xA8,0,0x33,0xC0,0x3B,0xCA,0x0F,0x94,0xC0,0xC3};
 uint8_t actual[sizeof tail]{};return read(0x50B710,actual,sizeof actual)&&!memcmp(actual,tail,sizeof tail);
}
}
