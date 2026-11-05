#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
namespace external { namespace vulkancompute { namespace scenebins {
using Bounds=std::array<int,4>;
struct Cost {uint32_t size=32,columns=0,rows=0;uint64_t references=0,pixelVisits=0;};
struct Plan {
 Cost cost;std::vector<uint32_t>counts,starts,references,positions;
};
// Sum exactly how many bin references and pixel/draw tests this partition
// introduces. No per-bin allocations or speculative CSR builds are needed.
inline Cost measure(const std::vector<Bounds>&bounds,int width,int height,uint32_t size,Bounds clip={0,0,-1,-1}){
 const uint32_t shift=size==8?3:size==16?4:5;Cost out;out.size=size;out.columns=(uint32_t(width)+size-1)>>shift;out.rows=(uint32_t(height)+size-1)>>shift;
 clip[0]=std::clamp(clip[0],0,width);clip[1]=std::clamp(clip[1],0,height);clip[2]=clip[2]<0?width:std::clamp(clip[2],0,width);clip[3]=clip[3]<0?height:std::clamp(clip[3],0,height);
 for(const auto&b:bounds)if(b[0]<b[2]&&b[1]<b[3]){
  const uint32_t x0=uint32_t(b[0])>>shift,y0=uint32_t(b[1])>>shift,x1=uint32_t(b[2]-1)>>shift,y1=uint32_t(b[3]-1)>>shift;
  out.references+=uint64_t(x1-x0+1)*(y1-y0+1);
  out.pixelVisits+=uint64_t(std::max(0,std::min(clip[2],int((x1+1)*size))-std::max(clip[0],int(x0*size))))*std::max(0,std::min(clip[3],int((y1+1)*size))-std::max(clip[1],int(y0*size)));
 }return out;
}
inline Cost choose(const std::vector<Bounds>&bounds,int width,int height,uint64_t maximumReferences,Bounds clip={0,0,-1,-1}){
 const auto original=measure(bounds,width,height,32,clip);auto best=original;
 // A small bin is useful for tiny downscaled map tiles. Large full-resolution
 // sprites instead duplicate references, CPU CSR work and command uploads.
 // Keep those costs bounded by the original 32px plan; preserve its fallback.
 if(original.references>maximumReferences)return best;
 for(uint32_t size:{16u,8u}){auto candidate=measure(bounds,width,height,size,clip);
  if(uint64_t(candidate.columns)*candidate.rows>65536||candidate.references>maximumReferences||candidate.references>original.references*2)continue;
  if(candidate.pixelVisits*4>original.pixelVisits*3)continue;
  if(candidate.pixelVisits<best.pixelVisits)best=candidate;
 }return best;
}
inline bool build(const std::vector<Bounds>&bounds,int width,int height,Plan&out,uint64_t maximumReferences,Bounds clip={0,0,-1,-1}){
 if(width<1||height<1||width>8192||height>8192||bounds.size()>65536||maximumReferences>0xffffffffu)return false;
 for(const auto&b:bounds)if(b[0]<0||b[1]<0||b[0]>width||b[1]>height||b[2]<0||b[3]<0||b[2]>width||b[3]>height)return false;
 out.cost=choose(bounds,width,height,maximumReferences,clip);if(out.cost.references>maximumReferences)return false;
 const uint32_t shift=out.cost.size==8?3:out.cost.size==16?4:5,columns=out.cost.columns;const size_t bins=size_t(columns)*out.cost.rows;
 out.counts.assign(bins,0);out.starts.assign(bins+1,0);
 auto visit=[&](const Bounds&b,auto&&fn){if(b[0]>=b[2]||b[1]>=b[3])return;
  for(int y=b[1]>>shift;y<=(b[3]-1)>>shift;++y)for(int x=b[0]>>shift;x<=(b[2]-1)>>shift;++x)fn(size_t(y)*columns+x);
 };
 for(const auto&b:bounds)visit(b,[&](size_t bin){++out.counts[bin];});
 uint64_t references=0;for(size_t bin=0;bin<bins;++bin){out.starts[bin]=uint32_t(references);references+=out.counts[bin];}out.starts[bins]=uint32_t(references);
 out.references.resize(size_t(references));out.positions=out.starts;
 for(size_t n=0;n<bounds.size();++n)visit(bounds[n],[&](size_t bin){out.references[out.positions[bin]++]=uint32_t(n);});
 return references==out.cost.references;
}
}}}
