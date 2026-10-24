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
 int intensity=-1; // Native cell intensity already includes Ambient/Ground/Level.
 bool operator==(const State&o)const{return ambient==o.ambient&&tint==o.tint&&intensity==o.intensity;}
 bool operator!=(const State&o)const{return !(*this==o);}
 std::array<float,3> gains()const{int value=intensity<0?ambient*10:intensity;return {{value*tint[0]/1000000.f,value*tint[1]/1000000.f,value*tint[2]/1000000.f}};}
};
template<class Read>bool converterTint(Read read,uint32_t convert,std::array<int,3>&out){
 if(convert<0x10000||convert>0xfffffe00)return false;
 uint8_t tinted=0;if(!read(convert+0x1B0,&tinted,1)||tinted>1)return false;
 uint32_t at=convert+(tinted?0x1A4:0x198);std::array<int,3> current{},after{};
 if(!read(at,current.data(),12)||std::any_of(current.begin(),current.end(),[](int x){return x<0||x>2000;}))return false;
 uint8_t again=2;if(!read(at,after.data(),12)||after!=current||!read(convert+0x1B0,&again,1)||again!=tinted)return false;
 out=current;return true;
}
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
   if(converterTint(read,word(scheme+0x30C),out.tint))break;
  }
 }
 // Never publish a cross-map sample or a partially changing ambient value.
 int after=0;if(word(0xA8B230)!=scenario||!read(scenario+0x352C,&after,4)||after!=ambient)return {};
 return out;
}
enum class Layer {Object,Terrain,Bridge};
// Read the engine's evaluated cell, not a second approximation of [Lighting].
// 484680 combines height, Ground and current ambient (including special modes),
// then applies the slope/light-source intensity for terrain and bridge layers.
template<class Read>State cell(Read read,uint32_t p,const State&fallback,Layer layer=Layer::Object,bool localTint=true){
 if(p<0x10000||p>0xfffffe00)return fallback;
 uint32_t scale=0,convert=0;int16_t values[3]{},after[3]{};
 if(!read(p+0x104,&scale,4)||scale>262144||!read(p+0x34,&convert,4)||(!scale&&convert<0x10000)||
    !read(p+0x10A,values,6)||std::any_of(values,values+3,[](int x){return x<0||x>2000;}))return fallback;
 State out=fallback;out.intensity=values[int(layer)];if(localTint)converterTint(read,convert,out.tint);
 if(!read(p+0x10A,after,6)||memcmp(values,after,6))return fallback;
 return out;
}
template<class Read>uint32_t cellAt(Read read,int x,int y){
 if(x<0||y<0||x>=512*256||y>=512*256)return 0;
 int cx=x/256,cy=y/256,index=cy*512+cx,count=0;uint32_t items=0,p=0;
 if(!read(0x87F7E8+0x13C,&items,4)||items<0x10000||!read(0x87F7E8+0x140,&count,4)||count<=index||count>512*512||uint64_t(items)+index*4+4>0x100000000ull||!read(items+index*4,&p,4)||p<0x10000||p>0xfffffe00)return 0;
 int16_t coordinates[2]{};return read(p+0x24,coordinates,4)&&coordinates[0]==cx&&coordinates[1]==cy?p:0;
}
template<class Read>State at(Read read,int x,int y,int z,const State&fallback,bool terrain=false){
 uint32_t p=cellAt(read,x,y);Layer layer=terrain?Layer::Terrain:Layer::Object;
 uint32_t flags=0;int8_t level=0;int height=0;
 if(!terrain&&p&&read(p+0x140,&flags,4)&&(flags&0x80)&&read(p+0x11B,&level,1)&&read(0x89E7C0,&height,4)&&height>0&&height<=1024&&z>=(int(level)+4)*height-1)layer=Layer::Bridge;
 return cell(read,p,fallback,layer);
}
// Reference transform for premultiplied pixels (also used by pixel tests).
// GDI+ applies the equivalent diagonal RGB matrix at presentation time, leaving
// cached poses and alpha untouched when a map trigger changes the lighting.
inline uint32_t pixel(uint32_t p,const State&s){
 auto g=s.gains();unsigned a=p>>24;uint32_t out=a<<24;
 for(int n=0;n<3;++n){int shift=16-n*8;unsigned c=unsigned(std::clamp(float((p>>shift)&255)*g[n],0.f,float(a)));out|=c<<shift;}return out;
}
}}
