#pragma once
#include <cstdint>
namespace external { namespace nativepacked {
// GPL-3.0, Vera20K 3d42426: native_packed.wgsl, terrain_packed.wgsl
// and native_surface_format.rs. These are RGB565 compatibility operations;
// the high-precision/filter path continues to use premultiplied RGB8.
inline uint32_t word(uint32_t c){return ((c>>8)&0xf800u)|((c>>5)&0x7e0u)|((c>>3)&31u);}
inline uint32_t half(uint32_t w){return (w>>1)&0x7befu;}
inline uint32_t color(uint32_t w,uint32_t alpha=0xff000000u){
 static constexpr uint8_t five[32]={0,8,16,25,33,41,49,58,66,74,82,90,99,107,115,123,132,140,148,156,164,173,181,189,197,206,214,222,230,238,247,255};
 static constexpr uint8_t six[64]={0,4,8,12,16,20,24,28,32,36,40,45,49,53,57,61,65,69,73,77,81,85,89,93,97,101,105,109,113,117,121,125,129,133,138,142,146,150,154,158,162,166,170,174,178,182,186,190,194,198,202,206,210,214,219,223,227,231,235,239,243,247,251,255};
 return alpha|(uint32_t(five[(w>>11)&31])<<16)|(uint32_t(six[(w>>5)&63])<<8)|five[w&31];
}
inline uint32_t shadow(uint32_t destination){return color(half(word(destination)),destination&0xff000000u);}
inline uint32_t blend(uint32_t source,uint32_t destination,unsigned selector){
 const uint32_t s=word(source),d=word(destination);uint32_t result=s;
 if(selector==2)result=half(s)+half(d);
 else if(selector==1)result=((s>>2)&0x39e7u)*3+((d>>2)&0x39e7u);
 else if(selector==3)result=((s>>2)&0x39e7u)+((d>>2)&0x39e7u)*3;
 return color(result&0xffffu);
}
}}
