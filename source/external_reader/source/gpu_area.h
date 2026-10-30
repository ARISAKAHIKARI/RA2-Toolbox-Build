#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>
#include "vulkan_scene_types.h"
namespace external {namespace gpushroud {struct Plan;} namespace gpueffects {struct Plan;} namespace gpuarea {
constexpr uint32_t PageSize=128,PagePixels=PageSize*PageSize,BlockSize=4,PrefixSide=33,PrefixWords=PrefixSide*PrefixSide*3,MaximumPages=sizeof(void*)>=8?8192:2048,Missing=0xffffffffu;
inline uint64_t atlasBytes(size_t pages,uint64_t deviceLimit){
 const uint64_t required=uint64_t(pages)*PagePixels*4,maximum=uint64_t(MaximumPages)*PagePixels*4;
 uint64_t capacity=128ull*1024*1024;while(capacity<required&&capacity<maximum)capacity*=2;
 return capacity>=required&&capacity<=maximum&&capacity<=deviceLimit?capacity:0;
}
struct Page {uint32_t x=0,y=0;std::shared_ptr<const std::vector<uint32_t>>pixels;uint64_t version=0;
 std::shared_ptr<const std::vector<float>>depth;std::vector<uint32_t>draws;
};
struct Plan {
 int nativeWidth=0,nativeHeight=0,width=0,height=0;double spanX=0,spanY=0;uint32_t background=0xff080c08u;
 std::vector<Page>pages;std::shared_ptr<const std::vector<uint8_t>>fog;
 std::shared_ptr<const gpushroud::Plan>mask;
 std::shared_ptr<const gpueffects::Plan>effects;
 std::shared_ptr<const std::vector<vulkancompute::DrawJob>>objects;
};
inline bool valid(const Plan&p){
 if(p.nativeWidth<1||p.nativeHeight<1||p.nativeWidth>32768||p.nativeHeight>32768||p.width<1||p.height<1||p.width>2048||p.height>2048||
  !std::isfinite(p.spanX)||!std::isfinite(p.spanY)||p.spanX<=0||p.spanY<=0||p.spanX>p.nativeWidth||p.spanY>p.nativeHeight||p.pages.size()>MaximumPages||
  (p.fog&&p.fog->size()!=uint64_t(p.width)*p.height)||(p.fog&&p.mask))return false;
 const uint32_t cols=(p.nativeWidth+PageSize-1)/PageSize,rows=(p.nativeHeight+PageSize-1)/PageSize;std::vector<uint8_t>seen(size_t(cols)*rows);
 uint64_t refs=0;if(p.objects&&p.objects->size()>65536)return false;
 for(const auto&page:p.pages){if(page.x>=cols||page.y>=rows||!page.pixels||page.pixels->size()!=PagePixels||(page.depth&&page.depth->size()!=PagePixels)||seen[size_t(page.y)*cols+page.x]++)return false;
  if(!page.draws.empty()&&!p.objects)return false;uint32_t prior=0;bool first=true;for(auto n:page.draws){if(n>=p.objects->size()||(!first&&n<=prior)||++refs>2*1024*1024)return false;prior=n;first=false;}
 }
 return true;
}
// Slow independent area oracle. Production uses NativeCanvas::Filter on CPU
// and the GPU's integer coarse summed-area cache, not this loop.
inline bool reference(const Plan&p,std::vector<uint32_t>&out){
 if(!valid(p)||p.mask||p.effects||(p.objects&&!p.objects->empty()))return false;const uint32_t cols=(p.nativeWidth+127)/128,rows=(p.nativeHeight+127)/128;
 std::vector<const std::vector<uint32_t>*>table(size_t(cols)*rows,nullptr);for(const auto&page:p.pages)table[size_t(page.y)*cols+page.x]=page.pixels.get();
 std::vector<uint32_t>result(size_t(p.width)*p.height);double sx=p.spanX/p.width,sy=p.spanY/p.height;
 for(int y=0;y<p.height;++y)for(int x=0;x<p.width;++x){double l=std::clamp(x*sx,0.,double(p.nativeWidth)),r=std::clamp((x+1)*sx,0.,double(p.nativeWidth)),t=std::clamp(y*sy,0.,double(p.nativeHeight)),b=std::clamp((y+1)*sy,0.,double(p.nativeHeight));double sum[3]{};
  for(int yy=int(t);yy<int(std::ceil(b));++yy)for(int xx=int(l);xx<int(std::ceil(r));++xx){double weight=(std::min(r,double(xx+1))-std::max(l,double(xx)))*(std::min(b,double(yy+1))-std::max(t,double(yy)));const auto*page=table[size_t(yy/128)*cols+xx/128];uint32_t color=page?(*page)[size_t(yy%128)*128+xx%128]:p.background;for(unsigned c=0;c<3;++c)sum[c]+=weight*((color>>(16-c*8))&255);}
  uint32_t color=0xff000000u;for(unsigned c=0;c<3;++c)color|=uint32_t(std::clamp(std::floor(sum[c]/((r-l)*(b-t))+.5),0.,255.))<<(16-c*8);
  if(p.fog){unsigned a=std::min<unsigned>((*p.fog)[size_t(y)*p.width+x],127);uint32_t dark=0xff000000u;for(unsigned shift:{0u,8u,16u})dark|=((((color>>shift)&255)*a+63)/127)<<shift;color=dark;}result[size_t(y)*p.width+x]=color;
 }out=std::move(result);return true;
}
}}
