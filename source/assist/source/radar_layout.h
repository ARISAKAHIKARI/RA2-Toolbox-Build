#pragma once
#include <algorithm>
#include <cstdint>
#include <climits>
namespace radarlayout {
struct Box {int x=0,y=0,w=0,h=0;};
inline bool overlap(Box a,Box b,int gap=0){return a.w>0&&a.h>0&&b.w>0&&b.h>0&&a.x<b.x+b.w+gap&&a.x+a.w+gap>b.x&&a.y<b.y+b.h+gap&&a.y+a.h+gap>b.y;}
inline Box aircraft(int gw,int gh,int cw,int ch){
 if(gw<=0||gh<=0||cw<=0||ch<=0)return {};
 int w=std::min(cw,std::max(92,int(int64_t(104)*cw/gw))),h=std::min(ch,std::max(26,int(int64_t(28)*ch/gh)));
 return {std::max(0,int(int64_t(gw-160)*cw/gw)-w-6),std::clamp(int(int64_t(12)*ch/gh),0,ch-h),w,h};
}
struct Layout {Box source,destination;bool valid=false;};
inline Layout place(int gw,int gh,int cw,int ch,int scale,int mode,int userX,int userY,Box plane,double aspect=0,int alignX=0,int alignY=0){
 Layout result;if(gw<320||gh<200||cw<320||ch<200)return result;
 const int sw=std::max(1,int(int64_t(160)*cw/gw)),sh=std::max(1,int(int64_t(140)*ch/gh));
 result.source={cw-sw,0,sw,sh};
 const int gap=std::max(6,int(int64_t(10)*cw/gw)),pad=2;
 const int top=plane.w>0?plane.y+plane.h+gap:std::max(gap,int(int64_t(40)*ch/gh));
 int w=int(int64_t(sw)*std::clamp(scale,100,400)/100)+2,h=int(int64_t(sh)*std::clamp(scale,100,400)/100)+2;
 // There is always a fallback slot left of radar and below the aircraft button.
 const int availableW=cw-sw-gap-pad,availableH=ch-top-pad;
 if(availableW<3||availableH<3)return result;
 double fit=std::min(1.0,std::min(double(availableW)/w,double(availableH)/h));
 w=std::max(3,int(w*fit));h=std::max(3,int(h*fit));
 int boxW=w,boxH=h;
 // Fit the real map BEFORE anchoring and collision avoidance. Resizing an
 // already positioned square leaves a large gap below a wide minimap.
 if(aspect>0&&aspect<1024){double innerW=std::min(1024.,double(w-2)),innerH=std::min(1024.,double(h-2));
  innerW=std::min(innerW,innerH*aspect);innerH=innerW/aspect;w=std::max(3,int(innerW)+2);h=std::max(3,int(innerH)+2);}
 int x=cw-sw-gap-w,y=top;
 if(mode==1){x=gap;y=gap;}else if(mode==2){x=gap;y=ch-h-gap;}
 else if(mode==3){x=int(int64_t(std::clamp(userX,0,32768))*cw/gw)+(boxW-w)*std::clamp(alignX,0,2)/2;y=int(int64_t(std::clamp(userY,0,32768))*ch/gh)+(boxH-h)*std::clamp(alignY,0,2)/2;}
 x=std::clamp(x,pad,cw-w-pad);y=std::clamp(y,pad,ch-h-pad);
 int xs[7]={x,pad,cw-w-pad,result.source.x-w-gap,result.source.x+sw+gap,plane.x-w-gap,plane.x+plane.w+gap};
 int ys[7]={y,pad,ch-h-pad,result.source.y-h-gap,result.source.y+sh+gap,plane.y-h-gap,plane.y+plane.h+gap};
 int64_t best=INT64_MAX;
 for(int px:xs)for(int py:ys){Box candidate{px,py,w,h};
  if(px<pad||py<pad||px+w>cw-pad||py+h>ch-pad||overlap(candidate,result.source,gap)||overlap(candidate,plane,gap))continue;
  int64_t dx=int64_t(px)-x,dy=int64_t(py)-y,distance=dx*dx+dy*dy;
  if(distance<best){best=distance;result.destination=candidate;result.valid=true;}
 }
 return result;
}
}
