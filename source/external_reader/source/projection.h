#pragma once
#include <cmath>
#include <cstdint>
namespace external {
// Pure coordinate math shared by host display and click-time planning.
struct Projection {int cameraX=0,cameraY=0,width=0,height=0;double zScale=0.0866025403784;};
inline bool project(const Projection&p,int x,int y,int z,int&sx,int&sy){
 if(std::abs(int64_t(x))>1048576||std::abs(int64_t(y))>1048576||std::abs(int64_t(z))>1048576||!std::isfinite(p.zScale)||p.zScale<0||p.zScale>1)return false;
 sx=int((int64_t(x)-y)*30/256)-p.cameraX;sy=int((int64_t(x)+y)*15/256)-int(z*p.zScale+int(z>=728)+.5)-p.cameraY;
 return sx>=-80&&sx<p.width+80&&sy>=-60&&sy<p.height+60;
}
}
