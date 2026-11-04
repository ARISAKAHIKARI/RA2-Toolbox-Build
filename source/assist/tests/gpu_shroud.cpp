#include "gpu_shroud_cases.h"
#include <cassert>
#include <chrono>
#include <cstdio>
int main(){
 using namespace external;auto g=shroudcases::geometry();assert(g.valid());auto cells=shroudcases::cells(g);
 gpushroud::Builder builder;uint64_t pixels=0,stamps=0;unsigned tested=0;double cpu4k=0,plan4k=0;
 std::shared_ptr<const gpushroud::Plan> retained;
 for(const auto&c:shroudcases::cases()){
  radar::shroudmask::Raster cpu;auto start=std::chrono::steady_clock::now();
  assert(cpu.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},c.x,c.y));
  const double cpuMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::shared_ptr<const gpushroud::Plan> plan;start=std::chrono::steady_clock::now();
  assert(builder.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},plan,c.x,c.y));
  const double planMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
  std::vector<uint8_t>actual;assert(gpushroud::reference(*plan,actual));assert(actual==cpu.alpha);
  gpushroud::Packed packed;assert(gpushroud::pack(*plan,packed));assert(plan->outputWords()*4>=actual.size()&&plan->outputWords()*4<actual.size()+4);
  pixels+=actual.size();stamps+=plan->stamps.size();++tested;
  if(c.w==3840){cpu4k=cpuMs;plan4k=planMs;retained=plan;std::printf("4K controlled shroud CPU_stamp_ms=%.3f CPU_GPUplan_ms=%.3f sources=%zu stamps=%zu coords=%zu binrefs=%zu packed_mask_bytes=%zu\n",cpuMs,planMs,plan->sources.size(),plan->stamps.size(),plan->coordinates.size(),plan->references.size(),plan->outputWords()*4);}
 }
 // Source ownership is stable for identical bytes and changes when the SHP
 // changes in-place; an old Plan keeps actual previous source storage alive.
 std::shared_ptr<const gpushroud::Plan> same,changed;auto c=shroudcases::cases().back();
 assert(builder.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},same,c.x,c.y));
 assert(same->sources.size()==retained->sources.size());
 for(size_t n=0;n<same->sources.size();++n)assert(same->sources[n].indices==retained->sources[n].indices);
 assert(builder.build(g,cells,c.w,c.h,c.spanX,c.spanY,c.fog,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i,1);},changed,c.x,c.y));
 for(size_t n=0;n<same->sources.size();++n)assert(changed->sources[n].indices!=same->sources[n].indices);
 auto invalid=std::make_shared<gpushroud::Plan>(*same);invalid->stamps.front().source[0]=9999;gpushroud::Packed bad;assert(!gpushroud::pack(*invalid,bad));
 auto badCells=cells;badCells[0].index=512*512;assert(!builder.build(g,badCells,240,120,240,120,true,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},changed));
 assert(!builder.build(g,cells,240,120,240,120,true,[](bool f,int n,voxel::Image&i){shroudcases::shape(f,n,i);i.canvasWidth=59;return true;},changed));
 assert(!builder.build(g,cells,240,120,std::numeric_limits<double>::max(),120,true,[](bool f,int n,voxel::Image&i){return shroudcases::shape(f,n,i);},changed));
 std::printf("PASS: GPU shroud integer upload plan %u cases, %llu exact native-mask pixels, %llu stamps; cropped/fog/order/downsample/owner/invalid checks; 4K CPU %.3fms plan %.3fms (synthetic, no GPU speed claim)\n",tested,(unsigned long long)pixels,(unsigned long long)stamps,cpu4k,plan4k);
}
