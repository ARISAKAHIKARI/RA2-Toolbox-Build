#pragma once
// SPDX-License-Identifier: MIT
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
namespace neuralpixels {
inline uint16_t half(float f){
 uint32_t v=0;std::memcpy(&v,&f,4);uint32_t sign=(v>>16)&0x8000,exponent=(v>>23)&255,mantissa=v&0x7fffff;
 if(exponent==255)return uint16_t(sign|0x7c00|(mantissa?0x200:0));int e=int(exponent)-127+15;
 if(e>=31)return uint16_t(sign|0x7c00);if(e<=0){if(e<-10)return uint16_t(sign);mantissa|=0x800000;unsigned shift=unsigned(14-e);uint32_t result=mantissa>>shift,tail=mantissa&((1u<<shift)-1),mid=1u<<(shift-1);return uint16_t(sign+result+(tail>mid||(tail==mid&&(result&1))));}
 uint32_t result=uint32_t(e)<<10|(mantissa>>13),tail=mantissa&8191;return uint16_t(sign+result+(tail>4096||(tail==4096&&(result&1))));
}
inline float unhalf(uint16_t h){
 uint32_t sign=uint32_t(h&0x8000)<<16,e=(h>>10)&31,m=h&1023,v=0;
 if(!e){if(!m)v=sign;else{int shift=0;while(!(m&1024)){m<<=1;++shift;}v=sign|(uint32_t(127-14-shift)<<23)|((m&1023)<<13);}}
 else v=sign|((e==31?255:e+112)<<23)|(m<<13);float f=0;std::memcpy(&f,&v,4);return f;
}
// uint32 input is 0xAARRGGBB (BGRA in little-endian memory). The Vulkan
// image is R16G16B16A16_SFLOAT, so unpack channels explicitly.
inline void unpack(uint32_t color,uint16_t*out){out[0]=half(float((color>>16)&255)/255);out[1]=half(float((color>>8)&255)/255);out[2]=half(float(color&255)/255);out[3]=half(float(color>>24)/255);}
inline bool pack(const uint16_t*in,uint32_t source,float confidence,uint32_t&out){
 if(confidence<=0||(source>>24)!=255){out=source;return true;}
 uint32_t rgb=0;for(int i=0;i<3;++i){float v=unhalf(in[i]);if(!std::isfinite(v))return false;unsigned c=unsigned(std::clamp(std::lround(std::clamp(v,0.f,1.f)*255.f),0l,255l));rgb|=c<<(16-i*8);}
 out=(source&0xff000000)|rgb;return true;
}
}
