#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// Fixed local-presentation sites from MO_ExperienceEnhancements master
// 33ed558bd38f36b5012eddcc9c0fb109cc68e38a. Operands are replaced only by
// the exact constants below; entry destinations must remain Runtime MEM_IMAGE.
namespace nativesidebarpolicy {
enum class Kind : uint8_t { Bytes,Entry };
struct Site {uint32_t address;uint8_t size;Kind kind;uint8_t before[8],after[8];};
inline constexpr Site Sites[]={
 {0x72AD60,5,Kind::Bytes,{0xB9,0xA8,0,0,0},{0x90,0x90,0x90,0x90,0x90}},
 {0x72AD67,6,Kind::Bytes,{0x81,0xEA,0xA8,0,0,0},{0x81,0xEA,0,0,0,0}},
 {0x65386B,6,Kind::Bytes,{0x8B,0x15,0xA8,0x6F,0x88,0},{0x8B,0x15,0x90,0x6F,0x88,0}},
 {0x654332,6,Kind::Bytes,{0x8B,0x0D,0xA8,0x6F,0x88,0},{0x8B,0x0D,0x90,0x6F,0x88,0}},
 {0x653A3F,6,Kind::Bytes,{0x8B,0x1D,0xA8,0x6F,0x88,0},{0x8B,0x1D,0x90,0x6F,0x88,0}},
 {0x6D06C7,6,Kind::Bytes,{0x8B,0x3D,0xA8,0x6F,0x88,0},{0x8B,0x3D,0x90,0x6F,0x88,0}},
 {0x692BB4,4,Kind::Bytes,{0x8D,0x6C,0x28,0xFF},{0x8D,0x68,0xFF,0x90}},
 {0x6A70E0,6,Kind::Entry,{0x83,0xEC,0x38,0x53,0x55,0x56},{}},
 {0x6ABD30,5,Kind::Entry,{0x83,0xEC,0x24,0x53,0x55},{}},
 {0x4E1640,6,Kind::Entry,{0x83,0xEC,0x0C,0x53,0x55,0x56},{}},
 {0x692406,6,Kind::Entry,{0x51,0xB9,0xE8,0xF7,0x87,0},{}}
};
inline const Site* find(uint32_t at){for(const auto&s:Sites)if(s.address==at)return &s;return nullptr;}
inline bool original(uint32_t at,const uint8_t*b,unsigned n){auto s=find(at);return s&&b&&n==s->size&&!memcmp(b,s->before,n);}
inline bool knownOriginal(uint32_t at,const uint8_t*b,unsigned n){return original(at,b,n);}
inline bool installed(uint32_t at,const uint8_t*b,unsigned n){auto s=find(at);return s&&s->kind==Kind::Bytes&&b&&n==s->size&&!memcmp(b,s->after,n);}
inline uint32_t destination(uint32_t at,const uint8_t*b,unsigned n){
 auto s=find(at);if(!s||s->kind!=Kind::Entry||!b||n!=s->size||b[0]!=0xE9)return 0;
 for(unsigned i=5;i<n;++i)if(b[i]!=0x90)return 0;
 uint32_t d=0;memcpy(&d,b+1,4);return at+5+d;
}
template<class Read> bool compatible(uint32_t at,Read read){
 const auto*s=find(at);if(!s)return false;
 // Validate complete instructions rather than changing untyped immediates.
 // The body guard excludes current entry bytes, so owned patches revalidate.
 if(at==0x72AD60||at==0x72AD67){uint8_t b[6]{};
  if(!read(0x72AD67,b,6))return false;
  const auto*q=find(0x72AD67);
  if(memcmp(b,q->before,6)&&memcmp(b,q->after,6))return false;
  if(!read(0x72AD60,b,5))return false;q=find(0x72AD60);
  return !memcmp(b,q->before,5)||!memcmp(b,q->after,5);
 }
 uint8_t b[8]{};if(!read(at,b,s->size))return false;
 return original(at,b,s->size)||installed(at,b,s->size)||
        (s->kind==Kind::Entry&&destination(at,b,s->size)!=0);
}
}
