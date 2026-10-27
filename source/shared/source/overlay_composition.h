#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <vector>
// Host-only, platform-independent layer contract. Pixels are immutable PARGB.
// A view token is invalid after a match/window/size change. Camera-dependent
// surfaces carry the camera they were actually rendered for, not a guess.
namespace overlaycomposition {
struct Rect {int x=0,y=0,w=0,h=0;};
inline bool operator==(const Rect&a,const Rect&b){return a.x==b.x&&a.y==b.y&&a.w==b.w&&a.h==b.h;}
inline bool operator!=(const Rect&a,const Rect&b){return !(a==b);}
struct Context {
 uint64_t token=0;uintptr_t window=0;uint32_t pid=0,match=0;
 int width=0,height=0,cameraX=0,cameraY=0;
};
struct Surface {
 uint64_t id=0;int order=0;Context context{};Rect rect{};
 bool cameraBound=false;uint8_t opacity=255;
 std::shared_ptr<const std::vector<uint32_t>> pixels;
};
inline bool valid(const Surface&s,const Context&c){
 return c.token&&s.context.token==c.token&&s.context.window==c.window&&s.context.pid==c.pid&&
  s.context.match==c.match&&s.context.width==c.width&&s.context.height==c.height&&
  (!s.cameraBound||(s.context.cameraX==c.cameraX&&s.context.cameraY==c.cameraY))&&
  s.id&&s.rect.w>0&&s.rect.h>0&&s.rect.w<=8192&&s.rect.h<=8192&&
  uint64_t(s.rect.w)*s.rect.h<=16*1024*1024&&s.pixels&&
  s.pixels->size()==size_t(s.rect.w)*s.rect.h;
}
inline uint32_t over(uint32_t source,uint32_t destination,unsigned opacity){
 const unsigned alpha=((source>>24)*opacity+127)/255;
 if(!alpha)return destination;
 if(alpha==255)return source;
 const unsigned inv=255-alpha;uint32_t out=0;
 for(int shift:{0,8,16,24}){
  unsigned a=(((source>>shift)&255)*opacity+127)/255;
  unsigned b=(((destination>>shift)&255)*inv+127)/255;
  out|=std::min(255u,a+b)<<shift;
 }return out;
}
inline bool compose(const Context&context,std::vector<Surface>layers,std::vector<uint32_t>&pixels){
 if(context.width<1||context.height<1||context.width>8192||context.height>8192||
    uint64_t(context.width)*context.height>16*1024*1024)return false;
 layers.erase(std::remove_if(layers.begin(),layers.end(),[&](const Surface&s){return !valid(s,context);}),layers.end());
 std::stable_sort(layers.begin(),layers.end(),[](const Surface&a,const Surface&b){return a.order!=b.order?a.order<b.order:a.id<b.id;});
 pixels.assign(size_t(context.width)*context.height,0);
 for(const auto&s:layers){
  const auto&r=s.rect;
  const int left=int(std::max<int64_t>(0,r.x)),top=int(std::max<int64_t>(0,r.y));
  const int right=int(std::min<int64_t>(context.width,int64_t(r.x)+r.w)),bottom=int(std::min<int64_t>(context.height,int64_t(r.y)+r.h));
  if(left>=right||top>=bottom)continue;
  for(int y=top;y<bottom;++y){
   const auto*src=s.pixels->data()+size_t(int64_t(y)-r.y)*r.w+int64_t(left)-r.x;
   auto*dst=pixels.data()+size_t(y)*context.width+left;
   for(int x=left;x<right;++x,++src,++dst)*dst=over(*src,*dst,s.opacity);
  }
 }return true;
}
}
