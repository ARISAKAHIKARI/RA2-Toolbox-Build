#pragma once
#include "../../third_party/xbrz/xbrz.h"
#include "vulkan_compute.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <vector>
#if !defined(RA2_XBRZ_SCALAR_ONLY) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP>=2))
#define RA2_XBRZ_SSE2 1
#include <emmintrin.h>
#if defined(_M_IX86)
#include <intrin.h>
#elif defined(__i386__)
#include <cpuid.h>
#endif
#else
#define RA2_XBRZ_SSE2 0
#endif

// The upstream scaler stays unmodified. This adapter handles our premultiplied
// surfaces, physical crop coordinates and bounded caches; it adds no edge rules.
namespace external { namespace xbrzadapter {
struct GpuPresentation {
 bool requested=false,attempted=false,used=false;
 std::unique_ptr<vulkancompute::Resolve> resolve;
};
inline GpuPresentation&gpuState(){static thread_local GpuPresentation state;return state;}
inline void gpuPresentation(bool requested){
 auto&state=gpuState();state.requested=requested;state.attempted=state.used=false;
 // Retain one backend across toggles. In particular a timed-out device is
 // quarantined once and must not be recreated on each off/on transition.
}
inline bool gpuAttempted(){return gpuState().attempted;}
inline bool gpuUsed(){return gpuState().used;}
constexpr size_t MaxPixels=16*1024*1024, CachePixels=4*1024*1024;
// Standard bilinear resolve with Q8 weights and ONE final rounding. Each axis
// differs from its real weight by <=1/512; with channels in [0,255], total
// pre-round error is <=255/256<1, so final bytes differ by at most one unit.
// All channels use the same positive weights, preserving premultiplied alpha.
inline uint32_t bilinearPacked(uint32_t a,uint32_t b,uint32_t c,uint32_t d,unsigned wx,unsigned wy){
 // wx/wy are 0..256. Two channels occupy independent 32-bit lanes;
 // even the final rounded accumulator is <2^24 per lane. Unsigned modular
 // subtraction allows the complementary-weight form without signed overflow.
 auto pack=[](uint32_t p){return uint64_t(p&255)|(uint64_t((p>>16)&255)<<32);};
 auto pair=[&](uint64_t a,uint64_t b,uint64_t c,uint64_t d){
  uint64_t top=(a<<8)+(b-a)*wx,bottom=(c<<8)+(d-c)*wx;
  return (((top<<8)+(bottom-top)*wy+0x0000800000008000ull)>>16)&0x000000ff000000ffull;
 };
 const uint64_t rb=pair(pack(a),pack(b),pack(c),pack(d)),ag=pair(pack(a>>8),pack(b>>8),pack(c>>8),pack(d>>8));
 return uint32_t(rb&255)|uint32_t((rb>>16)&0x00ff0000)|uint32_t((ag&255)<<8)|uint32_t((ag>>8)&0xff000000);
}
inline bool bilinearSse2Available(){
#if RA2_XBRZ_SSE2 && (defined(__x86_64__) || defined(_M_X64))
 return true; // SSE2 is part of the x86-64 architecture.
#elif RA2_XBRZ_SSE2 && defined(_M_IX86)
 static const bool available=[](){int registers[4]{};__cpuid(registers,1);return (registers[3]&(1<<26))!=0;}();return available;
#elif RA2_XBRZ_SSE2 && defined(__i386__)
 static const bool available=[](){unsigned a=0,b=0,c=0,d=0;return __get_cpuid(1,&a,&b,&c,&d)&&(d&bit_SSE2);}();return available;
#else
 return false;
#endif
}
#if RA2_XBRZ_SSE2
inline uint32_t bilinearSse2(uint32_t a,uint32_t b,uint32_t c,uint32_t d,unsigned wx,__m128i vertical){
 const auto zero=_mm_setzero_si128(),x=_mm_set1_epi16(short(wx)),ix=_mm_set1_epi16(short(256-wx));
 auto pixel=[&](uint32_t p){return _mm_unpacklo_epi8(_mm_cvtsi32_si128(int(p)),zero);};
 auto top=_mm_add_epi16(_mm_mullo_epi16(pixel(a),ix),_mm_mullo_epi16(pixel(b),x));
 auto bottom=_mm_add_epi16(_mm_mullo_epi16(pixel(c),ix),_mm_mullo_epi16(pixel(d),x));
 // Horizontal sums are unsigned 16-bit (0..65280). Bias them into signed
 // lanes for madd, then undo the exact bias before the single final round.
 const auto bias=_mm_set1_epi16(-32768);top=_mm_xor_si128(top,bias);bottom=_mm_xor_si128(bottom,bias);
 auto v=_mm_madd_epi16(_mm_unpacklo_epi16(top,bottom),vertical);
 v=_mm_srli_epi32(_mm_add_epi32(v,_mm_set1_epi32(0x00808000)),16);
 const auto small=_mm_packs_epi32(v,v);return uint32_t(_mm_cvtsi128_si32(_mm_packus_epi16(small,small)));
}
#endif
inline uint32_t bilinearResolve(uint32_t a,uint32_t b,uint32_t c,uint32_t d,unsigned wx,unsigned wy){
#if RA2_XBRZ_SSE2
 if(bilinearSse2Available())return bilinearSse2(a,b,c,d,wx,_mm_set1_epi32(int((wy<<16)|(256-wy))));
#endif
 return bilinearPacked(a,b,c,d,wx,wy);
}
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
 uint32_t out=0;for(int s:{0,8,16,24}){double v=(1-fy)*((1-fx)*((p[0]>>s)&255)+fx*((p[1]>>s)&255))+fy*((1-fx)*((p[2]>>s)&255)+fx*((p[3]>>s)&255));out|=uint32_t(v+.5)<<s;}return out;
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
 static constexpr size_t MaximumBudget=24*1024*1024;
 size_t pixelBudget_=MaxPixels;
 using Key=std::tuple<int,int,int>;
 struct Entry {std::shared_ptr<const ScaledImage> image;uint64_t used=0;};
 struct Span {int tile=0,first=0,last=0;};
 struct Coordinate {int first=0,second=0;uint16_t weight=0;bool centered=false;};
 std::map<Key,Entry> entries_;uint64_t serial_=0,builds_=0,hits_=0;size_t resident_=0;
 static std::vector<Span> spans(double origin,double step,int count){
  std::vector<Span> result;
  for(int n=0;n<count;++n){int tile=int(std::floor((origin+(n+.5)*step)/Tile));
   if(result.empty()||result.back().tile!=tile)result.push_back({tile,n,n+1});else result.back().last=n+1;
  }return result;
 }
 void trim(size_t incoming=0){
  while(!entries_.empty()&&resident_+incoming>pixelBudget_){
   auto oldest=std::min_element(entries_.begin(),entries_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});
   resident_-=oldest->second.image->pixels.size();entries_.erase(oldest);
  }
 }
 template<class Fetch>std::shared_ptr<const ScaledImage>get(Fetch fetch,int tx,int ty,int f){
  Key key{tx,ty,f};auto found=entries_.find(key);
  if(found!=entries_.end()){found->second.used=++serial_;++hits_;return found->second.image;}
  const size_t cost=size_t(SourceSize)*SourceSize*f*f;
  // Release before allocating the next tile. The caller finishes drawing each
  // tile before requesting another, so even oversized viewports cannot pin a
  // second complete intermediate framebuffer or invalidate active pointers.
  trim(cost);
  std::vector<uint32_t> source(size_t(SourceSize)*SourceSize);
  int ox=tx*Tile-Halo,oy=ty*Tile-Halo;
  for(int yy=0;yy<SourceSize;++yy)for(int xx=0;xx<SourceSize;++xx)source[size_t(yy)*SourceSize+xx]=fetch(ox+xx,oy+yy);
  auto image=std::make_shared<ScaledImage>();if(!scale(source,SourceSize,SourceSize,f,*image))return {};
  ++builds_;resident_+=image->pixels.size();entries_.emplace(key,Entry{image,++serial_});return image;
 }
public:
 size_t residentPixels()const{return resident_;}size_t entries()const{return entries_.size();}size_t budgetPixels()const{return pixelBudget_;}
 uint64_t builds()const{return builds_;}uint64_t hits()const{return hits_;}
 template<class Fetch>bool sampleRegion(Fetch fetch,double x,double y,double spanX,double spanY,int w,int h,std::vector<uint32_t>&out){
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>MaxPixels||!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e7||std::abs(y)>1e7||!std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||spanX>32768||spanY>32768)return false;
  const int f=factor(w/spanX,h/spanY);if(!f||w<spanX||h<spanY)return false;
  const double dx=spanX/w,dy=spanY/h;out.resize(size_t(w)*h);
  const auto columns=spans(x,dx,w),rows=spans(y,dy,h);
  // Fit the currently visible tile set when it narrowly exceeds 16M pixels,
  // without retaining an unbounded map. A later small view trims back to 16M.
  const uint64_t visibleCost=uint64_t(columns.size())*rows.size()*SourceSize*SourceSize*f*f;
  pixelBudget_=size_t(std::clamp<uint64_t>(visibleCost,MaxPixels,MaximumBudget));trim();
  // Resolve coordinates once per output column/row, without a map lookup or
  // repeated floor/clamp for every output pixel. Retained halo pixels cover
  // bilinear footprints crossing the fixed native tile boundary.
  std::vector<Coordinate> xs(w),ys(h);
  for(const auto&column:columns){int ox=column.tile*Tile-Halo;
   for(int n=column.first;n<column.last;++n){double p=(x+(n+.5)*dx-ox)*f-.5;int first=int(p);double fraction=p-first;xs[n]={first,first+1,uint16_t(fraction*256+.5),std::abs(fraction)<1e-9};}}
  for(const auto&row:rows){int oy=row.tile*Tile-Halo;
   for(int n=row.first;n<row.last;++n){double p=(y+(n+.5)*dy-oy)*f-.5;int first=int(p);double fraction=p-first;ys[n]={first,first+1,uint16_t(fraction*256+.5),std::abs(fraction)<1e-9};}}
#if RA2_XBRZ_SSE2
  const bool useSse2=bilinearSse2Available();
#endif
  auto draw=[&](const Span&row,const Span&column,const ScaledImage&image){
   // At an exact integer output ratio, source centers land directly on the
   // xBRZ raster. Copy its rows verbatim rather than filtering each pixel.
   const bool directX=dx*f==1&&xs[column.first].centered;
   for(int yy=row.first;yy<row.last;++yy){const auto&cy=ys[yy];const auto*top=image.pixels.data()+size_t(cy.first)*image.width;const auto*bottom=image.pixels.data()+size_t(cy.second)*image.width;
    if(directX&&cy.centered){std::copy_n(top+xs[column.first].first,column.last-column.first,out.data()+size_t(yy)*w+column.first);continue;}
#if RA2_XBRZ_SSE2
    if(useSse2){const auto vertical=_mm_set1_epi32(int((unsigned(cy.weight)<<16)|(256-cy.weight)));
     for(int xx=column.first;xx<column.last;++xx){const auto&cx=xs[xx];out[size_t(yy)*w+xx]=bilinearSse2(top[cx.first],top[cx.second],bottom[cx.first],bottom[cx.second],cx.weight,vertical);}continue;
    }
#endif
    for(int xx=column.first;xx<column.last;++xx){const auto&cx=xs[xx];const uint32_t a=top[cx.first],b=top[cx.second],c=bottom[cx.first],d=bottom[cx.second];uint32_t value=0;
     value=bilinearPacked(a,b,c,d,cx.weight,cy.weight);
     out[size_t(yy)*w+xx]=value;
    }
   }
  };
  // The optional GPU path pins at most one bounded visible intermediate set.
  // Immutable aliasing owners retain each image without copying its pixels;
  // the backend uploads only owners missing from its persistent device atlas.
  // Integer ratios already use a row copy below and need no GPU resolve.
  if(gpuState().requested&&visibleCost<=MaximumBudget&&!(dx*f==1&&dy*f==1)){
   struct Patch {size_t row,column;std::shared_ptr<const ScaledImage> image;};
   std::vector<Patch> patches;patches.reserve(rows.size()*columns.size());
   std::vector<std::pair<size_t,size_t>> missing;
   for(size_t row=0;row<rows.size();++row)for(size_t column=0;column<columns.size();++column){
    auto found=entries_.find(Key{columns[column].tile,rows[row].tile,f});
    if(found==entries_.end())missing.emplace_back(row,column);
    else{found->second.used=++serial_;++hits_;patches.push_back({row,column,found->second.image});}
   }
   for(const auto&at:missing){auto image=get(fetch,columns[at.second].tile,rows[at.first].tile,f);if(!image)return false;patches.push_back({at.first,at.second,std::move(image)});}
   std::vector<vulkancompute::TileJob> jobs;jobs.reserve(patches.size());
   for(const auto&patch:patches){const auto&row=rows[patch.row];const auto&column=columns[patch.column];const auto&image=patch.image;
    const int ox=column.tile*Tile-Halo,oy=row.tile*Tile-Halo;
    jobs.push_back({std::shared_ptr<const std::vector<uint32_t>>(image,&image->pixels),image->width,image->height,
     column.first,row.first,column.last-column.first,row.last-row.first,
     (x+(column.first+.5)*dx-ox)*f-.5,(y+(row.first+.5)*dy-oy)*f-.5,dx*f,dy*f});
   }
   auto&gpu=gpuState();gpu.attempted=true;if(!gpu.resolve)gpu.resolve=std::make_unique<vulkancompute::Resolve>();
   if(gpu.resolve->render(w,h,jobs,out)){gpu.used=true;return true;}
   // Same-frame fallback uses the already retained tiles. A failed driver
   // cannot publish a partial frame or force duplicate xBRZ reconstruction.
   for(const auto&patch:patches)draw(rows[patch.row],columns[patch.column],*patch.image);
   return true;
  }
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
#undef RA2_XBRZ_SSE2
