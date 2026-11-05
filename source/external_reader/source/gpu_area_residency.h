#pragma once
#include "gpu_area.h"
#include "gpu_draw_encoding.h"
#include <numeric>
namespace external {namespace gpuarea {
inline uint64_t pageKey(uint32_t x,uint32_t y){return (uint64_t(x)<<32)|y;}
// A native cloak reads another framebuffer location in painter order. Both
// endpoints must be rebuilt together: a cached source contains its FINAL
// color, which may include draws later than the cloak. Missing old pages
// also invalidate readers even though there is no current job for that page.
struct DirtyClosure {size_t dependencies=0;bool full=false;};
inline DirtyClosure expandNativeDirty(const Plan&p,std::vector<size_t>&dirty,const std::vector<uint64_t>&removed){
 DirtyClosure result;if(!p.nativeSurface||!p.objects||p.pages.empty()||dirty.size()==p.pages.size())return result;
 const bool cloak=std::any_of(p.objects->begin(),p.objects->end(),[](const auto&d){return d.displace;});if(!cloak)return result;
 auto full=[&]{dirty.resize(p.pages.size());std::iota(dirty.begin(),dirty.end(),size_t(0));result.full=true;return result;};
 const uint32_t cols=(p.nativeWidth+127)/128,rows=(p.nativeHeight+127)/128;const size_t total=size_t(cols)*rows;
 std::vector<uint32_t>parent(total);std::iota(parent.begin(),parent.end(),uint32_t(0));std::vector<uint8_t>linked(total),invalid(total);
 auto root=[&](uint32_t a){uint32_t r=a;while(parent[r]!=r)r=parent[r];while(parent[a]!=r){auto next=parent[a];parent[a]=r;a=next;}return r;};
 auto join=[&](uint32_t a,uint32_t b){if(!linked[a]){linked[a]=1;++result.dependencies;}if(!linked[b]){linked[b]=1;++result.dependencies;}a=root(a);b=root(b);if(a!=b)parent[b]=a;};
 size_t visits=0;
 for(const auto&d:*p.objects)if(d.displace){
  // ObjectCanvas records native pages at unit scale/pitch. A hand-built or
  // future scaled plan keeps the established full replay rather than use a
  // different rounding rule from the shader's nativeScale mapping.
  if(d.nativeScaleX!=1||d.nativeScaleY!=1||d.nativePitch!=p.nativeWidth||!std::isfinite(d.left)||!std::isfinite(d.top)||!std::isfinite(d.scaleX)||!std::isfinite(d.scaleY)||!std::isfinite(d.clipBottom))return full();
  const auto bounds=vulkancompute::drawBounds(d,p.nativeWidth,p.nativeHeight);const int64_t limit=int64_t(p.nativeWidth)*p.nativeHeight;
  for(int y=bounds[1];y<bounds[3];++y)for(int x=bounds[0];x<bounds[2];){
   if(++visits>1024*1024)return full();
   const int end=std::min(bounds[2],(x/128+1)*128);const auto dest=uint32_t(y/128)*cols+uint32_t(x/128);
   int64_t first=int64_t(y)*p.nativeWidth+x+int64_t(d.cloakWords),last=first+end-x;first=std::max(int64_t(0),first);last=std::min(limit,last);
   while(first<last){const int sx=int(first%p.nativeWidth),sy=int(first/p.nativeWidth);join(dest,uint32_t(sy/128)*cols+uint32_t(sx/128));
    first+=std::min({last-first,int64_t(p.nativeWidth-sx),int64_t(128-sx%128)});
   }x=end;
  }
 }
 for(size_t n:dirty){const auto&a=p.pages[n];invalid[root(a.y*cols+a.x)]=1;}
 for(uint64_t key:removed){const uint32_t x=uint32_t(key>>32),y=uint32_t(key);if(x<cols&&y<rows)invalid[root(y*cols+x)]=1;}
 std::vector<size_t>expanded;expanded.reserve(dirty.size());for(size_t n=0;n<p.pages.size();++n){const auto&a=p.pages[n];if(invalid[root(a.y*cols+a.x)])expanded.push_back(n);}
 dirty=std::move(expanded);return result;
}
}}
