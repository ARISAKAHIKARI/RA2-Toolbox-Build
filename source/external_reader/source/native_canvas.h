#pragma once
#include "camera_raster.h"
#include "pixel_upgrade.h"
#include <map>
#include <memory>
#include <set>
#include <array>
#include <new>
#include <atomic>
#include <unordered_map>
#include "render_memory_budget.h"
#include "../../shared/source/parallel_rows.h"
// Native-pixel composition storage, or explicitly retained ground inputs for
// small panels. The latter are replayed by ViewportCanvas at output pixels.
namespace external { namespace camera {
class NativeCanvas {
 friend class ObjectCanvas;
public:
 struct TerrainDraw {std::shared_ptr<const voxel::Image>image;float x=0,y=0,base=0;lighting::State light;Material material;};
 struct Underlay {int left,top,right,bottom;uint32_t color;};
 using AlphaPages=std::map<uint32_t,std::shared_ptr<const std::vector<uint8_t>>>;
 static constexpr int PageSize=128,MaximumDimension=32768;
 static constexpr size_t MaximumPages=rendermemory::NativePages,DynamicReserve=256;
 struct Budget {
  std::atomic<size_t>pages{0};size_t maximum=MaximumPages;
  bool acquire(){auto count=pages.load(std::memory_order_relaxed);while(count<maximum)if(pages.compare_exchange_weak(count,count+1,std::memory_order_relaxed))return true;return false;}
 };
 static std::shared_ptr<Budget>sharedBudget(){static auto value=std::make_shared<Budget>();return value;}
private:
 // Retain exact sums with the immutable native page, across presentations
 // and CPU row workers. Admission is shared by all canvases; a full cache
 // still permits the bounded per-filter scratch path below.
 struct AreaPrefix {
  static constexpr size_t Bytes=size_t(PageSize+1)*(PageSize+1)*sizeof(std::array<uint32_t,3>);
  inline static std::atomic<size_t> retained{0};
  inline static std::atomic<uint64_t> builds{0};
#ifdef RA2_CPU_AREA_CACHE_BYTES
  static constexpr size_t Limit=RA2_CPU_AREA_CACHE_BYTES;
#else
  static constexpr size_t Limit=rendermemory::AreaPrefixes;
#endif
  std::vector<std::array<uint32_t,3>>sum;uint64_t revision=0;bool cached=false;
  ~AreaPrefix(){if(cached)retained.fetch_sub(Bytes,std::memory_order_relaxed);}
  static bool reserve(size_t cost){if(cost>Limit)return false;auto bytes=retained.load(std::memory_order_relaxed);
   while(bytes<=Limit-cost)if(retained.compare_exchange_weak(bytes,bytes+cost,std::memory_order_relaxed))return true;return false;}
  bool retain(){cached=reserve(Bytes);return cached;}
 };
 // Exact integer/half-integer reduction needs only each output box's sum,
 // rather than a 129x129 RGB integral image for every changed native page.
 // Coordinates and weights use half-pixels; their sums remain exact integers.
 struct BoxReduction {
  struct Credit {size_t bytes=0;~Credit(){if(bytes)AreaPrefix::retained.fetch_sub(bytes,std::memory_order_relaxed);}}credit;
  int x=0,y=0,stepX=0,stepY=0,firstX=0,firstY=0,columns=0,rows=0;uint64_t revision=0;
  std::vector<std::array<uint64_t,3>>sum;
  inline static std::atomic<uint64_t>builds{0};
  bool matches(int px,int py,int sx,int sy,uint64_t version)const{return x==px&&y==py&&stepX==sx&&stepY==sy&&revision==version;}
  static std::array<uint64_t,3>box(const Surface&s,int left,int top,int right,int bottom){
   std::array<uint64_t,3>total{};const int first=(left+1)/2,last=right/2;
   for(int y=top/2;y<(bottom+1)/2;++y){const auto*row=s.color.data()+size_t(y)*PageSize;uint64_t rb=0,g=0;
    // Separate 32-bit red/blue lanes cannot carry within a 128-pixel row.
    for(int x=first;x<last;++x){const uint32_t c=row[x];rb+=(uint64_t(c&0xff0000u)<<16)|(c&255u);g+=(c>>8)&255u;}
    rb*=2;g*=2;
    if(left&1){const uint32_t c=row[left/2];rb+=(uint64_t(c&0xff0000u)<<16)|(c&255u);g+=(c>>8)&255u;}
    if(right&1){const uint32_t c=row[right/2];rb+=(uint64_t(c&0xff0000u)<<16)|(c&255u);g+=(c>>8)&255u;}
    const unsigned weight=unsigned(std::min(bottom,2*y+2)-std::max(top,2*y));
    total[0]+=(rb>>32)*weight;total[1]+=g*weight;total[2]+=uint32_t(rb)*uint64_t(weight);
   }return total;
  }
 };
 struct Page {
  std::shared_ptr<Budget> budget;Surface surface;uint64_t revision=0;
  mutable std::mutex prefixMutex;mutable std::shared_ptr<const AreaPrefix>area;
  mutable std::shared_ptr<const BoxReduction>reduction;
  std::shared_ptr<const BoxReduction>boxes(int x,int y,int sx,int sy)const{
   std::lock_guard<std::mutex>lock(prefixMutex);
   if(reduction&&reduction->matches(x,y,sx,sy,revision))return reduction;
   // Reuse the small table only after every reader of its old revision has
   // released it. Frozen snapshots/concurrent scales keep their own sums.
   const bool reusable=reduction&&reduction.use_count()==1&&reduction->x==x&&reduction->y==y&&reduction->stepX==sx&&reduction->stepY==sy;
   auto next=reusable?std::const_pointer_cast<BoxReduction>(reduction):std::shared_ptr<BoxReduction>{};
   if(!reusable)reduction.reset();if(area&&area->revision!=revision)area.reset();
   if(!next)next=std::make_shared<BoxReduction>();next->x=x;next->y=y;next->stepX=sx;next->stepY=sy;next->revision=revision;
   const int left=x*PageSize*2,top=y*PageSize*2,right=left+PageSize*2,bottom=top+PageSize*2;
   next->firstX=left/sx;next->firstY=top/sy;next->columns=(right+sx-1)/sx-next->firstX;next->rows=(bottom+sy-1)/sy-next->firstY;
   const size_t count=size_t(next->columns)*next->rows,cost=sizeof(BoxReduction)+count*sizeof(next->sum[0])+128;
   if(!reusable){if(!AreaPrefix::reserve(cost))return {};next->credit.bytes=cost;next->sum.resize(count);}
   for(int yy=0;yy<next->rows;++yy)for(int xx=0;xx<next->columns;++xx){const int ox=next->firstX+xx,oy=next->firstY+yy;
    next->sum[size_t(yy)*next->columns+xx]=BoxReduction::box(surface,std::max(left,ox*sx)-left,std::max(top,oy*sy)-top,std::min(right,(ox+1)*sx)-left,std::min(bottom,(oy+1)*sy)-top);
   }
   ++BoxReduction::builds;reduction=next;return next;
  }
  std::shared_ptr<const AreaPrefix>prefix(bool build)const{
   std::lock_guard<std::mutex>lock(prefixMutex);
   if(area&&area->revision==revision)return area;
   area.reset();if(!build)return {};
   auto next=std::make_shared<AreaPrefix>();next->revision=revision;next->sum.resize(size_t(PageSize+1)*(PageSize+1));
   for(int y=0;y<PageSize;++y){uint32_t running[3]{};for(int x=0;x<PageSize;++x){uint32_t c=surface.color[size_t(y)*PageSize+x];
    auto&to=next->sum[size_t(y+1)*(PageSize+1)+x+1];const auto&above=next->sum[size_t(y)*(PageSize+1)+x+1];
    for(int channel=0;channel<3;++channel){running[channel]+=(c>>(16-channel*8))&255;to[channel]=above[channel]+running[channel];}
   }}
   ++AreaPrefix::builds;if(next->retain())area=next;return next;
  }
  explicit Page(std::shared_ptr<Budget>b):budget(std::move(b)){if(!budget->acquire())throw std::bad_alloc();try{if(!surface.reset(PageSize,PageSize))throw std::bad_alloc();}catch(...){--budget->pages;throw;}}
  Page(const Page&p):budget(p.budget),revision(p.revision){if(!budget->acquire())throw std::bad_alloc();try{surface=p.surface;}catch(...){--budget->pages;throw;}}
  ~Page(){--budget->pages;}
 };
 colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 int width_=0,height_=0;uint32_t background_=0xff080c08;bool failed_=false,dynamic_=false;
 std::shared_ptr<Budget> budget_;std::map<uint32_t,std::shared_ptr<Page>> pages_;std::set<uint32_t> dirty_;std::vector<std::array<int,4>> dirtyRects_;
 struct TerrainSources {std::vector<TerrainDraw>draws;std::unordered_map<size_t,size_t>indices;};
 std::shared_ptr<TerrainSources>terrainSources_;
 // Retained ground stores world-space inputs, not a world-sized framebuffer.
 // Small panels replay these inputs directly at their device-pixel size.
 bool retained_=false;
 std::shared_ptr<std::vector<Underlay>> underlays_;
 std::shared_ptr<const AlphaPages>nativeAlpha_;
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
   for(int y=y0/PageSize;y<=(y1-1)/PageSize;++y)for(int x=x0/PageSize;x<=(x1-1)/PageSize;++x){uint32_t k=key(x,y);if(repainting_&&!repaintPages_.count(k))continue;auto&page=*pages_.find(k)->second;auto&surface=page.surface;surface.precision(precision_);surface.nativeView(y*PageSize,1,1,width_,height_,x*PageSize);++page.revision;draw(surface,x*PageSize,y*PageSize);dirty_.insert(k);}
   dirtyRects_.push_back({x0,y0,x1,y1});
  }catch(const std::bad_alloc&){failed_=true;}
 }
 static void imageBounds(const voxel::Image&i,float ax,float ay,float shadowDrop,float&x0,float&y0,float&x1,float&y1){
  if(i.width>0&&i.height>0){x0=std::min(x0,ax+float(i.x)/i.rasterScale);y0=std::min(y0,ay+float(i.y)/i.rasterScale);x1=std::max(x1,ax+float(i.x+i.width)/i.rasterScale);y1=std::max(y1,ay+float(i.y+i.height)/i.rasterScale);}
  for(const auto&p:i.attachments)imageBounds(p,ax,ay+(p.shadow?shadowDrop:0),0,x0,y0,x1,y1);
 }
 // Each worker holds at most two page rows of sums. Pages also retain them
 // under the shared byte budget, so unchanged pages and neighboring workers
 // do not repeatedly scan the same 128x128 native pixels.
 class Filter {
  struct Prefix {std::shared_ptr<const AreaPrefix>sum;uint64_t used=0,work=0;};
  const NativeCanvas&canvas;std::map<const Page*,Prefix> cache;uint64_t serial=0;size_t prefixes=0,limit=16;
  const Prefix&prefix(const Page*p){
   auto found=cache.find(p);if(found!=cache.end()&&found->second.sum){found->second.used=++serial;return found->second;}
   if(prefixes>=limit){auto oldest=cache.end();for(auto i=cache.begin();i!=cache.end();++i)if(i->second.sum&&(oldest==cache.end()||i->second.used<oldest->second.used))oldest=i;if(oldest!=cache.end()){cache.erase(oldest);--prefixes;}}
   Prefix row;row.used=++serial;row.sum=p->prefix(true);
   ++prefixes;return cache.insert_or_assign(p,std::move(row)).first->second;
  }
  static double integral(const Prefix&p,const Page*page,double x,double y,int channel){
   x=std::clamp(x,0.,double(PageSize));y=std::clamp(y,0.,double(PageSize));int ix=int(x),iy=int(y);double fx=x-ix,fy=y-iy;
   const auto at=[&](int xx,int yy){return p.sum->sum[size_t(yy)*(PageSize+1)+xx][channel];};double value=at(ix,iy);
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
     // Tiny footprints can use direct samples. Whole-map reduction must not
     // first evaluate half a page with double-precision per-pixel weights,
     // only to scan the complete page again when constructing its sums.
     if(!entry.sum&&(right-left)*(bottom-top)<16&&entry.work<PageSize*PageSize/2){
      entry.work+=uint64_t((std::ceil(right)-std::floor(left))*(std::ceil(bottom)-std::floor(top)));
      for(int yy=int(top);yy<int(std::ceil(bottom));++yy)for(int xx=int(left);xx<int(std::ceil(right));++xx){double weight=(std::min(right,double(xx+1))-std::max(left,double(xx)))*(std::min(bottom,double(yy+1))-std::max(top,double(yy)));uint32_t color=page->surface.color[size_t(yy)*PageSize+xx];for(int c=0;c<3;++c)total[c]+=weight*((color>>(16-c*8))&255);}
     }else{const auto&s=prefix(page);for(int c=0;c<3;++c)total[c]+=integral(s,page,right,bottom,c)-integral(s,page,left,bottom,c)-integral(s,page,right,top,c)+integral(s,page,left,top,c);}
    }
   }
   uint32_t color=0xff000000u;for(int c=0;c<3;++c)color|=uint32_t(std::clamp(std::floor(total[c]/area+.5),0.,255.))<<(16-c*8);return color;
  }
 };
