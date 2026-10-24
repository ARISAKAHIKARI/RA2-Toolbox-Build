#pragma once
#include "radar_map.h"
namespace external { namespace radar {
// Client coordinates include the one physical pixel frame. Invert the same
// projection used by the terrain and camera rectangle. Native positioning
// accepts the center and subtracts the viewport half-size itself.
inline bool navigate(const Geometry&g,int px,int py,int width,int height,int viewWidth,int viewHeight,int&x,int&y){
 if(width<=2||height<=2||viewWidth<=0||viewHeight<=0||g.rw<=0||g.rh<=0||!std::isfinite(g.factor)||g.factor<=0)return false;
 double u=std::clamp(double(px-1)/(width-2),0.,1.),v=std::clamp(double(py-1)/(height-2),0.,1.);
 double sx=(u*g.rw/g.factor-g.offsetX)*30,sy=(v*g.rh/g.factor+g.offsetY)*15;
 if(!std::isfinite(sx)||!std::isfinite(sy)||std::abs(sx)>100000||std::abs(sy)>100000)return false;
 x=int(std::lround(sx));y=int(std::lround(sy));return true;
}
}}
