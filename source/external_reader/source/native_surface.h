#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <cmath>
#include <memory>
#include <vector>
#include "native_packed_color.h"
namespace external { namespace nativesurface {
// GPL-3.0 adaptation of Vera20K native_z.rs / palette_light.rs @3d42426.
// Compare signed candidates BEFORE narrowing. Empty storage is FFFF, whereas
// the row origin is 8000. Neither is a reversed floating point scene depth.
enum class Gradient:uint32_t {Flat=0,Moderate=1,Vertical=2,Tile=3};
struct Depth {
 bool enabled=false,shape=false,tileEqual=false,voxel=false,composite=false;
 Gradient gradient=Gradient::Vertical;
 int adjust=0,tileTop=0,clipTop=0,clipBottom=std::numeric_limits<int>::max();
 bool operator==(const Depth&o)const{return enabled==o.enabled&&shape==o.shape&&tileEqual==o.tileEqual&&voxel==o.voxel&&composite==o.composite&&gradient==o.gradient&&adjust==o.adjust&&tileTop==o.tileTop&&clipTop==o.clipTop&&clipBottom==o.clipBottom;}
};
struct Row {int seed=0,increment=1,threshold=1,step=-1,accumulator=0;
 bool operator==(const Row&o)const{return seed==o.seed&&increment==o.increment&&threshold==o.threshold&&step==o.step&&accumulator==o.accumulator;}
 int at(int row)const{return seed+step*((accumulator+std::max(row,0)*increment)/threshold);}
};
inline Row rows(Gradient gradient,int screenTop,int height,int adjust,bool shape=false){
 Row out;if(gradient==Gradient::Tile){out.seed=int(uint16_t(32768-screenTop-30))+adjust;out.step=0;return out;}
 if(gradient!=Gradient::Vertical){int q=gradient==Gradient::Moderate?3:1;out.seed=(int(uint16_t(32768-screenTop))+adjust)/q*q;out.increment=gradient==Gradient::Moderate?2:1;out.threshold=q;return out;}
 int raw=int(uint16_t(32768-height-screenTop+1))+adjust;
 if(shape){out.seed=raw;out.step=0;return out;}
 out.seed=raw/3*3-height/3;out.threshold=3;out.step=1;out.accumulator=3-height%3;
 if(out.accumulator==3){out.accumulator=0;++out.seed;}return out;
}
inline bool admits(int candidate,uint16_t stored,bool equal=false){return equal?candidate<=int(stored):candidate<int(stored);}
struct Walk {
 Row upper,lower;int first=0,split=0;bool divided=false;
 bool operator==(const Walk&o)const{return upper==o.upper&&lower==o.lower&&first==o.first&&split==o.split&&divided==o.divided;}
 int at(int row,int delta=0)const{return (divided&&row>=split?lower.at(row-split):upper.at(row-first))+delta;}
};
inline Walk walk(const Depth&depth,int screenTop,int nativeHeight,int viewHeight,int adjust,bool bridgeSplit=false){
 Walk out;int first=std::clamp(std::max(-screenTop,depth.clipTop),0,nativeHeight),last=std::clamp(std::min(viewHeight-screenTop,depth.clipBottom),first,nativeHeight);out.first=first;
 if(depth.gradient==Gradient::Tile){out.upper=rows(Gradient::Tile,screenTop+depth.tileTop,nativeHeight,adjust+depth.adjust);return out;}
 out.upper=rows(depth.gradient,screenTop+first,last-first,adjust+depth.adjust,depth.shape);
 if(bridgeSplit&&(depth.voxel||depth.composite)&&nativeHeight>16){int boundary=nativeHeight-16;out.split=std::max(first,boundary);out.divided=out.split<last;
  out.upper=rows(Gradient::Flat,screenTop+first,std::max(0,std::min(last,boundary)-first),adjust+depth.adjust-5);
  out.lower=rows(Gradient::Vertical,screenTop+out.split,last-out.split,adjust+depth.adjust);}
 return out;
}
inline uint32_t scale16(int milli){uint32_t x=uint32_t(std::clamp(milli,0,2000));return (x?x*65536u-1u:0u)/1000u;}
struct Palette {
 unsigned rows=53;int brightness=1000;std::array<uint32_t,3>scale{{65535,65535,65535}};
 bool special=false,scalar=false,rgb555=false;uint16_t colour=0;std::shared_ptr<const std::vector<uint32_t>>table;
 bool operator==(const Palette&o)const{return rows==o.rows&&brightness==o.brightness&&scale==o.scale&&special==o.special&&scalar==o.scalar&&rgb555==o.rgb555&&colour==o.colour&&table==o.table;}
 uint32_t apply(uint32_t rgb,uint8_t index,unsigned a=127,bool opaque=true)const{
  const unsigned n=std::clamp(rows,1u,256u),q=std::min((unsigned(std::clamp(brightness,0,2000))*261u)>>11,254u);
  const unsigned row=std::min(a*q*(n-1)/32258u,n-1);if(table&&table->size()==size_t(n)*256)return nativepacked::color((*table)[size_t(row)*256+index]|(opaque?colour:0),rgb&0xff000000u,rgb555);auto gain=scale;
  for(auto&c:gain)if(n>1)c=c*(2*row)/(n-1);
  if(special&&index>=240&&index<=254){unsigned d=std::min(n*30/200?n*30/200-1:0,(n-1)/2),neutral=65536;
   if(n>1&&row<=d)neutral=d?row*65536/d:0;gain.fill(neutral);}
  uint32_t out=0;for(unsigned c=0;c<3;++c){unsigned sh=16-c*8,value=(rgb>>sh)&255u;
   value=std::min(scalar?(value*gain[c])>>16:(value*(gain[c]>>4))>>12,255u);out|=value<<sh;}
  uint32_t word=nativepacked::word(out,rgb555);if(opaque)word|=colour;
  return nativepacked::color(word,rgb&0xff000000u,rgb555);
 }
};
// A is an independent byte surface. SHROUD skips FE, FOG admits only 0..127;
// repeated fog shapes subtract darkness rather than replacing the prior A.
inline uint8_t shroud(uint8_t old,uint8_t source){return source==254?old:source;}
inline uint8_t fog(uint8_t old,uint8_t source){return source>127?old:uint8_t(std::max(0,int(old)+int(source)-127));}
}}
