#pragma once
#include "vulkan_scene_types.h"
namespace external { namespace vulkancompute {
inline uint32_t drawBits(float v){uint32_t u;std::memcpy(&u,&v,4);return u;}
inline std::array<int,4> drawBounds(const DrawJob&d,int w,int h){
 auto edge=[](float v,int limit,bool upper){double x=std::clamp(double(v),-1e7,1e7);return std::clamp(int(upper?std::ceil(x):std::floor(x)),0,limit);};
 int top=edge(d.top,h,false);return {{edge(d.left,w,false),top,edge(d.left+d.source.width*d.scaleX,w,true),std::max(top,edge(std::min(d.top+d.source.height*d.scaleY,d.clipBottom-.5f),h,true))}};
}
// Shared by viewport and sparse native-page shaders; offsets are uint words.
inline void encodeDraw(const DrawJob&d,const std::array<uint32_t,7>&source,const std::array<int,4>&bounds,uint32_t*dst){
 std::fill_n(dst,64,0);dst[0]=source[0];dst[1]=source[1];dst[2]=source[2];dst[3]=d.source.width;dst[4]=d.source.height;
 dst[5]=source[3];dst[6]=d.reconstructedWidth;dst[7]=d.reconstructedHeight;dst[8]=d.reconstructionFactor;
 dst[9]=(d.shadow?1:0)|(d.writeDepth?2:0)|(d.flat?4:0)|(d.objectPalette?8:0)|(d.smoothRGB?16:0)|(d.skipZ?32:0)|(d.gpuRaster?64:0)|(d.translucency<<8)|(d.mixStrength<<16);
 dst[10]=uint32_t(d.sampling);dst[11]=d.rasterScale;dst[12]=uint32_t(d.originY);dst[13]=uint32_t(d.depthBottom);
 dst[14]=drawBits(d.left);dst[15]=drawBits(d.top);dst[16]=drawBits(d.scaleX);dst[17]=drawBits(d.scaleY);dst[18]=drawBits(d.depthBase);dst[19]=drawBits(d.lift);
 for(int n=0;n<3;++n)dst[20+n]=drawBits(d.gains[n]);dst[23]=d.nativeBlend;for(int n=0;n<4;++n)dst[24+n]=bounds[n];dst[31]=source[4];
 dst[32]=(d.nativeDepth.enabled?1u:0u)|(d.nativeDepth.tileEqual?2u:0u)|(d.nativeWalk.divided?4u:0u)|(d.palette.rgb555?8u:0u)|(d.palette.scalar?16u:0u)|(d.displace?32u:0u);
 const auto&a=d.nativeWalk.upper;const auto&b=d.nativeWalk.lower;
 dst[33]=uint32_t(a.seed);dst[34]=a.increment;dst[35]=a.threshold;dst[36]=uint32_t(a.step);dst[37]=a.accumulator;dst[38]=d.nativeWalk.first;
 dst[39]=uint32_t(b.seed);dst[40]=b.accumulator;dst[41]=d.nativeWalk.split;dst[42]=b.increment;dst[43]=b.threshold;dst[44]=uint32_t(b.step);
 dst[45]=source[5];dst[46]=source[6];dst[47]=d.palette.rows;dst[48]=uint32_t(d.palette.brightness);
 for(unsigned n=0;n<3;++n)dst[49+n]=d.palette.scale[n];dst[52]=d.palette.special;dst[53]=d.palette.colour;dst[54]=uint32_t(d.cloakWords);
 dst[55]=drawBits(d.nativeScaleX);dst[56]=drawBits(d.nativeScaleY);dst[57]=d.nativePitch;
}
// VERA read-only interval lowering, adapted to our per-pixel compute owner.
// No sorting occurs: an interval ends at every actual Z writer in this bin.
inline void encodeReadOnlyRuns(const std::vector<DrawJob>&draws,std::vector<uint32_t>&refs,const std::vector<uint32_t>&starts){
 for(size_t bin=0;bin+1<starts.size();++bin)for(uint32_t i=starts[bin];i<starts[bin+1];){
  if(draws[refs[i]].writeDepth||draws[refs[i]].displace){++i;continue;}uint32_t end=i+1;
  while(end<starts[bin+1]&&!draws[refs[end]].writeDepth&&!draws[refs[end]].displace&&end-i<65536)++end;
  if(end-i>2)refs[i]|=(end-i-1)<<16;i=end;
 }
}
}}
