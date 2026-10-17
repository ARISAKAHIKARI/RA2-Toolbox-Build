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
 const long long dx=ax*nb-bx*na,dy=ay*nb-by*na;
 const long long limit=static_cast<long long>(std::max(w,h)/6+1)*na*nb;
 return dx*dx+dy*dy <= limit*limit;
}
} // namespace cursorstream
