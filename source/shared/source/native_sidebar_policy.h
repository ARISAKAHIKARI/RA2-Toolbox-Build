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
 {0x692406,6,Kind::Entry,{0x51,0xB9,0xE8,0xF7,0x87,0},{}},
 {0x4F4480,8,Kind::Entry,{0x83,0xEC,8,0xA1,0x1C,0x73,0x88,0},{}},
 {0x4F459D,6,Kind::Entry,{0x89,0x2D,0x14,0x73,0x88,0},{}},
};
static_assert(sizeof(Sites)/sizeof(Sites[0])==13,"sidebar complete thirteen-site group");
inline const Site* find(uint32_t at){for(const auto&s:Sites)if(s.address==at)return &s;return nullptr;}
inline bool original(uint32_t at,const uint8_t*b,unsigned n){auto s=find(at);return s&&b&&n==s->size&&!memcmp(b,s->before,n);}
inline bool knownOriginal(uint32_t at,const uint8_t*b,unsigned n){return original(at,b,n);}
inline bool installed(uint32_t at,const uint8_t*b,unsigned n){auto s=find(at);return s&&s->kind==Kind::Bytes&&b&&n==s->size&&!memcmp(b,s->after,n);}
inline uint32_t destination(uint32_t at,const uint8_t*b,unsigned n){
 auto s=find(at);if(!s||s->kind!=Kind::Entry||!b||n!=s->size||b[0]!=0xE9)return 0;
 for(unsigned i=5;i<n;++i)if(b[i]!=0x90)return 0;
 uint32_t d=0;memcpy(&d,b+1,4);return at+5+d;
}
struct Context {uint32_t at;unsigned n;uint32_t hash;};
inline constexpr Context Contexts[]={
 {0x72ad60,5,0x5D26169Cu},
 {0x72ad67,6,0x57810031u},
 {0x65386b,6,0x3AD037C6u},
 {0x654332,6,0x096C228Bu},
 {0x653a3f,6,0x9C7903BFu},
 {0x6d06c7,6,0xD52A3451u},
 {0x692bb4,4,0x5EDEB698u},
 {0x6a70e0,6,0x0D9AE8D6u},
 {0x6abd30,5,0x95338E03u},
 {0x4e1640,6,0x98FA4A3Fu},
 {0x692406,6,0xCFEE96A9u},
 {0x4f4480,8,0xE57D1D7Au},
 {0x4f459d,6,0x1B532FA5u}
};
template<class Read> bool compatible(uint32_t at,Read read){
 const auto*s=find(at);if(!s)return false;
 const Context*c=nullptr;for(const auto&v:Contexts)if(v.at==at)c=&v;
 if(!c)return false;uint8_t bytes[64]{};
 if(!read(at-16,bytes,s->size+48))return false;
 uint32_t hash=2166136261u;
 for(unsigned i=0;i<s->size+48;++i){uint32_t p=at-16+i;bool managed=false;
  for(const auto&v:Sites)if(p>=v.address&&p<v.address+v.size){managed=true;break;}
  if(!managed)hash=(hash^bytes[i])*16777619u;
 }
 return hash==c->hash;
}
}
