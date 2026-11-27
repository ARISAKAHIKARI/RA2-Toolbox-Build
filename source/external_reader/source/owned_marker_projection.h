#pragma once
#include "projection.h"
#include <cstdint>
namespace external { namespace ownedmarkers {
// Own-unit anchors are independent of the screen rectangle. They are clipped
// by the presenter, so crossing a viewport edge is not a data acquisition.
inline bool anchor(const Projection&p,int x,int y,int z,int&ax,int&ay){
 if(std::abs(int64_t(x))>1048576||std::abs(int64_t(y))>1048576||
    std::abs(int64_t(z))>1048576||!std::isfinite(p.zScale)||p.zScale<0||p.zScale>1)return false;
 ax=int((int64_t(x)-y)*30/256);
 ay=int((int64_t(x)+y)*15/256)-screenHeight(z,p.zScale);
 return true;
}
inline float screen(int anchor,int camera){return float(int64_t(anchor)-camera);}
inline bool local(uint32_t owner,uint32_t house){return house!=0&&owner==house;}
}}
