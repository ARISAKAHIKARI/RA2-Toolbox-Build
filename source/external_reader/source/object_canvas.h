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
  std::shared_ptr<const voxel::Image> image;float x=0,y=0,base=0,lift=1,drop=0;lighting::State light;Material material;
  uint32_t object=0,id=0;
  bool operator==(const Draw&b)const{return image.get()==b.image.get()&&!image.owner_before(b.image)&&!b.image.owner_before(image)&&
    x==b.x&&y==b.y&&base==b.base&&lift==b.lift&&drop==b.drop&&light==b.light&&material==b.material&&object==b.object&&id==b.id;}
 };
 struct Stats {size_t prefixBuilds=0,prefixHits=0,prefixRasterCalls=0,changed=0,reused=0,removed=0,references=0,rasterBuilds=0,rasterHits=0,orderedReplayPages=0,cloakDependencyPages=0,partialPages=0,restoredPixels=0;}stats_;
private:
 using TerrainList=std::shared_ptr<const std::vector<Draw>>;
 struct Job {TerrainList terrain;std::vector<Draw>draws;};
 struct Entry {
  bool hasBase=false;std::weak_ptr<NativeCanvas::Page>base;uint64_t version=0,used=0;
  TerrainList terrain;std::vector<Draw>draws;std::shared_ptr<NativeCanvas::Page>page;std::shared_ptr<const std::vector<uint8_t>>alpha;
 };
 std::map<uint32_t,Entry>pages_;uint64_t prefixSerial_=0;
 struct PrefixCharge {
  inline static std::atomic<size_t>used{0};static constexpr size_t Limit=
#ifdef RA2_CPU_TERRAIN_PREFIX_BYTES
   RA2_CPU_TERRAIN_PREFIX_BYTES;
#else
   sizeof(void*)>=8?256ull*1024*1024:64ull*1024*1024;
#endif
  size_t bytes=0;explicit PrefixCharge(size_t n){auto now=used.load(std::memory_order_relaxed);while(n<=Limit&&now<=Limit-n)if(used.compare_exchange_weak(now,now+n,std::memory_order_relaxed)){bytes=n;break;}}
  ~PrefixCharge(){if(bytes)used.fetch_sub(bytes,std::memory_order_relaxed);}PrefixCharge(const PrefixCharge&)=delete;
 };
 // Charge is destroyed LAST, after every owned Page plane and Draw buffer.
 struct PrefixState {PrefixCharge charge;Entry entry;explicit PrefixState(size_t bytes):charge(bytes){}};
 std::map<uint32_t,std::shared_ptr<PrefixState>>terrainPrefixes_;
 static std::shared_ptr<NativeCanvas::Budget>prefixPageBudget(){static auto b=[](){auto p=std::make_shared<NativeCanvas::Budget>();p->maximum=std::numeric_limits<size_t>::max();return p;}();return b;}