public:
private:
 bool presentBoxes(int width,int height,std::vector<uint32_t>&out,int stepX,int stepY,const std::vector<std::vector<std::pair<int,int>>>*regions=nullptr)const{
  try{
   const int columns=(width*stepX+PageSize*2-1)/(PageSize*2),rows=(height*stepY+PageSize*2-1)/(PageSize*2);
   std::vector<std::shared_ptr<const BoxReduction>>boxes(size_t(columns)*rows);
   std::vector<uint8_t>needed;
   if(regions){needed.resize(boxes.size());for(int y=0;y<height;++y)for(auto span:(*regions)[y])if(span.first<span.second){const int l=span.first*stepX,r=span.second*stepX,t=y*stepY,b=t+stepY;
    for(int py=t/(PageSize*2);py<=(b-1)/(PageSize*2);++py)for(int px=l/(PageSize*2);px<=(r-1)/(PageSize*2);++px)needed[size_t(py)*columns+px]=1;
   }}
   struct Work {int x,y;const Page*page;};std::vector<Work>work;work.reserve(pages_.size());
   for(const auto&e:pages_){const int x=pageX(e.first),y=pageY(e.first);if(x<columns&&y<rows&&(!regions||needed[size_t(y)*columns+x]))work.push_back({x,y,e.second.get()});}
   std::atomic<bool>ready{true};
   parallelrows::rows(work.size(),work.size()*size_t(PageSize)*PageSize,[&](size_t first,size_t last){
    try{for(size_t n=first;n<last;++n){const auto&w=work[n];auto reduced=w.page->boxes(w.x,w.y,stepX,stepY);if(!reduced){ready=false;continue;}boxes[size_t(w.y)*columns+w.x]=std::move(reduced);}}
    catch(const std::bad_alloc&){ready=false;}
   });
   if(!ready)return false;
   const uint64_t area=uint64_t(stepX)*stepY;
   auto write=[&](int x,int y){const int l=x*stepX,t=y*stepY,r=l+stepX,b=t+stepY;uint64_t total[3]{};
     for(int py=t/(PageSize*2);py<=(b-1)/(PageSize*2);++py)for(int px=l/(PageSize*2);px<=(r-1)/(PageSize*2);++px){const auto&p=boxes[size_t(py)*columns+px];
      if(p){const auto&sum=p->sum[size_t(y-p->firstY)*p->columns+x-p->firstX];for(int c=0;c<3;++c)total[c]+=sum[c];}
      else{const uint64_t coverage=uint64_t(std::min(r,(px+1)*PageSize*2)-std::max(l,px*PageSize*2))*(std::min(b,(py+1)*PageSize*2)-std::max(t,py*PageSize*2));for(int c=0;c<3;++c)total[c]+=coverage*((background_>>(16-c*8))&255);}
     }
     uint32_t color=0xff000000u;for(int c=0;c<3;++c)color|=uint32_t((total[c]+area/2)/area)<<(16-c*8);out[size_t(y)*width+x]=color;
   };
   // Three integer channel divisions plus page gathers per output pixel.
   parallelrows::rows(size_t(height),size_t(width)*height*3,[&](size_t first,size_t last){
    for(int y=int(first);y<int(last);++y){if(regions){for(auto span:(*regions)[y])for(int x=span.first;x<span.second;++x)write(x,y);}else for(int x=0;x<width;++x)write(x,y);}
   });return true;
  }catch(const std::bad_alloc&){return false;}
 }
