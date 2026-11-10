#pragma once
#include <algorithm>
#include <cstdint>
namespace assist::panelplacement {
struct Rect { int x=0,y=0,w=0,h=0; };
inline bool overlaps(Rect a,Rect b,int gap=0){return a.w>0&&a.h>0&&b.w>0&&b.h>0&&a.x<b.x+b.w+gap&&a.x+a.w+gap>b.x&&a.y<b.y+b.h+gap&&a.y+a.h+gap>b.y;}
inline int rows(int entries,int limit,int available){return std::max(1,std::min({(std::max(0,entries)+1)/2,std::max(1,limit),std::max(1,available)}));}
inline Rect clamp(Rect r,int w,int h){r.x=std::clamp(r.x,0,std::max(0,w-r.w));r.y=std::clamp(r.y,0,std::max(0,h-r.h));return r;}
inline Rect avoid(Rect preferred,Rect occupied,int w,int h,int gap){
 preferred=clamp(preferred,w,h);if(!overlaps(preferred,occupied,gap))return preferred;
 const Rect choices[]={{occupied.x-preferred.w-gap,preferred.y,preferred.w,preferred.h},
  {preferred.x,occupied.y-preferred.h-gap,preferred.w,preferred.h},
  {preferred.x,occupied.y+occupied.h+gap,preferred.w,preferred.h},
  {occupied.x+occupied.w+gap,preferred.y,preferred.w,preferred.h}};
 Rect best=preferred;int64_t distance=INT64_MAX;
 for(auto r:choices){r=clamp(r,w,h);if(overlaps(r,occupied,gap))continue;
  const int64_t dx=r.x-preferred.x,dy=r.y-preferred.y,d=dx*dx+dy*dy;
  if(d<distance){distance=d;best=r;}}
 return best;
}
}
