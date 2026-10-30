#pragma once
#include "radar_map.h"
#include "watch_view.h"
#include "camera_raster.h"
#include "native_canvas.h"
#include "object_canvas.h"
#include "radar_shroud_mask.h"
#include "radar_live_shroud.h"
#include "terrain_coverage.h"
#include "camera_repair.h"
#include "viewport_canvas.h"
#include "material_blend.h"
#include "gpu_shroud.h"
#include "render_profile.h"
#include <map>
#include <tuple>
#include <set>
#include <chrono>
namespace external { namespace radar {
struct SceneRegion {double x=0,y=0,width=0,height=0;bool active=false;};
template<class Assets>class SceneRenderer {
 presentation::Profile features_;
 struct Cached {std::shared_ptr<voxel::Image> image=std::make_shared<voxel::Image>();bool ready=false;uint32_t retry=0;};
 struct SpriteCached {std::shared_ptr<const voxel::Image> image=std::make_shared<voxel::Image>();bool ready=false;uint32_t retry=0;};
 using ArtKey=std::tuple<uint32_t,uint32_t,int,int,uint32_t,uint32_t>;
 using ImageKey=std::tuple<uint32_t,uint32_t,int,int,int,int,int,int,int,int,int,uint32_t,uint32_t,uint64_t,int,int,int>;
 using ObjectKey=std::pair<uint32_t,uint32_t>;
 static ArtKey artKey(const Marker&m){return {m.type,m.owner,m.kind,(m.kind==5||m.kind==7)?0:m.pose.variant,m.kind>=5?m.object:0,m.kind>=5?m.id:0};}
 static ImageKey imageKey(const Marker&m){return {m.type,m.owner,m.kind,m.pose.body,m.pose.turret,m.pose.frame,m.pose.variant,m.pose.buildingFlags,m.pose.doorFrame,m.pose.bodyFine,m.pose.turretFine,m.kind>=5?m.object:0,m.kind>=5?m.id:0,visual::postureSignature(m.pose),m.pose.turretFrame,m.pose.hvaBody,m.pose.hvaTurret};}
 static ArtKey artKey(const ImageKey&k){return {std::get<0>(k),std::get<1>(k),std::get<2>(k),(std::get<2>(k)==5||std::get<2>(k)==7)?0:std::get<6>(k),std::get<11>(k),std::get<12>(k)};}
 template<class A>static auto voxelArt(const A&a,int)->decltype(bool(a.voxel)){return a.voxel;}
 template<class A>static bool voxelArt(const A&,long){return false;}
 template<class A>static auto voxelRevision(const A&a,int)->decltype(a.voxelLightingRevision(),a.voxelLightingAvailable(),std::pair<bool,uint64_t>{}){return {a.voxelLightingAvailable(),a.voxelLightingRevision()};}
 template<class A>static std::pair<bool,uint64_t> voxelRevision(const A&,long){return {false,0};}
 template<class A>static auto faceCount(const A&a,int)->decltype(a.geometry().faces.size()){return a.geometry().faces.size();}
 template<class A>static auto faceCount(const A&a,long)->decltype(a.model.faces.size()){return a.model.faces.size();}
 template<class A>static size_t faceCount(const A&,...){return 0;}
 template<class A>static auto beginPalette(A&a,int)->decltype(a.beginPresentation(),void()){a.beginPresentation();}
 template<class A>static void beginPalette(A&,long){}
 template<class A>static auto gpuPresentation(const A&a,int)->decltype(bool(a.presentationGPU())){return a.presentationGPU();}
 template<class A>static bool gpuPresentation(const A&,long){return false;}
 template<class A>static auto refreshPalette(A&a,typename A::ActorArt&art,bool&changed,int)->decltype(a.refreshActorPalette(art,changed),bool()){return a.refreshActorPalette(art,changed);}
 template<class A>static bool refreshPalette(A&,typename A::ActorArt&,bool&changed,long){changed=false;return true;}
 template<class A>static auto refreshOverlay(A&a,uint32_t t,uint32_t p,int f,bool terrain,bool&changed,int)->decltype(a.refreshOverlayPalette(t,p,f,terrain,changed),bool()){return a.refreshOverlayPalette(t,p,f,terrain,changed);}
 template<class A>static bool refreshOverlay(A&,uint32_t,uint32_t,int,bool,bool&changed,long){changed=false;return true;}
 template<class A>static auto sharedActor(A&a,const typename A::ActorArt&art,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out,int)->decltype(a.renderSharedActor(art,pose,out),bool()){return a.renderSharedActor(art,pose,out);}
 template<class A>static bool sharedActor(A&a,const typename A::ActorArt&art,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out,long){voxel::Image image;if(!a.renderActor(art,pose,image))return false;camera::opaque(image);out=std::make_shared<const voxel::Image>(std::move(image));return true;}
 template<class A>static auto captureActor(A&a,const Marker&m,typename A::ActorArt&art,int)->decltype(a.captureActor(m.object,m.type,m.id,art,&m.pose),bool()){return a.captureActor(m.object,m.type,m.id,art,&m.pose);}
 template<class A>static bool captureActor(A&a,const Marker&m,typename A::ActorArt&art,long){return a.captureActor(m.object,m.type,m.id,art);}
 template<class Read>static auto currentVisibility(Read read,int)->decltype(read.fresh(0,nullptr,0),bool()){return true;}
 template<class Read>static bool currentVisibility(Read,long){return false;}
 template<class A>static auto alphaImage(A&a,uint32_t pointer,voxel::Image&out,int)->decltype(a.effectAlpha(pointer,out),bool()){return a.effectAlpha(pointer,out);}
 template<class A>static bool alphaImage(A&,uint32_t,voxel::Image&,long){return false;}
 std::map<uint32_t,std::shared_ptr<const voxel::Image>>effectAlphas_;
 void invalidateArt(const ArtKey&key){
  for(auto i=sprites_.begin();i!=sprites_.end();)if(artKey(i->first)==key)i=sprites_.erase(i);else ++i;
  for(auto i=displayed_.begin();i!=displayed_.end();)if(artKey(i->second)==key)i=displayed_.erase(i);else ++i;
 }
 std::array<uint32_t,256> terrainPalette_{};bool terrainPaletteReady_=false;
 colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 lighting::State buildingLight_;uint64_t voxelLightRevision_=0;bool voxelLightKnown_=false;
 struct DecodeStats {unsigned attempts=0,hits=0;double milliseconds=0,budget=0;} decodeStats_;
 struct StageStats {double terrainMs=0,materialsMs=0,palettesMs=0,decodeMs=0,voxelMs=0,objectsMs=0,resolveMs=0,effectsMs=0;size_t overlayScanCells=0,actors=0;} stageStats_;
 std::shared_ptr<const Terrain>overlayScanSource_;std::vector<std::pair<uint32_t,unsigned>>overlaySources_;
 std::map<std::pair<uint32_t,unsigned>,Cached> tiles_,overlays_;
 std::map<ArtKey,typename Assets::ActorArt> art_;
 std::map<ImageKey,SpriteCached> sprites_;
 std::map<ObjectKey,ImageKey> displayed_;size_t actorCursor_=0,transientCursor_=0;repairvisuals::Assets repairAssets_;
 std::shared_ptr<const Terrain> building_,complete_,blocked_;bool dynamicFailure_=false,materialsReady_=false;
 struct Paint {lighting::State light,overlayLight;int overlayOffset=0;bool bridge=false;uint64_t signature=0;};
 std::shared_ptr<camera::NativeCanvas::Budget> nativeBudget_;
 camera::NativeCanvas pending_,background_;camera::ObjectCanvas objectCanvas_;camera::ViewportCanvas detailed_;uint64_t detailKey_=0;std::shared_ptr<const Scene> baseScene_,maskedBaseScene_;
 bool gpuMode_=false,areaGpuMode_=false,areaCachedGpu_=false;
 bool guideEnabled_=false;uint64_t guideExternalContext_=0,motionTerrainRevision_=0;
 std::shared_ptr<const void> motionContext_=std::make_shared<const uint8_t>(0);
 std::shared_ptr<const std::vector<vulkancompute::TileJob>> gpuBaseTiles_;
 std::function<bool(std::vector<uint32_t>&)> cpuBaseFallback_;
 struct NativePresentation {
  std::shared_ptr<const std::vector<uint32_t>>pixels;uint64_t background=0,objects=0;bool gpu=false,used=false;
  std::shared_ptr<const gpushroud::Plan>mask;std::shared_ptr<const std::vector<uint8_t>>fog;
 }nativePresentation_;
 shroudmask::Raster mask_;std::shared_ptr<const Terrain> maskSource_;bool maskFog_=false;
 gpushroud::Builder gpuMaskBuilder_;std::shared_ptr<const gpushroud::Plan> gpuMaskPlan_,cpuMaskPlan_;
 std::shared_ptr<const std::vector<uint8_t>> maskPixels_;
 shroudmask::Live liveMasks_;shroudmask::Snapshot liveMaskSnapshot_;bool liveMaskValid_=false;
 terraincoverage::Region coverage_;bool coverageLocal_=false;
 std::vector<size_t> cells_,baseCells_;std::vector<Paint> prepared_;std::vector<uint64_t> painted_;size_t cursor_=0,baseCursor_=0;
 bool partialBank_=false;std::vector<size_t> changedOverlayCells_,repaintCells_;uint64_t partialBanks_=0;
 bool bankSameSource_=false,probing_=false,probeChanged_=false,pendingIncomplete_=false,backgroundIncomplete_=false;uint32_t backgroundRetry_=0;int width_=0,height_=0;double spanX_=0,spanY_=0;uint64_t revision_=0;
 lighting::State backgroundLight_;uint64_t nativeCells_=0,basePresents_=0;SceneRegion region_{};unsigned pixelScaleMode_=0;bool detailedBase_=false;
 static uint64_t mix(uint64_t h,uint64_t v){return (h^v)*1099511628211ull;}
 static uint64_t lightKey(uint64_t h,const lighting::State&s){h=mix(h,uint32_t(s.ambient));h=mix(h,uint32_t(s.intensity));for(int c:s.tint)h=mix(h,uint32_t(c));return h;}
 static bool sameSource(const Terrain&a,const Terrain&b,bool local=false){
  if(!a.geo.same(b.geo)||a.reveal!=b.reveal||a.colors.size()!=b.colors.size()||a.baseColors!=b.baseColors||a.art.size()!=b.art.size())return false;
  // Exploration belongs to the destination mask and actor admission. It must
  // not rebuild static TMP pages/xBRZ caches when their source art is intact.
  for(size_t n=0;n<a.art.size();++n){const auto&x=a.art[n];const auto&y=b.art[n];if(x.cell!=y.cell||x.type!=y.type||x.overlayType!=y.overlayType||x.overlaySourceType!=y.overlaySourceType||x.overlay!=y.overlay||x.sub!=y.sub||x.level!=y.level||x.overlayFrame!=y.overlayFrame||x.sourceColor!=y.sourceColor)return false;
   if(!local&&x.sourceHidden!=y.sourceHidden)return false;
   if(!x.sourceColor&&n<a.colors.size()&&a.colors[n]!=b.colors[n])return false;
  }return true;
 }
 static bool overlayFrameChanges(const Terrain&a,const Terrain&b,std::vector<size_t>&changed){
  changed.clear();if(!a.geo.same(b.geo)||a.reveal!=b.reveal||a.colors.size()!=b.colors.size()||a.baseColors!=b.baseColors||a.art.size()!=b.art.size())return false;
  for(size_t n=0;n<a.art.size();++n){const auto&x=a.art[n];const auto&y=b.art[n];
   if(x.cell!=y.cell||x.type!=y.type||x.overlayType!=y.overlayType||x.overlaySourceType!=y.overlaySourceType||x.overlay!=y.overlay||x.sub!=y.sub||x.level!=y.level||x.sourceColor!=y.sourceColor||(!x.sourceColor&&n<a.colors.size()&&a.colors[n]!=b.colors[n]))return false;
   if(x.overlayType&&x.overlayFrame!=y.overlayFrame){if(changed.size()>=256)return false;changed.push_back(n);}
  }return !changed.empty();
 }
 void clearBackground(){nativePresentation_={};objectCanvas_.clear();++motionTerrainRevision_;partialBank_=false;changedOverlayCells_.clear();repaintCells_.clear();building_.reset();complete_.reset();blocked_.reset();dynamicFailure_=false;pending_={};background_={};baseScene_.reset();maskedBaseScene_.reset();gpuBaseTiles_.reset();cpuBaseFallback_={};maskSource_.reset();mask_={};gpuMaskPlan_.reset();cpuMaskPlan_.reset();maskPixels_.reset();liveMaskValid_=false;cells_.clear();baseCells_.clear();prepared_.clear();painted_.clear();cursor_=baseCursor_=0;probing_=probeChanged_=pendingIncomplete_=backgroundIncomplete_=false;backgroundRetry_=0;}

public:
 explicit SceneRenderer(size_t pages=camera::NativeCanvas::MaximumPages):nativeBudget_(pages==camera::NativeCanvas::MaximumPages?camera::NativeCanvas::sharedBudget():std::make_shared<camera::NativeCanvas::Budget>()){if(pages!=camera::NativeCanvas::MaximumPages)nativeBudget_->maximum=std::clamp<size_t>(pages,1,camera::NativeCanvas::MaximumPages);}
 void guideEnabled(bool enabled,uint64_t contextKey=0){if(enabled!=guideEnabled_||contextKey!=guideExternalContext_){guideEnabled_=enabled;guideExternalContext_=contextKey;motionContext_=std::make_shared<const uint8_t>(0);}}
 bool materialsReady()const{return materialsReady_;}
 const DecodeStats&decodeStats()const{return decodeStats_;}
 const StageStats&stageStats()const{return stageStats_;}
 bool capacityExceeded()const{return bool(blocked_)||dynamicFailure_;}
 // Read-only metadata from the sprite actually displayed; no second decode.
 bool feedbackBounds(const Marker&m,int&x,int&y,int&w,int&h)const{
  auto found=sprites_.find(imageKey(m));
  if(found==sprites_.end()||!found->second.ready){auto old=displayed_.find({m.object,m.id});if(old!=displayed_.end())found=sprites_.find(old->second);}
  if(found==sprites_.end()||!found->second.ready)return false;
  const auto&i=*found->second.image;x=int(std::floor(double(i.x)/i.rasterScale));y=int(std::floor(double(i.y)/i.rasterScale));w=int(std::ceil(double(i.x+i.width)/i.rasterScale))-x;h=int(std::ceil(double(i.y+i.height)/i.rasterScale))-y;
  return w>0&&h>0&&w<=2048&&h<=2048;
 }
 bool feedbackVisibility(const std::vector<uint8_t>*&out)const{
  out=nullptr;if(!complete_)return false;if(complete_->reveal)return true;
  if(maskSource_!=complete_||mask_.width!=width_||mask_.height!=height_||mask_.alpha.size()!=size_t(width_)*height_)return false;
  out=&mask_.alpha;return true;
 }
 struct NativeStats {int width,height;size_t pages,resident;uint64_t cells,presents,partialBanks;};
 NativeStats nativeStats()const{return {background_.width(),background_.height(),background_.pageCount(),nativeBudget_->pages,nativeCells_,basePresents_,partialBanks_};}
 void reset(){overlayScanSource_.reset();overlaySources_.clear();effectAlphas_.clear();voxelLightKnown_=false;tiles_.clear();overlays_.clear();art_.clear();sprites_.clear();displayed_.clear();actorCursor_=transientCursor_=0;repairAssets_.reset();clearBackground();coverage_={};coverageLocal_=false;width_=height_=0;spanX_=spanY_=0;revision_=0;terrainPaletteReady_=false;terrainPalette_={};nativeCells_=basePresents_=partialBanks_=0;gpuMaskBuilder_.reset();detailed_.reset();detailKey_=0;}
 void features(const presentation::Profile&p){if(p.voxelLighting!=features_.voxelLighting||p.spriteLighting!=features_.spriteLighting){art_.clear();sprites_.clear();displayed_.clear();voxelLightKnown_=false;}features_=p;}
 template<class Read>std::shared_ptr<const Scene> render(const Frame&frame,int width,int height,Read read,Assets&assets,SceneRegion region={},unsigned pixelScaleMode=0,bool refined=false,bool liveVoxels=true,bool highColorPrecision=false,bool forceFrame=false,double decodeBudgetMs=4.){
  const bool warming=!materialsReady_;materialsReady_=false;decodeStats_={};stageStats_={};
  auto stageAt=std::chrono::steady_clock::now();auto stage=[&](double&ms){auto now=std::chrono::steady_clock::now();ms=std::chrono::duration<double,std::milli>(now-stageAt).count();stageAt=now;};
  if(!frame.terrain||!frame.terrain->geo.valid()||frame.terrain->art.size()!=512*512||width<3||height<3||width>(region.active?8192:2048)||height>(region.active?8192:2048)||uint64_t(width)*height>16*1024*1024)return {};
  const bool gpuMode=region.active&&gpuPresentation(assets,0);
  // Geometry quality belongs to Assets. Both quality settings share actor,
  // attachment, repair, depth and mask composition on the GPU.
  // Basic CPU output batches native object draws by independent page.
  const bool detail=region.active&&(refined||gpuMode);
  if(gpuMode!=gpuMode_){gpuMode_=gpuMode;sprites_.clear();displayed_.clear();detailed_.reset();baseScene_.reset();maskedBaseScene_.reset();gpuBaseTiles_.reset();cpuBaseFallback_={};maskSource_.reset();gpuMaskPlan_.reset();cpuMaskPlan_.reset();maskPixels_.reset();}
  const auto precision=highColorPrecision?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps;
  if(precision!=precision_){reset();precision_=precision;}
  beginPalette(assets,0);
  if(revision_!=assets.revision()){reset();revision_=assets.revision();}
  if(region.active&&(!std::isfinite(region.x)||!std::isfinite(region.y)||!std::isfinite(region.width)||!std::isfinite(region.height)||region.width<=0||region.height<=0||std::abs(region.x)>1e7||std::abs(region.y)>1e7||region.width>32768||region.height>32768))return {};
  if(width!=width_||height!=height_||region.x!=region_.x||region.y!=region_.y||region.width!=region_.width||region.height!=region_.height||region.active!=region_.active||pixelScaleMode!=pixelScaleMode_||detail!=detailedBase_){width_=width;height_=height;region_=region;pixelScaleMode_=pixelScaleMode;detailedBase_=detail;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}
  const auto&domain=frame.terrain->geo;auto viewport=domain.nativeViewport();double exactX=viewport.width,exactY=viewport.height;
  if(!std::isfinite(exactX)||!std::isfinite(exactY)||exactX<=0||exactY<=0||exactX>camera::NativeCanvas::MaximumDimension||exactY>camera::NativeCanvas::MaximumDimension)return {};
  // Pixel storage is rounded outwards; the final filter still uses the exact
  // native viewport. No ceil-induced pixel row becomes part of the view.
  int nativeWidth=viewport.backingWidth(),nativeHeight=viewport.backingHeight();
  if((complete_&&(!complete_->geo.same(domain)||complete_->reveal!=frame.terrain->reveal))||(building_&&(!building_->geo.same(domain)||building_->reveal!=frame.terrain->reveal))||exactX!=spanX_||exactY!=spanY_)clearBackground();
  if(region.active){
   if(!coverageLocal_||!coverage_.contains(region.x,region.y,region.width,region.height,nativeWidth,nativeHeight)){
    clearBackground();coverage_=terraincoverage::Region::plan(region.x,region.y,region.width,region.height,nativeWidth,nativeHeight);coverageLocal_=true;
    if(!coverage_.ready)return {};
   }
  }else if(coverageLocal_){clearBackground();coverage_={};coverageLocal_=false;}
  spanX_=exactX;spanY_=exactY;dynamicFailure_=false;
  if(blocked_){if(blocked_==frame.terrain||sameSource(*blocked_,*frame.terrain,region.active)){blocked_=frame.terrain;return {};}blocked_.reset();}
  lighting::State scene=lighting::sample(read);GameView<Read> game(read);
  std::array<uint32_t,256> liveTerrainPalette{};const bool paletteReady=assets.palette(true,liveTerrainPalette);
  if(!paletteReady&&terrainPaletteReady_)return {};
  if(paletteReady){if(terrainPaletteReady_&&terrainPalette_!=liveTerrainPalette){tiles_.clear();clearBackground();}terrainPalette_=liveTerrainPalette;terrainPaletteReady_=true;}
  if(overlayScanSource_!=frame.terrain){std::set<std::pair<uint32_t,unsigned>>found;
   for(const auto&cell:frame.terrain->art){++stageStats_.overlayScanCells;if(cell.overlayType)found.insert({cell.overlayType,cell.overlayFrame});}
   overlaySources_.assign(found.begin(),found.end());overlayScanSource_=frame.terrain;
  }
  bool overlayChanged=false;
  for(const auto&key:overlaySources_){bool changed=false;if(!refreshOverlay(assets,key.first,game.template get<uint32_t>(key.first+0xA4),int(key.second),false,changed,0))return {};
   if(changed){for(auto i=overlays_.begin();i!=overlays_.end();)if(i->first.first==key.first)i=overlays_.erase(i);else ++i;overlayChanged=true;}}
  if(overlayChanged)clearBackground();
  // A pending bank cannot combine lighting from two different global states.
  if(building_&&buildingLight_!=scene){partialBank_=false;changedOverlayCells_.clear();repaintCells_.clear();building_.reset();pending_={};prepared_.clear();cursor_=baseCursor_=0;}

  auto resetPending=[&](){if(!pending_.reset(nativeWidth,nativeHeight,nativeBudget_))return false;
   if(coverageLocal_)pending_.restrictPaint(int(coverage_.left),int(coverage_.top),int(coverage_.right),int(coverage_.bottom));return true;};
  if(!building_){
   building_=frame.terrain;buildingLight_=scene;bankSameSource_=complete_&&(complete_==building_||sameSource(*complete_,*building_,region.active));cursor_=baseCursor_=0;prepared_.clear();probeChanged_=pendingIncomplete_=false;
   partialBank_=region.active&&complete_&&!bankSameSource_&&!backgroundIncomplete_&&backgroundLight_==scene&&painted_.size()==cells_.size()&&overlayFrameChanges(*complete_,*building_,changedOverlayCells_);
   if(!bankSameSource_&&!partialBank_){cells_.clear();baseCells_.clear();
   for(size_t n=0;n<building_->colors.size();++n)if(building_->colors[n]||(building_->art[n].sourceColor&&(region.active||!building_->art[n].sourceHidden))){
    if(region.active){int x=int(n%512),y=int(n/512);mapbounds::TileOrigin origin;
     if(!building_->geo.tileOrigin(x,y,building_->art[n].level,origin)||
       (!coverage_.tile(origin.x,origin.y)&&!coverage_.rgb((x-y+building_->geo.offsetX)*30,(x+y-building_->geo.offsetY)*15)))continue;
    }
    cells_.push_back(n);
   }
   if(building_->reveal&&building_->baseColors.size()==building_->colors.size())
    for(size_t n=0;n<building_->baseColors.size();++n)
     if(building_->baseColors[n]&&!building_->colors[n]&&!building_->art[n].sourceColor&&
       (!region.active||coverage_.rgb((int(n%512)-int(n/512)+building_->geo.offsetX)*30,(int(n%512)+int(n/512)-building_->geo.offsetY)*15)))baseCells_.push_back(n);
   std::stable_sort(cells_.begin(),cells_.end(),[](size_t a,size_t b){return a%512+a/512<b%512+b/512;});}
   // New sampling banks commonly contain identical terrain. Compare evaluated
   // per-cell lighting before deciding to rebuild millions of native pixels.
   probing_=!(backgroundIncomplete_&&int32_t(frame.tick-backgroundRetry_)>=0)&&bankSameSource_&&painted_.size()==cells_.size();prepared_.resize(cells_.size());
   if(!probing_&&!partialBank_&&!resetPending())return {};
  }
  size_t staticPixels=0;for(const auto&i:tiles_)staticPixels+=camera::pixels(*i.second.image);for(const auto&i:overlays_)staticPixels+=camera::pixels(*i.second.image);
  const Terrain*evaluatedTerrain=building_.get();
  auto prepare=[&](size_t index){const auto&cell=evaluatedTerrain->art[index];Paint value;value.light=lighting::cell(read,cell.cell,scene,lighting::Layer::Terrain);value.overlayLight=value.light;
   if(cell.overlayType){uint32_t source=cell.overlaySourceType?cell.overlaySourceType:cell.overlayType;value.overlayOffset=watch::overlayOffset(read,cell.cell,source,cell.overlay,cell.overlayFrame);value.bridge=(game.template get<uint32_t>(cell.cell+0x140)&0x80)!=0;
    if(value.bridge)value.overlayLight=lighting::cell(read,cell.cell,scene,lighting::Layer::Bridge);
    else if(game.template get<uint8_t>(source+0x2A9)){value.overlayLight=scene;value.overlayLight.intensity=1000;}
    else if(game.template get<uint8_t>(source+0x2A8))value.overlayLight=lighting::cell(read,cell.cell,scene,lighting::Layer::Object,false);
   }
   value.signature=lightKey(lightKey(1469598103934665603ull,value.light),value.overlayLight);value.signature=mix(mix(value.signature,uint32_t(value.overlayOffset)),value.bridge);return value;
  };
  // Validate the CURRENT crop synchronously. Offscreen incremental checks
  // must not make a stable main viewport flash back to the game every pass.
  auto verifyVisible=[&](){
   if(!region.active||!evaluatedTerrain||painted_.size()!=cells_.size())return false;
   double haloX=512,haloY=512;
   auto imageExtent=[&](auto&&self,const voxel::Image&i,unsigned nesting)->void{if(nesting>16||i.rasterScale<1)return;
    haloX=std::max({haloX,std::abs(double(i.x)/i.rasterScale),std::abs((double(i.x)+i.width)/i.rasterScale)});
    haloY=std::max({haloY,std::abs(double(i.y)/i.rasterScale)+64.,std::abs((double(i.y)+i.height)/i.rasterScale)+64.});
    for(const auto&part:i.attachments)self(self,part,nesting+1);
   };
   auto extent=[&](const auto&cache){for(const auto&item:cache)if(item.second.ready)imageExtent(imageExtent,*item.second.image,0);};
   extent(tiles_);extent(overlays_);
   for(size_t n=0;n<cells_.size();++n){size_t index=cells_[n];const auto&cell=evaluatedTerrain->art[index];mapbounds::TileOrigin origin;
    if(!evaluatedTerrain->geo.tileOrigin(int(index%512),int(index/512),int(cell.level),origin))continue;
    double x=origin.x+30,y=origin.y+15;
    if(x<region.x-haloX||x>region.x+region.width+haloX||y<region.y-haloY||y>region.y+region.height+haloY)continue;
    if(prepare(index).signature!=painted_[n])return false;
   }return true;
  };
  bool visibleVerified=building_&&(bankSameSource_||partialBank_)&&verifyVisible();
  if(partialBank_&&cursor_==0&&baseCursor_==0){
   auto beginPartial=[&](){
    if(!visibleVerified||!complete_||prepared_.size()!=painted_.size())return false;
    std::vector<std::array<int,4>> dirty;unsigned decoded=0;bool valid=true;
    auto bounds=[&](auto&&self,const voxel::Image&i,double ax,double ay,auto&&visit,unsigned nesting=0)->void{
     if(nesting>16||i.rasterScale<1||i.rasterScale>4){valid=false;return;}
     if(i.width>0&&i.height>0)visit(ax+double(i.x)/i.rasterScale,ay+double(i.y)/i.rasterScale,ax+double(i.x+i.width)/i.rasterScale,ay+double(i.y+i.height)/i.rasterScale);
     for(const auto&part:i.attachments)self(self,part,ax,ay,visit,nesting+1);
    };
    for(size_t index:changedOverlayCells_){
     if(std::find(cells_.begin(),cells_.end(),index)==cells_.end())continue;
     const auto&old=complete_->art[index];const auto&cell=building_->art[index];
     auto before=overlays_.find({old.overlayType,old.overlayFrame});if(before==overlays_.end()||!before->second.ready)return false;
     auto&after=overlays_[{cell.overlayType,cell.overlayFrame}];
     if(!after.ready){
      if(decoded>=64||(after.retry&&int32_t(frame.tick-after.retry)<0))return false;++decoded;
      after.ready=assets.overlay(cell.overlayType,game.template get<uint32_t>(cell.overlayType+0xA4),cell.overlayFrame,*after.image);after.retry=frame.tick+2000;
      if(!after.ready)return false;size_t cost=camera::pixels(*after.image);if(staticPixels+cost>16*1024*1024){after.ready=false;after.image=std::make_shared<voxel::Image>();return false;}staticPixels+=cost;
     }
     mapbounds::TileOrigin origin;if(!building_->geo.tileOrigin(int(index%512),int(index/512),cell.level,origin))return false;
     auto append=[&](double l,double t,double r,double b){if(!std::isfinite(l)||!std::isfinite(t)||!std::isfinite(r)||!std::isfinite(b)||std::abs(l)>1e7||std::abs(t)>1e7||std::abs(r)>1e7||std::abs(b)>1e7){valid=false;return;}
      dirty.push_back({int(std::floor(l)),int(std::floor(t)),int(std::ceil(r)),int(std::ceil(b))});};
     uint32_t oldSource=old.overlaySourceType?old.overlaySourceType:old.overlayType;
     const int oldOffset=watch::overlayOffset(read,old.cell,oldSource,old.overlay,old.overlayFrame);
     bounds(bounds,*before->second.image,origin.x+30,origin.y+15+oldOffset,append);
     const auto current=prepare(index);bounds(bounds,*after.image,origin.x+30,origin.y+15+current.overlayOffset,append);
    }
    if(!valid||!pending_.repaint(background_,dirty))return false;
    repaintCells_.clear();
    for(size_t n=0;n<cells_.size();++n){prepared_[n].signature=painted_[n];const size_t index=cells_[n];const auto&cell=building_->art[index];const int x=int(index%512),y=int(index/512);
     mapbounds::TileOrigin origin;if(!building_->geo.tileOrigin(x,y,cell.level,origin))continue;
     const int flatX=(x-y+building_->geo.offsetX)*30,flatY=(x+y-building_->geo.offsetY)*15;
     bool hit=pending_.intersectsRepaint(flatX,flatY,flatX+60,flatY+15);
     auto visit=[&](double l,double t,double r,double b){hit|=pending_.intersectsRepaint(l,t,r,b);};
     if(cell.type){auto tile=tiles_.find({cell.type,cell.sub});if(tile==tiles_.end()||!tile->second.ready)return false;bounds(bounds,*tile->second.image,origin.x+30,origin.y+15,visit);}
     if(cell.overlayType){auto overlay=overlays_.find({cell.overlayType,cell.overlayFrame});if(overlay==overlays_.end()||!overlay->second.ready)return false;const auto paint=prepare(index);bounds(bounds,*overlay->second.image,origin.x+30,origin.y+15+paint.overlayOffset,visit);}
     if(!valid)return false;if(hit)repaintCells_.push_back(n);
    }
    return true;
   };
   if(!beginPartial()){partialBank_=false;changedOverlayCells_.clear();repaintCells_.clear();cursor_=baseCursor_=0;probeChanged_=false;probing_=false;if(!resetPending())return {};}
  }
  if(region.active&&bankSameSource_&&!visibleVerified)probeChanged_=true;
  bool committed=false;
  if(building_&&probing_){size_t end=std::min(cells_.size(),cursor_+size_t(4096));for(;cursor_<end;++cursor_){prepared_[cursor_]=prepare(cells_[cursor_]);probeChanged_|=prepared_[cursor_].signature!=painted_[cursor_];}
   if(cursor_==cells_.size()){
    if(!probeChanged_){complete_=building_;building_.reset();backgroundLight_=scene;cursor_=0;prepared_.clear();}
    else{probing_=false;cursor_=baseCursor_=0;if(!resetPending())return {};}
   }
  }
  if(building_&&!probing_){
   pending_.precision(precision_);
   const auto&terrain=*building_;const auto&geo=terrain.geo;
   // Native radar RGB keeps its original flat/raw admission. TMP sources may
   // straddle that crop; their decoded pixels are clipped by this same canvas.
   // Objects, overlays and effects retain the sampler's original admission.
   size_t baseEnd=std::min(baseCells_.size(),baseCursor_+size_t(4096));
   for(;baseCursor_<baseEnd;++baseCursor_){size_t index=baseCells_[baseCursor_];int x=int(index%512),y=int(index/512);
    int flatX=x-y+geo.offsetX,flatY=x+y-geo.offsetY;
    pending_.underlayRect(flatX*30,flatY*15,(flatX+2)*30,(flatY+1)*15,terrain.baseColors[index]);
    if(pending_.failed())break;
   }
   if(baseCursor_<baseCells_.size()&&!pending_.failed())return {};
   const size_t total=partialBank_?repaintCells_.size():cells_.size();
   const auto&palette=liveTerrainPalette;int loads=0;size_t end=std::min(total,cursor_+size_t(4096));
   for(;cursor_<end&&!pending_.failed();++cursor_){size_t position=partialBank_?repaintCells_[cursor_]:cursor_;size_t index=cells_[position];int x=int(index%512),y=int(index/512);const auto&cell=terrain.art[index];
    mapbounds::TileOrigin origin;if(!geo.tileOrigin(x,y,int(cell.level),origin))continue;float px=float(origin.x+30),py=float(origin.y+15),base=float((x+y)*15);
    // The original radar has a flat 2x1 RGB cell footprint beneath its
    // scene. A TMP diamond (especially on a raised edge) does not cover that
    // whole footprint; keep its native ground color in the uncovered pixels.
    int flatX=x-y+geo.offsetX,flatY=x+y-geo.offsetY;
    uint32_t baseColor=terrain.baseColors.size()==terrain.colors.size()&&terrain.baseColors[index]?terrain.baseColors[index]:cell.sourceColor?cell.sourceColor:terrain.colors[index];
    if(!coverageLocal_||coverage_.rgb(flatX*30,flatY*15))pending_.underlayRect(flatX*30,flatY*15,(flatX+2)*30,(flatY+1)*15,baseColor);
    auto&paint=prepared_[position];if(!probeChanged_)paint=prepare(index);
    if(cell.type&&(!coverageLocal_||coverage_.tile(origin.x,origin.y))){
     auto&tile=tiles_[{cell.type,cell.sub}];
     if(!tile.ready&&paletteReady&&loads<64&&(!tile.retry||int32_t(frame.tick-tile.retry)>=0)){++loads;tile.ready=assets.tile(cell.type,cell.sub,palette,*tile.image);if(tile.ready){auto n=camera::pixels(*tile.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{tile.ready=false;tile.image=std::make_shared<voxel::Image>();}}tile.retry=frame.tick+2000;}
     if(!tile.ready&&paletteReady&&loads>=64&&!tile.retry)break;
     if(tile.ready)pending_.blit(*tile.image,px,py,1,1,base,paint.light,0);
     else{pendingIncomplete_=true;pending_.diamond(px,py,1,1,base,0xff000000u|baseColor,paint.light);}
    } // A cell without a resolved tile has no ground sprite to paint.
    if(cell.overlayType&&(!coverageLocal_||coverage_.tile(origin.x,origin.y))){auto&overlay=overlays_[{cell.overlayType,cell.overlayFrame}];
     if(!overlay.ready&&loads<64&&(!overlay.retry||int32_t(frame.tick-overlay.retry)>=0)){++loads;overlay.ready=assets.overlay(cell.overlayType,game.template get<uint32_t>(cell.overlayType+0xA4),cell.overlayFrame,*overlay.image);if(overlay.ready){auto n=camera::pixels(*overlay.image);if(staticPixels+n<=16*1024*1024)staticPixels+=n;else{overlay.ready=false;overlay.image=std::make_shared<voxel::Image>();}}overlay.retry=frame.tick+2000;}
     if(!overlay.ready&&loads>=64&&!overlay.retry)break;
     if(overlay.ready)pending_.blit(*overlay.image,px,py+paint.overlayOffset,1,1,base+(paint.bridge?60:0),paint.overlayLight,2);else pendingIncomplete_=true;
    }
    if(pending_.failed())break;++nativeCells_;
   }
   if(pending_.failed()){
    // Release the old native bank before retrying the current cell. The worker
    // may keep its last small published scene; never silently minify sources.
    if(background_.pageCount()){background_={};complete_.reset();baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();pending_.clearFailure();}
    else{blocked_=building_;building_.reset();pending_={};prepared_.clear();cells_.clear();baseCells_.clear();cursor_=baseCursor_=0;complete_.reset();baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}
    return {};
   }
   if(cursor_==total){++motionTerrainRevision_;committed=true;if(partialBank_)++partialBanks_;background_=std::move(pending_);background_.clean();painted_.resize(prepared_.size());for(size_t n=0;n<prepared_.size();++n)painted_[n]=prepared_[n].signature;prepared_.clear();complete_=building_;building_.reset();partialBank_=false;changedOverlayCells_.clear();repaintCells_.clear();backgroundLight_=scene;backgroundIncomplete_=pendingIncomplete_;backgroundRetry_=frame.tick+2000;baseScene_.reset();maskedBaseScene_.reset();}
  }
  // Reject local-light races during incremental builds before publication.
  if(committed&&region.active&&!verifyVisible())return {};
  // An incomplete bank may only reuse a synchronously validated crop.
  if(!complete_||(building_&&!visibleVerified))return {};
  // Scale the CURRENT lit terrain as one continuous native RGB backing.
  // Filtering individual transparent TMP diamonds introduces artificial
  // image borders at every adjoining tile; presentRegion supplies a source
  // neighborhood halo across those joins before xBRZ is evaluated.
  // Native terrain shadows were already composed exactly once above.
  const bool areaGpuMode=!region.active&&background_.pageCount()<=gpuarea::MaximumPages&&xbrzadapter::gpuAreaAvailable();
  if(areaGpuMode!=areaGpuMode_){areaGpuMode_=areaGpuMode;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();gpuMaskPlan_.reset();cpuMaskPlan_.reset();maskPixels_.reset();}
  if(!baseScene_){
   auto base=std::make_shared<Scene>();base->width=width;base->height=height;
   gpuBaseTiles_.reset();cpuBaseFallback_={};
   std::vector<vulkancompute::TileJob> jobs;
   if(areaGpuMode){
    // Resolve once after native actors, Z and shadows have been composed.
   }else if(gpuMode_&&background_.gpuRegion(width,height,region.x,region.y,region.width,region.height,pixelScaleMode,jobs)){
    gpuBaseTiles_=std::make_shared<const std::vector<vulkancompute::TileJob>>(std::move(jobs));
    cpuBaseFallback_=[canvas=background_.fork(),base,width,height,region,pixelScaleMode](std::vector<uint32_t>&out){
     if(base->pixels.size()==size_t(width)*height){out=base->pixels;return true;}
     auto&state=xbrzadapter::gpuState();struct Restore {bool&value;bool prior;~Restore(){value=prior;}} restore{state.requested,state.requested};state.requested=false;
     if(!canvas.presentRegion(width,height,out,region.x,region.y,region.width,region.height,pixelScaleMode))return false;
     base->pixels=out;return true;
    };
   }else{
    // A whole-scene GPU frame must not first submit/read back a separate
    // fractional-resolve frame. Unsupported source plans use the CPU cache.
    auto&state=xbrzadapter::gpuState();struct Restore {bool&value;bool prior;~Restore(){value=prior;}} restore{state.requested,state.requested};if(gpuMode_)state.requested=false;
    if(!(region.active?background_.presentRegion(width,height,base->pixels,region.x,region.y,region.width,region.height,pixelScaleMode):background_.present(width,height,base->pixels,spanX_,spanY_)))return {};
   }
   baseScene_=std::move(base);++basePresents_;++detailKey_;
  }
  // Color reconstruction never interpolates engine Z. Dynamic actors still
  // compose at target pixels against nearest original terrain depth.
  if(detail&&!detailed_.prepare(background_,baseScene_->pixels,width,height,region.x,region.y,region.width,region.height,detailKey_,precision_,pixelScaleMode,
    std::shared_ptr<const std::vector<uint32_t>>(baseScene_,&baseScene_->pixels),baseScene_->pixels.empty()?gpuBaseTiles_:nullptr,cpuBaseFallback_))return {};
  if(detail){vulkancompute::MotionView motion;motion.context=motionContext_;motion.x=region.x;motion.y=region.y;motion.scaleX=width/region.width;motion.scaleY=height/region.height;
   motion.depthFar=-4096;motion.depthNear=32768;motion.terrainRevision=motionTerrainRevision_;motion.tick=frame.tick;motion.frame=frame.frame;detailed_.motion(std::move(motion),guideEnabled_);}
  // Native Cell::DrawShroud places 60x30 alpha sprites on the flat cell
  // projection, after the world is drawn. Cache their final-presentation mask
  // separately from the original-size terrain so units and repairs cannot
  // escape into unseen neighboring cells. All frames use their raw SHP bytes.
  const bool masked=!complete_->reveal&&complete_->shade.size()==512*512;
  uint32_t options=game.template get<uint32_t>(0xA8B230);
  bool fog=masked&&options>=0x10000&&(game.template get<uint8_t>(options+1)&0x10)&&
   game.template get<uint8_t>(complete_->geo.house+0x1F5)==0;
  if(masked&&region.active&&currentVisibility(read,0)){
   shroudmask::Snapshot current;
   if(!liveMasks_.capture(read,complete_->geo,region.x,region.y,region.width,region.height,current))return {};
   fog=current.fog;
   if(!liveMaskValid_||!maskSource_||mask_.width!=width||mask_.height!=height||!liveMaskSnapshot_.sameMask(current)){
    if(gpuMode_){
     if(!gpuMaskBuilder_.build(current.geo,current.cells,width,height,region.width,region.height,fog,
       [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},gpuMaskPlan_,region.x,region.y))return {};
     mask_.width=width;mask_.height=height;mask_.alpha.clear();cpuMaskPlan_.reset();
    }else if(!mask_.build(current.geo,current.cells,width,height,region.width,region.height,fog,
       [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},region.x,region.y))return {};
    maskedBaseScene_.reset();maskPixels_.reset();
   }
   liveMaskSnapshot_=std::move(current);liveMaskValid_=true;maskSource_=complete_;maskFog_=fog;
  }else if(masked&&(maskSource_!=complete_||maskFog_!=fog||mask_.width!=width||mask_.height!=height)){
   bool same=maskSource_&&maskFog_==fog&&mask_.width==width&&mask_.height==height&&
    maskSource_->geo.same(complete_->geo)&&maskSource_->shade==complete_->shade;
   if(!same){
    if(gpuMode_||areaGpuMode){
     if(!gpuMaskBuilder_.build(*complete_,width,height,region.active?region.width:spanX_,region.active?region.height:spanY_,fog,
       [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},gpuMaskPlan_,region.active?region.x:0,region.active?region.y:0)){
      if(gpuMode_)return {};gpuMaskPlan_.reset();if(!mask_.build(*complete_,width,height,spanX_,spanY_,fog,[&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);}))return {};
     }else{mask_.width=width;mask_.height=height;mask_.alpha.clear();}cpuMaskPlan_.reset();
    }else if(!mask_.build(*complete_,width,height,region.active?region.width:spanX_,region.active?region.height:spanY_,fog,
       [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},region.active?region.x:0,region.active?region.y:0))return {};
    maskedBaseScene_.reset();maskPixels_.reset();}
   maskSource_=complete_;maskFog_=fog;
  }
  auto view=background_.fork();
  std::vector<camera::ObjectCanvas::Draw>objectDraws;objectDraws.reserve(frame.markers.size());
  auto draw=[&](const voxel::Image&i,float x,float y,float sx,float sy,float z,const lighting::State&light,float lift=1,float drop=0,std::shared_ptr<const voxel::Image>owner={},vulkancompute::MotionIdentity motion={}){if(detail)detailed_.blit(i,x,y,sx,sy,z,light,lift,drop,std::move(owner),std::move(motion));else{if(!owner||owner.get()!=&i)owner=std::make_shared<const voxel::Image>(i);objectDraws.push_back({std::move(owner),x,y,z,lift,drop,light});}};
  const double projectionScale=projectionZScale(read);
  const auto&geo=frame.terrain->geo;int work=0;size_t pixels=0;std::vector<const Marker*> fallbacks;
  stage(stageStats_.terrainMs);auto markers=frame.markers;
  if(!features_.particles)markers.erase(std::remove_if(markers.begin(),markers.end(),[](const Marker&m){return m.kind==7||m.kind==8||(m.kind==5&&!m.parent);}),markers.end());
  if(region.active)markers.erase(std::remove_if(markers.begin(),markers.end(),[&](const Marker&m){
   double x=(double(m.x)-m.y)*30/256.+geo.offsetX*30,y=(double(m.x)+m.y)*15/256.-geo.offsetY*15-screenHeight(m.z,projectionScale);
   return x<region.x-512||x>region.x+region.width+512||y<region.y-512||y>region.y+region.height+512;
  }),markers.end());
  for(auto&m:markers)if(m.kind==3&&!frame.posesPrepared)assets.buildingPose(m.object,m.type,m.pose);
  std::map<ObjectKey,ImageKey> visible;std::set<ImageKey> wanted;std::set<ArtKey> artWanted;
  for(const auto&m:markers){auto key=imageKey(m);if(m.pose.frameFrom>=0){auto previous=m;previous.pose.frame=m.pose.frameFrom;wanted.insert(imageKey(previous));}visible[{m.object,m.id}]=key;wanted.insert(key);artWanted.insert(artKey(m));}
  for(auto i=displayed_.begin();i!=displayed_.end();){auto current=visible.find(i->first);
   if(current==visible.end()||std::get<0>(current->second)!=std::get<0>(i->second)||std::get<1>(current->second)!=std::get<1>(i->second)||std::get<2>(current->second)!=std::get<2>(i->second)||std::get<6>(current->second)!=std::get<6>(i->second))i=displayed_.erase(i);
   else{wanted.insert(i->second);++i;}}
  if(sprites_.size()>1024){for(auto i=sprites_.begin();i!=sprites_.end();)if(!wanted.count(i->first))i=sprites_.erase(i);else ++i;}
  if(art_.size()>512){for(auto i=art_.begin();i!=art_.end();)if(!artWanted.count(i->first))i=art_.erase(i);else ++i;}
  const auto lightRevision=voxelRevision(assets,0);const bool voxelLightChanged=!lightRevision.first||!voxelLightKnown_||voxelLightRevision_!=lightRevision.second;
  voxelLightKnown_=lightRevision.first;voxelLightRevision_=lightRevision.second;
  if(liveVoxels&&voxelLightChanged)for(const auto&source:art_)if(voxelArt(source.second,0))invalidateArt(source.first);
  std::set<ArtKey> unavailablePalettes;
  for(const auto&key:artWanted){auto source=art_.find(key);if(source==art_.end())continue;bool changed=false;
   if(!refreshPalette(assets,source->second,changed,0)){invalidateArt(key);unavailablePalettes.insert(key);}
   else if(changed)invalidateArt(key);
  }
  for(const auto&m:markers)if(m.kind==4){bool changed=false;
   if(!refreshOverlay(assets,m.type,game.template get<uint32_t>(m.type+0xA4),m.pose.frame,true,changed,0)){invalidateArt(artKey(m));unavailablePalettes.insert(artKey(m));}
   else if(changed)invalidateArt(artKey(m));
  }
  if(!unavailablePalettes.empty()){
   // Explosions, smoke and projectiles may expire between the immutable
   // observation and this render. Drop only those instances, never the whole
   // completed world because one transient no longer has a live palette.
   for(const auto&m:markers)if(m.kind<5&&unavailablePalettes.count(artKey(m)))return {};
   markers.erase(std::remove_if(markers.begin(),markers.end(),[&](const Marker&m){return m.kind>=5&&unavailablePalettes.count(artKey(m));}),markers.end());
  }
  stage(stageStats_.palettesMs);
  for(const auto&i:sprites_)pixels+=camera::pixels(*i.second.image);size_t artBytes=0;for(const auto&i:art_)artBytes+=Assets::cost(i.second);
  if(pixels>rendermemory::PosePixels*3/4){for(auto i=sprites_.begin();i!=sprites_.end()&&pixels>rendermemory::PosePixels/2;)if(!wanted.count(i->first)){pixels-=camera::pixels(*i->second.image);i=sprites_.erase(i);}else ++i;}
  if(artBytes>rendermemory::Geometry*3/4){for(auto i=art_.begin();i!=art_.end()&&artBytes>rendermemory::Geometry/2;)if(!artWanted.count(i->first)){artBytes-=Assets::cost(i->second);i=art_.erase(i);}else ++i;}
  // Rotate the decode budget independently of draw order. Moving units at the
  // start of the map must not starve an oil pump or construction-yard anim.
  std::vector<Marker> decodeMarkers=markers;
  for(const auto&m:markers)if(m.pose.frameFrom>=0){auto prior=m;prior.pose.frame=m.pose.frameFrom;prior.pose.frameFrom=-1;decodeMarkers.push_back(prior);}
  auto decode=[&](const Marker&m,bool transition){if(unavailablePalettes.count(artKey(m)))return;auto existing=art_.find(artKey(m));if(liveVoxels&&existing!=art_.end()&&voxelArt(existing->second,0))return;auto&cached=sprites_[imageKey(m)];
   if(cached.ready&&!cached.image->incomplete){++decodeStats_.hits;return;}if(cached.retry&&int32_t(frame.tick-cached.retry)<0)return;
   ++work;++decodeStats_.attempts;voxel::Image image;std::shared_ptr<const voxel::Image>sharedImage;bool ready=false;auto source=art_.find(artKey(m));
   if(m.kind==4)ready=assets.overlay(m.type,game.template get<uint32_t>(m.type+0xA4),m.pose.frame,image,true);
   else{if(source==art_.end()){typename Assets::ActorArt art;if((m.kind==8?voxelanims::captureArt(assets,m.object,m.type,m.id,art):m.kind==7?particles::captureArt(assets,m.object,m.type,m.id,art):m.kind==6?projectiles::captureArt(assets,m.object,m.type,m.id,art):m.kind==5?assets.captureAnimation(m.object,m.type,m.id,m.parent,m.parentId,art):captureActor(assets,m,art,0))&&artBytes+Assets::cost(art)<=rendermemory::Geometry){artBytes+=Assets::cost(art);source=art_.emplace(artKey(m),std::move(art)).first;}}
    if(source!=art_.end()&&!(liveVoxels&&voxelArt(source->second,0)))ready=sharedActor(assets,source->second,m.pose,sharedImage,0);}
   size_t previous=camera::pixels(*cached.image),next=sharedImage?camera::pixels(*sharedImage):camera::pixels(image);
   if(ready&&pixels-previous+next<=rendermemory::PosePixels){if(!sharedImage){camera::opaque(image);sharedImage=std::make_shared<const voxel::Image>(std::move(image));}pixels=pixels-previous+next;cached.image=std::move(sharedImage);cached.ready=true;}
   cached.retry=cached.ready&&!cached.image->incomplete?0:frame.tick+((transition||(m.kind==3&&!cached.ready))?16u:2000u);
  };
  // Time budgets scale throughput with actual decode cost, not eight poses
  // per completed render. Every lane gets one attempt even after a slow job;
  // the 512-job ceiling only bounds pathological zero-cost/malformed input.
  using DecodeClock=std::chrono::steady_clock;const auto decodeStarted=DecodeClock::now();
  const double requested=std::isfinite(decodeBudgetMs)?std::clamp(decodeBudgetMs,.5,12.):4.;
  const double budget=warming?std::max(8.,requested):requested;decodeStats_.budget=budget;
  auto room=[&](unsigned attempted,DecodeClock::time_point start,double share){return attempted==0||(attempted<512&&std::chrono::duration<double,std::milli>(DecodeClock::now()-start).count()<budget*share);};
  std::set<ImageKey> priority;unsigned transitions=0;auto lane=DecodeClock::now();
  for(int phase=0;phase<2&&room(transitions,lane,.25);++phase)for(size_t scanned=0;scanned<decodeMarkers.size()&&room(transitions,lane,.25);++scanned){
   const auto&m=decodeMarkers[(actorCursor_+scanned)%decodeMarkers.size()];
   if(m.kind==3&&(m.pose.variant==-2)==(phase==0)&&priority.insert(imageKey(m)).second){int before=work;decode(m,m.pose.variant==-2);if(work!=before)++transitions;}
  }
  unsigned transients=0;lane=DecodeClock::now();
  for(size_t scanned=0;scanned<decodeMarkers.size()&&room(transients,lane,.35);++scanned){
   const auto&m=decodeMarkers[transientCursor_++%decodeMarkers.size()];
   if(m.kind>=5&&priority.insert(imageKey(m)).second){int before=work;decode(m,true);if(work!=before)++transients;}
  }
  work=0;lane=DecodeClock::now();
  for(size_t scanned=0;scanned<decodeMarkers.size()&&room(unsigned(work),lane,.40);++scanned){const auto&m=decodeMarkers[actorCursor_++%decodeMarkers.size()];if(!priority.count(imageKey(m)))decode(m,false);}
  decodeStats_.milliseconds=std::chrono::duration<double,std::milli>(DecodeClock::now()-decodeStarted).count();
  stage(stageStats_.decodeMs);
  if(liveVoxels){
   // Refresh VPL and palette provenance every pass; reuse pose pixels only
   // while both revisions and the complete body/shadow posture still agree.
   // A reader without revision support retains the conservative live path.
   std::set<ImageKey> rastered;size_t liveFaces=0;
   for(const auto&m:markers){auto source=art_.find(artKey(m));
    if(source==art_.end()){if(m.pose.voxel&&m.kind<5)return {};continue;}
    if(!voxelArt(source->second,0))continue;auto key=imageKey(m);if(!rastered.insert(key).second)continue;
    auto cachedImage=sprites_.find(key);
    if(!voxelLightChanged&&cachedImage!=sprites_.end()&&cachedImage->second.ready&&!cachedImage->second.image->incomplete){++decodeStats_.hits;continue;}
    liveFaces+=faceCount(source->second,0);if(liveFaces>rendermemory::LiveFaces){if(m.kind>=5)continue;return {};}
    std::shared_ptr<const voxel::Image>image;if(!sharedActor(assets,source->second,m.pose,image,0)||!image||image->incomplete){if(m.kind>=5)continue;return {};}
    auto&cached=sprites_[key];size_t previous=camera::pixels(*cached.image),next=camera::pixels(*image);
    if(pixels-previous+next>rendermemory::PosePixels){if(m.kind>=5)continue;return {};}
    pixels=pixels-previous+next;cached.image=std::move(image);cached.ready=true;cached.retry=0;
   }
  }
  for(const auto&m:markers){auto key=imageKey(m);auto i=sprites_.find(key);if(i!=sprites_.end()&&i->second.ready)displayed_[{m.object,m.id}]=key;}
  materialsReady_=!backgroundIncomplete_&&!building_;
  for(const auto&m:decodeMarkers){auto source=art_.find(artKey(m));if(liveVoxels&&source!=art_.end()&&voxelArt(source->second,0))continue;auto found=sprites_.find(imageKey(m));materialsReady_&=found!=sprites_.end()&&found->second.ready&&!found->second.image->incomplete;}
  stage(stageStats_.voxelMs);stageStats_.materialsMs=stageStats_.palettesMs+stageStats_.decodeMs+stageStats_.voxelMs;stageStats_.actors=markers.size();
  std::vector<const Marker*> order;for(const auto&m:markers)order.push_back(&m);
  // Native ground animations share object order (Layer, GetYSort). A blanket
  // final animation pass incorrectly paints pumps/fire over foreground units.
  std::stable_sort(order.begin(),order.end(),[](const Marker*a,const Marker*b){return camera::orderKey(a->layer,a->kind,a->x,a->y,a->sort)<camera::orderKey(b->layer,b->kind,b->x,b->y,b->sort);});
  for(const auto*marker:order){const auto&m=*marker;if(m.x<0||m.y<0||m.x>=512*256||m.y>=512*256)continue;
   auto found=sprites_.find(imageKey(m));if(found==sprites_.end()||!found->second.ready){auto old=displayed_.find({m.object,m.id});if(old!=displayed_.end())found=sprites_.find(old->second);}
   float px=float((double(m.x)-m.y)*30/256.+geo.offsetX*30),py=float((double(m.x)+m.y)*15/256.-geo.offsetY*15+camera::anchorY(m.kind)-screenHeight(m.z,projectionScale));
   if(found!=sprites_.end()&&found->second.ready){
    std::shared_ptr<const voxel::Image> imageOwner=found->second.image;const voxel::Image*image=imageOwner.get();voxel::Image blended;
    const float lift=m.kind==4?12:(m.kind==3||m.kind==5)?2:1;
    auto source=art_.find(artKey(m));
    if(m.pose.frameFrom>=0&&(source==art_.end()||!voxelArt(source->second,0))&&found==sprites_.find(imageKey(m))){auto prior=m;prior.pose.frame=m.pose.frameFrom;auto old=sprites_.find(imageKey(prior));
     if(old!=sprites_.end()&&old->second.ready&&materialblend::image(*old->second.image,*image,unsigned(m.pose.frameWeight),lift,blended)){imageOwner=std::make_shared<const voxel::Image>(std::move(blended));image=imageOwner.get();}
    }
    const bool relight=(source!=art_.end()&&voxelArt(source->second,0))?features_.voxelLighting:features_.spriteLighting;
    auto light=(!relight||m.kind==7||m.kind==8)?lighting::State{}:m.kind==6?projectiles::light(read,uint32_t(m.pose.variant)):m.kind==5?lighting::animation(read,m.object,m.type,m.x,m.y,m.z,scene):m.kind==4?lighting::terrainObject(read,m.type,m.x,m.y,m.z,scene):lighting::object(read,m.type,m.owner,m.kind,m.x,m.y,m.z,scene);uint32_t cell=frame.terrain->art[(m.y/256)*512+m.x/256].cell;bool bridge=m.onBridge;int level=game.template get<int8_t>(cell+0x11B);
    vulkancompute::MotionIdentity motion;motion.object=m.object;motion.id=m.id;motion.type=m.type;motion.kind=uint32_t(m.kind);motion.known=guideEnabled_&&detail&&m.object>=0x10000&&m.type>=0x10000&&m.id!=0xffffffffu&&m.kind>=0&&m.kind<=6;
    draw(*image,px,py,1,1,float((int64_t(m.x)+m.y)*15/256.)-15+camera::anchorY(m.kind)-m.drawZ+(bridge?60:0),light,m.kind==4?12:(m.kind==3||m.kind==5)?2:1,m.pose.shadow.known?float(m.pose.shadow.drop):float(std::max(0,screenHeight(m.z,projectionScale)-(level+(bridge?4:0))*15)),std::move(imageOwner),std::move(motion));}
   else if(m.kind<4)fallbacks.push_back(&m);
  }
  // Local status glyphs are drawn by local_ui_task on the main view only.
  // Keep physical border scenery; the native radar projection and output
  // canvas crop the image. Camera movement limits are not a pixel mask.
  if(tiles_.size()>4096)tiles_.clear();if(overlays_.size()>4096)overlays_.clear();
  // The saved CPU output is unmasked and predates annotations/effects. Only
  // reuse it if it describes the immediately preceding ObjectCanvas revision;
  // a failed intermediate render must fall back to the complete terrain base.
  const bool incrementalNative=!region.active&&!areaGpuMode&&nativePresentation_.pixels&&
   nativePresentation_.pixels->size()==size_t(width)*height&&nativePresentation_.background==detailKey_&&
   nativePresentation_.objects==objectCanvas_.revision()&&!nativePresentation_.gpu&&!nativePresentation_.used;
  if(!detail&&!objectCanvas_.compose(background_,objectDraws,view,incrementalNative)){dynamicFailure_=true;return {};}
  if(view.failed()){dynamicFailure_=true;return {};}
  stage(stageStats_.objectsMs);
  const bool worldEffects=frame.effects&&frame.effects->world&&!frame.effects->world->empty();
  const bool staticView=!worldEffects&&(detail?(!detailed_.dirty()&&!gpuMode_):(!view.dirtyPages()&&objectDraws.empty()))&&fallbacks.empty();
  if(staticView&&!forceFrame){
   if(areaGpuMode){
    auto cached=masked?maskedBaseScene_:baseScene_;if(cached&&cached->pixels.size()==size_t(width)*height){xbrzadapter::gpuAreaCached(areaCachedGpu_);return cached;}
   }else{
    if(!masked)return baseScene_;
    if(!maskedBaseScene_){auto out=std::make_shared<Scene>(*baseScene_);mask_.apply(out->pixels);maskedBaseScene_=std::move(out);}return maskedBaseScene_;
   }
  }
  auto dot=[&](const Marker&m){return std::pair<int,int>{int(((double(m.x)-m.y)*30/256.+geo.offsetX*30-(region.active?region.x:0))*width/(region.active?region.width:spanX_)),int(((double(m.x)+m.y)*15/256.-geo.offsetY*15-(region.active?region.y:0))*height/(region.active?region.height:spanY_))};};
  if(masked&&(gpuMode_||areaGpuMode)&&!gpuMaskPlan_&&!maskPixels_)maskPixels_=std::make_shared<const std::vector<uint8_t>>(mask_.alpha);
  auto*maskReadback=masked&&gpuMaskPlan_&&cpuMaskPlan_!=gpuMaskPlan_?&mask_.alpha:nullptr;
  auto out=std::make_shared<Scene>();out->width=width;out->height=height;bool areaUsed=false;
  const bool sameNative=!region.active&&nativePresentation_.pixels&&nativePresentation_.pixels->size()==size_t(width)*height&&
    nativePresentation_.background==detailKey_&&nativePresentation_.objects==objectCanvas_.revision()&&nativePresentation_.gpu==areaGpuMode&&
    (!nativePresentation_.used||(nativePresentation_.mask==(masked?gpuMaskPlan_:nullptr)&&nativePresentation_.fog==(masked?maskPixels_:nullptr)));
  if(sameNative){out->pixels=*nativePresentation_.pixels;areaUsed=nativePresentation_.used;if(areaGpuMode)xbrzadapter::gpuAreaCached(areaUsed);}
  else if(areaGpuMode){
   areaUsed=view.presentGPU(width,height,out->pixels,spanX_,spanY_,masked?maskPixels_:nullptr,masked?gpuMaskPlan_:nullptr);
   if(!areaUsed&&!view.present(width,height,out->pixels,spanX_,spanY_)){dynamicFailure_=true;return {};}
  }else{
   if(!detail)out->pixels=incrementalNative?*nativePresentation_.pixels:baseScene_->pixels;
   if(!(detail?detailed_.finish(out->pixels,true,masked?maskPixels_:nullptr,masked?gpuMaskPlan_:nullptr,maskReadback):region.active?view.presentRegion(width,height,out->pixels,region.x,region.y,region.width,region.height,pixelScaleMode):view.presentDirty(width,height,out->pixels,spanX_,spanY_))){dynamicFailure_=true;return {};}
  }
  if(!region.active&&!sameNative){nativePresentation_.pixels=std::make_shared<const std::vector<uint32_t>>(out->pixels);
   nativePresentation_.background=detailKey_;nativePresentation_.objects=objectCanvas_.revision();nativePresentation_.gpu=areaGpuMode;nativePresentation_.used=areaUsed;
   nativePresentation_.mask=areaUsed&&masked?gpuMaskPlan_:nullptr;nativePresentation_.fog=areaUsed&&masked?maskPixels_:nullptr;
  }
  stage(stageStats_.resolveMs);
  if(masked&&gpuMaskPlan_&&cpuMaskPlan_!=gpuMaskPlan_&&(!areaUsed||!fallbacks.empty()||worldEffects)){
   // CPU replay and missing-art dots need a native mask; a successful whole
   // minimap stays GPU-resident through its mask and returns only final RGBA.
   if((!detail||!detailed_.gpuMasked())&&!gpushroud::reference(*gpuMaskPlan_,mask_.alpha))return {};
   if(mask_.alpha.size()!=size_t(width)*height)return {};cpuMaskPlan_=gpuMaskPlan_;
  }
  if(detail&&fallbacks.empty())out->guides=detailed_.guides();
  // Missing-art dots are presentation annotations, not minified game sprites.
  for(const auto*m:fallbacks){auto p=dot(*m);for(int yy=std::max(0,p.second-1);yy<std::min(height,p.second+2);++yy)for(int xx=std::max(0,p.first-1);xx<std::min(width,p.first+2);++xx){
   const size_t at=size_t(yy)*width+xx;uint32_t color=0xff000000u|(m->color&0xffffffu);
   if(masked&&(areaUsed||(detail&&detailed_.gpuMasked()))){unsigned alpha=mask_.alpha[at];if(alpha<127){uint32_t lit=0xff000000u;for(int shift:{0,8,16})lit|=((((color>>shift)&255u)*alpha+63)/127)<<shift;color=lit;}}
   out->pixels[at]=color;
  }}
  if(worldEffects){worldfx::View effectView;effectView.x=region.active?region.x:0;effectView.y=region.active?region.y:0;effectView.width=region.active?region.width:spanX_;effectView.height=region.active?region.height:spanY_;effectView.pixelsW=width;effectView.pixelsH=height;effectView.offsetX=geo.offsetX;effectView.offsetY=geo.offsetY;effectView.zscale=projectionScale;worldfx::Painter painter(out->pixels,effectView,masked&&(areaUsed||(detail&&detailed_.gpuMasked()))?&mask_.alpha:nullptr);const auto&effects=*frame.effects->world;
   if(features_.particles)for(const auto&alpha:effects.alphas){auto found=effectAlphas_.find(alpha.image);if(found==effectAlphas_.end()){voxel::Image image;if(!alphaImage(assets,alpha.image,image,0))continue;size_t bytes=0;for(const auto&item:effectAlphas_)bytes+=camera::pixels(*item.second)*5;if(bytes+camera::pixels(image)*5>16*1024*1024)effectAlphas_.clear();found=effectAlphas_.emplace(alpha.image,std::make_shared<const voxel::Image>(std::move(image))).first;}painter.alpha(alpha,*found->second);}painter.paint(effects,features_.particles,features_.lasers);
  }
  if(masked&&!areaUsed&&(!detail||!detailed_.gpuMasked()))mask_.apply(out->pixels);
  if(areaGpuMode&&staticView){areaCachedGpu_=areaUsed;if(masked)maskedBaseScene_=out;else baseScene_=out;}
  stage(stageStats_.effectsMs);return out;
 }
};
}}
