#pragma once
#include <array>
#include <cstdint>
namespace external { namespace paletterecovery {
constexpr uint16_t rgb565(uint32_t color){
 return uint16_t(((color>>19)&31)<<11|((color>>10)&63)<<5|((color>>3)&31));
}
// A known source palette may retain precision that its RGB565 drawer lost.
// Promote the complete source table only when it reproduces every live
// nontransparent code. This is source-guided reconstruction, not an inverse
// that can recover arbitrary discarded bits from a 16-bit image alone.
inline bool promote(std::array<uint32_t,256>&live,const std::array<uint32_t,256>&source){
 for(unsigned i=1;i<256;++i)
  if((live[i]>>24)!=255||(source[i]>>24)!=255||rgb565(live[i])!=rgb565(source[i]))return false;
 live=source;live[0]=0;return true;
}
}}
