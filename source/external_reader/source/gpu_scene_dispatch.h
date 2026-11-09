#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace external { namespace scenedispatch {
struct Region {
 uint32_t x=0,y=0,groupsX=0,groupsY=0;
 uint64_t invocations()const{return uint64_t(groupsX)*groupsY*64;}
 uint32_t origin()const{return x|(y<<16);}
};
#ifdef RA2_SCENE_DISPATCH_TEST_ACCESS
inline bool& forceFull(){static thread_local bool value=false;return value;}
#endif
// The first pass covers the image to initialize Z/A and clear the clip exterior.
// A final pass is global only when it applies fog/guides. Other parallel phases
// only need the union of their own destinations. Displacement keeps its serial
// row-major shader and its original read/write order.
inline Region region(const std::vector<std::array<int,4>>&bounds,uint32_t first,uint32_t last,
                     int width,int height,bool initial,bool serial,bool globalFinal,
                     std::array<int,4>clip={0,0,-1,-1}){
 if(serial)return {0,0,1,1};
 const Region full{0,0,uint32_t((width+7)/8),uint32_t((height+7)/8)};
 if(initial||globalFinal)return full;
#ifdef RA2_SCENE_DISPATCH_TEST_ACCESS
 if(forceFull())return full;
#endif
 clip[0]=std::clamp(clip[0],0,width);clip[1]=std::clamp(clip[1],0,height);
 clip[2]=clip[2]<0?width:std::clamp(clip[2],0,width);
 clip[3]=clip[3]<0?height:std::clamp(clip[3],0,height);
 int left=width,top=height,right=0,bottom=0;
 for(uint32_t n=first;n<last&&n<bounds.size();++n){const auto&b=bounds[n];
  const int l=std::max(b[0],clip[0]),t=std::max(b[1],clip[1]);
  const int r=std::min(b[2],clip[2]),e=std::min(b[3],clip[3]);
  if(l>=r||t>=e)continue;
  left=std::min(left,l);top=std::min(top,t);right=std::max(right,r);bottom=std::max(bottom,e);
 }
 if(left>=right||top>=bottom)return {};
 left&=~7;top&=~7;
 return {uint32_t(left),uint32_t(top),uint32_t((right-left+7)/8),uint32_t((bottom-top+7)/8)};
}
}}
