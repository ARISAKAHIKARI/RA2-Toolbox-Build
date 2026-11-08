#pragma once
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
namespace cursorstream {
struct Detector {
 int last=-1, consecutive=0;
 std::uint32_t at=0, period=60;
 void reset(){last=-1;consecutive=0;at=0;period=60;}
 void observe(int frame,std::uint32_t now){
  if(frame<0){return;} // transient read failure is not an animation boundary
  if(last==frame)return;
  if(last>=0){
   auto dt=now-at;
   if(frame==last+1&&dt>=8&&dt<=350){
    consecutive=std::min(consecutive+1,32);
    if(consecutive>=2)period=std::clamp((period*2+dt)/3,12u,200u);
   }else consecutive=0;
  }
  last=frame;at=now;
 }
 bool predicts(int frame,std::size_t total)const{
  return frame==last&&consecutive>=2&&frame>=0&&std::size_t(frame+1)<total;
 }
 unsigned duration(unsigned manual)const{return consecutive>=3?period:manual;}
};
// Conservative shape gate: do not invent a transition between unrelated
// index-adjacent cursor icons. Moving / rotating sprites have comparable
// opaque area and nearby center; no requirement that exact pixels overlap.
inline bool plausible(const unsigned char* a,const unsigned char* b,
 int w,int h,int strideA,int strideB){
 if(!a||!b||w<=0||h<=0||strideA<w||strideB<w)return false;
 int na=0,nb=0;long long ax=0,ay=0,bx=0,by=0;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  if(a[y*strideA+x]){++na;ax+=x;ay+=y;}
  if(b[y*strideB+x]){++nb;bx+=x;by+=y;}
 }
 if(na<3||nb<3||na>nb*3||nb>na*3)return false;
 // Supported SHA canvases reach 512x512. The integer centroid products
 // still fit int64, but their squares already overflow on a full 128x128
 // canvas. Convert before squaring so the conservative distance gate is
 // defined for every canvas accepted by decode().
 const long double dx=static_cast<long double>(ax)*nb-static_cast<long double>(bx)*na;
 const long double dy=static_cast<long double>(ay)*nb-static_cast<long double>(by)*na;
 const long double limit=static_cast<long double>(std::max(w,h)/6+1)*na*nb;
 return dx*dx+dy*dy <= limit*limit;
}
// Shape support is immutable while one original SHA resource is loaded.
// Presentation refreshes can repeat the same source frame many times. Keep
// that pair's decision; the owner must reset before replacing the resource.
class PairShapeGate {
 int frame_=-1;bool valid_=false,value_=false;
public:
 void reset(){frame_=-1;valid_=false;value_=false;}
 template<class Evaluate>bool get(int frame,Evaluate evaluate){
  if(!valid_||frame_!=frame){const bool value=evaluate();frame_=frame;value_=value;valid_=true;}
  return value_;
 }
};
} // namespace cursorstream
