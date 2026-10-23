#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
namespace external { namespace lighting {
// Raw palettes contain expanded RGB, not a rendered lighting ramp. Read the
// current ambient and LightConvert tint separately; never call the renderer.
// Native offsets/units and special-light transitions: research pre.12.
struct State {
 int ambient=100;std::array<int,3> tint{{1000,1000,1000}};
 bool operator==(const State&o)const{return ambient==o.ambient&&tint==o.tint;}
 bool operator!=(const State&o)const{return !(*this==o);}
 std::array<float,3> gains()const{return {{ambient*tint[0]/100000.f,ambient*tint[1]/100000.f,ambient*tint[2]/100000.f}};}
};
template<class Read>State sample(Read read){
 State out;auto word=[&](uint32_t p){uint32_t v=0;read(p,&v,4);return v;};
 uint32_t scenario=word(0xA8B230);if(scenario<0x10000||scenario>0xffffc000)return out;
 int ambient=0;std::array<int,3> normal{};
 if(!read(scenario+0x352C,&ambient,4)||ambient<0||ambient>300||!read(scenario+0x3534,normal.data(),12))return out;
 if(std::any_of(normal.begin(),normal.end(),[](int x){return x<0||x>200;}))return out;
 out.ambient=ambient;for(int n=0;n<3;++n)out.tint[n]=normal[n]*10;
 uint32_t items=word(0xB054D4);int count=int(word(0xB054E0));
 if(items>=0x10000&&count>0&&count<=512&&uint64_t(items)+count*4<=0x100000000ull){
  for(int n=0;n<count;++n){uint32_t scheme=word(items+n*4);if(scheme<0x10000||scheme>0xfffffc00||word(scheme+0x310)<=1)continue;
   uint32_t convert=word(scheme+0x30C);if(convert<0x10000||convert>0xfffffe00)continue;
   uint8_t tinted=0;if(!read(convert+0x1B0,&tinted,1)||tinted>1)continue;
   uint32_t at=convert+(tinted?0x1A4:0x198);std::array<int,3> current{},after{};
   if(!read(at,current.data(),12)||std::any_of(current.begin(),current.end(),[](int x){return x<0||x>2000;}))continue;
   uint8_t again=2;if(!read(at,after.data(),12)||after!=current||!read(convert+0x1B0,&again,1)||again!=tinted)continue;
   out.tint=current;break;
  }
 }
 // Never publish a cross-map sample or a partially changing ambient value.
 int after=0;if(word(0xA8B230)!=scenario||!read(scenario+0x352C,&after,4)||after!=ambient)return {};
 return out;
}
// Reference transform for premultiplied pixels (also used by pixel tests).
// GDI+ applies the equivalent diagonal RGB matrix at presentation time, leaving
// cached poses and alpha untouched when a map trigger changes the lighting.
inline uint32_t pixel(uint32_t p,const State&s){
 auto g=s.gains();unsigned a=p>>24;uint32_t out=a<<24;
 for(int n=0;n<3;++n){int shift=16-n*8;unsigned c=unsigned(std::clamp(float((p>>shift)&255)*g[n],0.f,float(a)));out|=c<<shift;}return out;
}
}}