public:
 struct AreaCacheStats {size_t bytes;uint64_t builds;uint64_t boxBuilds;};
 static AreaCacheStats areaCacheStats(){const auto boxes=BoxReduction::builds.load(std::memory_order_relaxed);return {AreaPrefix::retained.load(std::memory_order_relaxed),AreaPrefix::builds.load(std::memory_order_relaxed)+boxes,boxes};}
 bool reset(int width,int height,std::shared_ptr<Budget>budget={},uint32_t background=0xff080c08u){
  retained_=false;underlays_.reset();
  regionCache_.reset();terrainSources_.reset();nativeAlpha_.reset();pages_.clear();dirty_.clear();dirtyRects_.clear();repaintPages_.clear();repainting_=false;failed_=dynamic_=false;width_=height_=0;paintLeft_=paintTop_=paintRight_=paintBottom_=0;background_=background;budget_=budget?std::move(budget):sharedBudget();
  if(width<1||height<1||width>MaximumDimension||height>MaximumDimension||!budget_->maximum)return false;width_=width;height_=height;paintRight_=width;paintBottom_=height;return true;
 }
 void precision(colorprecision::Mode mode){if(mode!=precision_)regionCache_.reset();precision_=mode;}
 void retainGround(bool enabled){retained_=enabled;}
 bool retainedGround()const{return retained_;}
 int width()const{return width_;}int height()const{return height_;}bool failed()const{return failed_;}void clearFailure(){failed_=false;}
 size_t pageCount()const{return pages_.size();}size_t dirtyPages()const{return dirty_.size();}size_t residentPages()const{return budget_?budget_->pages.load(std::memory_order_relaxed):0;}
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
  if(retained_){if(!underlays_)underlays_=std::make_shared<std::vector<Underlay>>();else if(!underlays_.unique())underlays_=std::make_shared<std::vector<Underlay>>(*underlays_);underlays_->push_back({x0,y0,x1,y1,rgb});return;}
  paint(x0,y0,x1,y1,[&](Surface&s,int pageX,int pageY){
   for(int y=std::max(y0,pageY);y<std::min(y1,pageY+PageSize);++y)
    for(int x=std::max(x0,pageX);x<std::min(x1,pageX+PageSize);++x){
     size_t index=size_t(y-pageY)*PageSize+x-pageX;
     if(s.color[index]==background_&&s.depth[index]==-std::numeric_limits<float>::infinity())s.color[index]=rgb;
    }
  });
 }
 void diamond(float ax,float ay,float sx,float sy,float base,uint32_t color,const lighting::State&light){
  if(retained_)return; // Missing TMP remains its admitted radar-color underlay.
  if(!std::isfinite(ax)||!std::isfinite(ay)||sx!=1||sy!=1)return;
  paint(int(std::floor(ax-30)),int(std::floor(ay-15)),int(std::ceil(ax+30)),int(std::ceil(ay+15)),[&](Surface&s,int x,int y){s.diamond(ax-x,ay-y,1,1,base,color,light);});
 }
 void blit(const voxel::Image&image,float ax,float ay,float sx,float sy,float base,const lighting::State&light,float lift=1,float shadowDrop=0,Material material={}){
  if(retained_)return; // terrainSource owns the draw; ViewportCanvas replays it.
  if(!std::isfinite(ax)||!std::isfinite(ay)||sx!=1||sy!=1)return;
  float left=float(width_),top=float(height_),right=0,bottom=0;imageBounds(image,ax,ay,shadowDrop,left,top,right,bottom);
  if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return;
  paint(int(std::floor(std::clamp(left,-1e7f,1e7f))),int(std::floor(std::clamp(top,-1e7f,1e7f))),int(std::ceil(std::clamp(right,-1e7f,1e7f))),int(std::ceil(std::clamp(bottom,-1e7f,1e7f))),[&](Surface&s,int x,int y){s.blit(image,ax-x,ay-y,1,1,base,light,lift,shadowDrop,false,material);});
 }
 // Retain indexed TMP owners, not a second decoded copy. Camera-dependent A
 // and native Z are resolved again before objects; cached lit RGB is only the
 // reconstruction/underlay source. Dirty banks replace the same cell's entry.
 void terrainSource(size_t key,std::shared_ptr<const voxel::Image>image,float x,float y,float base,const lighting::State&light,int level){
  if(!terrainSources_)terrainSources_=std::make_shared<TerrainSources>();else if(!terrainSources_.unique())terrainSources_=std::make_shared<TerrainSources>(*terrainSources_);
  auto [at,added]=terrainSources_->indices.emplace(key,terrainSources_->draws.size());if(added)terrainSources_->draws.emplace_back();
  Material m;m.nativeZ=true;m.nativeZAdjust=-15*level;terrainSources_->draws[at->second]={std::move(image),x,y,base,light,m};
 }
 const std::vector<TerrainDraw>&terrainSources()const{static const std::vector<TerrainDraw>empty;return terrainSources_?terrainSources_->draws:empty;}
 // Retaining this identity also retains immutability: terrainSource() forks
 // on the next write, including light/material changes at the same game frame.
 std::shared_ptr<const void> terrainSourceOwner()const{return terrainSources_;}
 void nativeAlpha(std::shared_ptr<const AlphaPages>pages){nativeAlpha_=std::move(pages);}
 std::shared_ptr<const std::vector<uint8_t>>nativeAlpha(uint32_t k)const{if(!nativeAlpha_)return {};auto at=nativeAlpha_->find(k);return at==nativeAlpha_->end()?nullptr:at->second;}
 std::vector<uint32_t>pageKeys()const{std::vector<uint32_t>out;for(const auto&p:pages_)out.push_back(p.first);return out;}
 uint16_t nativeDepth(int x,int y)const{if(x<0||y<0||x>=width_||y>=height_)return 65535;auto p=pages_.find(key(x/PageSize,y/PageSize));return p==pages_.end()?65535:p->second->surface.nativeZ[size_t(y%PageSize)*PageSize+x%PageSize];}
 unsigned nativeAlphaAt(int x,int y)const{if(x<0||y<0||x>=width_||y>=height_)return 0;auto a=nativeAlpha(key(x/PageSize,y/PageSize));return a?(*a)[size_t(y%PageSize)*PageSize+x%PageSize]:127;}
 // Crop a native-pixel scene BEFORE presentation scaling. This is deliberately
 // independent of the radar's whole-map downscale. Outside backing falls back
 // to the underlying game through transparent pixels instead of stretching.
 bool gpuRegion(int width,int height,double x,double y,double spanX,double spanY,unsigned mode,std::vector<vulkancompute::TileJob>&out)const{
  if(retained_)return false; // No native framebuffer to reconstruct or upload.
  if(failed_||width_<1||height_<1||width<1||height<1||width>8192||height>8192||uint64_t(width)*height>16*1024*1024||
     !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(spanX)||!std::isfinite(spanY)||spanX<=0||spanY<=0||std::abs(x)>1e7||std::abs(y)>1e7||spanX>32768||spanY>32768)return false;
  if(mode==1){out.clear();const double sx=spanX/width,sy=spanY/height;
   // The established CPU path uses area integration for a fractional native
   // crop or shrink. Those cases retain its cached CPU base, then GPU actors.
   if(!(sx<=1&&sy<=1&&(sx<1||sy<1))&&!(sx==1&&sy==1&&std::floor(x)==x&&std::floor(y)==y))return false;
   auto xs=std::make_shared<std::vector<int32_t>>(width),ys=std::make_shared<std::vector<int32_t>>(height);
   for(int xx=0;xx<width;++xx)(*xs)[xx]=int32_t(std::floor(x+(xx+.5)*spanX/width));
   for(int yy=0;yy<height;++yy)(*ys)[yy]=int32_t(std::floor(y+(yy+.5)*spanY/height));
   for(const auto&entry:pages_){int px=pageX(entry.first)*PageSize,py=pageY(entry.first)*PageSize;
    auto edge=[](double v,int limit){return int(std::clamp(std::ceil(v-.5),0.,double(limit)));};
    int l=edge((px-x)/sx,width),r=edge((std::min(px+PageSize,width_)-x)/sx,width);
    int t=edge((py-y)/sy,height),b=edge((std::min(py+PageSize,height_)-y)/sy,height);if(l>=r||t>=b)continue;
    auto pixels=std::shared_ptr<const std::vector<uint32_t>>(entry.second,&entry.second->surface.color);
    vulkancompute::TileJob job{std::move(pixels),PageSize,PageSize,l,t,r-l,b-t,double(-px),double(-py),1,1};job.nearest=true;job.nearestXs=xs;job.nearestYs=ys;out.push_back(std::move(job));
   }return !out.empty();
  }
  if(mode!=2&&mode!=3)return false;
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
  if(retained_){
   // Pixel centers preserve fractional crops and transparent backing edges.
   auto edge=[](double v,int limit){return int(std::clamp(std::ceil(v-.5),0.,double(limit)));};
   const int l=edge(-x/sx,width),r=edge((width_-x)/sx,width),t=edge(-y/sy,height),b=edge((height_-y)/sy,height);
   for(int yy=t;yy<b;++yy)std::fill(out.begin()+size_t(yy)*width+l,out.begin()+size_t(yy)*width+r,background_);
   if(underlays_)for(const auto&rect:*underlays_){
    const int x0=edge((rect.left-x)/sx,width),x1=edge((rect.right-x)/sx,width),y0=edge((rect.top-y)/sy,height),y1=edge((rect.bottom-y)/sy,height);
    for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){auto&pixel=out[size_t(yy)*width+xx];if(pixel==background_)pixel=rect.color;}
   }return true;
  }
  if(pixelScaleMode==1&&sx<=1&&sy<=1&&(sx<1||sy<1)){
   // Fetch only source centers actually sampled. A fractional crop can span
   // 8193 cells while its 8192 output centers still touch at most 8192 cells.
   const int x0=int(std::floor(x+.5*sx)),y0=int(std::floor(y+.5*sy));
   const int w=int(std::floor(x+(width-.5)*sx))-x0+1,h=int(std::floor(y+(height-.5)*sy))-y0+1;
   std::vector<uint32_t> source;if(!presentRegion(w,h,source,x0,y0,w,h,0))return false;
   auto fetch=[&](int px,int py)->uint32_t{int xx=px-x0,yy=py-y0;return xx>=0&&yy>=0&&xx<w&&yy<h?source[size_t(yy)*w+xx]:0;};
   std::vector<int> xs(width);for(int xx=0;xx<width;++xx)xs[xx]=int(std::floor(x+(xx+.5)*spanX/width));
   parallelrows::rows(size_t(height),size_t(width)*height,[&](size_t first,size_t last){for(int yy=int(first);yy<int(last);++yy){
    int py=int(std::floor(y+(yy+.5)*spanY/height));for(int xx=0;xx<width;++xx)out[size_t(yy)*width+xx]=fetch(xs[xx],py);
   }});return true;
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
   parallelrows::rows(size_t(height),size_t(width)*height,[&](size_t first,size_t last){for(int dy=int(first);dy<int(last);++dy){int yy=int(y)+dy;if(yy<0||yy>=height_)continue;
    int lo=std::max(0,-int(x)),hi=std::min(width,width_-int(x));
    for(int dx=lo;dx<hi;){int xx=int(x)+dx,end=std::min(hi,dx+PageSize-xx%PageSize);
     auto page=pages_.find(key(xx/PageSize,yy/PageSize));auto*dest=out.data()+size_t(dy)*width+dx;
     if(page==pages_.end())std::fill(dest,dest+end-dx,background_);
     else std::copy_n(page->second->surface.color.data()+size_t(yy%PageSize)*PageSize+xx%PageSize,end-dx,dest);
     dx=end;
    }
   }});return true;
  }
  Filter filter(*this);
  for(int dy=0;dy<height;++dy)for(int dx=0;dx<width;++dx){
   double l=x+dx*sx,t=y+dy*sy,r=l+sx,b=t+sy;
   if(r<=0||b<=0||l>=width_||t>=height_)continue;
   out[size_t(dy)*width+dx]=filter.pixel(l,t,r,b);
  }return true;
 }
 // Snapshot aliases remain strong for the synchronous GPU submission only.
 // A version changes on every native page paint, including in-place writes;
 // the weak resident cache never pins old pages or exhausts the COW budget.
 bool areaPlan(int width,int height,gpuarea::Plan&plan,double spanX=-1,double spanY=-1)const{
  plan={};if(failed_||!width_||!height_)return false;if(spanX<=0)spanX=width_;if(spanY<=0)spanY=height_;
  plan.nativeWidth=width_;plan.nativeHeight=height_;plan.width=width;plan.height=height;plan.spanX=spanX;plan.spanY=spanY;plan.background=background_;
  if(pages_.size()>gpuarea::MaximumPages)return false;
  for(const auto&entry:pages_){const auto&page=entry.second;plan.pages.push_back({uint32_t(pageX(entry.first)),uint32_t(pageY(entry.first)),std::shared_ptr<const std::vector<uint32_t>>(page,&page->surface.color),page->revision,std::shared_ptr<const std::vector<float>>(page,&page->surface.depth),{},nativeAlpha(entry.first)});}
  return gpuarea::valid(plan);
 }
 bool presentGPU(int width,int height,std::vector<uint32_t>&out,double spanX=-1,double spanY=-1,
  std::shared_ptr<const std::vector<uint8_t>>fog={},std::shared_ptr<const gpushroud::Plan>mask={},
  std::shared_ptr<const gpueffects::Plan>effects={},std::shared_ptr<const gpupresent::Frame>*gpuOut=nullptr)const{
  if(!xbrzadapter::gpuAreaAvailable())return false;gpuarea::Plan plan;if(!areaPlan(width,height,plan,spanX,spanY))return false;plan.fog=std::move(fog);plan.mask=std::move(mask);plan.effects=std::move(effects);return xbrzadapter::gpuArea(plan,out,gpuOut);
 }
 bool present(int width,int height,std::vector<uint32_t>&out,double spanX=-1,double spanY=-1)const{
  if(failed_||width<1||height<1||width>2048||height>2048||width_<1||height_<1)return false;
  if(spanX<=0)spanX=width_;if(spanY<=0)spanY=height_;if(spanX>width_||spanY>height_||!std::isfinite(spanX)||!std::isfinite(spanY))return false;
  if(spanX==width&&spanY==height){out.resize(size_t(width)*height);for(int y=0;y<height;++y)nativeRow(y,0,width,out.data()+size_t(y)*width);return true;}
  out.resize(size_t(width)*height);double sx=spanX/width,sy=spanY/height;
  if(sx>=2&&sy>=2&&std::floor(sx*2)==sx*2&&std::floor(sy*2)==sy*2&&presentBoxes(width,height,out,int(sx*2),int(sy*2)))return true;
  parallelrows::rows(size_t(height),size_t(std::ceil(spanX))*size_t(std::ceil(spanY)),[&](size_t first,size_t last){Filter filter(*this);
   for(int y=int(first);y<int(last);++y)for(int x=0;x<width;++x)out[size_t(y)*width+x]=filter.pixel(x*sx,y*sy,(x+1)*sx,(y+1)*sy);
  });return true;
 }
 bool presentDirty(int width,int height,std::vector<uint32_t>&out,double spanX=-1,double spanY=-1)const{
  if(failed_||width<1||height<1||width>2048||height>2048||out.size()!=size_t(width)*height)return false;
  if(spanX<=0)spanX=width_;if(spanY<=0)spanY=height_;if(spanX>width_||spanY>height_||!std::isfinite(spanX)||!std::isfinite(spanY))return false;
  if(dirtyRects_.empty())return true;
  bool exact=spanX==width&&spanY==height;
  std::vector<std::vector<std::pair<int,int>>> rows(height);double sx=spanX/width,sy=spanY/height;
  for(const auto&r:dirtyRects_){int x0=std::clamp(int(std::floor(r[0]/sx)),0,width),x1=std::clamp(int(std::ceil(r[2]/sx)),0,width);
   int y0=std::clamp(int(std::floor(r[1]/sy)),0,height),y1=std::clamp(int(std::ceil(r[3]/sy)),0,height);for(int y=y0;y<y1;++y)rows[y].emplace_back(x0,x1);}
  for(auto&spans:rows){std::sort(spans.begin(),spans.end());size_t used=0;for(auto span:spans)if(span.first<span.second){if(used&&span.first<=spans[used-1].second)spans[used-1].second=std::max(spans[used-1].second,span.second);else spans[used++]=span;}spans.resize(used);}
  if(sx>=2&&sy>=2&&std::floor(sx*2)==sx*2&&std::floor(sy*2)==sy*2&&presentBoxes(width,height,out,int(sx*2),int(sy*2),&rows))return true;
  parallelrows::rows(size_t(height),size_t(std::ceil(spanX))*size_t(std::ceil(spanY)),[&](size_t first,size_t end){Filter filter(*this);
  for(int y=int(first);y<int(end);++y){auto&spans=rows[y];std::sort(spans.begin(),spans.end());int last=0;for(auto span:spans){int from=std::max(last,span.first);
   if(exact)nativeRow(y,from,span.second,out.data()+size_t(y)*width);
   else for(int x=from;x<span.second;++x)out[size_t(y)*width+x]=filter.pixel(x*sx,y*sy,(x+1)*sx,(y+1)*sy);
   last=std::max(last,span.second);}}});
  return true;
 }
};
}}
