#pragma once
#include <algorithm>
#include <cstdint>
namespace assist { namespace statusbadges {
// Drawing and dirty bounds share one geometry and the overlay Motion clock.
// A map-cell clamp makes a smoothly sampled marker hold, then jump a cell.
constexpr int SpiderSize=20,BuffSize=9;
constexpr float SpiderScale=SpiderSize/14.f,SpiderExtent=8.6f*SpiderScale;
struct Point {float x=0,y=0;};
struct Layout {Point icons[3];unsigned kinds[3]{};int count=0;};
inline Layout layout(float x,float y,uint32_t buffs,bool parasite){
 Layout r;for(unsigned i=0;i<3;++i)if(buffs&(1u<<i))r.kinds[r.count++]=i;
 float below=parasite?SpiderExtent+BuffSize*.5f+2.f:8.f;
 for(int i=0;i<r.count;++i)r.icons[i]={x+(i-(r.count-1)*.5f)*11.f,y+below};
 return r;
}
struct Bounds {float x=0,y=0,w=0,h=0;};
inline Bounds bounds(float x,float y,uint32_t buffs,bool parasite){
 auto l=layout(x,y,buffs,parasite);if(!parasite&&!l.count)return {};
 float left=parasite?x-SpiderExtent:l.icons[0].x-5.f,top=parasite?y-SpiderExtent:l.icons[0].y-5.f;
 float right=parasite?x+SpiderExtent:left,bottom=parasite?y+SpiderExtent:top;
 for(int i=0;i<l.count;++i){auto p=l.icons[i];left=std::min(left,p.x-5.f);right=std::max(right,p.x+5.f);top=std::min(top,p.y-5.f);bottom=std::max(bottom,p.y+5.f);}
 return {left,top,right-left,bottom-top};
}
}}
