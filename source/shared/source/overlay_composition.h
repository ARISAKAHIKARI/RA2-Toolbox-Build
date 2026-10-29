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
 // Optional destination clip. Overscan scenes can share their original
 // immutable pixels while presenting only inside the current game viewport.
 Rect clip{};
 // Optional sparse horizontal spans left transparent in this layer. Relative
 // to rect's source pixels, preclipped and single-row; other layers still draw.
 std::shared_ptr<const std::vector<Rect>> cutouts;
};
// Producers rebuild small masks at the input cadence. Equal spans do not
// change the image merely because their vector was allocated again.
inline bool sameCutouts(const std::shared_ptr<const std::vector<Rect>>&a,const std::shared_ptr<const std::vector<Rect>>&b){
 if(a==b)return true;
 if(!a)return b->empty();if(!b)return a->empty();return *a==*b;
}
inline bool valid(const Surface&s,const Context&c){
 bool image=c.token&&s.context.token==c.token&&s.context.window==c.window&&s.context.pid==c.pid&&
  s.context.match==c.match&&s.context.width==c.width&&s.context.height==c.height&&
  (!s.cameraBound||(s.context.cameraX==c.cameraX&&s.context.cameraY==c.cameraY))&&
  s.id&&s.rect.w>0&&s.rect.h>0&&s.rect.w<=8192&&s.rect.h<=8192&&
  uint64_t(s.rect.w)*s.rect.h<=16*1024*1024&&s.pixels&&
  s.pixels->size()==size_t(s.rect.w)*s.rect.h;
 if(!image)return false;
 if(s.cutouts){if(s.cutouts->size()>16384)return false;for(const auto&r:*s.cutouts)
  if(r.x<0||r.y<0||r.w<=0||r.h!=1||int64_t(r.x)+r.w>s.rect.w||r.y>=s.rect.h)return false;}
 return true;
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
inline Rect bounds(const Surface&s,const Context&context){
 int64_t left=std::max<int64_t>(0,s.rect.x),top=std::max<int64_t>(0,s.rect.y);
 int64_t right=std::min<int64_t>(context.width,int64_t(s.rect.x)+s.rect.w),bottom=std::min<int64_t>(context.height,int64_t(s.rect.y)+s.rect.h);
 if(s.clip.w>0&&s.clip.h>0){
  left=std::max<int64_t>(left,s.clip.x);top=std::max<int64_t>(top,s.clip.y);
  right=std::min<int64_t>(right,int64_t(s.clip.x)+s.clip.w);bottom=std::min<int64_t>(bottom,int64_t(s.clip.y)+s.clip.h);
 }
 return right>left&&bottom>top?Rect{int(left),int(top),int(right-left),int(bottom-top)}:Rect{};
}
inline Rect intersection(const Rect&a,const Rect&b){
 const int64_t left=std::max<int64_t>(a.x,b.x),top=std::max<int64_t>(a.y,b.y);
 const int64_t right=std::min<int64_t>(int64_t(a.x)+a.w,int64_t(b.x)+b.w),bottom=std::min<int64_t>(int64_t(a.y)+a.h,int64_t(b.y)+b.h);
 return a.w>0&&a.h>0&&b.w>0&&b.h>0&&right>left&&bottom>top?Rect{int(left),int(top),int(right-left),int(bottom-top)}:Rect{};
}
inline void order(std::vector<Surface>&layers,const Context&context){
 layers.erase(std::remove_if(layers.begin(),layers.end(),[&](const Surface&s){return !valid(s,context);}),layers.end());
 std::stable_sort(layers.begin(),layers.end(),[](const Surface&a,const Surface&b){return a.order!=b.order?a.order<b.order:a.id<b.id;});
}
inline void paint(const Context&context,const std::vector<Surface>&layers,std::vector<uint32_t>&pixels,const Rect&dirty){
 if(dirty.w<=0||dirty.h<=0)return;
 for(int y=dirty.y;y<dirty.y+dirty.h;++y)std::fill_n(pixels.data()+size_t(y)*context.width+dirty.x,dirty.w,0);
 for(const auto&s:layers){
  const auto&r=s.rect;const auto clipped=intersection(bounds(s,context),dirty);if(clipped.w<=0||clipped.h<=0)continue;
  const int left=clipped.x,top=clipped.y,right=left+clipped.w,bottom=top+clipped.h;
  std::vector<Rect>holes;if(s.cutouts){holes=*s.cutouts;std::sort(holes.begin(),holes.end(),[](const Rect&a,const Rect&b){return a.y!=b.y?a.y<b.y:a.x<b.x;});}
  for(int y=top;y<bottom;++y){
   auto draw=[&](int first,int end){
    const auto*src=s.pixels->data()+size_t(int64_t(y)-r.y)*r.w+int64_t(first)-r.x;
    auto*dst=pixels.data()+size_t(y)*context.width+first;
    for(int x=first;x<end;++x,++src,++dst)*dst=over(*src,*dst,s.opacity);
   };
   int first=left;
   auto hole=std::lower_bound(holes.begin(),holes.end(),y-r.y,[](const Rect&a,int yy){return a.y<yy;});
   for(;hole!=holes.end()&&hole->y==y-r.y;++hole){int begin=int(std::max<int64_t>(left,int64_t(r.x)+hole->x)),end=int(std::min<int64_t>(right,int64_t(r.x)+hole->x+hole->w));if(end<=begin)continue;if(begin>first)draw(first,begin);first=std::max(first,end);}
   if(first<right)draw(first,right);
  }
 }
}
inline bool compose(const Context&context,std::vector<Surface>layers,std::vector<uint32_t>&pixels){
 if(context.width<1||context.height<1||context.width>8192||context.height>8192||
    uint64_t(context.width)*context.height>16*1024*1024)return false;
 order(layers,context);pixels.resize(size_t(context.width)*context.height);
 paint(context,layers,pixels,{0,0,context.width,context.height});return true;
}
// Small feedback changes must not scan a full 4K scene. Keep distant affected
// bounds separate: a cursor and a counter at opposite edges must not repaint
// every pixel between them. Each region is cleared and replayed from all
// current layers, including overlaps, so alpha/removal/order leave no trails.
// The region list is bounded; its conservative union remains available to
// existing callers and is also the fallback for unusually fragmented work.
class Incremental {
 Context context_{};std::vector<Surface>layers_;std::vector<uint32_t>pixels_;Rect dirty_{};
 std::vector<Rect>dirtyRegions_;bool collapsed_=false;
 static bool sameBase(const Surface&a,const Surface&b){return a.id==b.id&&a.order==b.order&&a.rect==b.rect&&a.clip==b.clip&&a.opacity==b.opacity&&a.pixels==b.pixels;}
 static bool same(const Surface&a,const Surface&b){return sameBase(a,b)&&sameCutouts(a.cutouts,b.cutouts);}
 void holes(const Surface&s,const Context&context){
  const auto clip=bounds(s,context);if(!s.cutouts||clip.w<=0||clip.h<=0)return;
  for(const auto&r:*s.cutouts){const int64_t x=int64_t(s.rect.x)+r.x,y=int64_t(s.rect.y)+r.y;
   const int64_t left=std::max<int64_t>(clip.x,x),right=std::min<int64_t>(clip.x+clip.w,x+r.w);
   if(y>=clip.y&&y<clip.y+clip.h&&right>left)include({int(left),int(y),int(right-left),1});
  }
 }
 static Rect unite(const Rect&a,const Rect&b){
  if(a.w<=0||a.h<=0)return b;if(b.w<=0||b.h<=0)return a;
  const int left=std::min(a.x,b.x),top=std::min(a.y,b.y),right=std::max(a.x+a.w,b.x+b.w),bottom=std::max(a.y+a.h,b.y+b.h);
  return {left,top,right-left,bottom-top};
 }
 static uint64_t area(const Rect&r){return uint64_t(r.w)*r.h;}
 void include(Rect r){
  if(r.w<=0||r.h<=0)return;dirty_=unite(dirty_,r);
  if(collapsed_){dirtyRegions_[0]=dirty_;return;}
  for(size_t n=0;n<dirtyRegions_.size();){const auto&old=dirtyRegions_[n];const auto combined=unite(old,r);
   const bool touching=r.x<=old.x+old.w&&old.x<=r.x+r.w&&r.y<=old.y+old.h&&old.y<=r.y+r.h;
   // Nearby rectangles only coalesce if the bounding box costs no more than
   // replaying both. Thin crossing strips therefore stay separate.
   if(touching&&area(combined)<=area(old)+area(r)){r=combined;dirtyRegions_.erase(dirtyRegions_.begin()+n);n=0;}else ++n;
  }
  if(dirtyRegions_.size()==MaximumDirtyRegions){dirtyRegions_.assign(1,dirty_);collapsed_=true;}
  else dirtyRegions_.push_back(r);
 }
public:
 static constexpr size_t MaximumDirtyRegions=64;
 const std::vector<uint32_t>&pixels()const{return pixels_;}
 Rect dirty()const{return dirty_;}
 const std::vector<Rect>&dirtyRegions()const{return dirtyRegions_;}
 bool update(const Context&context,std::vector<Surface>layers){
  if(context.width<1||context.height<1||context.width>8192||context.height>8192||uint64_t(context.width)*context.height>16*1024*1024)return false;
  order(layers,context);dirty_={};dirtyRegions_.clear();collapsed_=false;
  bool full=context_.token!=context.token||context_.window!=context.window||context_.pid!=context.pid||context_.match!=context.match||context_.width!=context.width||context_.height!=context.height||pixels_.size()!=size_t(context.width)*context.height;
  auto duplicate=[](const std::vector<Surface>&surfaces){for(size_t n=0;n<surfaces.size();++n)for(size_t m=0;m<n;++m)if(surfaces[n].id==surfaces[m].id)return true;return false;};
  full=full||duplicate(layers)||duplicate(layers_);
  if(full){pixels_.resize(size_t(context.width)*context.height);include({0,0,context.width,context.height});}
  else{
   for(const auto&old:layers_){auto next=std::find_if(layers.begin(),layers.end(),[&](const Surface&s){return s.id==old.id;});if(next==layers.end()||!same(old,*next)){
    if(next!=layers.end()&&sameBase(old,*next)){holes(old,context_);holes(*next,context);}
    else{include(bounds(old,context_));if(next!=layers.end())include(bounds(*next,context));}
   }}
   for(const auto&next:layers)if(std::none_of(layers_.begin(),layers_.end(),[&](const Surface&s){return s.id==next.id;}))include(bounds(next,context));
  }
  // Replaying overlapping strips is safe, but never do more pixel work than
  // the previous single-union path when several such strips become dense.
  uint64_t work=0;for(const auto&r:dirtyRegions_)work+=area(r);
  if(dirtyRegions_.size()>1&&work>=area(dirty_))dirtyRegions_.assign(1,dirty_);
  for(const auto&r:dirtyRegions_)paint(context,layers,pixels_,r);
  context_=context;layers_=std::move(layers);return true;
 }
};
}
