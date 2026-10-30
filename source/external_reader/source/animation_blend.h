#pragma once
#include <cstdint>
namespace external { namespace animations {
// Anim::Draw_It 423061..4231F3: the stored fade level is 0..15, not a
// percentage. The animation's current Value drives Translucent's 20/40/60%
// stages. Keep the x87 comparison boundary: binary .6 is slightly below 3/5,
// whereas .4 and .2 are above their fractions (integer equality differs).
inline int blend(uint32_t flags,bool temporal,bool doubleThick,int detail,int threshold,bool fading,int value,int end,int translucency,unsigned level){
 if(temporal)flags|=doubleThick?6u:4u;
 if(threshold<=detail){
  if(fading){
   if(level>=15)return -1;const int64_t scaled=int64_t(value)*5;
   if(scaled>int64_t(end)*3||(end>0&&scaled==int64_t(end)*3))flags|=6;
   else if(scaled>int64_t(end)*2)flags|=4;
   else if(scaled>end)flags|=2;
  }else if(translucency>0){
   if(level>=15)return -1;
   if(translucency==25)flags|=2;else if(translucency==50)flags|=4;else if(translucency==75)flags|=6;
  }else if(level){if(level>15)return -1;flags|=level<=5?2u:4u;}
 }
 return int((flags>>1)&3);
}
inline unsigned opacity(int blend){constexpr unsigned alpha[4]={255,192,128,64};return alpha[unsigned(blend)&3];}
}}
