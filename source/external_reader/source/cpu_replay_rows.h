#pragma once
#include "../../shared/source/parallel_rows.h"
namespace external { namespace camera { namespace cpureplay {
// A destination-reading material is an ordered whole-target barrier. Ordinary
// alpha/packed/shadow blending reads only its own pixel and is row-independent.
// Keep deferred rasterization and xBRZ on the owner: splitting their source
// preparation among row workers was the expensive HF28 replay behavior.
inline bool rowIndependent(const voxel::Image&i,float sx,float sy,bool smooth,unsigned nesting=0){
 if(nesting>16||i.gpuRaster)return false;
 if(smooth&&!i.shadow&&i.rasterScale==1&&(sx>1||sy>1))return false;
 for(const auto&part:i.attachments)if(!rowIndependent(part,sx,sy,smooth,nesting+1))return false;
 return true;
}
inline size_t work(const voxel::Image&i,float sx,float sy,unsigned nesting=0){
 if(nesting>16||i.width<=0||i.height<=0||i.rasterScale<1||!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0)return 0;
 const double pixels=double(i.width)*i.height*(sx/i.rasterScale)*(sy/i.rasterScale);
 size_t out=size_t(std::min(16777216.,std::max(0.,pixels)));
 for(const auto&part:i.attachments)out=std::min<size_t>(16777216,out+work(part,sx,sy,nesting+1));return out;
}
struct Stats {size_t rowSegments=0,serialCalls=0,barriers=0;};
}}}
