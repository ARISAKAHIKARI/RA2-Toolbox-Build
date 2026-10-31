#pragma once
#include "vulkan_scene_types.h"
namespace external { namespace vulkancompute {
inline uint32_t drawBits(float v){uint32_t u;std::memcpy(&u,&v,4);return u;}
inline std::array<int,4> drawBounds(const DrawJob&d,int w,int h){
 auto edge=[](float v,int limit,bool upper){double x=std::clamp(double(v),-1e7,1e7);return std::clamp(int(upper?std::ceil(x):std::floor(x)),0,limit);};
 int top=edge(d.top,h,false);return {{edge(d.left,w,false),top,edge(d.left+d.source.width*d.scaleX,w,true),std::max(top,edge(std::min(d.top+d.source.height*d.scaleY,d.clipBottom-.5f),h,true))}};
}
// Shared by viewport and sparse native-page shaders; offsets are uint words.
inline void encodeDraw(const DrawJob&d,const std::array<uint32_t,5>&source,const std::array<int,4>&bounds,uint32_t*dst){
 std::fill_n(dst,32,0);dst[0]=source[0];dst[1]=source[1];dst[2]=source[2];dst[3]=d.source.width;dst[4]=d.source.height;
 dst[5]=source[3];dst[6]=d.reconstructedWidth;dst[7]=d.reconstructedHeight;dst[8]=d.reconstructionFactor;
 dst[9]=(d.shadow?1:0)|(d.writeDepth?2:0)|(d.flat?4:0)|(d.objectPalette?8:0)|(d.smoothRGB?16:0)|(d.skipZ?32:0)|(d.gpuRaster?64:0)|(d.translucency<<8)|(d.mixStrength<<16);
 dst[10]=uint32_t(d.sampling);dst[11]=d.rasterScale;dst[12]=uint32_t(d.originY);dst[13]=uint32_t(d.depthBottom);
 dst[14]=drawBits(d.left);dst[15]=drawBits(d.top);dst[16]=drawBits(d.scaleX);dst[17]=drawBits(d.scaleY);dst[18]=drawBits(d.depthBase);dst[19]=drawBits(d.lift);
 for(int n=0;n<3;++n)dst[20+n]=drawBits(d.gains[n]);dst[23]=d.nativeBlend;for(int n=0;n<4;++n)dst[24+n]=bounds[n];dst[31]=source[4];
}
// VERA read-only interval lowering, adapted to our per-pixel compute owner.
// No sorting occurs: an interval ends at every actual Z writer in this bin.
inline void encodeReadOnlyRuns(const std::vector<DrawJob>&draws,std::vector<uint32_t>&refs,const std::vector<uint32_t>&starts){
 for(size_t bin=0;bin+1<starts.size();++bin)for(uint32_t i=starts[bin];i<starts[bin+1];){
  if(draws[refs[i]].writeDepth){++i;continue;}uint32_t end=i+1;
  while(end<starts[bin+1]&&!draws[refs[end]].writeDepth&&end-i<65536)++end;
  if(end-i>2)refs[i]|=(end-i-1)<<16;i=end;
 }
}
}}
