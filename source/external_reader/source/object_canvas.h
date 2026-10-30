#pragma once
#include "native_canvas.h"
#include "gpu_scene_record.h"
#include <atomic>
namespace external {namespace camera {
// Objects keep native pixels and painter order. Independent pages can run on
// separate CPU lanes; an unchanged ordered page has identical RGBA AND depth.
class ObjectCanvas {
public:
 struct Draw {
  std::shared_ptr<const voxel::Image> image;float x=0,y=0,base=0,lift=1,drop=0;lighting::State light;
  bool operator==(const Draw&b)const{return image.get()==b.image.get()&&!image.owner_before(b.image)&&!b.image.owner_before(image)&&
    x==b.x&&y==b.y&&base==b.base&&lift==b.lift&&drop==b.drop&&light==b.light;}
 };
 struct Stats {size_t changed=0,reused=0,removed=0,references=0,rasterBuilds=0,rasterHits=0;}stats_;
private:
 struct Entry {
  bool hasBase=false;std::weak_ptr<NativeCanvas::Page>base;uint64_t version=0;
  std::vector<Draw>draws;std::shared_ptr<NativeCanvas::Page>page;
 };
 std::map<uint32_t,Entry>pages_;std::weak_ptr<NativeCanvas::Budget>budget_;
 struct Raster {std::weak_ptr<const voxel::Image>source;std::shared_ptr<const voxel::Image>image;size_t bytes=0;uint64_t used=0;};
 std::map<const voxel::Image*,Raster>rasters_;size_t rasterBytes_=0;uint64_t rasterSerial_=0;
 int width_=0,height_=0;uint64_t revision_=0;uint32_t background_=0; colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 static bool same(const Entry&e,const std::shared_ptr<NativeCanvas::Page>&base,const std::vector<Draw>&draws){
  return e.hasBase==bool(base)&&(!base||(e.base.lock()==base&&e.version==base->revision))&&e.draws==draws;
 }
 static bool gpu(const voxel::Image&i){if(i.gpuRaster)return true;for(const auto&p:i.attachments)if(gpu(p))return true;return false;}
 static size_t bytes(const voxel::Image&i){size_t n=sizeof i+i.pixels.size()*4+i.indices.size()+i.depth.size()*4+(i.sourcePalette?i.sourcePalette->size()*4:0)+(i.blendSource?i.blendSource->size()*4:0);for(const auto&p:i.attachments)n+=bytes(p);return n;}
 static bool materialize(const voxel::Image&in,voxel::Image&out){
  if(!gpuvoxel::materialize(in,out))return false;
  for(auto&part:out.attachments)if(gpu(part)){voxel::Image next;if(!materialize(part,next))return false;part=std::move(next);}return true;
 }
 std::shared_ptr<const voxel::Image>native(std::shared_ptr<const voxel::Image>source){
  if(!source||!gpu(*source))return source;auto found=rasters_.find(source.get());
  if(found!=rasters_.end()&&!found->second.source.owner_before(source)&&!source.owner_before(found->second.source)){
   found->second.used=++rasterSerial_;++stats_.rasterHits;return found->second.image;}
  voxel::Image next;if(!materialize(*source,next))return {};auto image=std::make_shared<const voxel::Image>(std::move(next));++stats_.rasterBuilds;
  if(found!=rasters_.end()){rasterBytes_-=found->second.bytes;rasters_.erase(found);}
  for(auto p=rasters_.begin();p!=rasters_.end();)if(p->second.source.expired()){rasterBytes_-=p->second.bytes;p=rasters_.erase(p);}else ++p;
  const size_t cost=bytes(*image);constexpr size_t Budget=rendermemory::RasterBytes;
  if(cost<=Budget){while(!rasters_.empty()&&rasterBytes_+cost>Budget){auto old=std::min_element(rasters_.begin(),rasters_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});rasterBytes_-=old->second.bytes;rasters_.erase(old);}
   rasterBytes_+=cost;rasters_.emplace(source.get(),Raster{source,image,cost,++rasterSerial_});}
  return image;
 }
public:
 void clear(){pages_.clear();rasters_.clear();rasterBytes_=rasterSerial_=0;budget_.reset();width_=height_=0;++revision_;stats_={};}
 const Stats&stats()const{return stats_;}uint64_t revision()const{return revision_;}
 // Vera's retained native source -> ordered draws -> final resolve contract.
 // Only metadata is built here. CPU rasterization is deferred to compose()
 // when the GPU cannot complete the whole map transaction.
 bool planGPU(const NativeCanvas&base,const std::vector<Draw>&draws,int width,int height,double spanX,double spanY,gpuarea::Plan&out){
  if(!base.areaPlan(width,height,out,spanX,spanY))return false;
  GpuSceneRecord record;record.beginObjects(base.width_,base.height_,base.precision_);
  for(const auto&d:draws)if(!d.image||!record.append(*d.image,d.image,d.x,d.y,1,1,d.base,d.light,d.lift,d.drop,false))return false;
  auto objects=std::make_shared<const std::vector<vulkancompute::DrawJob>>(record.frame().draws);out.objects=objects;
  std::map<uint32_t,size_t>lookup;for(size_t n=0;n<out.pages.size();++n)lookup[NativeCanvas::key(out.pages[n].x,out.pages[n].y)]=n;
  std::shared_ptr<const std::vector<uint32_t>>blank;size_t refs=0;
  for(size_t n=0;n<objects->size();++n){const auto&d=(*objects)[n];
   auto edge=[](float x,int limit,bool high){return std::clamp(int(high?std::ceil(std::clamp(x,-1e7f,1e7f)):std::floor(std::clamp(x,-1e7f,1e7f))),0,limit);};
   int l=edge(d.left,base.width_,false),t=edge(d.top,base.height_,false),r=edge(d.left+d.source.width*d.scaleX,base.width_,true),b=edge(d.top+d.source.height*d.scaleY,base.height_,true);
   l=std::max(l,base.paintLeft_);t=std::max(t,base.paintTop_);r=std::min(r,base.paintRight_);b=std::min(b,base.paintBottom_);if(l>=r||t>=b)continue;
   for(int y=t/128;y<=(b-1)/128;++y)for(int x=l/128;x<=(r-1)/128;++x){if(++refs>2*1024*1024)return false;auto k=NativeCanvas::key(x,y);auto found=lookup.find(k);
    if(found==lookup.end()){if(out.pages.size()>=gpuarea::MaximumPages)return false;if(!blank)blank=std::make_shared<const std::vector<uint32_t>>(gpuarea::PagePixels,base.background_);
     found=lookup.emplace(k,out.pages.size()).first;out.pages.push_back({uint32_t(x),uint32_t(y),blank,0,{},{}});
    }out.pages[found->second].draws.push_back(uint32_t(n));
   }
  }return gpuarea::valid(out);
 }
 // relativeOutput requires the caller's last resolved pixels to match our
 // previous revision AND the current base/view. Otherwise dirty means every
 // object page, so callers starting with terrain alone remain correct.
 bool compose(const NativeCanvas&base,const std::vector<Draw>&draws,NativeCanvas&out,bool relativeOutput=false){
  stats_={};out=base.fork();auto fail=[&]{clear();out.failed_=true;return false;};if(base.failed_)return fail();
  if(budget_.lock()!=base.budget_||width_!=base.width_||height_!=base.height_||background_!=base.background_||precision_!=base.precision_){
   relativeOutput=false;clear();budget_=base.budget_;width_=base.width_;height_=base.height_;background_=base.background_;precision_=base.precision_;
  }
  try{
   std::map<uint32_t,std::vector<Draw>>jobs;
   for(const auto&input:draws){Draw draw=input;draw.image=native(input.image);if(!draw.image)return fail();float left=1e7f,top=1e7f,right=-1e7f,bottom=-1e7f;
    NativeCanvas::imageBounds(*draw.image,draw.x,draw.y,draw.drop,left,top,right,bottom);
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return fail();
    int l=std::clamp(int(std::floor(std::clamp(left,-1e7f,1e7f))),base.paintLeft_,base.paintRight_);
    int t=std::clamp(int(std::floor(std::clamp(top,-1e7f,1e7f))),base.paintTop_,base.paintBottom_);
    int r=std::clamp(int(std::ceil(std::clamp(right,-1e7f,1e7f))),base.paintLeft_,base.paintRight_);
    int b=std::clamp(int(std::ceil(std::clamp(bottom,-1e7f,1e7f))),base.paintTop_,base.paintBottom_);
    if(l>=r||t>=b)continue;
    for(int y=t/NativeCanvas::PageSize;y<=(b-1)/NativeCanvas::PageSize;++y)for(int x=l/NativeCanvas::PageSize;x<=(r-1)/NativeCanvas::PageSize;++x){
     if(++stats_.references>64*1024*1024/sizeof(Draw))return fail();jobs[NativeCanvas::key(x,y)].push_back(draw);
    }
   }
   if(!jobs.empty())out.regionCache_.reset();
   auto dirty=[&](uint32_t k){out.dirty_.insert(k);
    int x=NativeCanvas::pageX(k)*NativeCanvas::PageSize,y=NativeCanvas::pageY(k)*NativeCanvas::PageSize;
    out.dirtyRects_.push_back({x,y,std::min(x+NativeCanvas::PageSize,base.width_),std::min(y+NativeCanvas::PageSize,base.height_)});
   };
   for(auto p=pages_.begin();p!=pages_.end();)if(!jobs.count(p->first)){if(relativeOutput)dirty(p->first);p=pages_.erase(p);++stats_.removed;}else ++p;
   struct Work {uint32_t key;Entry*entry;};std::vector<Work>changed;changed.reserve(jobs.size());
   for(auto&job:jobs){auto source=base.pages_.find(job.first);std::shared_ptr<NativeCanvas::Page>sourcePage=source==base.pages_.end()?nullptr:source->second;
    auto&entry=pages_[job.first];
    const bool reused=entry.page&&same(entry,sourcePage,job.second);
    if(reused){++stats_.reused;}
    else{
     // A previous synchronous presentation normally released its page owners.
     // Reuse those two 128x128 buffers instead of free/allocate/copy per page.
     // A retained snapshot forces COW and keeps its old color/depth immutable.
     const bool reusable=entry.page&&entry.page.use_count()==1;
     if(!reusable)entry.page.reset();
     // Release cached pages before allocating another copy; the global native
     // page budget remains authoritative for both static and dynamic storage.
     while(!reusable&&!out.room()){
      auto evict=std::find_if(pages_.begin(),pages_.end(),[&](const auto&p){return p.first!=job.first&&p.second.page&&(!out.pages_.count(p.first)||out.pages_.at(p.first)!=p.second.page);});
      if(evict==pages_.end())return fail();evict->second.page.reset();
     }
     entry.hasBase=bool(sourcePage);entry.base=sourcePage;entry.version=sourcePage?sourcePage->revision:0;entry.draws=std::move(job.second);
     if(reusable){if(sourcePage)entry.page->surface=sourcePage->surface;else if(!entry.page->surface.reset(NativeCanvas::PageSize,NativeCanvas::PageSize,base.background_))return fail();}
     else entry.page=sourcePage?std::make_shared<NativeCanvas::Page>(*sourcePage):std::make_shared<NativeCanvas::Page>(base.budget_);
     if(!sourcePage&&base.background_!=0xff080c08u)std::fill(entry.page->surface.color.begin(),entry.page->surface.color.end(),base.background_);
     entry.page->surface.precision(base.precision_);++entry.page->revision;changed.push_back({job.first,&entry});++stats_.changed;
    }
    out.pages_[job.first]=entry.page;if(!relativeOutput||!reused)dirty(job.first);
   }
   std::atomic<bool>failed{false};
   parallelrows::rows(changed.size(),changed.size()*size_t(NativeCanvas::PageSize)*NativeCanvas::PageSize,[&](size_t first,size_t last){
    try{for(size_t n=first;n<last;++n){const auto&work=changed[n];auto&surface=work.entry->page->surface;
      const int x=NativeCanvas::pageX(work.key)*NativeCanvas::PageSize,y=NativeCanvas::pageY(work.key)*NativeCanvas::PageSize;
      for(const auto&draw:work.entry->draws)surface.blit(*draw.image,draw.x-x,draw.y-y,1,1,draw.base,draw.light,draw.lift,draw.drop);
      if(surface.failed())failed=true;
     }}catch(const std::bad_alloc&){failed=true;}
   });
   if(stats_.changed||stats_.removed)++revision_;
   if(failed){clear();out.failed_=true;return false;}return true;
  }catch(const std::bad_alloc&){clear();out.failed_=true;return false;}
 }
};
}}
