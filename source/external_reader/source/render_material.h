#pragma once
#include <algorithm>
#include <cstdint>
namespace external { namespace camera {
// Instance state, deliberately separate from shared decoded SHP/VXL geometry.
// clipBottom is a native-pixel offset from the instance anchor, not the camera.
struct Material {
 unsigned translucency=0,mixStrength=0;
 float clipBottom=1e7f;
 int nativeZAdjust=-2,cloakWords=0;bool nativeZ=false,bridgeSplit=false,shpFlat=false,displace=false,rgb555=false;
 bool operator==(const Material&o)const{return translucency==o.translucency&&mixStrength==o.mixStrength&&clipBottom==o.clipBottom&&nativeZAdjust==o.nativeZAdjust&&cloakWords==o.cloakWords&&nativeZ==o.nativeZ&&bridgeSplit==o.bridgeSplit&&shpFlat==o.shpFlat&&displace==o.displace&&rgb555==o.rgb555;}
 Material shifted(float y)const{auto out=*this;out.clipBottom-=y;out.nativeZAdjust+=int(y);return out;}
};
inline uint32_t materialColor(uint32_t source,uint32_t destination,unsigned selector){
 if(!selector)return source;unsigned weight=selector==1?3:selector==2?2:1;uint32_t out=0;
 for(int shift:{0,8,16,24})out|=((((source>>shift)&255)*weight+((destination>>shift)&255)*(4-weight))/4)<<shift;
 return out;
}
}}
