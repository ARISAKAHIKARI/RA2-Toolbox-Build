#pragma once
#include "camera_raster.h"
#include "pixel_upgrade.h"
#include <map>
#include <memory>
#include <set>
#include <array>
// Native-pixel composition storage. Source sprites are never individually
// minified: all depth/shadow/color operations run at one pixel per source pixel.
namespace external { namespace camera {
class NativeCanvas {
public:
 static constexpr int PageSize=128,MaximumDimension=32768;
 static constexpr size_t MaximumPages=4096,DynamicReserve=256;
 struct Budget {size_t pages=0;size_t maximum=MaximumPages;};
private:
 struct Page {
  std::shared_ptr<Budget> budget;Surface surface;
  explicit Page(std::shared_ptr<Budget>b):budget(std::move(b)){++budget->pages;surface.reset(PageSize,PageSize);}
  Page(const Page&p):budget(p.budget),surface(p.surface){++budget->pages;}
  ~Page(){--budget->pages;}
 };
 int width_=0,height_=0;uint32_t background_=0xff080c08;bool failed_=false,dynamic_=false;
 std::shared_ptr<Budget> budget_;std::map<uint32_t,std::shared_ptr<Page>> pages_;std::set<uint32_t> dirty_;std::vector<std::array<int,4>> dirtyRects_;
 static uint32_t key(int x,int y){return uint32_t(y)*256u+uint32_t(x);}
 static int pageX(uint32_t k){return int(k%256u);}static int pageY(uint32_t k){return int(k/256u);}
 // Exact-size presentation needs neither an area prefix nor per-pixel page
 // lookups. Keep the filter's opaque RGB contract, including sparse pages and
 // source surfaces with a non-opaque background.
 void nativeRow(int y,int first,int last,uint32_t*out)const{
  for(int x=first;x<last;){int end=std::min(last,(x/PageSize+1)*PageSize);
   auto page=pages_.find(key(x/PageSize,y/PageSize));
   if(page==pages_.end())std::fill(out+x,out+end,0xff000000u|(background_&0xffffffu));
   else{const auto*source=page->second->surface.color.data()+size_t(y%PageSize)*PageSize+x%PageSize;
    for(int n=x;n<end;++n)out[n]=0xff000000u|(source[n-x]&0xffffffu);
   }x=end;
  }
 }
 bool room()const{return budget_&&budget_->pages<(dynamic_?budget_->maximum:budget_->maximum-std::min(DynamicReserve,budget_->maximum/4));}
 bool ensure(int x0,int y0,int x1,int y1){
  x0=std::clamp(x0,0,width_);x1=std::clamp(x1,0,width_);y0=std::clamp(y0,0,height_);y1=std::clamp(y1,0,height_);if(x0>=x1||y0>=y1)return true;
  for(int y=y0/PageSize;y<=(y1-1)/PageSize;++y)for(int x=x0/PageSize;x<=(x1-1)/PageSize;++x){uint32_t k=key(x,y);auto i=pages_.find(k);
   if(i==pages_.end()){if(!room()){failed_=true;return false;}auto p=std::make_shared<Page>(budget_);if(background_!=0xff080c08)std::fill(p->surface.color.begin(),p->surface.color.end(),background_);pages_.emplace(k,std::move(p));}
   else if(!i->second.unique()){if(!room()){failed_=true;return false;}i->second=std::make_shared<Page>(*i->second);}
  }return true;
 }
 template<class Draw>void paint(int x0,int y0,int x1,int y1,Draw draw){
  x0=std::clamp(x0,0,width_);x1=std::clamp(x1,0,width_);y0=std::clamp(y0,0,height_);y1=std::clamp(y1,0,height_);if(x0>=x1||y0>=y1||!ensure(x0,y0,x1,y1))return;
  for(int y=y0/PageSize;y<=(y1-1)/PageSize;++y)for(int x=x0/PageSize;x<=(x1-1)/PageSize;++x){uint32_t k=key(x,y);draw(pages_.find(k)->second->surface,x*PageSize,y*PageSize);dirty_.insert(k);}
  dirtyRects_.push_back({x0,y0,x1,y1});
 }
 static void imageBounds(const voxel::Image&i,float ax,float ay,float shadowDrop,float&x0,float&y0,float&x1,float&y1){
  if(i.width>0&&i.height>0){x0=std::min(x0,ax+i.x);y0=std::min(y0,ay+i.y);x1=std::max(x1,ax+i.x+i.width);y1=std::max(y1,ay+i.y+i.height);}
  for(const auto&p:i.attachments)imageBounds(p,ax,ay+(p.shadow?shadowDrop:0),0,x0,y0,x1,y1);
 }
 // Bounded per-presentation summed-area cache. It accelerates exact box-area
 // filtering without retaining a second full native framebuffer or its sums.
 class Filter {
  struct Prefix {std::vector<std::array<uint32_t,3>> sum;uint64_t used=0,work=0;};
  const NativeCanvas&canvas;std::map<const Page*,Prefix> cache;uint64_t serial=0;size_t prefixes=0,limit=16;
  const Prefix&prefix(const Page*p){
   auto found=cache.find(p);if(found!=cache.end()&&!found->second.sum.empty()){found->second.used=++serial;return found->second;}
   if(prefixes>=limit){auto oldest=cache.end();for(auto i=cache.begin();i!=cache.end();++i)if(!i->second.sum.empty()&&(oldest==cache.end()||i->second.used<oldest->second.used))oldest=i;if(oldest!=cache.end()){cache.erase(oldest);--prefixes;}}
   Prefix row;row.used=++serial;row.sum.resize(size_t(PageSize+1)*(PageSize+1));
   for(int y=0;y<PageSize;++y){uint32_t running[3]{};for(int x=0;x<PageSize;++x){uint32_t c=p->surface.color[size_t(y)*PageSize+x];auto&to=row.sum[size_t(y+1)*(PageSize+1)+x+1];const auto&above=row.sum[size_t(y)*(PageSize+1)+x+1];for(int channel=0;channel<3;++channel){running[channel]+=(c>>(16-channel*8))&255;to[channel]=above[channel]+running[channel];}}}
   ++prefixes;return cache.insert_or_assign(p,std::move(row)).first->second;
  }
  static double integral(const Prefix&p,const Page*page,double x,double y,int channel){
   x=std::clamp(x,0.,double(PageSize));y=std::clamp(y,0.,double(PageSize));int ix=int(x),iy=int(y);double fx=x-ix,fy=y-iy;
   const auto at=[&](int xx,int yy){return p.sum[size_t(yy)*(PageSize+1)+xx][channel];};double value=at(ix,iy);
   if(fx&&ix<PageSize)value+=fx*double(at(ix+1,iy)-at(ix,iy));
   if(fy&&iy<PageSize)value+=fy*double(at(ix,iy+1)-at(ix,iy));
   if(fx&&fy&&ix<PageSize&&iy<PageSize)value+=fx*fy*((page->surface.color[size_t(iy)*PageSize+ix]>>(16-channel*8))&255);
   return value;
  }
public:
  explicit Filter(const NativeCanvas&c):canvas(c),limit(std::max<size_t>(16,2*((c.width_+PageSize-1)/PageSize)+2)){}
  uint32_t pixel(double x0,double y0,double x1,double y1){
   x0=std::clamp(x0,0.,double(canvas.width_));x1=std::clamp(x1,0.,double(canvas.width_));y0=std::clamp(y0,0.,double(canvas.height_));y1=std::clamp(y1,0.,double(canvas.height_));double area=(x1-x0)*(y1-y0);if(area<=0)return canvas.background_;
   double total[3]{};
   for(int y=int(y0)/PageSize;y<=int(std::ceil(y1)-1)/PageSize;++y)for(int x=int(x0)/PageSize;x<=int(std::ceil(x1)-1)/PageSize;++x){
    double left=std::max(x0,double(x*PageSize))-x*PageSize,right=std::min(x1,double((x+1)*PageSize))-x*PageSize;
    double top=std::max(y0,double(y*PageSize))-y*PageSize,bottom=std::min(y1,double((y+1)*PageSize))-y*PageSize;if(left>=right||top>=bottom)continue;
    auto found=canvas.pages_.find(key(x,y));if(found==canvas.pages_.end()){double a=(right-left)*(bottom-top);for(int c=0;c<3;++c)total[c]+=a*((canvas.background_>>(16-c*8))&255);}
    else{const auto*page=found->second.get();auto&entry=cache[page];entry.used=++serial;
     // Sparse unit/status changes should not rebuild a 128² prefix for a
     // single output footprint. Dense work amortizes one prefix; retaining
     // two full native page rows prevents scanline LRU thrashing.
     if(entry.sum.empty()&&(right-left)*(bottom-top)<=1024&&entry.work<PageSize*PageSize/2){
      entry.work+=uint64_t((std::ceil(right)-std::floor(left))*(std::ceil(bottom)-std::floor(top)));
      for(int yy=int(top);yy<int(std::ceil(bottom));++yy)for(int xx=int(left);xx<int(std::ceil(right));++xx){double weight=(std::min(right,double(xx+1))-std::max(left,double(xx)))*(std::min(bottom,double(yy+1))-std::max(top,double(yy)));uint32_t color=page->surface.color[size_t(yy)*PageSize+xx];for(int c=0;c<3;++c)total[c]+=weight*((color>>(16-c*8))&255);}
     }else{const auto&s=prefix(page);for(int c=0;c<3;++c)total[c]+=integral(s,page,right,bottom,c)-integral(s,page,left,bottom,c)-integral(s,page,right,top,c)+integral(s,page,left,top,c);}
    }
   }
   uint32_t color=0xff000000u;for(int c=0;c<3;++c)color|=uint32_t(std::clamp(std::floor(total[c]/area+.5),0.,255.))<<(16-c*8);return color;
  }
 };
public:
 bool reset(int width,int height,std::shared_ptr<Budget>budget={},uint32_t background=0xff080c08u){
  pages_.clear();dirty_.clear();dirtyRects_.clear();failed_=dynamic_=false;width_=height_=0;background_=background;budget_=budget?std::move(budget):std::make_shared<Budget>();
  if(width<1||height<1||width>MaximumDimension||height>MaximumDimension||!budget_->maximum)return false;width_=width;height_=height;return true;
 }
 int width()const{return width_;}int height()const{return height_;}bool failed()const{return failed_;}void clearFailure(){failed_=false;}
 size_t pageCount()const{return pages_.size();}size_t dirtyPages()const{return dirty_.size();}size_t residentPages()const{return budget_?budget_->pages:0;}
 void clean(){dirty_.clear();dirtyRects_.clear();}
 NativeCanvas fork()const{NativeCanvas out=*this;out.dynamic_=true;out.failed_=false;out.dirty_.clear();out.dirtyRects_.clear();return out;}
 uint32_t pixel(int x,int y)const{if(x<0||y<0||x>=width_||y>=height_)return background_;auto p=pages_.find(key(x/PageSize,y/PageSize));return p==pages_.end()?background_:p->second->surface.color[size_t(y%PageSize)*PageSize+x%PageSize];}
 float depth(int x,int y)const{if(x<0||y<0||x>=width_||y>=height_)return -std::numeric_limits<float>::infinity();auto p=pages_.find(key(x/PageSize,y/PageSize));return p==pages_.end()?-std::numeric_limits<float>::infinity():p->second->surface.depth[size_t(y%PageSize)*PageSize+x%PageSize];}
 // RadarClass::FillTerrainColors first paints each admitted cell's flat 2x1
 // RGB footprint. The full-size TMP can then sit above that ground color.
 // During an incremental scene build other TMPs may already overlap this
 // rectangle; fill only untouched canvas pixels so they retain their art.
 void underlayRect(int x0,int y0,int x1,int y1,uint32_t rgb){
  x0=std::clamp(x0,0,width_);x1=std::clamp(x1,0,width_);
  y0=std::clamp(y0,0,height_);y1=std::clamp(y1,0,height_);
  if(x0>=x1||y0>=y1)return;
  rgb=0xff000000u|(rgb&0xffffffu);
  paint(x0,y0,x1,y1,[&](Surface&s,int pageX,int pageY){
   for(int y=std::max(y0,pageY);y<std::min(y1,pageY+PageSize);++y)
    for(int x=std::max(x0,pageX);x<std::min(x1,pageX+PageSize);++x){
     size_t index=size_t(y-pageY)*PageSize+x-pageX;
     if(s.color[index]==background_&&s.depth[index]==-std::numeric_limits<float>::infinity())s.color[index]=rgb;
    }
  });
 }
 void diamond(float ax,float ay,float sx,float sy,float base,uint32_t color,const lighting::State&light){
  if(!std::isfinite(ax)||!std::isfinite(ay)||sx!=1||sy!=1)return;
  paint(int(std::floor(ax-30)),int(std::floor(ay-15)),int(std::ceil(ax+30)),int(std::ceil(ay+15)),[&](Surface&s,int x,int y){s.diamond(ax-x,ay-y,1,1,base,color,light);});
 }
 void blit(const voxel::Image&image,float ax,float ay,float sx,float sy,float base,const lighting::State&light,float lift=1,float shadowDrop=0){
  if(!std::isfinite(ax)||!std::isfinite(ay)||sx!=1||sy!=1)return;
  float left=float(width_),top=float(height_),right=0,bottom=0;imageBounds(image,ax,ay,shadowDrop,left,top,right,bottom);
  if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return;
  paint(int(std::floor(std::clamp(left,-1e7f,1e7f))),int(std::floor(std::clamp(top,-1e7f,1e7f))),int(std::ceil(std::clamp(right,-1e7f,1e7f))),int(std::ceil(std::clamp(bottom,-1e7f,1e7f))),[&](Surface&s,int x,int y){s.blit(image,ax-x,ay-y,1,1,base,light,lift,shadowDrop);});
 }
 // Crop a native-pixel scene BEFORE presentation scaling. This is deliberately
 // independent of the radar's whole-map downscale. Outside backing falls back
 // to the underlying game through transparent pixels instead of stretching.
 bool presentRegion(int width,int height,std::vector<uint32_t>&out,
                    double x,double y,double spanX,double spanY,unsigned pixelScaleMode=0)const{
  if(failed_||width<1||height<1||width>8192||height>8192||
     uint64_t(width)*height>16*1024*1024||!std::isfinite(x)||!std::isfinite(y)||
     !std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||
     std::abs(x)>1e7||std::abs(y)>1e7||spanX>32768||spanY>32768)return false;
  out.assign(size_t(width)*height,0);
  const double sx=spanX/width,sy=spanY/height;
  if(pixelScaleMode==3&&sx>=.495&&sy>=.495&&sx<.995&&sy<.995){
   // Fractional physical-pixel output (e.g. 2560x1440 -> 3840x2160).
   // Prefetch a one-pixel halo in native coordinates ONCE, not 4-8 map
   // lookups for each physical output pixel. No source camera/logic zoom.
   const int x0=int(std::floor(x))-2,y0=int(std::floor(y))-2;
   const int hw=int(std::ceil(x+spanX))+2-x0;
   const int hh=int(std::ceil(y+spanY))+2-y0;
   if(hw>=2&&hh>=2&&hw<=8192&&hh<=8192&&
      uint64_t(hw)*hh<=16*1024*1024){
    std::vector<uint32_t> halo;
    if(presentRegion(hw,hh,halo,x0,y0,hw,hh,0)){
     auto fetch=[&](int px,int py)->uint32_t{
      int64_t hx=int64_t(px)-x0,hy=int64_t(py)-y0;
      return hx>=0&&hy>=0&&hx<hw&&hy<hh?
        halo[size_t(hy)*hw+size_t(hx)]:0;
     };
     if(pixelupgrade::sampleAdaptive(fetch,x,y,spanX,spanY,width,height,out))
      return true;
    }
   }
   // Incomplete backing/unsupported footprint: use original renderer, not
   // an incompletely reconstructed frame or a guessed pixel.
  }
  // Apply edge-aware 2x only for an ACTUAL 2:1 integer presentation ratio.
  // Do not zoom the camera, change target coordinates or interpolate shroud
  // across different visibility cells. At 1:1 keep exact native pixels.
  if(pixelScaleMode>=1&&pixelScaleMode<=2&&sx==.5&&sy==.5&&
     std::floor(x)==x&&std::floor(y)==y){
   // Prefetch one native pixel halo using page-row copies. A direct pixel()
   // call for all five neighborhood samples would repeat millions of map
   // lookups on a 2560x1440 view and defeat the worker's frame budget.
   const int sw=width/2,sh=height/2,haloW=sw+2,haloH=sh+2;
   std::vector<uint32_t> halo;
   if(!presentRegion(haloW,haloH,halo,x-1,y-1,haloW,haloH,0))return false;
   const int baseX=int(x),baseY=int(y);
   const auto fetch=[&](int px,int py)->uint32_t{
    int xx=px-baseX+1,yy=py-baseY+1;
    if(xx<0||yy<0||xx>=haloW||yy>=haloH)return 0;
    return halo[size_t(yy)*haloW+xx];
   };
   return pixelupgrade::sample2x(fetch,baseX,baseY,width,height,out,
     pixelScaleMode==1?pixelupgrade::TwoX::Nearest:pixelupgrade::TwoX::EdgeBlend);
  }
  if(sx==1&&sy==1&&std::floor(x)==x&&std::floor(y)==y){
   // Contiguous page runs, not per-pixel map lookups, in the native case.
   for(int dy=0;dy<height;++dy){int yy=int(y)+dy;if(yy<0||yy>=height_)continue;
    int lo=std::max(0,-int(x)),hi=std::min(width,width_-int(x));
    for(int dx=lo;dx<hi;){int xx=int(x)+dx,end=std::min(hi,dx+PageSize-xx%PageSize);
     auto page=pages_.find(key(xx/PageSize,yy/PageSize));auto*dest=out.data()+size_t(dy)*width+dx;
     if(page==pages_.end())std::fill(dest,dest+end-dx,background_);
     else std::copy_n(page->second->surface.color.data()+size_t(yy%PageSize)*PageSize+xx%PageSize,end-dx,dest);
     dx=end;
    }
   }return true;
  }
  Filter filter(*this);
  for(int dy=0;dy<height;++dy)for(int dx=0;dx<width;++dx){
   double l=x+dx*sx,t=y+dy*sy,r=l+sx,b=t+sy;
   if(r<=0||b<=0||l>=width_||t>=height_)continue;
   out[size_t(dy)*width+dx]=filter.pixel(l,t,r,b);
  }return true;
 }
 bool present(int width,int height,std::vector<uint32_t>&out,double spanX=-1,double spanY=-1)const{
  if(failed_||width<1||height<1||width>2048||height>2048||width_<1||height_<1)return false;
  if(spanX<=0)spanX=width_;if(spanY<=0)spanY=height_;if(spanX>width_||spanY>height_||!std::isfinite(spanX)||!std::isfinite(spanY))return false;
  if(spanX==width&&spanY==height){out.resize(size_t(width)*height);for(int y=0;y<height;++y)nativeRow(y,0,width,out.data()+size_t(y)*width);return true;}
  out.resize(size_t(width)*height);Filter filter(*this);double sx=spanX/width,sy=spanY/height;
  for(int y=0;y<height;++y)for(int x=0;x<width;++x)out[size_t(y)*width+x]=filter.pixel(x*sx,y*sy,(x+1)*sx,(y+1)*sy);return true;
 }
 bool presentDirty(int width,int height,std::vector<uint32_t>&out,double spanX=-1,double spanY=-1)const{
  if(failed_||width<1||height<1||width>2048||height>2048||out.size()!=size_t(width)*height)return false;
  if(spanX<=0)spanX=width_;if(spanY<=0)spanY=height_;if(spanX>width_||spanY>height_||!std::isfinite(spanX)||!std::isfinite(spanY))return false;
  if(dirtyRects_.empty())return true;
  bool exact=spanX==width&&spanY==height;
  std::vector<std::vector<std::pair<int,int>>> rows(height);double sx=spanX/width,sy=spanY/height;
  for(const auto&r:dirtyRects_){int x0=std::clamp(int(std::floor(r[0]/sx)),0,width),x1=std::clamp(int(std::ceil(r[2]/sx)),0,width);
   int y0=std::clamp(int(std::floor(r[1]/sy)),0,height),y1=std::clamp(int(std::ceil(r[3]/sy)),0,height);for(int y=y0;y<y1;++y)rows[y].emplace_back(x0,x1);}
  Filter filter(*this);
  for(int y=0;y<height;++y){auto&spans=rows[y];std::sort(spans.begin(),spans.end());int last=0;for(auto span:spans){int from=std::max(last,span.first);
   if(exact)nativeRow(y,from,span.second,out.data()+size_t(y)*width);
   else for(int x=from;x<span.second;++x)out[size_t(y)*width+x]=filter.pixel(x*sx,y*sy,(x+1)*sx,(y+1)*sy);
   last=std::max(last,span.second);}}
  return true;
 }
};
}}
