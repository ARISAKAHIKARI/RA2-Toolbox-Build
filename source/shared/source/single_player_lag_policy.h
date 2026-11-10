#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Strict semantic replacements, not user-bypassable ABI probes. A foreign
// implementation may have side effects which invalidate the equivalence proof.
namespace singlelag {
constexpr uint32_t PathClear=0x42A5B0, MindCurve=0x70508A;
constexpr uint32_t Path=1, Mind=2;
inline bool singlePlayer(int session){return session==0||session==5;}
inline unsigned size(uint32_t at){return at==PathClear?6:at==MindCurve?7:0;}
inline bool original(uint32_t at,const uint8_t*bytes,unsigned n){
 static const uint8_t path[]={0x8B,0x41,0x0C,0x83,0xEC,0x08};
 static const uint8_t mind[]={0x8B,0x84,0x24,0xB0,0,0,0};
 return bytes&&n==size(at)&&n&&!memcmp(bytes,at==PathClear?path:mind,n);
}
template<class Read> bool hash(Read read,uint32_t at,unsigned n,uint32_t expected){
 uint8_t bytes[256];uint32_t h=2166136261u;
 while(n){unsigned part=n>sizeof bytes?sizeof bytes:n;
  const unsigned pageLeft=4096-(at&4095);if(part>pageLeft)part=pageLeft;
  if(!read(at,bytes,part))return false;
  for(unsigned i=0;i<part;++i)h=(h^bytes[i])*16777619u;
  at+=part;n-=part;
 }return h==expected;
}
template<class Read> bool compatible(uint32_t at,Read read){
 if(at==PathClear)return hash(read,0x42A5B6,216,0xA4AC3F17);
 if(at!=MindCurve)return false;
 return hash(read,0x704E40,586,0xB4223762)&&hash(read,0x705091,466,0xE8813826)&&
  hash(read,0x6D2140,306,0x8E27CF95)&&hash(read,0x7F4E40,16,0x836334C8)&&
  hash(read,0x7E1718,8,0x8C6A9878)&&hash(read,0x7EC050,8,0xDAA4A1A5);
}
}
