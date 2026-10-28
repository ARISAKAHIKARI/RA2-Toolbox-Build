#pragma once
#include "../../third_party/xbrz/xbrz.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>

// The upstream scaler stays unmodified. This adapter handles our premultiplied
// surfaces, physical crop coordinates and bounded caches; it adds no edge rules.
namespace external { namespace xbrzadapter {
constexpr size_t MaxPixels=16*1024*1024, CachePixels=4*1024*1024;
struct ScaledImage {int width=0,height=0,factor=0;std::vector<uint32_t> pixels;};
inline uint32_t straight(uint32_t p){
 const unsigned a=p>>24;if(!a)return 0;if(a==255)return p;
 uint32_t out=a<<24;for(int s:{0,8,16})out|=std::min(255u,(((p>>s)&255)*255+a/2)/a)<<s;return out;
}
inline uint32_t premultiplied(uint32_t p){
 const unsigned a=p>>24;uint32_t out=a<<24;
 for(int s:{0,8,16})out|=((((p>>s)&255)*a+127)/255)<<s;return out;
}
inline bool scale(const std::vector<uint32_t>&src,int w,int h,int factor,ScaledImage&out){
 if(w<1||h<1||w>8192||h>8192||factor<2||factor>4||
    uint64_t(w)*factor>8192||uint64_t(h)*factor>8192||
    uint64_t(w)*h*factor*factor>MaxPixels||src.size()!=size_t(w)*h)return false;
 std::vector<uint32_t> input(src.size());std::transform(src.begin(),src.end(),input.begin(),straight);
 out.width=w*factor;out.height=h*factor;out.factor=factor;out.pixels.resize(size_t(out.width)*out.height);
 // Avoid the upstream 64 MiB distance table in the 32-bit host.
 xbrz::scale(factor,input.data(),out.pixels.data(),w,h,xbrz::ColorFormat::argbUnbuffered);
 for(auto&p:out.pixels)p=premultiplied(p);return true;
}
// Bilinear resolve of the xBRZ image to the exact output ratio. Coordinates
// refer to source pixel boundaries: (.5,.5) is the first native pixel center.
inline uint32_t sample(const ScaledImage&i,double x,double y){
 if(i.width<1||i.height<1||i.factor<2||i.pixels.size()!=size_t(i.width)*i.height||!std::isfinite(x)||!std::isfinite(y))return 0;
 const double px=std::clamp(x*i.factor-.5,0.,double(i.width-1)),py=std::clamp(y*i.factor-.5,0.,double(i.height-1));
 int ix=int(px),iy=int(py),rx=std::min(ix+1,i.width-1),by=std::min(iy+1,i.height-1);double fx=px-ix,fy=py-iy;
 const uint32_t p[4]={i.pixels[size_t(iy)*i.width+ix],i.pixels[size_t(iy)*i.width+rx],i.pixels[size_t(by)*i.width+ix],i.pixels[size_t(by)*i.width+rx]};
 uint32_t out=0;for(int s:{0,8,16,24}){double v=(1-fy)*((1-fx)*((p[0]>>s)&255)+fx*((p[1]>>s)&255))+fy*((1-fx)*((p[2]>>s)&255)+fx*((p[3]>>s)&255));out|=uint32_t(std::floor(v+.5))<<s;}return out;
}
inline int factor(double sx,double sy){
 if(!std::isfinite(sx)||!std::isfinite(sy)||sx<=0||sy<=0||sx>4||sy>4||std::max(sx,sy)<=1)return 0;
 return std::clamp(int(std::ceil(std::max(sx,sy))),2,4);
}
// Complete composited terrain is sampled with real neighboring pixels, rather
// than treating each TMP diamond as an isolated sprite. A three-cell halo
// includes xBRZ's two-cell analysis plus the final bilinear resolve footprint.
template<class Fetch>bool sampleRegion(Fetch fetch,double x,double y,double spanX,double spanY,int w,int h,std::vector<uint32_t>&out){
 if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>MaxPixels||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e7||std::abs(y)>1e7||!std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
 const int f=factor(w/spanX,h/spanY);if(!f||w<spanX||h<spanY)return false;
 const double dx=spanX/w,dy=spanY/h;out.resize(size_t(w)*h);
 constexpr int Tile=128,Halo=3;
 for(int ty=0;ty<h;ty+=Tile)for(int tx=0;tx<w;tx+=Tile){
  int endX=std::min(w,tx+Tile),endY=std::min(h,ty+Tile);
  int ox=int(std::floor(x+(tx+.5)*dx))-Halo,oy=int(std::floor(y+(ty+.5)*dy))-Halo;
  int sw=int(std::floor(x+(endX-.5)*dx))-ox+1+Halo,sh=int(std::floor(y+(endY-.5)*dy))-oy+1+Halo;
  std::vector<uint32_t> source(size_t(sw)*sh);for(int yy=0;yy<sh;++yy)for(int xx=0;xx<sw;++xx)source[size_t(yy)*sw+xx]=fetch(ox+xx,oy+yy);
  ScaledImage scaled;if(!scale(source,sw,sh,f,scaled))return false;
  for(int yy=ty;yy<endY;++yy)for(int xx=tx;xx<endX;++xx)out[size_t(yy)*w+xx]=sample(scaled,x+(xx+.5)*dx-ox,y+(yy+.5)*dy-oy);
 }return true;
}
// A cache belongs to one immutable native canvas revision. Tiles use a fixed
// SOURCE-coordinate grid, so camera movement and fractional output ratios do
// not change the upstream xBRZ work for an already reconstructed neighborhood.
// A canvas copy/fork may share this cache until either copy changes its pixels.
class RegionCache {
 static constexpr int Tile=256,Halo=3,SourceSize=Tile+Halo*2;
 static constexpr size_t PixelBudget=MaxPixels;
 using Key=std::tuple<int,int,int>;
 struct Entry {std::shared_ptr<const ScaledImage> image;uint64_t used=0;};
 struct Span {int tile=0,first=0,last=0;};
 struct Coordinate {int first=0,second=0;double fraction=0;};
 std::map<Key,Entry> entries_;uint64_t serial_=0,builds_=0,hits_=0;size_t resident_=0;
 static std::vector<Span> spans(double origin,double step,int count){
  std::vector<Span> result;
  for(int n=0;n<count;++n){int tile=int(std::floor((origin+(n+.5)*step)/Tile));
   if(result.empty()||result.back().tile!=tile)result.push_back({tile,n,n+1});else result.back().last=n+1;
  }return result;
 }
 template<class Fetch>std::shared_ptr<const ScaledImage>get(Fetch fetch,int tx,int ty,int f){
  Key key{tx,ty,f};auto found=entries_.find(key);
  if(found!=entries_.end()){found->second.used=++serial_;++hits_;return found->second.image;}
  const size_t cost=size_t(SourceSize)*SourceSize*f*f;
  // Release before allocating the next tile. The caller finishes drawing each
  // tile before requesting another, so even oversized viewports cannot pin a
  // second complete intermediate framebuffer or invalidate active pointers.
  while(!entries_.empty()&&resident_+cost>PixelBudget){
   auto oldest=std::min_element(entries_.begin(),entries_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});
   resident_-=oldest->second.image->pixels.size();entries_.erase(oldest);
  }
  std::vector<uint32_t> source(size_t(SourceSize)*SourceSize);
  int ox=tx*Tile-Halo,oy=ty*Tile-Halo;
  for(int yy=0;yy<SourceSize;++yy)for(int xx=0;xx<SourceSize;++xx)source[size_t(yy)*SourceSize+xx]=fetch(ox+xx,oy+yy);
  auto image=std::make_shared<ScaledImage>();if(!scale(source,SourceSize,SourceSize,f,*image))return {};
  ++builds_;resident_+=image->pixels.size();entries_.emplace(key,Entry{image,++serial_});return image;
 }
public:
 size_t residentPixels()const{return resident_;}size_t entries()const{return entries_.size();}
 uint64_t builds()const{return builds_;}uint64_t hits()const{return hits_;}
 template<class Fetch>bool sampleRegion(Fetch fetch,double x,double y,double spanX,double spanY,int w,int h,std::vector<uint32_t>&out){
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>MaxPixels||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e7||std::abs(y)>1e7||!std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
  const int f=factor(w/spanX,h/spanY);if(!f||w<spanX||h<spanY)return false;
  const double dx=spanX/w,dy=spanY/h;out.resize(size_t(w)*h);
  const auto columns=spans(x,dx,w),rows=spans(y,dy,h);
  // Resolve coordinates once per output column/row, without a map lookup or
  // repeated floor/clamp for every output pixel. Retained halo pixels cover
  // bilinear footprints crossing the fixed native tile boundary.
  std::vector<Coordinate> xs(w),ys(h);
  for(const auto&column:columns){int ox=column.tile*Tile-Halo;
   for(int n=column.first;n<column.last;++n){double p=(x+(n+.5)*dx-ox)*f-.5;int first=int(p);xs[n]={first,first+1,p-first};}}
  for(const auto&row:rows){int oy=row.tile*Tile-Halo;
   for(int n=row.first;n<row.last;++n){double p=(y+(n+.5)*dy-oy)*f-.5;int first=int(p);ys[n]={first,first+1,p-first};}}
  auto draw=[&](const Span&row,const Span&column,const ScaledImage&image){
   // At an exact integer output ratio, source centers land directly on the
   // xBRZ raster. Copy its rows verbatim rather than filtering each pixel.
   const bool directX=dx*f==1&&std::abs(xs[column.first].fraction)<1e-9;
   for(int yy=row.first;yy<row.last;++yy){const auto&cy=ys[yy];const auto*top=image.pixels.data()+size_t(cy.first)*image.width;const auto*bottom=image.pixels.data()+size_t(cy.second)*image.width;
    if(directX&&std::abs(cy.fraction)<1e-9){std::copy_n(top+xs[column.first].first,column.last-column.first,out.data()+size_t(yy)*w+column.first);continue;}
    for(int xx=column.first;xx<column.last;++xx){const auto&cx=xs[xx];const uint32_t a=top[cx.first],b=top[cx.second],c=bottom[cx.first],d=bottom[cx.second];uint32_t value=0;
     for(int s:{0,8,16,24}){double v=(1-cy.fraction)*((1-cx.fraction)*((a>>s)&255)+cx.fraction*((b>>s)&255))+cy.fraction*((1-cx.fraction)*((c>>s)&255)+cx.fraction*((d>>s)&255));value|=uint32_t(std::floor(v+.5))<<s;}
     out[size_t(yy)*w+xx]=value;
    }
   }
  };
  // Draw every visible cache hit before admitting misses. Even when the
  // complete intermediate viewport exceeds this cache's budget, a new tile
  // cannot evict a tile still awaiting use and cascade into a full-frame miss.
  // Only small row/column indices are retained, never pinned image buffers.
  std::vector<std::pair<size_t,size_t>> missing;
  for(size_t row=0;row<rows.size();++row)for(size_t column=0;column<columns.size();++column){
   auto found=entries_.find(Key{columns[column].tile,rows[row].tile,f});
   if(found==entries_.end())missing.emplace_back(row,column);
   else{found->second.used=++serial_;++hits_;draw(rows[row],columns[column],*found->second.image);}
  }
  for(const auto&at:missing){const auto&row=rows[at.first];const auto&column=columns[at.second];auto image=get(fetch,column.tile,row.tile,f);if(!image)return false;draw(row,column,*image);}
  return true;
 }
};
// Match complete lit source contents as well as dimensions/factor. Pointer
// reuse, live palette changes and emissive/ordinary lighting changes cannot
// reuse stale colors, even if a hash collides. Cache lifetime is per Surface.
class Cache {
 struct Entry {uint64_t hash=0,used=0;int w=0,h=0;std::vector<uint32_t> source;ScaledImage image;};
 std::vector<Entry> entries_;uint64_t serial_=0;size_t resident_=0;ScaledImage scratch_;
public:
 size_t residentPixels()const{return resident_;}size_t entries()const{return entries_.size();}
 const ScaledImage*get(const std::vector<uint32_t>&src,int w,int h,int f){
  scratch_={}; // A previous uncached image is valid only until the next get.
  uint64_t hash=1469598103934665603ull;for(auto p:src){hash^=p;hash*=1099511628211ull;}
  for(auto&e:entries_)if(e.hash==hash&&e.w==w&&e.h==h&&e.image.factor==f&&e.source==src){e.used=++serial_;return &e.image;}
  ScaledImage image;if(!scale(src,w,h,f,image))return nullptr;
  size_t cost=src.size()+image.pixels.size();if(cost>CachePixels){scratch_=std::move(image);return &scratch_;}
  while(!entries_.empty()&&(resident_+cost>CachePixels||entries_.size()>=128)){
   auto oldest=std::min_element(entries_.begin(),entries_.end(),[](const Entry&a,const Entry&b){return a.used<b.used;});
   resident_-=oldest->source.size()+oldest->image.pixels.size();entries_.erase(oldest);
  }
  // A large uncached source must not remain resident after a later small one.
  scratch_={};Entry e;e.hash=hash;e.used=++serial_;e.w=w;e.h=h;e.source=src;e.image=std::move(image);resident_+=cost;entries_.push_back(std::move(e));return &entries_.back().image;
 }
};
}}