std::weak_ptr<NativeCanvas::Budget>budget_;
 struct CloakSource {bool present=false;std::weak_ptr<NativeCanvas::Page>page;uint64_t revision=0;};
 std::map<uint32_t,CloakSource>cloakSources_;
 GpuSceneRecord areaRecord_;std::shared_ptr<const void>areaTerrainOwner_;
 std::array<int,6>areaTerrainBounds_{};colorprecision::Mode areaTerrainPrecision_=colorprecision::Mode::NativeSteps;
 std::map<uint32_t,std::vector<uint32_t>>areaTerrainRefs_;size_t areaTerrainDraws_=0,areaTerrainRefCount_=0;
 std::shared_ptr<const std::vector<uint32_t>>areaBlank_;uint32_t areaBlankColor_=0;
 std::shared_ptr<const void>cpuTerrainOwner_;std::array<int,6>cpuTerrainBounds_{};
 colorprecision::Mode cpuTerrainPrecision_=colorprecision::Mode::NativeSteps;bool cpuTerrainValid_=false;
 std::vector<Draw>cpuTerrainDraws_;std::map<uint32_t,TerrainList>cpuTerrainJobs_;size_t cpuTerrainReferences_=0;
 struct Raster {std::weak_ptr<const voxel::Image>source;std::shared_ptr<const voxel::Image>image;size_t bytes=0;uint64_t used=0;};
 std::map<const voxel::Image*,Raster>rasters_;size_t rasterBytes_=0;uint64_t rasterSerial_=0;
 int width_=0,height_=0;uint64_t revision_=0;uint32_t background_=0; colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 static bool sameBase(const Entry&e,const std::shared_ptr<NativeCanvas::Page>&base,const std::shared_ptr<const std::vector<uint8_t>>&alpha){
  return e.alpha==alpha&&e.hasBase==bool(base)&&(!base||(e.base.lock()==base&&e.version==base->revision));
 }
 static bool same(const Entry&e,const std::shared_ptr<NativeCanvas::Page>&base,const Job&job,const std::shared_ptr<const std::vector<uint8_t>>&alpha){
  return sameBase(e,base,alpha)&&e.terrain==job.terrain&&e.draws==job.draws;
 }
 static bool sameTerrain(const Entry&e,const std::shared_ptr<NativeCanvas::Page>&base,const TerrainList&terrain,const std::shared_ptr<const std::vector<uint8_t>>&alpha){
  return sameBase(e,base,alpha)&&e.terrain==terrain;
 }
 static std::array<int,4> changedBounds(uint32_t key,const std::vector<Draw>&old,const std::vector<Draw>&next){
  size_t first=0,endOld=old.size(),endNext=next.size();while(first<endOld&&first<endNext&&old[first]==next[first])++first;
  while(endOld>first&&endNext>first&&old[endOld-1]==next[endNext-1]){--endOld;--endNext;}
  float left=1e7f,top=1e7f,right=-1e7f,bottom=-1e7f;
  auto include=[&](const Draw&d){if(d.image)NativeCanvas::imageBounds(*d.image,d.x,d.y,d.drop,left,top,right,bottom);};
  for(size_t n=first;n<endOld;++n)include(old[n]);for(size_t n=first;n<endNext;++n)include(next[n]);
  const int x=NativeCanvas::pageX(key)*128,y=NativeCanvas::pageY(key)*128;
  return {{std::clamp(int(std::floor(left))-x,0,128),std::clamp(int(std::floor(top))-y,0,128),std::clamp(int(std::ceil(right))-x,0,128),std::clamp(int(std::ceil(bottom))-y,0,128)}};
 }
 static bool displaced(const std::vector<Draw>&draws){return std::any_of(draws.begin(),draws.end(),[](const Draw&d){return d.material.displace;});}
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
 void clear(){terrainPrefixes_.clear();pages_.clear();cloakSources_.clear();areaRecord_.clear();areaTerrainOwner_.reset();areaTerrainRefs_.clear();areaTerrainDraws_=areaTerrainRefCount_=0;areaBlank_.reset();cpuTerrainOwner_.reset();cpuTerrainDraws_.clear();cpuTerrainJobs_.clear();cpuTerrainReferences_=0;cpuTerrainValid_=false;rasters_.clear();rasterBytes_=rasterSerial_=0;budget_.reset();width_=height_=0;++revision_;stats_={};}
 size_t terrainPrefixBytes()const{size_t bytes=0;for(const auto&e:terrainPrefixes_)bytes+=e.second->charge.bytes;return bytes;}
 size_t terrainPrefixPages()const{size_t n=0;for(const auto&e:terrainPrefixes_)n+=bool(e.second->entry.page);return n;}
 static size_t terrainPrefixPoolBytes(){return PrefixCharge::used.load();}
 const Stats&stats()const{return stats_;}uint64_t revision()const{return revision_;}
 // Vera's retained native source -> ordered draws -> final resolve contract.
 // Only metadata is built here. CPU rasterization is deferred to compose()
 // when the GPU cannot complete the whole map transaction.
 bool planGPU(const NativeCanvas&base,const std::vector<Draw>&draws,int width,int height,double spanX,double spanY,gpuarea::Plan&out){
  if(!base.areaPlan(width,height,out,spanX,spanY))return false;
  const auto owner=base.terrainSourceOwner();const std::array<int,6>bounds{{base.width_,base.height_,base.paintLeft_,base.paintTop_,base.paintRight_,base.paintBottom_}};
  const bool reuse=owner&&owner==areaTerrainOwner_&&bounds==areaTerrainBounds_&&base.precision_==areaTerrainPrecision_&&areaRecord_.prefixAvailable();
  areaRecord_.beginObjects(base.width_,base.height_,base.precision_,reuse);out.nativeSurface=!base.terrainSources().empty();areaRecord_.nativeView(1,1,out.nativeSurface);
  auto references=[&](const vulkancompute::DrawJob&d,auto&&visit){
   auto edge=[](float x,int limit,bool high){return std::clamp(int(high?std::ceil(std::clamp(x,-1e7f,1e7f)):std::floor(std::clamp(x,-1e7f,1e7f))),0,limit);};
   int l=std::max(edge(d.left,base.width_,false),base.paintLeft_),t=std::max(edge(d.top,base.height_,false),base.paintTop_);
   int r=std::min(edge(d.left+d.source.width*d.scaleX,base.width_,true),base.paintRight_),b=std::min(edge(d.top+d.source.height*d.scaleY,base.height_,true),base.paintBottom_);
   if(l>=r||t>=b)return true;for(int y=t/128;y<=(b-1)/128;++y)for(int x=l/128;x<=(r-1)/128;++x)if(!visit(NativeCanvas::key(x,y)))return false;return true;
  };
  if(!reuse){
   areaTerrainOwner_.reset();areaTerrainRefs_.clear();areaTerrainDraws_=areaTerrainRefCount_=0;
   if(out.nativeSurface)for(const auto&t:base.terrainSources())if(t.image)if(!areaRecord_.append(*t.image,t.image,t.x,t.y,1,1,t.base,t.light,0,0,false,{},t.material))return false;
   areaTerrainDraws_=areaRecord_.frame().draws.size();
   for(uint32_t n=0;n<areaTerrainDraws_;++n)if(!references(areaRecord_.frame().draws[n],[&](uint32_t key){if(++areaTerrainRefCount_>2*1024*1024)return false;areaTerrainRefs_[key].push_back(n);return true;}))return false;
   areaRecord_.retainPrefix();areaTerrainOwner_=owner;areaTerrainBounds_=bounds;areaTerrainPrecision_=base.precision_;
  }
  for(const auto&d:draws)if(!d.image||!areaRecord_.append(*d.image,d.image,d.x,d.y,1,1,d.base,d.light,d.lift,d.drop,false,{},d.material))return false;
  auto objects=std::make_shared<const std::vector<vulkancompute::DrawJob>>(areaRecord_.frame().draws);out.objects=objects;
  std::map<uint32_t,size_t>lookup;for(size_t n=0;n<out.pages.size();++n)lookup[NativeCanvas::key(out.pages[n].x,out.pages[n].y)]=n;
  auto page=[&](uint32_t key)->gpuarea::Page*{auto found=lookup.find(key);if(found==lookup.end()){
    if(out.pages.size()>=gpuarea::MaximumPages)return nullptr;
    if(!areaBlank_||areaBlankColor_!=base.background_){areaBlank_=std::make_shared<const std::vector<uint32_t>>(gpuarea::PagePixels,base.background_);areaBlankColor_=base.background_;}
    found=lookup.emplace(key,out.pages.size()).first;out.pages.push_back({uint32_t(NativeCanvas::pageX(key)),uint32_t(NativeCanvas::pageY(key)),areaBlank_,0,{},{},base.nativeAlpha(key)});
   }return &out.pages[found->second];
  };
  size_t refs=areaTerrainRefCount_;
  for(const auto&entry:areaTerrainRefs_){auto target=page(entry.first);if(!target)return false;target->draws=entry.second;}
  for(size_t n=areaTerrainDraws_;n<objects->size();++n)if(!references((*objects)[n],[&](uint32_t key){if(++refs>2*1024*1024)return false;auto target=page(key);if(!target)return false;target->draws.push_back(uint32_t(n));return true;}))return false;
  return gpuarea::valid(out);
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
   bool prefixMetadataAvailable=true,prefixPlanningReady=false;
   auto addJob=[&](const Draw&draw,auto&&emit,size_t&references){float left=1e7f,top=1e7f,right=-1e7f,bottom=-1e7f;
    NativeCanvas::imageBounds(*draw.image,draw.x,draw.y,draw.drop,left,top,right,bottom);
    if(!std::isfinite(left)||!std::isfinite(top)||!std::isfinite(right)||!std::isfinite(bottom))return false;
    int l=std::clamp(int(std::floor(std::clamp(left,-1e7f,1e7f))),base.paintLeft_,base.paintRight_);
    int t=std::clamp(int(std::floor(std::clamp(top,-1e7f,1e7f))),base.paintTop_,base.paintBottom_);
    int r=std::clamp(int(std::ceil(std::clamp(right,-1e7f,1e7f))),base.paintLeft_,base.paintRight_);
    int b=std::clamp(int(std::ceil(std::clamp(bottom,-1e7f,1e7f))),base.paintTop_,base.paintBottom_);
    if(l>=r||t>=b)return true;
    for(int y=t/NativeCanvas::PageSize;y<=(b-1)/NativeCanvas::PageSize;++y)for(int x=l/NativeCanvas::PageSize;x<=(r-1)/NativeCanvas::PageSize;++x){
     if(++references>64*1024*1024/sizeof(Draw))return false;emit(NativeCanvas::key(x,y),draw);
    }
    return true;
   };
   const auto terrainOwner=base.terrainSourceOwner();const std::array<int,6>terrainBounds{{base.width_,base.height_,base.paintLeft_,base.paintTop_,base.paintRight_,base.paintBottom_}};
   if(!cpuTerrainValid_||cpuTerrainOwner_!=terrainOwner||cpuTerrainBounds_!=terrainBounds||cpuTerrainPrecision_!=base.precision_){
    cpuTerrainValid_=false;cpuTerrainDraws_.clear();cpuTerrainReferences_=0;
    std::map<uint32_t,std::vector<Draw>>rebuilt;
    cpuTerrainDraws_.reserve(base.terrainSources().size());for(const auto&t:base.terrainSources())if(t.image){Draw d{native(t.image),t.x,t.y,t.base,0,0,t.light,t.material};if(!d.image||!addJob(d,[&](uint32_t key,const Draw&draw){rebuilt[key].push_back(draw);},cpuTerrainReferences_))return fail();cpuTerrainDraws_.push_back(std::move(d));}
    // Reuse each unchanged immutable list even when another terrain cell
    // changes the owner. Dynamic frames retain pointers, never copy TMP draws.
    std::map<uint32_t,TerrainList>next;
    for(auto&job:rebuilt){auto old=cpuTerrainJobs_.find(job.first);
     next[job.first]=old!=cpuTerrainJobs_.end()&&*old->second==job.second?old->second:std::make_shared<const std::vector<Draw>>(std::move(job.second));}
    cpuTerrainJobs_=std::move(next);
    cpuTerrainOwner_=terrainOwner;cpuTerrainBounds_=terrainBounds;cpuTerrainPrecision_=base.precision_;cpuTerrainValid_=true;
   }
   const size_t terrainCount=cpuTerrainDraws_.size();auto objects=draws;for(auto&d:objects){d.image=native(d.image);if(!d.image)return fail();}
   auto drawAt=[&](size_t n)->const Draw&{return n<terrainCount?cpuTerrainDraws_[n]:objects[n-terrainCount];};
   const size_t drawCount=terrainCount+objects.size();const bool hasCloak=std::any_of(cpuTerrainDraws_.begin(),cpuTerrainDraws_.end(),[](const Draw&d){return d.material.displace;})||std::any_of(objects.begin(),objects.end(),[](const Draw&d){return d.material.displace;});
   std::map<uint32_t,Job>jobs;for(const auto&terrain:cpuTerrainJobs_)jobs[terrain.first].terrain=terrain.second;
   stats_.references=cpuTerrainReferences_;for(const auto&d:objects)if(!addJob(d,[&](uint32_t key,const Draw&draw){jobs[key].draws.push_back(draw);},stats_.references))return fail();
   auto terrainAt=[&](uint32_t key)->TerrainList{auto at=cpuTerrainJobs_.find(key);return at==cpuTerrainJobs_.end()?TerrainList{}:at->second;};
   if(!base.terrainSources().empty())for(const auto&page:base.pages_)jobs.try_emplace(page.first);
   if(!jobs.empty())out.regionCache_.reset();
   auto dirty=[&](uint32_t k,std::array<int,4>rect=std::array<int,4>{{0,0,128,128}}){out.dirty_.insert(k);
    int x=NativeCanvas::pageX(k)*NativeCanvas::PageSize,y=NativeCanvas::pageY(k)*NativeCanvas::PageSize;
    out.dirtyRects_.push_back({x+rect[0],y+rect[1],std::min(x+rect[2],base.width_),std::min(y+rect[3],base.height_)});
   };
   // A cloak reads a finite set of native pixels, not the entire framebuffer.
   // Replay the connected components of changed pages and cloak read/write
   // dependencies in global painter order. Including both ends is essential:
   // an unchanged source's final cached pixels may contain a LATER draw, and
   // a changed source must also invalidate its otherwise unchanged readers.
   std::set<uint32_t>invalid,replay;std::map<uint32_t,uint32_t>links;
   if(hasCloak){for(const auto&p:pages_)if(!jobs.count(p.first))invalid.insert(p.first);
    for(const auto&job:jobs){auto source=base.pages_.find(job.first);auto page=source==base.pages_.end()?nullptr:source->second;auto cached=pages_.find(job.first);
     if(cached==pages_.end()||!cached->second.page||!same(cached->second,page,job.second,base.nativeAlpha(job.first)))invalid.insert(job.first);
    }
   }
   auto root=[&](uint32_t k){auto at=links.emplace(k,k).first;uint32_t r=k;while(links.find(r)->second!=r)r=links.find(r)->second;
    while(at->second!=r){uint32_t next=at->second;at->second=r;at=links.find(next);}return r;
   };
   std::map<uint32_t,CloakSource>sources;
   if(hasCloak){
    std::function<void(const Draw&,const voxel::Image&,float,Material,float)>connect;
    connect=[&](const Draw&draw,const voxel::Image&image,float dy,Material material,float drop){
     for(const auto&part:image.attachments)if(part.shadow&&!image.shadowsAfter&&(!part.airborneShadow||drop>0))connect(draw,part,dy+drop,material.shifted(drop),0);
     if(material.displace&&image.nativeDepth.voxel&&image.rasterScale>0){
      int l=std::clamp(int(std::floor(draw.x+float(image.x)/image.rasterScale)),base.paintLeft_,base.paintRight_),r=std::clamp(int(std::ceil(draw.x+float(image.x+image.width)/image.rasterScale)),base.paintLeft_,base.paintRight_);
      int t=std::clamp(int(std::floor(draw.y+dy+float(image.y)/image.rasterScale)),base.paintTop_,base.paintBottom_),b=std::clamp(int(std::ceil(draw.y+dy+float(image.y+image.height)/image.rasterScale)),base.paintTop_,base.paintBottom_);
      const int64_t limit=int64_t(base.width_)*base.height_;
      for(int y=t;y<b;++y)for(int x=l;x<r;){const int end=std::min(r,(x/128+1)*128);const uint32_t destination=NativeCanvas::key(x/128,y/128);
       int64_t first=int64_t(y)*base.width_+x+material.cloakWords,last=first+end-x;first=std::max(int64_t(0),first);last=std::min(limit,last);
       while(first<last){const int sx=int(first%base.width_),sy=int(first/base.width_);const uint32_t source=NativeCanvas::key(sx/128,sy/128);auto a=root(destination),b=root(source);if(a!=b)links[b]=a;
        if(!sources.count(source)){auto at=base.pages_.find(source);auto page=at==base.pages_.end()?nullptr:at->second;sources.emplace(source,CloakSource{bool(page),page,page?page->revision:0});}
        first+=std::min({last-first,int64_t(base.width_-sx),int64_t(128-sx%128)});
       }x=end;
      }
     }
     for(const auto&part:image.attachments)if((!part.shadow||image.shadowsAfter)&&(!part.airborneShadow||drop>0))connect(draw,part,dy+(part.shadow?drop:0),material.shifted(part.shadow?drop:0),0);
    };
    for(size_t n=0;n<drawCount;++n){const auto&draw=drawAt(n);connect(draw,*draw.image,0,draw.material,draw.drop);}
    // Source-only base pages are not necessarily jobs. Their mutation must
    // still invalidate a cloak whose own destination page did not change.
    for(const auto&source:sources){auto old=cloakSources_.find(source.first);const auto&now=source.second;
     if(old==cloakSources_.end()||old->second.present!=now.present||(now.present&&(old->second.page.lock()!=now.page.lock()||old->second.revision!=now.revision)))invalid.insert(source.first);
    }
    std::set<uint32_t>components;for(uint32_t key:invalid)if(links.count(key))components.insert(root(key));
    for(const auto&link:links)if(components.count(root(link.first)))replay.insert(link.first);
   }
   cloakSources_=std::move(sources);stats_.cloakDependencyPages=links.size();
   replay.insert(invalid.begin(),invalid.end());
   for(auto p=pages_.begin();p!=pages_.end();)if(!jobs.count(p->first)){if(relativeOutput)dirty(p->first);p=pages_.erase(p);++stats_.removed;}else ++p;
   const bool orderedReplay=hasCloak&&!replay.empty();
   std::set<uint32_t>prefixApplied;
   struct PrefixTask {uint32_t key;std::shared_ptr<PrefixState>state;std::shared_ptr<NativeCanvas::Page>source;TerrainList draws;std::shared_ptr<const std::vector<uint8_t>>alpha;};
   std::vector<PrefixTask>prefixTasks;
   try{
   if(!terrainCount||!prefixMetadataAvailable)terrainPrefixes_.clear();
   else{
    ++prefixSerial_;std::set<uint32_t>wantedPrefixes;
    for(const auto&job:jobs){auto source=base.pages_.find(job.first);auto sourcePage=source==base.pages_.end()?nullptr:source->second;auto old=pages_.find(job.first);
     if(!replay.count(job.first)&&old!=pages_.end()&&old->second.page&&same(old->second,sourcePage,job.second,base.nativeAlpha(job.first)))continue;
     wantedPrefixes.insert(job.first);auto found=terrainPrefixes_.find(job.first);
     if(found!=terrainPrefixes_.end()&&(!found->second->entry.page||!sameTerrain(found->second->entry,sourcePage,terrainAt(job.first),base.nativeAlpha(job.first))))terrainPrefixes_.erase(found);
    }
    // Reclaim only this renderer's inactive prefixes. Newly admitted/current
    // entries are never evicted again during this frame. Other views consume
    // the same strictly charged byte pool and can simply decline admission.
    std::vector<std::pair<uint64_t,uint32_t>>inactive;for(const auto&e:terrainPrefixes_)if(!wantedPrefixes.count(e.first))inactive.push_back({e.second->entry.used,e.first});std::sort(inactive.begin(),inactive.end());size_t evictAt=0;
    for(const auto&job:jobs)if(wantedPrefixes.count(job.first)){auto source=base.pages_.find(job.first);auto sourcePage=source==base.pages_.end()?nullptr:source->second;const auto&staticDraws=terrainAt(job.first);auto found=terrainPrefixes_.find(job.first);
     if(found!=terrainPrefixes_.end()){found->second->entry.used=prefixSerial_;++stats_.prefixHits;continue;}
     // Charge the retained immutable draw owner as conservatively as before.
     const size_t estimate=sizeof(PrefixState)+sizeof(NativeCanvas::Page)+size_t(128)*128*10+(staticDraws?staticDraws->capacity()*sizeof(Draw):0)+1024;
     auto state=std::make_shared<PrefixState>(estimate);
     while(!state->charge.bytes&&evictAt<inactive.size()){terrainPrefixes_.erase(inactive[evictAt++].second);state=std::make_shared<PrefixState>(estimate);}
     if(!state->charge.bytes)continue;
     terrainPrefixes_[job.first]=state;prefixTasks.push_back({job.first,std::move(state),sourcePage,staticDraws,base.nativeAlpha(job.first)});
    }
    std::atomic<size_t>buildCount{0},rasterCount{0};
    parallelrows::rows(prefixTasks.size(),prefixTasks.size()*size_t(128)*128,[&](size_t first,size_t last){
     for(size_t n=first;n<last;++n){const auto&task=prefixTasks[n];auto&prefix=task.state->entry;
      try{
       auto page=std::make_shared<NativeCanvas::Page>(prefixPageBudget());
       if(task.source){const auto&src=task.source->surface;if(!page->surface.restoreFrame(128,128,src.color,src.depth,src.nativeZ,src.alphaPlane()))continue;}
       else if(base.background_!=0xff080c08u)std::fill(page->surface.color.begin(),page->surface.color.end(),base.background_);
       page->surface.precision(base.precision_);int px=NativeCanvas::pageX(task.key)*128,py=NativeCanvas::pageY(task.key)*128;
       page->surface.nativeView(py,1,1,base.width_,base.height_,px);page->surface.writeRows(0,128);page->surface.nativeBase(task.alpha);
       if(task.draws)for(const auto&draw:*task.draws)page->surface.blit(*draw.image,draw.x-px,draw.y-py,1,1,draw.base,draw.light,draw.lift,draw.drop,false,draw.material);
       if(page->surface.failed())continue;
       prefix.terrain=task.draws;
       const auto&surface=page->surface;const size_t actual=sizeof(PrefixState)+sizeof(NativeCanvas::Page)+surface.color.capacity()*4+surface.depth.capacity()*4+surface.nativeZ.capacity()*2+(prefix.terrain?prefix.terrain->capacity()*sizeof(Draw):0)+1024;
       if(actual>task.state->charge.bytes){prefix=Entry{};continue;}
       prefix.hasBase=bool(task.source);prefix.base=task.source;prefix.version=task.source?task.source->revision:0;prefix.alpha=task.alpha;prefix.used=prefixSerial_;prefix.page=std::move(page);++prefix.page->revision;
       ++buildCount;rasterCount.fetch_add(task.draws?task.draws->size():0,std::memory_order_relaxed);
      }catch(const std::bad_alloc&){prefix.page.reset();}
     }
    });stats_.prefixBuilds=buildCount;stats_.prefixRasterCalls=rasterCount;
    for(const auto&task:prefixTasks)if(!task.state->entry.page)terrainPrefixes_.erase(task.key);
    prefixPlanningReady=true;
   }
   }catch(const std::bad_alloc&){terrainPrefixes_.clear();prefixTasks.clear();prefixApplied.clear();stats_.prefixBuilds=stats_.prefixHits=stats_.prefixRasterCalls=0;prefixPlanningReady=false;}
   struct Work {uint32_t key;Entry*entry;std::shared_ptr<NativeCanvas::Page>initial,previous;bool fresh,partial;std::array<int,4>rect;};std::vector<Work>changed;changed.reserve(jobs.size());
   for(auto&job:jobs){auto source=base.pages_.find(job.first);std::shared_ptr<NativeCanvas::Page>sourcePage=source==base.pages_.end()?nullptr:source->second;
    auto&entry=pages_[job.first];
    std::array<int,4>damage{{0,0,128,128}};
    const bool reused=!replay.count(job.first)&&entry.page&&same(entry,sourcePage,job.second,base.nativeAlpha(job.first));
    if(reused){++stats_.reused;}
    else{
     std::shared_ptr<PrefixState>prefixOwner;
     std::shared_ptr<NativeCanvas::Page>terrainPage;
     if(terrainCount&&prefixPlanningReady){auto at=terrainPrefixes_.find(job.first);if(at!=terrainPrefixes_.end()){prefixOwner=at->second;terrainPage=prefixOwner->entry.page;}}
     auto initialPage=terrainPage?terrainPage:sourcePage;
     bool partial=entry.page&&!links.count(job.first)&&sameBase(entry,sourcePage,base.nativeAlpha(job.first))&&entry.terrain==job.second.terrain&&
      (!terrainCount||terrainPage)&&!displaced(entry.draws)&&!displaced(job.second.draws)&&(!job.second.terrain||!displaced(*job.second.terrain));
     if(partial){damage=changedBounds(job.first,entry.draws,job.second.draws);const int pixels=(damage[2]-damage[0])*(damage[3]-damage[1]);partial=pixels>0&&pixels<128*128;}
     if(!partial)damage={{0,0,128,128}};
     // A previous synchronous presentation normally released its page owners.
     // Reuse those two 128x128 buffers instead of free/allocate/copy per page.
     // A retained snapshot forces COW and keeps its old color/depth immutable.
     const bool reusable=entry.page&&entry.page.use_count()==1;
     std::shared_ptr<NativeCanvas::Page>previous;if(partial&&!reusable)previous=entry.page;
     if(!reusable)entry.page.reset();
     // Release cached pages before allocating another copy; the global native
     // page budget remains authoritative for both static and dynamic storage.
     while(!reusable&&!out.room()){
      auto evict=std::find_if(pages_.begin(),pages_.end(),[&](const auto&p){return p.first!=job.first&&p.second.page&&(!out.pages_.count(p.first)||out.pages_.at(p.first)!=p.second.page);});
      if(evict==pages_.end())return fail();
      // Eviction can turn a later cache hit into a rebuild after its cloak
      // dependency component was planned. Restart cold rather than publish a
      // page initialized here but omitted from the ordered replay subset.
      if(hasCloak){clear();out={};return compose(base,draws,out,false);}
      evict->second.page.reset();
     }
     entry.alpha=base.nativeAlpha(job.first);entry.hasBase=bool(sourcePage);entry.base=sourcePage;entry.version=sourcePage?sourcePage->revision:0;entry.terrain=job.second.terrain;entry.draws=std::move(job.second.draws);
     if(!reusable){entry.page=std::make_shared<NativeCanvas::Page>(base.budget_);entry.page->revision=sourcePage?sourcePage->revision:0;}
     if(terrainPage)prefixApplied.insert(job.first);
     // Keep the optional prefix charge alive with its page while a worker
     // restores it. Copy only framebuffer planes, on the owning CPU lane;
     // Surface's reconstruction caches and scratch belong to that lane.
     if(terrainPage)initialPage=std::shared_ptr<NativeCanvas::Page>(prefixOwner,terrainPage.get());
     if(partial)++stats_.partialPages;stats_.restoredPixels+=size_t(damage[2]-damage[0])*(damage[3]-damage[1])+(previous?size_t(128)*128:0);
     entry.page->changed(damage,partial&&reusable);changed.push_back({job.first,&entry,std::move(initialPage),std::move(previous),!reusable,partial,damage});++stats_.changed;
    }
    out.pages_[job.first]=entry.page;if(!relativeOutput||!reused)dirty(job.first,relativeOutput?damage:std::array<int,4>{{0,0,128,128}});
   }
   std::atomic<bool>failed{false};
   auto initialize=[&](const Work&work){auto&surface=work.entry->page->surface;
    if(work.partial){if(work.previous){const auto&s=work.previous->surface;if(!surface.restoreFrame(128,128,s.color,s.depth,s.nativeZ,s.alphaPlane()))return false;}
     if(!surface.restoreRegion(work.initial?&work.initial->surface:nullptr,base.background_,work.rect[0],work.rect[1],work.rect[2],work.rect[3]))return false;}
    else if(work.initial){const auto&s=work.initial->surface;if(!surface.restoreFrame(128,128,s.color,s.depth,s.nativeZ,s.alphaPlane()))return false;}
    else if(!work.fresh){if(!surface.reset(128,128,base.background_))return false;}
    else if(base.background_!=0xff080c08u)std::fill(surface.color.begin(),surface.color.end(),base.background_);
    surface.precision(base.precision_);int x=NativeCanvas::pageX(work.key)*128,y=NativeCanvas::pageY(work.key)*128;surface.nativeView(y,1,1,base.width_,base.height_,x);surface.writeRect(work.rect[0],work.rect[1],work.rect[2],work.rect[3]);if(!base.terrainSources().empty()&&!prefixApplied.count(work.key))surface.nativeBase(work.entry->alpha);return true;};
   std::vector<Work>independent;
   if(orderedReplay){
    // Global invalidation (e.g. current lighting) need not serialize distant
    // pages merely because one cloak exists. Pages outside every displacement
    // read/write component are independent even during a complete repaint.
    independent.reserve(changed.size());
    for(const auto&work:changed)if(links.count(work.key)){if(!initialize(work))return fail();++stats_.orderedReplayPages;}else independent.push_back(work);
    // Flatten attachments in the same shadow/body order as Surface::blit.
    // Only the cloak body needs a global scanline walk; ordinary planes have
    // no framebuffer read dependency and can visit each page as a unit.
    bool paintingTerrain=false;
    std::function<void(const Draw&,const voxel::Image&,float,Material,float)>paintPlane;
    paintPlane=[&](const Draw&draw,const voxel::Image&image,float dy,Material material,float drop){
     for(const auto&part:image.attachments)if(part.shadow&&!image.shadowsAfter&&(!part.airborneShadow||drop>0))paintPlane(draw,part,dy+drop,material.shifted(drop),0);
     int l=std::clamp(int(std::floor(draw.x+float(image.x)/image.rasterScale)),base.paintLeft_,base.paintRight_),r=std::clamp(int(std::ceil(draw.x+float(image.x+image.width)/image.rasterScale)),base.paintLeft_,base.paintRight_);
     int t=std::clamp(int(std::floor(draw.y+dy+float(image.y)/image.rasterScale)),base.paintTop_,base.paintBottom_),b=std::clamp(int(std::ceil(draw.y+dy+float(image.y+image.height)/image.rasterScale)),base.paintTop_,base.paintBottom_);
     auto paint=[&](int px,int py,int first,int last){uint32_t key=NativeCanvas::key(px,py);if(!replay.count(key)||!links.count(key)||(paintingTerrain&&prefixApplied.count(key)))return;auto page=out.pages_.find(key);if(page==out.pages_.end())return;auto&surface=page->second->surface;surface.writeRows(first-py*128,last-py*128);surface.nativeRead([&out](int x,int y){return out.pixel(x,y);});surface.blit(image,draw.x-px*128,draw.y+dy-py*128,1,1,draw.base,draw.light,draw.lift,0,false,material,false);surface.nativeRead({});surface.writeRows(0,128);if(surface.failed())failed=true;};
     if(l<r&&t<b){if(material.displace&&image.nativeDepth.voxel){for(int y=t;y<b;++y)for(int x=l/128;x<=(r-1)/128;++x)paint(x,y/128,y,y+1);}else for(int y=t/128;y<=(b-1)/128;++y)for(int x=l/128;x<=(r-1)/128;++x)paint(x,y,std::max(t,y*128),std::min(b,(y+1)*128));}
     for(const auto&part:image.attachments)if((!part.shadow||image.shadowsAfter)&&(!part.airborneShadow||drop>0))paintPlane(draw,part,dy+(part.shadow?drop:0),material.shifted(part.shadow?drop:0),0);
    };
    // Fully restored connected pages already contain this immutable terrain
    // prefix. Avoid walking every TMP just to reject all of its page writes.
    size_t firstOrdered=terrainCount;for(const auto&work:changed)if(links.count(work.key)&&!prefixApplied.count(work.key)){firstOrdered=0;break;}
    for(size_t n=firstOrdered;n<drawCount;++n){paintingTerrain=n<terrainCount;const auto&draw=drawAt(n);paintPlane(draw,*draw.image,0,draw.material,draw.drop);}
   }else independent=std::move(changed);
   parallelrows::rows(independent.size(),independent.size()*size_t(NativeCanvas::PageSize)*NativeCanvas::PageSize,[&](size_t first,size_t last){
    try{for(size_t n=first;n<last;++n){const auto&work=independent[n];auto&surface=work.entry->page->surface;
      const int x=NativeCanvas::pageX(work.key)*NativeCanvas::PageSize,y=NativeCanvas::pageY(work.key)*NativeCanvas::PageSize;if(!initialize(work)){failed=true;continue;}
      if(!prefixApplied.count(work.key)&&work.entry->terrain)for(const auto&draw:*work.entry->terrain)surface.blit(*draw.image,draw.x-x,draw.y-y,1,1,draw.base,draw.light,draw.lift,draw.drop,false,draw.material);
      for(const auto&draw:work.entry->draws)surface.blit(*draw.image,draw.x-x,draw.y-y,1,1,draw.base,draw.light,draw.lift,draw.drop,false,draw.material);
      surface.writeRows(0,128);
      if(surface.failed())failed=true;
     }}catch(const std::bad_alloc&){failed=true;}
   });
   if(stats_.changed||stats_.removed)++revision_;
   if(failed){clear();out.failed_=true;return false;}return true;
  }catch(const std::bad_alloc&){clear();out.failed_=true;return false;}
 }
};
}}

