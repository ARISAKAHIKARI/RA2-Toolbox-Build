#pragma once
#include "native_packed_color.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <vector>
namespace external { namespace nativecombat {
// Vera20K combat_light.rs / building_light.rs, GPL-3.0. Retail5FF850 maps
// the stage at +0C through83358C, then scales only normal masks by +10/64.
inline int surface(int stage,int size){
 static constexpr uint8_t table[90]={5,10,15,20,25,30,35,40,45,50,55,60,61,62,63,63,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,64,65,66,67,68,69,70,71,72,73};
 if(stage<0||stage>=90)return -1;int value=table[stage];if(value<64)value=int32_t(uint32_t(size)*uint32_t(value))/64;return value>=0&&value<74?value:-1;
}
inline void circle(std::vector<uint8_t>&s,int radius,uint8_t intensity){
 if(radius<0)return;if(radius>=182){std::fill(s.begin(),s.end(),intensity);return;}
 auto span=[&](int y,int first,int last){if(y<0||y>=256)return;first=std::max(first,0);last=std::min(last,255);if(first<=last)std::fill(s.begin()+size_t(y)*256+first,s.begin()+size_t(y)*256+last+1,intensity);};
 int x=0,y=radius,d=3-2*radius;while(y>=x){span(128+x,128-y,128+y);span(128+y,128-x,128+x);span(128-x,128-y,128+y);span(128-y,128-x,128+x);
  if(d<0)d+=4*x+6;else{d+=4*(x-y)+10;--y;}++x;}
}
inline std::shared_ptr<const std::vector<uint8_t>> mask(int index,int spotlightRadius){
 if(index<0||index>=74)return {};using Key=std::pair<int,int>;static thread_local std::map<Key,std::shared_ptr<const std::vector<uint8_t>>>cache;Key key{index,index<64?0:spotlightRadius};if(auto found=cache.find(key);found!=cache.end())return found->second;
 std::vector<uint8_t>scratch(256*256);if(index<64){unsigned strength=12;for(int radius=index*2+1;radius>0;radius-=2){circle(scratch,radius,uint8_t(strength));strength=std::min(255u,strength+4);}}
 else{static constexpr uint8_t peaks[10]={128,122,116,110,104,98,92,86,80,74};uint64_t bits=0xbf66db6db6db6db7ull;double scalar;std::memcpy(&scalar,&bits,8);int32_t raw=int32_t(uint32_t(spotlightRadius)*60u);double product=double(raw)*scalar;
  int32_t chopped=product>=-2147483648.&&product<2147483648.?int32_t(product):int32_t(0x80000000u);int radius=int32_t(0u-uint32_t(chopped)+uint32_t(index-64));circle(scratch,radius,peaks[index-64]);}
 auto out=std::make_shared<std::vector<uint8_t>>(256*128);for(int y=0;y<128;++y)std::copy_n(scratch.data()+size_t(y)*512,256,out->data()+size_t(y)*256);
 if(cache.size()>=94)cache.clear();cache.emplace(key,out);return out;
}
inline uint32_t apply(uint32_t destination,unsigned mask,unsigned flags,bool rgb555=false){
 if(!mask)return destination;uint32_t word=nativepacked::word(destination,rgb555),out=0;
 const unsigned components[3]={((word>>(rgb555?10:11))&31u)<<3,((word>>5)&(rgb555?31u:63u))<<(rgb555?3:2),(word&31u)<<3};
 for(unsigned n=0;n<3;++n){unsigned c=components[n];if(flags&1)c=c*(256-mask)>>8;else if(!(flags&(2u<<n)))c=std::min(c*(256+mask)>>8,255u);out|=c<<(16-n*8);}
 return nativepacked::color(nativepacked::word(out,rgb555),destination&0xff000000u,rgb555);
}
}}
