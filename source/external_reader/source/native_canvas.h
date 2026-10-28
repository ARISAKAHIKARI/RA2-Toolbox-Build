#pragma once
#include "camera_raster.h"
#include "pixel_upgrade.h"
#include <map>
#include <memory>
#include <set>
#include <array>
#include <new>
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
 colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 int width_=0,height_=0;uint32_t background_=0xff080c08;bool failed_=false,dynamic_=false;
 std::shared_ptr<Budget> budget_;std::map<uint32_t,std::shared_ptr<Page>> pages_;std::set<uint32_t> dirty_;std::vector<std::array<int,4>> dirtyRects_;
 // Source discovery remains conservative. Local terrain banks only paint the
 // page-aligned output neighborhood; repaint jobs further restrict writes to
 // pages which were explicitly cleared for reconstruction.
 int paintLeft_=0,paintTop_=0,paintRight_=0,paintBottom_=0;
 bool repainting_=false;std::set<uint32_t> repaintPages_;
 // Forks share a reconstruction only while their native pixels agree. A
 // mutating copy drops its own pointer before painting, preserving the parent
 // revision's immutable tile contents and allowing camera pans to reuse them.
 mutable std::shared_ptr<xbrzadapter::RegionCache> regionCache_;
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
  for(int y=y0/PageSize;y<=(y1-1)/PageSize;++y)for(int x=x0/PageSize;x<=(x1-1)/PageSize;++x){uint32_t k=key(x,y);if(repainting_&&!repaintPages_.count(k))continue;auto i=pages_.find(k);
   if(i==pages_.end()){if(!room()){failed_=true;return false;}auto p=std::make_shared<Page>(budget_);if(background_!=0xff080c08)std::fill(p->surface.color.begin(),p->surface.color.end(),background_);pages_.emplace(k,std::move(p));}
   else if(!i->second.unique()){if(!room()){failed_=true;return false;}i->second=std::make_shared<Page>(*i->second);}
  }return true;
 }
 template<class Draw>void paint(int x0,int y0,int x1,int y1,Draw draw){
  x0=std::clamp(x0,paintLeft_,paintRight_);x1=std::clamp(x1,paintLeft_,paintRight_);y0=std::clamp(y0,paintTop_,paintBottom_);y1=std::clamp(y1,paintTop_,paintBottom_);if(x0>=x1||y0>=y1||!intersectsRepaint(x0,y0,x1,y1))return;
  // repaint() already forked and invalidated the complete dirty pages. Keep
  // unaffected immutable xBRZ/raw tiles alive while reconstructing those pages.
  if(!repainting_)regionCache_.reset();
  try{
   if(!ensure(x0,y0,x1,y1))return;
   for(int y=y0/PageSize;y<=(y1-1)/PageSize;++y)for(int x=x0/PageSize;x<=(x1-1)/PageSize;++x){uint32_t k=key(x,y);if(repainting_&&!repaintPages_.count(k))continue;auto&surface=pages_.find(k)->second->surface;surface.precision(precision_);draw(surface,x*PageSize,y*PageSize);dirty_.insert(k);}
   dirtyRects_.push_back({x0,y0,x1,y1});
  }catch(const std::bad_alloc&){failed_=true;}
 }
 static void imageBounds(const voxel::Image&i,float ax,float ay,float shadowDrop,float&x0,float&y0,float&x1,float&y1){
  if(i.width>0&&i.height>0){x0=std::min(x0,ax+float(i.x)/i.rasterScale);y0=std::min(y0,ay+float(i.y)/i.rasterScale);x1=std::max(x1,ax+float(i.x+i.width)/i.rasterScale);y1=std::max(y1,ay+float(i.y+i.height)/i.rasterScale);}
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
  regionCache_.reset();pages_.clear();dirty_.clear();dirtyRects_.clear();repaintPages_.clear();repainting_=false;failed_=dynamic_=false;width_=height_=0;paintLeft_=paintTop_=paintRight_=paintBottom_=0;background_=background;budget_=budget?std::move(budget):std::make_shared<Budget>();
  if(width<1||height<1||width>MaximumDimension||height>MaximumDimension||!budget_->maximum)return false;width_=width;height_=height;paintRight_=width;paintBottom_=height;return true;
 }
 void precision(colorprecision::Mode mode){if(mode!=precision_)regionCache_.reset();precision_=mode;}
 int width()const{return width_;}int height()const{return height_;}bool failed()const{return failed_;}void clearFailure(){failed_=false;}
 size_t pageCount()const{return pages_.size();}size_t dirtyPages()const{return dirty_.size();}size_t residentPages()const{return budget_?budget_->pages:0;}
 void clean(){dirty_.clear();dirtyRects_.clear();repainting_=false;repaintPages_.clear();}
 NativeCanvas fork()const{NativeCanvas out=*this;out.dynamic_=true;out.failed_=false;out.clean();return out;}
 void restrictPaint(int left,int top,int right,int bottom){
  left=std::clamp(left,0,width_);right=std::clamp(right,0,width_);top=std::clamp(top,0,height_);bottom=std::clamp(bottom,0,height_);
  if(left>=right||top>=bottom){paintLeft_=paintTop_=paintRight_=paintBottom_=0;return;}
  paintLeft_=left/PageSize*PageSize;paintTop_=top/PageSize*PageSize;
  paintRight_=std::min(width_,((right+PageSize-1)/PageSize)*PageSize);paintBottom_=std::min(height_,((bottom+PageSize-1)/PageSize)*PageSize);
 }
 bool intersectsRepaint(double left,double top,double right,double bottom)const{
  if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return false;
  left=std::max(left,double(paintLeft_));top=std::max(top,double(paintTop_));right=std::min(right,double(paintRight_));bottom=std::min(bottom,double(paintBottom_));
  if(left>=right||top>=bottom)return false;if(!repainting_)return true;
  for(int y=int(top)/PageSize;y<=int(std::ceil(bottom)-1)/PageSize;++y)for(int x=int(left)/PageSize;x<=int(std::ceil(right)-1)/PageSize;++x)if(repaintPages_.count(key(x,y)))return true;
  return false;
 }
 bool repaint(const NativeCanvas&source,const std::vector<std::array<int,4>>&rectangles){
  if(this==&source||source.failed_||source.width_<1||source.height_<1||!source.budget_||rectangles.size()>65536)return false;
  try{
   NativeCanvas next=source;next.clean();next.dynamic_=true;next.repainting_=true;next.failed_=false;
   for(const auto&r:rectangles){int left=std::clamp(r[0],source.paintLeft_,source.paintRight_),top=std::clamp(r[1],source.paintTop_,source.paintBottom_);
    int right=std::clamp(r[2],source.paintLeft_,source.paintRight_),bottom=std::clamp(r[3],source.paintTop_,source.paintBottom_);if(left>=right||top>=bottom)continue;
    for(int y=top/PageSize;y<=(bottom-1)/PageSize;++y)for(int x=left/PageSize;x<=(right-1)/PageSize;++x){next.repaintPages_.insert(key(x,y));if(next.repaintPages_.size()>DynamicReserve)return false;}
   }
   // The old bank remains immutable until publication. Its dirty pages and
   // their replacements coexist briefly, using the dynamic page reserve.
   if(next.budget_->pages>next.budget_->maximum||next.repaintPages_.size()>next.budget_->maximum-next.budget_->pages)return false;
   if(next.regionCache_)next.regionCache_=std::make_shared<xbrzadapter::RegionCache>(*next.regionCache_);
   for(uint32_t page:next.repaintPages_){next.pages_.erase(page);next.dirty_.insert(page);const int x=pageX(page)*PageSize,y=pageY(page)*PageSize;
    next.dirtyRects_.push_back({x,y,std::min(x+PageSize,next.width_),std::min(y+PageSize,next.height_)});
    if(next.regionCache_)next.regionCache_->invalidateRegion(x,y,std::min(x+PageSize,next.width_),std::min(y+PageSize,next.height_));
   }
   *this=std::move(next);return true;
  }catch(const std::bad_alloc&){return false;}
 }
 uint32_t pixel(int x,int y)const{if(x<0||y<0||x>=width_||y>=height_)return background_;auto p=pages_.find(key(x/PageSize,y/PageSize));return p==pages_.end()?background_:p->second->surface.color[size_t(y%PageSize)*PageSize+x%PageSize];}
 float depth(int x,int y)const{if(x<0||y<0||x>=width_||y>=height_)return -std::numeric_limits<float>::infinity();auto p=pages_.find(key(x/PageSize,y/PageSize));return p==pages_.end()?-std::numeric_limits<float>::infinity():p->second->surface.depth[size_t(y%PageSize)*PageSize+x%PageSize];}
 bool depthRegion(int x,int y,int w,int h,std::vector<float>&out)const{
  if(w<1||h<1||w>8192||h>8192||uint64_t(w)*h>16*1024*1024)return false;
  out.assign(size_t(w)*h,-std::numeric_limits<float>::infinity());
  for(int dy=0;dy<h;++dy){int yy=y+dy;if(yy<0||yy>=height_)continue;
   int lo=std::max(0,-x),hi=std::min(w,width_-x);
   for(int dx=lo;dx<hi;){int xx=x+dx,end=std::min(hi,dx+PageSize-xx%PageSize);
    auto page=pages_.find(key(xx/PageSize,yy/PageSize));
    if(page!=pages_.end())std::copy_n(page->second->surface.depth.data()+size_t(yy%PageSize)*PageSize+xx%PageSize,end-dx,out.data()+size_t(dy)*w+dx);
    dx=end;
   }
  }return true;
 }
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
 bool gpuRegion(int width,int height,double x,double y,double spanX,double spanY,unsigned mode,std::vector<vulkancompute::TileJob>&out)const{
  if(failed_||(mode!=2&&mode!=3)||width_<1||height_<1)return false;
  if(!regionCache_)regionCache_=std::make_shared<xbrzadapter::RegionCache>();
  auto fetch=[&](int px,int py){return pixel(std::clamp(px,0,width_-1),std::clamp(py,0,height_-1));};
  return regionCache_->gpuRegion(fetch,x,y,spanX,spanY,width,height,out);
 }
 bool presentRegion(int width,int height,std::vector<uint32_t>&out,
                    double x,double y,double spanX,double spanY,unsigned pixelScaleMode=0)const{
  if(failed_||width_<1||height_<1||width<1||height<1||width>8192||height>8192||
     uint64_t(width)*height>16*1024*1024||!std::isfinite(x)||!std::isfinite(y)||
     !std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||
     std::abs(x)>1e7||std::abs(y)>1e7||spanX>32768||spanY>32768)return false;
  out.assign(size_t(width)*height,0);
  const double sx=spanX/width,sy=spanY/height;
  if(pixelScaleMode==1&&sx<=1&&sy<=1&&(sx<1||sy<1)){
   // Fetch only source centers actually sampled. A fractional crop can span
   // 8193 cells while its 8192 output centers still touch at most 8192 cells.
   const int x0=int(std::floor(x+.5*sx)),y0=int(std::floor(y+.5*sy));
   const int w=int(std::floor(x+(width-.5)*sx))-x0+1,h=int(std::floor(y+(height-.5)*sy))-y0+1;
   std::vector<uint32_t> source;if(!presentRegion(w,h,source,x0,y0,w,h,0))return false;
   auto fetch=[&](int px,int py)->uint32_t{int xx=px-x0,yy=py-y0;return xx>=0&&yy>=0&&xx<w&&yy<h?source[size_t(yy)*w+xx]:0;};
   return pixelupgrade::sampleNearest(fetch,x,y,spanX,spanY,width,height,out);
  }
  if((pixelScaleMode==3||pixelScaleMode==2)&&sx>=.25&&sy>=.25&&sx<=1&&sy<=1&&(sx<1||sy<1)){
   // Scale the current lit, fully composited terrain with a real neighborhood.
   // Bounded tiles include xBRZ's analysis halo; no independent TMP seams.
   // Terrain is an opaque framebuffer: extend its edge for the filter, then
   // clip legal coverage below. Transparent padding would fade its inside rim.
   auto fetch=[&](int px,int py)->uint32_t{return pixel(std::clamp(px,0,width_-1),std::clamp(py,0,height_-1));};
   if(!regionCache_)regionCache_=std::make_shared<xbrzadapter::RegionCache>();
   if(!regionCache_->sampleRegion(fetch,x,y,spanX,spanY,width,height,out))return false;
   // The underlying game must remain visible outside the legal backing.
   if(x>=0&&y>=0&&x+spanX<=width_&&y+spanY<=height_)return true;
   for(int yy=0;yy<height;++yy)for(int xx=0;xx<width;++xx){
    double px=x+(xx+.5)*sx,py=y+(yy+.5)*sy;
    if(px<0||py<0||px>=width_||py>=height_)out[size_t(yy)*width+xx]=0;
   }return true;
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
