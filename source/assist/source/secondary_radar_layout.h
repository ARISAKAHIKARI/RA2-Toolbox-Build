#pragma once
#include "radar_layout.h"
#include <cmath>
#include <cstdint>
#include <limits>

namespace secondaryradar {
constexpr int PositionMax=10000;
// Position is a percentage of the free travel distance, after the requested
// panel is fitted to the secondary monitor. This preserves 0/100 anchors when
// the display size, scaling, or desktop origin changes.
inline radarlayout::Box place(int left,int top,int right,int bottom,
                              int xPercent,int yPercent,int desiredWidth,double aspect){
 const int64_t workW=int64_t(right)-left,workH=int64_t(bottom)-top;
 if(workW<3||workH<3||workW>std::numeric_limits<int>::max()||workH>std::numeric_limits<int>::max())return {};
 const int margin=std::min<int64_t>(12,std::min((workW-3)/2,(workH-3)/2));
 const int maxW=int(workW)-2*margin,maxH=int(workH)-2*margin;
 if(maxW<3||maxH<3)return {};
 if(!std::isfinite(aspect)||aspect<=0||aspect>=1024)aspect=1;
 int width=std::min(maxW,std::clamp(desiredWidth,160,4096));
 double nativeHeight=std::ceil((width-2)/aspect)+2;
 int height=nativeHeight>maxH?maxH:std::max(3,int(nativeHeight));
 if(nativeHeight>maxH){
  double fitWidth=std::min(double(width-2),std::floor(double(height-2)*aspect));
  width=std::clamp(int(fitWidth)+2,3,width);
 }
 const int remainingX=maxW-width,remainingY=maxH-height;
 const int64_t x=int64_t(left)+margin+(int64_t(remainingX)*std::clamp(xPercent,0,PositionMax)+PositionMax/2)/PositionMax;
 const int64_t y=int64_t(top)+margin+(int64_t(remainingY)*std::clamp(yPercent,0,PositionMax)+PositionMax/2)/PositionMax;
 return {int(x),int(y),width,height};
}
struct Settings {int x=PositionMax,y=PositionMax,width=320;};
inline Settings capture(int left,int top,int right,int bottom,
                        radarlayout::Box box,int requestedWidth){
 const int64_t workW=int64_t(right)-left,workH=int64_t(bottom)-top;
 if(workW<3||workH<3||workW>std::numeric_limits<int>::max()||workH>std::numeric_limits<int>::max()||box.w<3||box.h<3)return {};
 const int margin=std::min<int64_t>(12,std::min((workW-3)/2,(workH-3)/2));
 const int64_t freeX=workW-2*margin-box.w,freeY=workH-2*margin-box.h;
 auto percent=[](int64_t offset,int64_t free)->int{
  if(free<=0)return PositionMax;
  return int(std::clamp((offset*PositionMax+free/2)/free,int64_t(0),int64_t(PositionMax)));
 };
 return {percent(int64_t(box.x)-left-margin,freeX),percent(int64_t(box.y)-top-margin,freeY),
         std::clamp(requestedWidth,160,4096)};
}
}
