#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "native_sidebar_policy.h"
namespace nativeviewportpolicy {
enum class Kind:uint8_t {Jump,Pointer,DataOperand};
struct Site {uint32_t at;uint8_t n,imm;Kind kind;uint8_t before[16];};
static constexpr Site Sites[]={
 {0x4F4480,8,0,Kind::Jump,{0x83,0xEC,8,0xA1,0x1C,0x73,0x88,0}},
 {0x4F4492,5,0,Kind::Jump,{0xA3,0x14,0x73,0x88,0}},
 {0x4F44CB,5,0,Kind::Jump,{0xA1,0x1C,0x73,0x88,0}},
 {0x4F44FD,5,0,Kind::Jump,{0x8B,0x16,0x50,0x8B,0xCE}},
 {0x4F4505,6,0,Kind::Jump,{0x8B,0x0D,0x1C,0x73,0x88,0}},
 {0x4F451B,5,0,Kind::Jump,{0xA0,0x19,0xB5,0xB0,0}},
 {0x692B60,7,0,Kind::Jump,{0x83,0xEC,0x18,0x53,0x55,0x56,0x57}},
 {0x692300,7,0,Kind::Jump,{0x8B,0x44,0x24,0x10,0x83,0xEC,0x18}},
 {0x6930A0,8,0,Kind::Jump,{0xA0,0x78,0xE3,0xA8,0,0x83,0xEC,0x1C}},
 {0x6D7840,5,0,Kind::Jump,{0x83,0xEC,0x48,0x56,0x57}},
 {0x6A7089,7,0,Kind::Jump,{0x8B,0x11,0x6A,1,0xFF,0x52,0x0C}},
 {0x7F7B60,4,0,Kind::Pointer,{0x60,0xA3,0x7B,0}},
 {0x478BD1,6,2,Kind::DataOperand,{0x8B,0x15,0xA0,0x6F,0x88,0}},
 {0x478BD7,5,1,Kind::DataOperand,{0xA1,0xA8,0x6F,0x88,0}},
 {0x478C17,5,1,Kind::DataOperand,{0xBF,0xA0,0x6F,0x88,0}},
 {0x478E4A,6,2,Kind::DataOperand,{0x8B,0x35,0xA0,0x6F,0x88,0}},
 {0x478E50,6,2,Kind::DataOperand,{0x8B,0x1D,0xA8,0x6F,0x88,0}}
};
inline const Site*find(uint32_t at){for(const auto&s:Sites)if(s.at==at)return &s;return nullptr;}
inline unsigned size(uint32_t at){auto*s=find(at);return s?s->n:0;}
inline bool original(uint32_t at,const uint8_t*b,unsigned n){auto*s=find(at);return s&&b&&n==s->n&&!memcmp(b,s->before,n);}
inline bool dataOperand(uint32_t at){auto*s=find(at);return s&&s->kind==Kind::DataOperand;}
inline uint32_t destination(uint32_t at,const uint8_t*b,unsigned n){
 auto*s=find(at);if(!s||!b||n!=s->n)return 0;uint32_t p=0;
 if(s->kind==Kind::Pointer){memcpy(&p,b,4);return p;}
 if(s->kind==Kind::DataOperand){if(memcmp(b,s->before,s->imm)||memcmp(b+s->imm+4,s->before+s->imm+4,n-s->imm-4))return 0;memcpy(&p,b+s->imm,4);return p;}
 if(b[0]!=0xE9)return 0;for(unsigned i=5;i<n;++i)if(b[i]!=0x90)return 0;memcpy(&p,b+1,4);return at+5+p;
}
struct Context {uint32_t at;unsigned n;uint32_t hash;};
inline constexpr Context Contexts[]={
 {0x4f4480,8,0x1BB7CE26u},
 {0x4f4492,5,0xDA29432Bu},
 {0x4f44cb,5,0xEE1A4367u},
 {0x4f44fd,5,0xD0818FDFu},
 {0x4f4505,6,0xEF57579Cu},
 {0x4f451b,5,0xF4C52041u},
 {0x692b60,7,0xC87714C4u},
 {0x692300,7,0x563AE980u},
 {0x6930a0,8,0x0CC73DB2u},
 {0x6d7840,5,0xA5BAC8DEu},
 {0x6a7089,7,0xD41BD9A2u},
 {0x7f7b60,4,0xA533B997u},
 {0x478bd1,6,0xF5D6BD53u},
 {0x478bd7,5,0x70619570u},
 {0x478c17,5,0xA0EF2D2Du},
 {0x478e4a,6,0xDD3472E2u},
 {0x478e50,6,0x43126C6Du},
};
template<class Read>bool compatible(uint32_t at,Read read){
 const auto*s=find(at);if(!s)return false;const Context*c=nullptr;for(const auto&v:Contexts)if(v.at==at)c=&v;
 if(!c)return false;uint8_t bytes[64]{};if(!read(at-16,bytes,s->n+48))return false;uint32_t hash=2166136261u;
 for(unsigned i=0;i<s->n+48;++i){const uint32_t pos=at-16+i;bool managed=false;
  for(const auto&v:Sites)if(pos>=v.at&&pos<v.at+v.n){managed=true;break;}
  for(const auto&v:nativesidebarpolicy::Sites)if(pos>=v.address&&pos<v.address+v.size){managed=true;break;}
  if(!managed)hash=(hash^bytes[i])*16777619u;
 }return hash==c->hash;
}
}
