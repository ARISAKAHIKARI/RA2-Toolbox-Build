#pragma once
#include "../../shared/source/frame_pool.h"
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
#include "native_page_masks.h"
#include "gpu_shroud.h"
#include "render_profile.h"
#include "scene_frame_reuse.h"
#include "gpu_effect_plan.h"
#include "world_scene_pipeline.h"
#include "native_effect_stencil.h"
#include "live_shape_cache.h"
#include "scene_extra_layers.h"
#include "line_trail_layers.h"
#include "radar_decoration.h"
#include <map>
#include <tuple>
#include <set>
#include <chrono>
namespace external { namespace radar {
struct SceneRegion {double x=0,y=0,width=0,height=0;bool active=false;};
enum class RenderFailure : uint8_t {None, TerrainPalette, ActorPalette, MissingVoxelArt, VoxelFaceBudget, VoxelRaster, VoxelImageMissing, VoxelIncomplete, VoxelPoseBudget};
inline const char* renderFailureName(RenderFailure f){switch(f){
 case RenderFailure::TerrainPalette:return "terrain-palette";case RenderFailure::ActorPalette:return "actor-palette";
 case RenderFailure::MissingVoxelArt:return "voxel-art-missing";case RenderFailure::VoxelFaceBudget:return "voxel-face-budget";
 case RenderFailure::VoxelRaster:return "voxel-raster-failed";case RenderFailure::VoxelImageMissing:return "voxel-image-missing";
 case RenderFailure::VoxelIncomplete:return "voxel-image-incomplete";case RenderFailure::VoxelPoseBudget:return "voxel-pose-budget";
 default:return "other-stage";
}}
template<class Assets>class SceneRenderer {
 presentation::Profile features_;
 liveshape::Cache gatherShapes_;sceneextras::Layers extraLayers_;sceneextras::TrailLayers trailLayers_;
 bool prepareWorldOnly_=false;
 bool outputSized_=false;
 std::shared_ptr<const worldscene::Snapshot> preparedWorld_;
 std::array<std::array<std::shared_ptr<const voxel::Image>,47>,2> worldShroud_{};
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
 template<class A>static auto assetRevision(A&a,uint64_t&out,int)->decltype(a.revisionChecked(out),bool()){return a.revisionChecked(out);}
 template<class A>static bool assetRevision(A&a,uint64_t&out,long){out=a.revision();return true;}
 template<class A>static auto gpuPresentation(const A&a,int)->decltype(bool(a.presentationGPU())){return a.presentationGPU();}
 template<class A>static bool gpuPresentation(const A&,long){return false;}
 template<class A>static auto refreshPalette(A&a,typename A::ActorArt&art,bool&changed,int)->decltype(a.refreshActorPalette(art,changed),bool()){return a.refreshActorPalette(art,changed);}
 template<class A>static bool refreshPalette(A&,typename A::ActorArt&,bool&changed,long){changed=false;return true;}
 template<class A>static auto refreshOverlay(A&a,uint32_t t,uint32_t p,int f,bool terrain,bool&changed,int)->decltype(a.refreshOverlayPalette(t,p,f,terrain,changed),bool()){return a.refreshOverlayPalette(t,p,f,terrain,changed);}
 template<class A>static bool refreshOverlay(A&,uint32_t,uint32_t,int,bool,bool&changed,long){changed=false;return true;}
 template<class A>static auto sharedActor(A&a,const typename A::ActorArt&art,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out,int)->decltype(a.renderSharedActor(art,pose,out),bool()){return a.renderSharedActor(art,pose,out);}
 template<class A>static bool sharedActor(A&a,const typename A::ActorArt&art,const visual::Pose&pose,std::shared_ptr<const voxel::Image>&out,long){voxel::Image image;if(!a.renderActor(art,pose,image))return false;camera::opaque(image);out=std::make_shared<const voxel::Image>(std::move(image));return true;}
 template<class A>static auto captureActor(A&a,const Marker&m,typename A::ActorArt&art,int)->decltype(a.captureActor(m.object,m.type,m.id,art,&m.pose,m.disguised?m.kind:-1,m.owner),bool()){return a.captureActor(m.object,m.type,m.id,art,&m.pose,m.disguised?m.kind:-1,m.owner);}
 template<class A>static bool captureActor(A&a,const Marker&m,typename A::ActorArt&art,long){return a.captureActor(m.object,m.type,m.id,art);}
 template<class Read>static auto readCount(const Read&read,int)->decltype(uint64_t(read.reader->reads),std::pair<bool,uint64_t>{}){return {true,uint64_t(read.reader->reads)};}
 template<class Read>static std::pair<bool,uint64_t> readCount(const Read&,long){return {false,0};}
 template<class Read>static auto currentVisibility(Read read,int)->decltype(read.fresh(0,nullptr,0),bool()){return true;}
 template<class Read>static bool currentVisibility(Read,long){return false;}
 struct CellReadPlan {std::vector<uint32_t> pages;size_t first=~size_t(0),end=~size_t(0);void clear(){pages.clear();first=end=~size_t(0);}};
 template<class Read>static auto prefetchCells(Read read,const Terrain&terrain,const std::vector<size_t>&cells,size_t first,size_t end,CellReadPlan&plan,int)->decltype(read.prefetch(plan.pages,size_t(4096)),void()){
  if(plan.first!=first||plan.end!=end){
   auto&pages=plan.pages;pages.clear();pages.reserve((end-first)*2);
   for(size_t n=first;n<end;++n){const auto p=terrain.art[cells[n]].cell;if(p<0x10000||uint64_t(p)+0x110>0x80000000ull)continue;
    const auto firstPage=p&~4095u,lastPage=(p+0x10F)&~4095u;pages.push_back(firstPage);if(lastPage!=firstPage)pages.push_back(lastPage);
   }
   std::sort(pages.begin(),pages.end());pages.erase(std::unique(pages.begin(),pages.end()),pages.end());if(pages.size()>4096)pages.resize(4096);
   plan.first=first;plan.end=end;
  }
  // Speculative page batching only: the same checked lighting::cell reads
  // below still decide every value, including per-field failure fallback.
  // Retain only addresses, never page bytes. Each reader.begin starts fresh.
  read.prefetch(plan.pages,4096);
 }
 template<class Read>static void prefetchCells(Read,const Terrain&,const std::vector<size_t>&,size_t,size_t,CellReadPlan&,long){}
 template<class A>static auto alphaImage(A&a,uint32_t pointer,voxel::Image&out,int)->decltype(a.effectAlpha(pointer,out),bool()){return a.effectAlpha(pointer,out);}
 template<class A>static bool alphaImage(A&,uint32_t,voxel::Image&,long){return false;}
 std::map<uint32_t,std::shared_ptr<const voxel::Image>>effectAlphas_;
 void invalidateArts(const std::set<ArtKey>&keys){
  if(keys.empty())return;stageStats_.invalidationKeys=keys.size();
  // Many palette/VPL changes in one presentation retire one combined set.
  // Repeated full-cache scans made this work proportional to art * poses.
  for(auto i=sprites_.begin();i!=sprites_.end();){++stageStats_.invalidationVisits;if(keys.count(artKey(i->first)))i=sprites_.erase(i);else ++i;}
  for(auto i=displayed_.begin();i!=displayed_.end();){++stageStats_.invalidationVisits;if(keys.count(artKey(i->second)))i=displayed_.erase(i);else ++i;}
 }
 std::array<uint32_t,256> terrainPalette_{};bool terrainPaletteReady_=false;
 colorprecision::Mode precision_=colorprecision::Mode::NativeSteps;
 lighting::State buildingLight_;uint64_t voxelLightRevision_=0;bool voxelLightKnown_=false;
 struct DecodeStats {unsigned attempts=0,hits=0;double milliseconds=0,budget=0;} decodeStats_;
 struct StageStats {double terrainMs=0,materialsMs=0,palettesMs=0,decodeMs=0,voxelMs=0,objectsMs=0,resolveMs=0,effectsMs=0;size_t overlayScanCells=0,actors=0,terrainLightCells=0,terrainGeometryChecks=0,terrainProbeCopies=0;bool reused=false;
  bool readCountsKnown=false;uint64_t terrainReads=0,paletteReads=0,decodeReads=0,voxelReads=0,objectReads=0,resolveReads=0,effectReads=0;
  uint64_t setupReads=0,terrainScanReads=0,maskReads=0,extraReads=0;size_t artWanted=0,artScanned=0,invalidationKeys=0,invalidationVisits=0;
  double visibilityMs=0;size_t visibilityCalls=0,visibilityPages=0,visibilityCells=0;bool visibilityAttempted=false,cpuTerrainReused=false;
  RenderFailure failure=RenderFailure::None;uint32_t failureObject=0,failureType=0,failureID=0;int failureKind=-1,rasterScale=0,hvaBody=0,hvaTurret=0;
  bool voxelLightAvailable=false,voxelLightChanged=false,gunPoseValid=false,postureAvailable=false,shadowAvailable=false;
  size_t candidates=0,liveFaces=0,posePixels=0,replacementPixels=0,newPixels=0,geometryBytes=0;std::string assetFailure;
 } stageStats_;
 template<class A>static auto clearAssetFailure(A&a,int)->decltype(a.failure.clear(),void()){a.failure.clear();}
 template<class A>static void clearAssetFailure(A&,long){}
 template<class A>static auto assetFailure(const A&a,int)->decltype(std::string(a.failure)){return a.failure;}
 template<class A>static std::string assetFailure(const A&,long){return {};}
 template<class A>static auto rasterScale(const A&a,int)->decltype(int(a.presentationRasterScale())){return a.presentationRasterScale();}
 template<class A>static int rasterScale(const A&,long){return 0;}
 void recordFailure(RenderFailure reason,const Marker&m){auto&s=stageStats_;s.failure=reason;s.failureObject=m.object;s.failureType=m.type;s.failureID=m.id;s.failureKind=m.kind;
  s.gunPoseValid=m.pose.gunPoseValid;s.postureAvailable=m.pose.posture.available;s.shadowAvailable=m.pose.shadow.posture.available;s.hvaBody=m.pose.hvaBody;s.hvaTurret=m.pose.hvaTurret;
 }

 std::map<std::pair<uint32_t,unsigned>,Cached> tiles_;
 std::map<ArtKey,typename Assets::ActorArt> art_;
 std::map<ImageKey,SpriteCached> sprites_;
 std::map<ObjectKey,ImageKey> displayed_;size_t actorCursor_=0,transientCursor_=0;repairvisuals::Assets repairAssets_;
 std::shared_ptr<const Terrain> building_,complete_,blocked_;bool dynamicFailure_=false,materialsReady_=false;
 std::weak_ptr<const worldframe::Snapshot> verifiedTerrainSource_;SceneRegion verifiedTerrainRegion_{};
 struct Paint {lighting::State light;uint64_t signature=0;};
 std::vector<Paint> evaluatedPaint_;std::vector<uint32_t> evaluatedStamp_;uint32_t evaluationEpoch_=0;
 asyncwork::FramePool<Scene> outputPool_;
 camera::FrameReuse<Scene> gpuFrameReuse_;
 std::shared_ptr<camera::NativeCanvas::Budget> nativeBudget_;
 camera::NativePageMasks nativePageMasks_;camera::NativeCanvas pending_,background_;camera::ObjectCanvas objectCanvas_;camera::ViewportCanvas detailed_;uint64_t detailKey_=0;std::shared_ptr<const Scene> baseScene_,maskedBaseScene_;
 bool gpuMode_=false,areaGpuMode_=false,areaCachedGpu_=false;
 uint32_t requestedObjects_=0,drawnObjects_=0;
 bool guideEnabled_=false,directPresentation_=false;uint64_t guideExternalContext_=0,motionTerrainRevision_=0;
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
 shroudmask::Live liveMasks_;shroudmask::Snapshot liveMaskSnapshot_;bool liveMaskValid_=false;uint32_t liveMaskTick_=0;
 terraincoverage::Region coverage_;bool coverageLocal_=false;
 std::vector<size_t> cells_,baseCells_;CellReadPlan cellReadPlan_;std::vector<Paint> prepared_;std::vector<uint64_t> painted_;size_t cursor_=0,baseCursor_=0;
 uint64_t partialBanks_=0; // ABI-compatible diagnostics; overlays now use independent instances.
 bool bankSameSource_=false,probing_=false,probeChanged_=false,pendingIncomplete_=false,backgroundIncomplete_=false;uint32_t backgroundRetry_=0;int width_=0,height_=0;double spanX_=0,spanY_=0;uint64_t revision_=0;
 lighting::State backgroundLight_;uint64_t nativeCells_=0,basePresents_=0;SceneRegion region_{};unsigned pixelScaleMode_=0;bool detailedBase_=false;
 static uint64_t mix(uint64_t h,uint64_t v){return (h^v)*1099511628211ull;}
 static uint64_t lightKey(uint64_t h,const lighting::State&s){
  h=mix(h,uint32_t(s.ambient));h=mix(h,uint32_t(s.intensity));for(int c:s.tint)h=mix(h,uint32_t(c));
  h=mix(h,s.paletteRows);h=mix(h,s.scalar);h=mix(h,s.rgb555);
  // converterMetadata retains this immutable owner when row bytes agree.
  // Ambient/tint equality alone cannot retain changed native palette rows.
  return mix(h,reinterpret_cast<uintptr_t>(s.table.get()));
 }
 static bool sameSource(const Terrain&a,const Terrain&b,bool local=false){
  if(!a.geo.same(b.geo)||a.reveal!=b.reveal||a.colors.size()!=b.colors.size()||a.baseColors!=b.baseColors||a.art.size()!=b.art.size())return false;
  // Exploration belongs to the destination mask and actor admission. It must
  // not rebuild static TMP pages/xBRZ caches when their source art is intact.
  for(size_t n=0;n<a.art.size();++n){const auto&x=a.art[n];const auto&y=b.art[n];if(x.cell!=y.cell||x.type!=y.type||x.sub!=y.sub||x.level!=y.level||x.sourceColor!=y.sourceColor)return false;
   // Terrain pixels exist independently of exploration. The destination's
   // live mask is applied after drawing; old/new sampling banks must not
   // rebuild or remove identical TMP sources when a cell becomes explored.
   if(!x.sourceColor&&n<a.colors.size()&&a.colors[n]!=b.colors[n])return false;
  }return true;
 }
 void clearBackground(){verifiedTerrainSource_.reset();nativePageMasks_.reset();gpuFrameReuse_.reset();nativePresentation_={};objectCanvas_.clear();++motionTerrainRevision_;building_.reset();complete_.reset();blocked_.reset();dynamicFailure_=false;pending_={};background_={};baseScene_.reset();maskedBaseScene_.reset();gpuBaseTiles_.reset();cpuBaseFallback_={};maskSource_.reset();mask_={};gpuMaskPlan_.reset();cpuMaskPlan_.reset();maskPixels_.reset();liveMaskValid_=false;cells_.clear();baseCells_.clear();cellReadPlan_.clear();prepared_.clear();painted_.clear();cursor_=baseCursor_=0;probing_=probeChanged_=pendingIncomplete_=backgroundIncomplete_=false;backgroundRetry_=0;}

public:
 explicit SceneRenderer(size_t pages=camera::NativeCanvas::MaximumPages):nativeBudget_(pages==camera::NativeCanvas::MaximumPages?camera::NativeCanvas::sharedBudget():std::make_shared<camera::NativeCanvas::Budget>()){if(pages!=camera::NativeCanvas::MaximumPages)nativeBudget_->maximum=std::clamp<size_t>(pages,1,camera::NativeCanvas::MaximumPages);}
 void guideEnabled(bool enabled,uint64_t contextKey=0){if(enabled!=guideEnabled_||contextKey!=guideExternalContext_){guideEnabled_=enabled;guideExternalContext_=contextKey;motionContext_=std::make_shared<const uint8_t>(0);}}
 void directPresentation(bool enabled){if(enabled!=directPresentation_){gpuFrameReuse_.reset();directPresentation_=enabled;}}
 void prepareWorld(bool enabled){if(prepareWorldOnly_!=enabled){reset();prepareWorldOnly_=enabled;}}
 // Independent of SceneRegion: radar keeps its mask, annotations, admission
 // and projection while raster work follows the physical panel dimensions.
 void outputSized(bool enabled){if(outputSized_!=enabled){reset();outputSized_=enabled;}}
 std::shared_ptr<const worldscene::Snapshot> preparedWorld()const{return preparedWorld_;}
 bool materialsReady()const{return materialsReady_;}
 std::pair<uint32_t,uint32_t> objectCoverage()const{return {drawnObjects_,requestedObjects_};}
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
 std::shared_ptr<const voxel::Image> exactImage(const Marker&m)const{
  const auto found=sprites_.find(imageKey(m));
  return found!=sprites_.end()&&found->second.ready&&found->second.image&&!found->second.image->incomplete?found->second.image:nullptr;
 }
 bool feedbackVisibility(const std::vector<uint8_t>*&out)const{
  out=nullptr;if(!complete_)return false;if(complete_->reveal)return true;
  if(maskSource_!=complete_||mask_.width!=width_||mask_.height!=height_||mask_.alpha.size()!=size_t(width_)*height_)return false;
  out=&mask_.alpha;return true;
 }
 struct NativeStats {int width,height;size_t pages,resident;uint64_t cells,presents,partialBanks;};
 NativeStats nativeStats()const{return {background_.width(),background_.height(),background_.pageCount(),nativeBudget_->pages,nativeCells_,basePresents_,partialBanks_};}
 void reset(){trailLayers_.reset();extraLayers_.reset();gatherShapes_.reset();preparedWorld_.reset();worldShroud_={};gpuFrameReuse_.reset();outputPool_.clear();evaluatedPaint_.clear();evaluatedStamp_.clear();evaluationEpoch_=0;effectAlphas_.clear();voxelLightKnown_=false;tiles_.clear();art_.clear();sprites_.clear();displayed_.clear();actorCursor_=transientCursor_=0;repairAssets_.reset();clearBackground();coverage_={};coverageLocal_=false;width_=height_=0;spanX_=spanY_=0;revision_=0;terrainPaletteReady_=false;terrainPalette_={};nativeCells_=basePresents_=partialBanks_=0;gpuMaskBuilder_.reset();detailed_.reset();detailKey_=0;}
 void features(const presentation::Profile&p){if(p.voxelLighting!=features_.voxelLighting||p.spriteLighting!=features_.spriteLighting){art_.clear();sprites_.clear();displayed_.clear();voxelLightKnown_=false;}features_=p;}
 template<class Read>std::shared_ptr<const Scene> render(const Frame&frame,int width,int height,Read read,Assets&assets,SceneRegion region={},unsigned pixelScaleMode=0,bool refined=false,bool liveVoxels=true,bool highColorPrecision=false,bool forceFrame=false,double decodeBudgetMs=4.){
  const bool warming=!materialsReady_;materialsReady_=false;decodeStats_={};stageStats_={};requestedObjects_=drawnObjects_=0;
  const auto initialReads=readCount(read,0);stageStats_.readCountsKnown=initialReads.first;uint64_t readAt=initialReads.second,phaseReadAt=initialReads.second;
  auto phaseReads=[&](uint64_t&out){const auto count=readCount(read,0);if(count.first){out=count.second-phaseReadAt;phaseReadAt=count.second;}};
  auto stageAt=std::chrono::steady_clock::now();auto stage=[&](double&ms,uint64_t&reads){auto now=std::chrono::steady_clock::now();ms=std::chrono::duration<double,std::milli>(now-stageAt).count();stageAt=now;const auto count=readCount(read,0);if(count.first){reads=count.second-readAt;readAt=count.second;}};
  if(!frame.terrain||!frame.terrain->geo.valid()||frame.terrain->art.size()!=512*512||width<3||height<3||width>(region.active?16384:4096)||height>(region.active?16384:4096)||uint64_t(width)*height>32*1024*1024)return {};
  const bool gpuMode=(region.active||outputSized_)&&gpuPresentation(assets,0);
  // Geometry quality belongs to Assets. Both quality settings share actor,
  // attachment, repair, depth and mask composition on the GPU.
  // Basic CPU output batches native object draws by independent page.
  const bool detail=outputSized_||(region.active&&(refined||gpuMode));
  if(gpuMode!=gpuMode_){gpuMode_=gpuMode;sprites_.clear();displayed_.clear();detailed_.reset();baseScene_.reset();maskedBaseScene_.reset();gpuBaseTiles_.reset();cpuBaseFallback_={};maskSource_.reset();gpuMaskPlan_.reset();cpuMaskPlan_.reset();maskPixels_.reset();}
  const auto precision=highColorPrecision?colorprecision::Mode::SmoothRGB:colorprecision::Mode::NativeSteps;
  if(precision!=precision_){reset();precision_=precision;}
  beginPalette(assets,0);extraLayers_.begin();
  uint64_t currentRevision=0;if(!assetRevision(assets,currentRevision,0))return {};
  if(revision_!=currentRevision){reset();revision_=currentRevision;}
  if(region.active&&(!std::isfinite(region.x)||!std::isfinite(region.y)||!std::isfinite(region.width)||!std::isfinite(region.height)||region.width<=0||region.height<=0||std::abs(region.x)>1e7||std::abs(region.y)>1e7||region.width>32768||region.height>32768))return {};
  if(width!=width_||height!=height_||region.x!=region_.x||region.y!=region_.y||region.width!=region_.width||region.height!=region_.height||region.active!=region_.active||pixelScaleMode!=pixelScaleMode_||detail!=detailedBase_){width_=width;height_=height;region_=region;pixelScaleMode_=pixelScaleMode;detailedBase_=detail;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}
  const auto&domain=frame.terrain->geo;auto viewport=domain.nativeViewport();double exactX=viewport.width,exactY=viewport.height;
  if(!std::isfinite(exactX)||!std::isfinite(exactY)||exactX<=0||exactY<=0||exactX>camera::NativeCanvas::MaximumDimension||exactY>camera::NativeCanvas::MaximumDimension)return {};
  // Pixel storage is rounded outwards; the final filter still uses the exact
  // native viewport. No ceil-induced pixel row becomes part of the view.
  int nativeWidth=viewport.backingWidth(),nativeHeight=viewport.backingHeight();
  if((complete_&&(!complete_->geo.same(domain)||complete_->reveal!=frame.terrain->reveal))||(building_&&(!building_->geo.same(domain)||building_->reveal!=frame.terrain->reveal))||exactX!=spanX_||exactY!=spanY_)clearBackground();
  if(region.active&&!prepareWorldOnly_){
   if(!coverageLocal_||!coverage_.contains(region.x,region.y,region.width,region.height,nativeWidth,nativeHeight)){
    clearBackground();coverage_=terraincoverage::Region::plan(region.x,region.y,region.width,region.height,nativeWidth,nativeHeight);coverageLocal_=true;
    if(!coverage_.ready)return {};
   }
  }else if(coverageLocal_){clearBackground();coverage_={};coverageLocal_=false;}
  spanX_=exactX;spanY_=exactY;dynamicFailure_=false;
  const SceneRegion raster=region.active?region:SceneRegion{0,0,spanX_,spanY_,true};
  if(blocked_){if(blocked_==frame.terrain||sameSource(*blocked_,*frame.terrain,region.active)){blocked_=frame.terrain;return {};}blocked_.reset();}
  lighting::State scene=lighting::sample(read);GameView<Read> game(read);
  std::array<uint32_t,256> liveTerrainPalette{};const bool paletteReady=assets.palette(true,liveTerrainPalette);
  if(!paletteReady&&terrainPaletteReady_){stageStats_.failure=RenderFailure::TerrainPalette;return {};}
  if(paletteReady){if(terrainPaletteReady_&&terrainPalette_!=liveTerrainPalette){tiles_.clear();clearBackground();}terrainPalette_=liveTerrainPalette;terrainPaletteReady_=true;}
  phaseReads(stageStats_.setupReads);
  // Presentation can run faster than raw world capture. Rechecking the exact
  // same sealed bytes cannot discover new Cell lighting. Retain only a fully
  // verified crop of a completed background; a new token (including a new
  // serial at the same engine frame), source/crop change or missing read goes
  // through the ordinary validation below. Palette/resource/geometry changes
  // above still invalidate the background before this decision.
  const auto terrainSource=lighting::sealedSource(read,0);
  const auto&verifiedRegion=verifiedTerrainRegion_;
  const bool sameVerifiedRegion=region.active==verifiedRegion.active&&region.x==verifiedRegion.x&&region.y==verifiedRegion.y&&region.width==verifiedRegion.width&&region.height==verifiedRegion.height;
  const bool reuseTerrain=terrainSource&&terrainSource==frame.sourceFrame&&verifiedTerrainSource_.lock()==terrainSource&&
   !building_&&!backgroundIncomplete_&&complete_==frame.terrain&&backgroundLight_==scene&&sameVerifiedRegion;
  if(!reuseTerrain){
  verifiedTerrainSource_.reset();
  // A pending bank cannot combine lighting from two different global states.
  if(building_&&buildingLight_!=scene){building_.reset();pending_={};prepared_.clear();cursor_=baseCursor_=0;}

  auto resetPending=[&](){if(!pending_.reset(nativeWidth,nativeHeight,nativeBudget_))return false;
   pending_.retainGround(outputSized_);
   if(coverageLocal_)pending_.restrictPaint(int(coverage_.left),int(coverage_.top),int(coverage_.right),int(coverage_.bottom));return true;};
  if(!building_){
   building_=frame.terrain;buildingLight_=scene;bankSameSource_=complete_&&(complete_==building_||sameSource(*complete_,*building_,region.active));cursor_=baseCursor_=0;prepared_.clear();probeChanged_=pendingIncomplete_=false;
   if(!bankSameSource_){cells_.clear();baseCells_.clear();cellReadPlan_.clear();
   for(size_t n=0;n<building_->colors.size();++n)if(building_->colors[n]||building_->art[n].sourceColor){
    int x=int(n%512),y=int(n/512);mapbounds::TileOrigin origin;
    if(!building_->geo.tileOrigin(x,y,building_->art[n].level,origin))continue;
    if(coverageLocal_&&!coverage_.tile(origin.x,origin.y)&&!coverage_.rgb((x-y+building_->geo.offsetX)*30,(x+y-building_->geo.offsetY)*15))continue;
    cells_.push_back(n);
   }
   if(building_->reveal&&building_->baseColors.size()==building_->colors.size())
    for(size_t n=0;n<building_->baseColors.size();++n)
     if(building_->baseColors[n]&&!building_->colors[n]&&!building_->art[n].sourceColor&&
       (!coverageLocal_||coverage_.rgb((int(n%512)-int(n/512)+building_->geo.offsetX)*30,(int(n%512)+int(n/512)-building_->geo.offsetY)*15)))baseCells_.push_back(n);
   std::stable_sort(cells_.begin(),cells_.end(),[](size_t a,size_t b){return a%512+a/512<b%512+b/512;});}
   // New sampling banks commonly contain identical terrain. Compare evaluated
   // per-cell lighting before deciding to rebuild millions of native pixels.
   probing_=!(backgroundIncomplete_&&int32_t(frame.tick-backgroundRetry_)>=0)&&bankSameSource_&&painted_.size()==cells_.size();prepared_.resize(cells_.size());
   if(!probing_&&!resetPending())return {};
  }
  size_t staticPixels=0;for(const auto&i:tiles_)staticPixels+=camera::pixels(*i.second.image);
  const Terrain*evaluatedTerrain=building_.get();
  if(!region.active){
   // Stable whole-map verification visits every Cell. A partial first build
   // only paints its current chunk; do not prefetch the entire map per chunk.
   const size_t first=probing_?0:cursor_,end=probing_?cells_.size():std::min(cells_.size(),cursor_+size_t(4096));
   prefetchCells(read,*evaluatedTerrain,cells_,first,end,cellReadPlan_,0);
  }
  // Validation, incremental probing and painting all use this presentation's
  // read epoch. Do not reread/recalculate the same cell in each pass. The final
  // race check below deliberately bypasses the memo after a bank commit.
  evaluatedPaint_.resize(evaluatedTerrain->art.size());evaluatedStamp_.resize(evaluatedTerrain->art.size());
  if(!++evaluationEpoch_){std::fill(evaluatedStamp_.begin(),evaluatedStamp_.end(),0);++evaluationEpoch_;}
  lighting::TintCache terrainTints;
  auto prepare=[&](size_t index,bool fresh=false){
   if(!fresh&&evaluatedStamp_[index]==evaluationEpoch_)return evaluatedPaint_[index];
   ++stageStats_.terrainLightCells;
   const auto&cell=evaluatedTerrain->art[index];Paint value;value.light=lighting::cell(read,cell.cell,scene,lighting::Layer::Terrain,true,fresh?nullptr:&terrainTints);
   value.signature=lightKey(1469598103934665603ull,value.light);
   evaluatedStamp_[index]=evaluationEpoch_;evaluatedPaint_[index]=value;return value;
  };
  // Validate the CURRENT crop synchronously. Offscreen incremental checks
  // must not make a stable main viewport flash back to the game every pass.
  auto verifyVisible=[&](bool fresh=false){
   if(!evaluatedTerrain||painted_.size()!=cells_.size())return false;
   double haloX=30,haloY=30;
   auto imageExtent=[&](auto&&self,const voxel::Image&i,unsigned nesting)->void{if(nesting>16||i.rasterScale<1)return;
    haloX=std::max({haloX,std::abs(double(i.x)/i.rasterScale),std::abs((double(i.x)+i.width)/i.rasterScale)});
    haloY=std::max({haloY,std::abs(double(i.y)/i.rasterScale)+64.,std::abs((double(i.y)+i.height)/i.rasterScale)+64.});
    for(const auto&part:i.attachments)self(self,part,nesting+1);
   };
   auto extent=[&](const auto&cache){for(const auto&item:cache){if(item.second.ready)imageExtent(imageExtent,*item.second.image,0);else{haloX=std::max(haloX,512.);haloY=std::max(haloY,512.);}}};
   extent(tiles_);
   for(size_t n=0;n<cells_.size();++n){size_t index=cells_[n];
    // cells_ already belongs to this exact geometry/source. Whole-map views
    // need every cell's current light, but have no crop to project it against.
    if(region.active){++stageStats_.terrainGeometryChecks;const auto&cell=evaluatedTerrain->art[index];mapbounds::TileOrigin origin;
     if(!evaluatedTerrain->geo.tileOrigin(int(index%512),int(index/512),int(cell.level),origin))continue;
     double x=origin.x+30,y=origin.y+15;
     if(x<region.x-haloX||x>region.x+region.width+haloX||y<region.y-haloY||y>region.y+region.height+haloY)continue;
    }
    if(prepare(index,fresh).signature!=painted_[n])return false;
   }return true;
  };
  bool visibleVerified=building_&&bankSameSource_&&verifyVisible();
  if(region.active&&bankSameSource_&&!visibleVerified)probeChanged_=true;
  bool committed=false;
  // Whole-map verification above already evaluated every cell. Do not stall
  // moving actors for several more 4096-cell probe passes on identical ground.
  if(building_&&probing_){
   // Successful whole-map verification already compared every signature.
   // Do not copy every Paint into a temporary vector only to discard it.
   if(!region.active&&visibleVerified)cursor_=cells_.size();
   else{size_t end=std::min(cells_.size(),cursor_+size_t(4096));for(;cursor_<end;++cursor_){++stageStats_.terrainProbeCopies;prepared_[cursor_]=prepare(cells_[cursor_]);probeChanged_|=prepared_[cursor_].signature!=painted_[cursor_];}}
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
   const size_t total=cells_.size();
   const auto&palette=liveTerrainPalette;int loads=0;size_t end=std::min(total,cursor_+size_t(4096));
   for(;cursor_<end&&!pending_.failed();++cursor_){size_t position=cursor_;size_t index=cells_[position];int x=int(index%512),y=int(index/512);const auto&cell=terrain.art[index];
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
     if(!tile.ready&&paletteReady&&loads<64&&(!tile.retry||int32_t(frame.tick-tile.retry)>=0)){++loads;tile.ready=assets.tile(cell.type,cell.sub,palette,*tile.image);if(tile.ready){auto n=camera::pixels(*tile.image);if(staticPixels+n<=64*1024*1024)staticPixels+=n;else{tile.ready=false;tile.image=std::make_shared<voxel::Image>();}}tile.retry=frame.tick+2000;}
     if(!tile.ready&&paletteReady&&loads>=64&&!tile.retry)break;
     pending_.terrainSource(index,tile.ready?tile.image:nullptr,px,py,base,paint.light,int(cell.level));
     if(tile.ready){camera::Material material;material.nativeZ=true;material.nativeZAdjust=-15*int(cell.level);pending_.blit(*tile.image,px,py,1,1,base,paint.light,0,0,material);}
     else{pendingIncomplete_=true;pending_.diamond(px,py,1,1,base,0xff000000u|baseColor,paint.light);}
    }else pending_.terrainSource(index,nullptr,px,py,base,paint.light,int(cell.level)); // No retained source may survive a removed tile.
    // Overlay bodies and their separate shadow sweep are dynamic world layers.
    if(pending_.failed())break;++nativeCells_;
   }
   if(pending_.failed()){
    // Release the old native bank before retrying the current cell. The worker
    // may keep its last small published scene; never silently minify sources.
    if(background_.pageCount()){background_={};complete_.reset();baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();pending_.clearFailure();}
    else{blocked_=building_;building_.reset();pending_={};prepared_.clear();cells_.clear();baseCells_.clear();cursor_=baseCursor_=0;complete_.reset();baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();}
    return {};
   }
   if(cursor_==total){++motionTerrainRevision_;committed=true;background_=std::move(pending_);background_.clean();painted_.resize(prepared_.size());for(size_t n=0;n<prepared_.size();++n)painted_[n]=prepared_[n].signature;prepared_.clear();complete_=building_;building_.reset();backgroundLight_=scene;backgroundIncomplete_=pendingIncomplete_;backgroundRetry_=frame.tick+2000;baseScene_.reset();maskedBaseScene_.reset();}
  }
  // Reject local-light races during incremental builds before publication.
  if(committed&&region.active&&!verifyVisible(true))return {};
  // An incomplete bank may only reuse a synchronously validated crop.
  if(!complete_||(building_&&!visibleVerified))return {};
  if(visibleVerified&&!building_&&!backgroundIncomplete_&&complete_==frame.terrain&&terrainSource&&terrainSource==frame.sourceFrame&&lighting::sealedSource(read,0)==terrainSource){
   verifiedTerrainSource_=terrainSource;verifiedTerrainRegion_=region;
  }
  }
  phaseReads(stageStats_.terrainScanReads);
  // Scale the CURRENT lit terrain as one continuous native RGB backing.
  // Filtering individual transparent TMP diamonds introduces artificial
  // image borders at every adjoining tile; presentRegion supplies a source
  // neighborhood halo across those joins before xBRZ is evaluated.
  // Native terrain shadows were already composed exactly once above.
  const bool areaGpuMode=!detail&&!region.active&&background_.pageCount()<=gpuarea::MaximumPages&&xbrzadapter::gpuAreaAvailable();
  if(areaGpuMode!=areaGpuMode_){areaGpuMode_=areaGpuMode;baseScene_.reset();maskedBaseScene_.reset();maskSource_.reset();gpuMaskPlan_.reset();cpuMaskPlan_.reset();maskPixels_.reset();}
  if(!prepareWorldOnly_&&!baseScene_){
   auto base=std::make_shared<Scene>();base->width=width;base->height=height;
   gpuBaseTiles_.reset();cpuBaseFallback_={};
   std::vector<vulkancompute::TileJob> jobs;
   if(outputSized_){
    if(!background_.presentRegion(width,height,base->pixels,raster.x,raster.y,raster.width,raster.height,pixelScaleMode))return {};
   }else if(areaGpuMode){
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
  if(!prepareWorldOnly_&&detail&&!detailed_.prepare(background_,baseScene_->pixels,width,height,raster.x,raster.y,raster.width,raster.height,detailKey_,precision_,pixelScaleMode,
    std::shared_ptr<const std::vector<uint32_t>>(baseScene_,&baseScene_->pixels),baseScene_->pixels.empty()?gpuBaseTiles_:nullptr,cpuBaseFallback_,motionTerrainRevision_+1))return {};
  if(!prepareWorldOnly_&&detail){vulkancompute::MotionView motion;motion.context=motionContext_;motion.x=raster.x;motion.y=raster.y;motion.scaleX=width/raster.width;motion.scaleY=height/raster.height;
   motion.depthFar=-4096;motion.depthNear=32768;motion.terrainRevision=motionTerrainRevision_;motion.tick=frame.tick;motion.frame=frame.frame;detailed_.motion(std::move(motion),guideEnabled_);}
  // Native Cell::DrawShroud places 60x30 alpha sprites on the flat cell
  // projection, after the world is drawn. Cache their final-presentation mask
  // separately from the original-size terrain so units and repairs cannot
  // escape into unseen neighboring cells. All frames use their raw SHP bytes.
  const bool masked=!complete_->reveal&&complete_->shade.size()==512*512;
  uint32_t options=game.template get<uint32_t>(0xA8B230);
  bool fog=masked&&options>=0x10000&&(game.template get<uint8_t>(options+1)&0x10)&&
   game.template get<uint8_t>(complete_->geo.house+0x1F5)==0;
  if(!prepareWorldOnly_&&masked&&(region.active||outputSized_)&&currentVisibility(read,0)){
   shroudmask::Snapshot current;
   // Full-map banks are collected over multiple bounded sampling passes.
   // Their shade can be older than another worker's bank even when its actor
   // timestamp is newer. Read current exploration for radar too, never replay
   // those alternating masks. A whole-map visibility snapshot has a separate
   // 30Hz budget; it does not throttle actors or resize the scene raster.
   const bool capture=region.active||!liveMaskValid_||!liveMaskSnapshot_.geo.same(complete_->geo)||
     frame.frame<liveMaskSnapshot_.frame||uint32_t(frame.tick-liveMaskTick_)>=33;
   if(capture){const auto visibilityAt=std::chrono::steady_clock::now();stageStats_.visibilityAttempted=true;
    const bool valid=liveMasks_.capture(read,complete_->geo,raster.x,raster.y,raster.width,raster.height,current);const auto stats=liveMasks_.stats();
    stageStats_.visibilityMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-visibilityAt).count();
    stageStats_.visibilityCalls=stats.calls;stageStats_.visibilityPages=stats.pages;stageStats_.visibilityCells=current.cells.size();if(!valid)return {};
   }
   const auto&visibility=capture?current:liveMaskSnapshot_;
   fog=visibility.fog;
   if(!liveMaskValid_||!maskSource_||mask_.width!=width||mask_.height!=height||!liveMaskSnapshot_.sameMask(visibility)){
    bool planned=false;
    if(gpuMode_){
     planned=gpuMaskBuilder_.build(visibility.geo,visibility.cells,width,height,raster.width,raster.height,fog,
       [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},gpuMaskPlan_,raster.x,raster.y)&&gpushroud::withinGpuBudget(*gpuMaskPlan_);
    }
    if(planned){mask_.width=width;mask_.height=height;mask_.alpha.clear();cpuMaskPlan_.reset();}
    else{
     // A dense whole-map mask can exceed the GPU's existing per-bin work
     // ceiling. Build the CPU stamp raster directly at target dimensions;
     // replaying the rejected bin plan on CPU scans billions of candidates.
     gpuMaskPlan_.reset();cpuMaskPlan_.reset();
     if(!mask_.build(visibility.geo,visibility.cells,width,height,raster.width,raster.height,fog,
       [&](bool isFog,int index,voxel::Image&out){return assets.shroud(isFog,index,out);},raster.x,raster.y))return {};
    }
    maskedBaseScene_.reset();maskPixels_.reset();
   }
   if(capture){liveMaskSnapshot_=std::move(current);liveMaskTick_=frame.tick;}liveMaskValid_=true;maskSource_=complete_;maskFog_=fog;
  }else if(!prepareWorldOnly_&&masked&&(maskSource_!=complete_||maskFog_!=fog||mask_.width!=width||mask_.height!=height)){
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
  auto alphaAsset=[&](uint32_t pointer)->std::shared_ptr<const voxel::Image>{auto found=effectAlphas_.find(pointer);if(found==effectAlphas_.end()){voxel::Image image;if(!alphaImage(assets,pointer,image,0))return {};size_t bytes=0;for(const auto&item:effectAlphas_)bytes+=camera::pixels(*item.second)*5;if(bytes+camera::pixels(image)*5>64*1024*1024)effectAlphas_.clear();found=effectAlphas_.emplace(pointer,std::make_shared<const voxel::Image>(std::move(image))).first;}return found->second;};
  const double projectionScale=projectionZScale(read);
  auto nativeBase=prepareWorldOnly_?camera::NativeCanvas{}:background_.fork();
  if(!detail&&!prepareWorldOnly_&&!nativePageMasks_.prepare(nativeBase,*frame.terrain,masked,fog,frame.effects&&frame.effects->world?frame.effects->world.get():nullptr,features_.particles,projectionScale,[&](bool fog,int n,voxel::Image&out){return assets.shroud(fog,n,out);},alphaAsset))return {};
  auto view=prepareWorldOnly_?camera::NativeCanvas{}:nativeBase.fork();
  std::vector<camera::ObjectCanvas::Draw>objectDraws;objectDraws.reserve(frame.markers.size());
  auto draw=[&](const voxel::Image&i,float x,float y,float sx,float sy,float z,const lighting::State&light,float lift=1,float drop=0,std::shared_ptr<const voxel::Image>owner={},vulkancompute::MotionIdentity motion={},camera::Material material={}){if(detail&&!prepareWorldOnly_)detailed_.blit(i,x,y,sx,sy,z,light,lift,drop,std::move(owner),std::move(motion),material);else{if(!owner||owner.get()!=&i)owner=std::make_shared<const voxel::Image>(i);objectDraws.push_back({std::move(owner),x,y,z,lift,drop,light,material,motion.object,motion.id});}};

  const auto&geo=frame.terrain->geo;int work=0;size_t pixels=0;std::vector<const Marker*> fallbacks;
  phaseReads(stageStats_.maskReads);
  if(!extraLayers_.ground(frame,read,assets,scene,draw)||!extraLayers_.frozen(frame,read,assets,scene,projectionScale,draw))return {};
  phaseReads(stageStats_.extraReads);
  stage(stageStats_.terrainMs,stageStats_.terrainReads);auto markers=frame.markers;
  if(!features_.particles)markers.erase(std::remove_if(markers.begin(),markers.end(),[](const Marker&m){return m.kind==7||m.kind==8||(m.kind==5&&!m.parent);}),markers.end());
  if(region.active&&!prepareWorldOnly_)markers.erase(std::remove_if(markers.begin(),markers.end(),[&](const Marker&m){
   double x=(double(m.x)-m.y)*30/256.+geo.offsetX*30,y=(double(m.x)+m.y)*15/256.-geo.offsetY*15-screenHeight(m.z,projectionScale);
   if(x<region.x-512||x>region.x+region.width+512||y<region.y-512||y>region.y+region.height+512)return true;
   // Once this exact pose is known, cull its real body+attachment bounds
   // before palette validation, VXL preparation and command recording.
   // Unknown poses keep the conservative discovery halo until decoded.
   auto cached=sprites_.find(imageKey(m));
   if(detail&&cached!=sprites_.end()&&cached->second.ready&&!cached->second.image->incomplete&&m.x>=0&&m.y>=0&&m.x<512*256&&m.y<512*256){
    const auto cell=frame.terrain->art[(m.y/256)*512+m.x/256].cell;const int level=game.template get<int8_t>(cell+0x11B);
    const float drop=m.pose.shadow.known?float(m.pose.shadow.drop):float(std::max(0,screenHeight(m.z,projectionScale)-(level+(m.onBridge?4:0))*15));
    return !detailed_.visible(*cached->second.image,float(x),float(y+camera::anchorY(m.kind)),drop);
   }return false;
  }),markers.end());
  for(auto&m:markers)if(m.kind==3&&!m.disguised&&!frame.posesPrepared)assets.buildingPose(m.object,m.type,m.pose);
  std::map<ObjectKey,ImageKey> visible;std::set<ImageKey> wanted;std::set<ArtKey> artWanted;
  for(const auto&m:markers){auto key=imageKey(m);if(m.pose.frameFrom>=0){auto previous=m;previous.pose.frame=m.pose.frameFrom;wanted.insert(imageKey(previous));}visible[{m.object,m.id}]=key;wanted.insert(key);artWanted.insert(artKey(m));}
  for(auto i=displayed_.begin();i!=displayed_.end();){auto current=visible.find(i->first);
   if(current==visible.end()||std::get<0>(current->second)!=std::get<0>(i->second)||std::get<1>(current->second)!=std::get<1>(i->second)||std::get<2>(current->second)!=std::get<2>(i->second)||std::get<6>(current->second)!=std::get<6>(i->second))i=displayed_.erase(i);
   else{wanted.insert(i->second);++i;}}
  if(sprites_.size()>1024){for(auto i=sprites_.begin();i!=sprites_.end();)if(!wanted.count(i->first))i=sprites_.erase(i);else ++i;}
  if(art_.size()>512){for(auto i=art_.begin();i!=art_.end();)if(!artWanted.count(i->first))i=art_.erase(i);else ++i;}
  std::set<ArtKey> invalidArts;stageStats_.artWanted=artWanted.size();
  const auto lightRevision=voxelRevision(assets,0);const bool voxelLightChanged=!lightRevision.first||!voxelLightKnown_||voxelLightRevision_!=lightRevision.second;
  voxelLightKnown_=lightRevision.first;voxelLightRevision_=lightRevision.second;
  if(liveVoxels&&voxelLightChanged)for(const auto&source:art_)if(voxelArt(source.second,0))invalidArts.insert(source.first);
  stageStats_.candidates=markers.size();stageStats_.rasterScale=rasterScale(assets,0);stageStats_.voxelLightAvailable=lightRevision.first;stageStats_.voxelLightChanged=voxelLightChanged;
  std::set<ArtKey> unavailablePalettes;
  for(const auto&key:artWanted){++stageStats_.artScanned;auto source=art_.find(key);if(source==art_.end())continue;bool changed=false;
   if(!refreshPalette(assets,source->second,changed,0)){invalidArts.insert(key);unavailablePalettes.insert(key);}
   else if(changed)invalidArts.insert(key);
  }
  for(const auto&m:markers)if(m.kind==4){bool changed=false;
   if(!refreshOverlay(assets,m.type,game.template get<uint32_t>(m.type+0xA4),m.pose.frame,true,changed,0)){invalidArts.insert(artKey(m));unavailablePalettes.insert(artKey(m));}
   else if(changed)invalidArts.insert(artKey(m));
  }
  invalidateArts(invalidArts);
  if(!unavailablePalettes.empty()){
   // Explosions, smoke and projectiles may expire between the immutable
   // observation and this render. Drop only those instances, never the whole
   // completed world because one transient no longer has a live palette.
   for(const auto&m:markers)if(m.kind<5&&unavailablePalettes.count(artKey(m))){recordFailure(RenderFailure::ActorPalette,m);stage(stageStats_.palettesMs,stageStats_.paletteReads);return {};}
   markers.erase(std::remove_if(markers.begin(),markers.end(),[&](const Marker&m){return m.kind>=5&&unavailablePalettes.count(artKey(m));}),markers.end());
  }
  stage(stageStats_.palettesMs,stageStats_.paletteReads);
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
   // Ordinary VXL art is mandatory for a new scene. Once a transient resource
   // read recovers, do not hold that whole scene behind the optional-art 2s
   // retry. The existing decode budget still bounds every preparation pass.
   const bool requiredActor=!cached.ready&&(m.kind==3||(m.kind<5&&m.pose.voxel));
   cached.retry=cached.ready&&!cached.image->incomplete?0:frame.tick+((transition||requiredActor)?16u:2000u);
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
  stage(stageStats_.decodeMs,stageStats_.decodeReads);
  stageStats_.geometryBytes=artBytes;stageStats_.posePixels=pixels;
  if(liveVoxels){
   // Refresh VPL and palette provenance every pass; reuse pose pixels only
   // while both revisions and the complete body/shadow posture still agree.
   // A reader without revision support retains the conservative live path.
   std::set<ImageKey> rastered;size_t liveFaces=0;
   for(const auto&m:markers){auto source=art_.find(artKey(m));
    if(source==art_.end()){if(m.pose.voxel&&m.kind<5){recordFailure(RenderFailure::MissingVoxelArt,m);stage(stageStats_.voxelMs,stageStats_.voxelReads);return {};}continue;}
    if(!voxelArt(source->second,0))continue;auto key=imageKey(m);if(!rastered.insert(key).second)continue;
    auto cachedImage=sprites_.find(key);
    if(!voxelLightChanged&&cachedImage!=sprites_.end()&&cachedImage->second.ready&&!cachedImage->second.image->incomplete){++decodeStats_.hits;continue;}
    liveFaces+=faceCount(source->second,0);stageStats_.liveFaces=liveFaces;if(liveFaces>rendermemory::LiveFaces){if(m.kind>=5)continue;recordFailure(RenderFailure::VoxelFaceBudget,m);stage(stageStats_.voxelMs,stageStats_.voxelReads);return {};}
    clearAssetFailure(assets,0);std::shared_ptr<const voxel::Image>image;const bool drawn=sharedActor(assets,source->second,m.pose,image,0);
    if(!drawn||!image||image->incomplete){if(m.kind>=5)continue;recordFailure(!drawn?RenderFailure::VoxelRaster:!image?RenderFailure::VoxelImageMissing:RenderFailure::VoxelIncomplete,m);stageStats_.assetFailure=assetFailure(assets,0);stage(stageStats_.voxelMs,stageStats_.voxelReads);return {};}
    auto&cached=sprites_[key];size_t previous=camera::pixels(*cached.image),next=camera::pixels(*image);
    if(m.kind<5&&pixels-previous+next>rendermemory::PosePixels){
     // Old displayed poses are useful fallbacks for time-budgeted SHP and
     // optional transient VXL. Ordinary live VXL instead requires its current
     // pose before ANY new frame can publish. Keeping both generations until
     // success can therefore pin the cache forever even when one frame fits.
     // Retire only those soft old VXL entries, once across the whole cache;
     // immutable published CPU/GPU/world frames keep their own image owners.
     // Build the stricter set only under actual pressure; the normal path
     // keeps its existing wanted set and performs no extra per-frame copies.
     std::set<ImageKey> required;for(const auto&current:markers){required.insert(imageKey(current));
      if(current.pose.frameFrom>=0){auto prior=current;prior.pose.frame=current.pose.frameFrom;required.insert(imageKey(prior));}
     }
     bool retired=false;
     for(auto i=sprites_.begin();i!=sprites_.end();){auto art=art_.find(artKey(i->first));
      if(std::get<2>(i->first)<5&&!required.count(i->first)&&art!=art_.end()&&voxelArt(art->second,0)){
       pixels-=camera::pixels(*i->second.image);i=sprites_.erase(i);retired=true;
      }else ++i;
     }
     if(retired)for(auto i=displayed_.begin();i!=displayed_.end();)if(!sprites_.count(i->second))i=displayed_.erase(i);else ++i;
    }
    stageStats_.posePixels=pixels;stageStats_.replacementPixels=previous;stageStats_.newPixels=next;
    if(pixels-previous+next>rendermemory::PosePixels){if(m.kind>=5)continue;recordFailure(RenderFailure::VoxelPoseBudget,m);stage(stageStats_.voxelMs,stageStats_.voxelReads);return {};}
    pixels=pixels-previous+next;cached.image=std::move(image);cached.ready=true;cached.retry=0;
   }
  }
  for(const auto&m:markers){auto key=imageKey(m);auto i=sprites_.find(key);if(i!=sprites_.end()&&i->second.ready)displayed_[{m.object,m.id}]=key;}
  materialsReady_=!backgroundIncomplete_&&!building_;
  for(const auto&m:decodeMarkers){auto source=art_.find(artKey(m));if(liveVoxels&&source!=art_.end()&&voxelArt(source->second,0))continue;auto found=sprites_.find(imageKey(m));materialsReady_&=found!=sprites_.end()&&found->second.ready&&!found->second.image->incomplete;}
  stage(stageStats_.voxelMs,stageStats_.voxelReads);stageStats_.materialsMs=stageStats_.palettesMs+stageStats_.decodeMs+stageStats_.voxelMs;stageStats_.actors=markers.size();
  auto order=tetherdraw::order(markers,!prepareWorldOnly_);
  // Native ground animations share object order (Layer, GetYSort). A blanket
  // final animation pass incorrectly paints pumps/fire over foreground units.
  lighting::ObjectLightContext objectLights;gatherShapes_.begin();
  for(const auto*marker:order){const auto&m=*marker;if(m.x<0||m.y<0||m.x>=512*256||m.y>=512*256)continue;
   ++requestedObjects_;
   auto found=sprites_.find(imageKey(m));if(found==sprites_.end()||!found->second.ready){auto old=displayed_.find({m.object,m.id});if(old!=displayed_.end())found=sprites_.find(old->second);}
   float px=float((double(m.x)-m.y)*30/256.+geo.offsetX*30),py=float((double(m.x)+m.y)*15/256.-geo.offsetY*15+camera::anchorY(m.kind)-screenHeight(m.z,projectionScale));
   if(found!=sprites_.end()&&found->second.ready){
    ++drawnObjects_;
    std::shared_ptr<const voxel::Image> imageOwner=found->second.image;const voxel::Image*image=imageOwner.get();voxel::Image blended;
    const float lift=m.kind==4?12:(m.kind==3||m.kind==5)?2:1;
    auto source=art_.find(artKey(m));
    if(m.pose.frameFrom>=0&&(source==art_.end()||!voxelArt(source->second,0))&&found==sprites_.find(imageKey(m))){auto prior=m;prior.pose.frame=m.pose.frameFrom;auto old=sprites_.find(imageKey(prior));
     if(old!=sprites_.end()&&old->second.ready&&materialblend::image(*old->second.image,*image,unsigned(m.pose.frameWeight),lift,blended)){imageOwner=std::make_shared<const voxel::Image>(std::move(blended));image=imageOwner.get();}
    }
    const bool relight=(source!=art_.end()&&voxelArt(source->second,0))?features_.voxelLighting:features_.spriteLighting;
    auto light=(!relight||m.kind==7||m.kind==8)?lighting::State{}:m.kind==6?projectiles::light(read,uint32_t(m.pose.variant)):m.kind==5?lighting::animation(read,m.object,m.type,m.x,m.y,m.z,scene):m.kind==4?lighting::terrainObject(read,m.type,m.x,m.y,m.z,scene):lighting::object(read,m.type,m.owner,m.kind,m.x,m.y,m.z,scene,&objectLights);uint32_t cell=frame.terrain->art[(m.y/256)*512+m.x/256].cell;bool bridge=m.onBridge;int level=game.template get<int8_t>(cell+0x11B);
    vulkancompute::MotionIdentity motion;motion.object=m.object;motion.id=m.id;motion.type=m.type;motion.kind=uint32_t(m.kind);motion.known=guideEnabled_&&detail&&m.object>=0x10000&&m.type>=0x10000&&m.id!=0xffffffffu&&m.kind>=0&&m.kind<=6;
    auto material=m.material;if(!material.nativeZ){material.nativeZ=true;material.nativeZAdjust=-screenHeight(m.z,projectionScale)-2;}if(m.waterline>0)material.clipBottom=float(m.waterline-geo.offsetY*15)-py;
    draw(*image,px,py,1,1,float((int64_t(m.x)+m.y)*15/256.)-15+camera::anchorY(m.kind)-m.drawZ+(bridge?60:0),light,m.kind==4?12:(m.kind==3||m.kind==5)?2:1,m.pose.shadow.known?float(m.pose.shadow.drop):float(std::max(0,screenHeight(m.z,projectionScale)-(level+(bridge?4:0))*15)),std::move(imageOwner),std::move(motion),material);
    // Vera units.rs: OREGATH is an owned, Z-read-only piece immediately
    // after its harvester. It follows the same world camera and shroud pass.
    if(features_.particles&&m.kind==2){nativeeffects::GatherState gather;nativefeedback::Object object;object.pointer=m.object;object.id=m.id;object.type=m.type;object.owner=m.owner;object.kind=m.kind;
     if(nativeeffects::gather(read,object,frame.frame,gather)){uint32_t shape=0;if(read(nativeeffects::GatherShape,&shape,4))if(auto effect=gatherShapes_.get(read,shape,gather.frame,0x87F6C0)){
      const int x=m.x+gather.point.x-gather.location.x,y=m.y+gather.point.y-gather.location.y;
      const float gx=float(int((int64_t(x)-y)*30/256)+geo.offsetX*30),gy=float(int((int64_t(x)+y)*15/256)-geo.offsetY*15-screenHeight(m.z,projectionScale));
      auto effectLight=features_.spriteLighting?lighting::object(read,m.type,m.owner,m.kind,m.x,m.y,m.z,scene,&objectLights):lighting::State{};
      draw(*effect,gx,gy,1,1,float((int64_t(m.x)+m.y)*15/256)-15-m.drawZ+(bridge?60:0),effectLight,3,0,effect,motion);
     }}
    }
   }
   else if(m.kind<4)fallbacks.push_back(&m);
  }
  if(features_.particles)trailLayers_.paint(frame,projectionScale,draw);
  std::vector<worldscene::ScreenSprite>beacons;
  if(prepareWorldOnly_||region.active)extraLayers_.beacons(frame,read,assets,projectionScale,[&](const voxel::Image&,float x,float y,float,float,float z,const lighting::State&light,float lift,float drop,std::shared_ptr<const voxel::Image>image){beacons.push_back({{image,x,y,z,lift,drop,light},extraLayers_.imageBytes(image)});});
  if(prepareWorldOnly_){
   // All terrain pages and world-space instances survive camera movement.
   // Decoding/live palette reads stay here; the presentation lane owns the
   // camera, pass recording and GPU submission for every new viewport.
   auto packet=std::make_shared<worldscene::Snapshot>();packet->ground=background_.fork();packet->state=frame;
   packet->beacons=beacons;packet->instances=std::move(objectDraws);packet->terrainRevision=motionTerrainRevision_;packet->profile=features_;packet->samplingMode=pixelScaleMode;packet->precision=precision_;packet->zscale=projectionScale;packet->masked=masked;
   for(const auto*m:fallbacks)packet->missing.push_back(*m);
   for(const auto&m:markers){int x,y,w,h;if(feedbackBounds(m,x,y,w,h))packet->bounds[{m.object,m.id}]={x,y,w,h};}
   if(masked)for(int kind=0;kind<2;++kind)for(int n=0;n<47;++n)if(!worldShroud_[kind][n]){
    voxel::Image image;if(assets.shroud(kind!=0,n,image))worldShroud_[kind][n]=std::make_shared<const voxel::Image>(std::move(image));
   }
   packet->shroud=worldShroud_;
   if(features_.particles&&frame.effects&&frame.effects->world)for(const auto&a:frame.effects->world->alphas){
    auto found=effectAlphas_.find(a.image);if(found==effectAlphas_.end()){voxel::Image image;if(!alphaImage(assets,a.image,image,0))continue;
     size_t bytes=0;for(const auto&item:effectAlphas_)bytes+=camera::pixels(*item.second)*5;
     if(bytes+camera::pixels(image)*5>64*1024*1024)effectAlphas_.clear();
     found=effectAlphas_.emplace(a.image,std::make_shared<const voxel::Image>(std::move(image))).first;
    }packet->alphas.emplace(found->first,found->second);
   }
   preparedWorld_=std::move(packet);stage(stageStats_.objectsMs,stageStats_.objectReads);
   auto out=std::make_shared<Scene>();out->width=width;out->height=height;return out;
  }
  // Radar insignia follows the very same native SHP recording/resolve path
  // as world sprites. Main-view statuses remain in local_ui_task, whose
  // Painter already applies the view's actual output transform.
  if(!region.active)sceneextras::radarRanks(frame,read,extraLayers_,[&](const auto&image,double x,double y){draw(*image,float(x),float(y),1,1,0,lighting::State{},0,0,image);});
  // Keep physical border scenery; the native radar projection and output
  // canvas crop the image. Camera movement limits are not a pixel mask.
  if(tiles_.size()>4096)tiles_.clear();
  // The saved CPU output is unmasked and predates annotations/effects. Only
  // reuse it if it describes the immediately preceding ObjectCanvas revision;
  // a failed intermediate render must fall back to the complete terrain base.
  const bool incrementalNative=!detail&&!region.active&&!areaGpuMode&&nativePresentation_.pixels&&
   nativePresentation_.pixels->size()==size_t(width)*height&&nativePresentation_.background==detailKey_&&
   nativePresentation_.objects==objectCanvas_.revision()&&!nativePresentation_.gpu&&!nativePresentation_.used;
  gpuarea::Plan nativeAreaPlan;
  const bool nativeObjectsGpu=areaGpuMode&&objectCanvas_.planGPU(nativeBase,objectDraws,width,height,spanX_,spanY_,nativeAreaPlan);
  if(!detail&&!nativeObjectsGpu&&!objectCanvas_.compose(nativeBase,objectDraws,view,incrementalNative)){dynamicFailure_=true;return {};}
  if(view.failed()){dynamicFailure_=true;return {};}
  stage(stageStats_.objectsMs,stageStats_.objectReads);
  const bool worldEffects=frame.effects&&frame.effects->world&&!frame.effects->world->empty();
  const bool radarExtras=!region.active&&sceneextras::hasRadar(frame);
  const bool nativePages=!detail&&!nativeBase.terrainSources().empty();
  const bool staticView=!background_.retainedGround()&&!nativePages&&!radarExtras&&beacons.empty()&&!(features_.lasers&&frame.effects&&frame.effects->boltsValid&&!frame.effects->bolts.empty())&&!worldEffects&&(detail?(!detailed_.dirty()&&!gpuMode_):(!view.dirtyPages()&&objectDraws.empty()))&&fallbacks.empty();
  if(staticView&&!forceFrame){
   if(areaGpuMode){
    auto cached=masked?maskedBaseScene_:baseScene_;if(cached&&(cached->gpu?gpupresent::valid(cached->gpu,width,height)&&gpupresent::available():cached->pixels.size()==size_t(width)*height)){xbrzadapter::gpuAreaCached(areaCachedGpu_);return cached;}
   }else{
    if(!masked)return baseScene_;
    if(!maskedBaseScene_){auto out=std::make_shared<Scene>(*baseScene_);mask_.apply(out->pixels);maskedBaseScene_=std::move(out);}return maskedBaseScene_;
   }
  }
  auto dot=[&](const Marker&m){return std::pair<int,int>{int(((double(m.x)-m.y)*30/256.+geo.offsetX*30-(region.active?region.x:0))*width/(region.active?region.width:spanX_)),int(((double(m.x)+m.y)*15/256.-geo.offsetY*15-(region.active?region.y:0))*height/(region.active?region.height:spanY_))};};
  const bool bolts=features_.lasers&&frame.effects&&frame.effects->boltsValid&&!frame.effects->bolts.empty();
  worldfx::View effectView;effectView.x=region.active?region.x:0;effectView.y=region.active?region.y:0;effectView.width=region.active?region.width:spanX_;effectView.height=region.active?region.height:spanY_;effectView.pixelsW=width;effectView.pixelsH=height;effectView.offsetX=geo.offsetX;effectView.offsetY=geo.offsetY;effectView.zscale=projectionScale;

  bool effectPlanReady=false;std::shared_ptr<const gpueffects::Plan> areaEffects;
  if(detail||(areaGpuMode&&fallbacks.empty())){
   if(gpuMode_)for(const auto*m:fallbacks){auto p=dot(*m);detailed_.annotationDot(p.first,p.second,m->color);}
   gpueffects::Builder builder(effectView);
   if(worldEffects){const auto&fx=*frame.effects->world;if(features_.particles)for(const auto&a:fx.alphas)if(auto image=alphaAsset(a.image))builder.alpha(a,image);builder.paint(fx,features_.particles,features_.lasers);}
   if(bolts)builder.bolts(frame.effects->bolts,frame.frame);
   for(const auto&b:beacons)builder.uiImage(b.draw.image,b.rgba,(b.draw.x-effectView.x)*width/effectView.width,(b.draw.y-effectView.y)*height/effectView.height,width/effectView.width,height/effectView.height);
   if(radarExtras)sceneextras::radarDecorations(frame,width,height,read,assets,extraLayers_,builder);
   auto plan=builder.finish();effectPlanReady=plan->valid;if(detail)detailed_.gpuEffects((worldEffects||bolts||radarExtras||!beacons.empty())?plan:nullptr);else areaEffects=std::move(plan);
  }
  if(masked&&(gpuMode_||areaGpuMode||detail)&&!gpuMaskPlan_&&!maskPixels_)maskPixels_=std::make_shared<const std::vector<uint8_t>>(mask_.alpha);
  // Current sampled state, live light, palette and mask validation above run
  // on every request. Identical complete inputs can also retain CPU output.
  // CPU effects/annotations use separate painters with double-precision input;
  // do not use their quantized GPU plan as an exact CPU postprocessing key.
  const bool cpuReusable=detail&&!worldEffects&&!bolts&&!radarExtras&&beacons.empty();
  const bool reusableResolve=gpuMode_||cpuReusable;
  const auto*reusePlan=reusableResolve&&fallbacks.empty()&&effectPlanReady&&!guideEnabled_?
   detailed_.completeGpuPlan(masked?maskPixels_:nullptr,masked?gpuMaskPlan_:nullptr):nullptr;
  if(reusePlan){if(auto previous=gpuFrameReuse_.get(*reusePlan)){if(!previous->gpu||gpupresent::available()){xbrzadapter::gpuState().used=bool(previous->gpu);stageStats_.reused=true;stage(stageStats_.resolveMs,stageStats_.resolveReads);return previous;}}}
  else gpuFrameReuse_.reset();
  const bool directReady=directPresentation_&&gpuMode_&&effectPlanReady&&!guideEnabled_;
  auto*maskReadback=!directReady&&masked&&gpuMaskPlan_&&cpuMaskPlan_!=gpuMaskPlan_?&mask_.alpha:nullptr;
  auto out=(gpuMode_||areaGpuMode)?outputPool_.acquire():std::make_shared<Scene>();out->width=width;out->height=height;out->guides.reset();out->gpu.reset();out->boltsRendered=false;bool areaUsed=false;
  const bool sameNative=!detail&&!region.active&&!areaGpuMode&&nativePresentation_.pixels&&nativePresentation_.pixels->size()==size_t(width)*height&&
    nativePresentation_.background==detailKey_&&nativePresentation_.objects==objectCanvas_.revision()&&nativePresentation_.gpu==areaGpuMode&&
    (!nativePresentation_.used||(nativePresentation_.mask==(masked?gpuMaskPlan_:nullptr)&&nativePresentation_.fog==(masked?maskPixels_:nullptr)));
  if(sameNative){out->pixels=*nativePresentation_.pixels;areaUsed=nativePresentation_.used;if(areaGpuMode)xbrzadapter::gpuAreaCached(areaUsed);}
  else if(areaGpuMode){
   auto*direct=directPresentation_&&effectPlanReady&&fallbacks.empty()?&out->gpu:nullptr;
   if(nativeObjectsGpu){nativeAreaPlan.fog=masked?maskPixels_:nullptr;nativeAreaPlan.mask=masked?gpuMaskPlan_:nullptr;nativeAreaPlan.effects=areaEffects;areaUsed=xbrzadapter::gpuArea(nativeAreaPlan,out->pixels,direct);}
   else areaUsed=view.presentGPU(width,height,out->pixels,spanX_,spanY_,masked?maskPixels_:nullptr,masked?gpuMaskPlan_:nullptr,areaEffects,direct);
   if(!areaUsed){if(nativeObjectsGpu&&!objectCanvas_.compose(nativeBase,objectDraws,view,false)){dynamicFailure_=true;return {};}
    if(!view.present(width,height,out->pixels,spanX_,spanY_)){dynamicFailure_=true;return {};}}

  }else{
   if(!detail)out->pixels=incrementalNative?*nativePresentation_.pixels:baseScene_->pixels;
   if(!(detail?detailed_.finish(out->pixels,true,masked?maskPixels_:nullptr,masked?gpuMaskPlan_:nullptr,maskReadback,directReady?&out->gpu:nullptr):region.active?view.presentRegion(width,height,out->pixels,region.x,region.y,region.width,region.height,pixelScaleMode):view.presentDirty(width,height,out->pixels,spanX_,spanY_))){dynamicFailure_=true;return {};}
  }
  if(!detail&&!region.active&&!sameNative&&!areaUsed){nativePresentation_.pixels=std::make_shared<const std::vector<uint32_t>>(out->pixels);
   nativePresentation_.background=detailKey_;nativePresentation_.objects=objectCanvas_.revision();nativePresentation_.gpu=areaGpuMode;nativePresentation_.used=areaUsed;
   nativePresentation_.mask=areaUsed&&masked?gpuMaskPlan_:nullptr;nativePresentation_.fog=areaUsed&&masked?maskPixels_:nullptr;
  }
  stageStats_.cpuTerrainReused=detail&&detailed_.cpuTerrainReused();stage(stageStats_.resolveMs,stageStats_.resolveReads);
  const bool gpuEffectsDone=effectPlanReady&&((gpuMode_&&detailed_.gpuCompleted())||(areaUsed&&areaEffects));out->boltsRendered=gpuEffectsDone&&features_.lasers&&frame.effects&&frame.effects->boltsValid;
  if(!out->gpu&&masked&&gpuMaskPlan_&&cpuMaskPlan_!=gpuMaskPlan_&&(!areaUsed||!fallbacks.empty()||worldEffects)){
   // CPU replay and missing-art dots need a native mask; a successful whole
   // minimap stays GPU-resident through its mask and returns only final RGBA.
   if((!maskReadback||!detail||!detailed_.gpuMasked())&&!gpushroud::reference(*gpuMaskPlan_,mask_.alpha))return {};
   if(mask_.alpha.size()!=size_t(width)*height)return {};cpuMaskPlan_=gpuMaskPlan_;
  }
  if(detail&&fallbacks.empty())out->guides=detailed_.guides();
  // Missing-art dots are presentation annotations, not minified game sprites.
  if(!gpuMode_||!detailed_.gpuCompleted())for(const auto*m:fallbacks){auto p=dot(*m);for(int yy=std::max(0,p.second-1);yy<std::min(height,p.second+2);++yy)for(int xx=std::max(0,p.first-1);xx<std::min(width,p.first+2);++xx){
   const size_t at=size_t(yy)*width+xx;uint32_t color=0xff000000u|(m->color&0xffffffu);
   if(masked&&(areaUsed||(detail&&detailed_.gpuMasked()))){unsigned alpha=mask_.alpha[at];if(alpha<127){uint32_t lit=0xff000000u;for(int shift:{0,8,16})lit|=((((color>>shift)&255u)*alpha+63)/127)<<shift;color=lit;}}
   out->pixels[at]=color;
  }}
  if(worldEffects&&!gpuEffectsDone){worldfx::Painter painter(out->pixels,effectView,detail&&detailed_.nativeScene()?detailed_.nativeAlpha().get():masked&&(areaUsed||(detail&&detailed_.gpuMasked()))?&mask_.alpha:nullptr,detail&&detailed_.nativeScene()?&detailed_.nativeDepth():nullptr);const auto&effects=*frame.effects->world;
   if(nativePages){auto sample=[&](int x,int y){return std::pair<int,int>{int(std::floor(effectView.x+(x+.5)*effectView.width/width)),int(std::floor(effectView.y+(y+.5)*effectView.height/height))};};painter.nativeSamples([&view,sample](int x,int y){auto p=sample(x,y);return unsigned(view.nativeDepth(p.first,p.second));},[&view,sample](int x,int y){auto p=sample(x,y);return view.nativeAlphaAt(p.first,p.second);});}
   if(features_.particles&&!nativePages&&(!detail||!detailed_.nativeScene()))for(const auto&alpha:effects.alphas)if(auto image=alphaAsset(alpha.image))painter.alpha(alpha,*image);painter.paint(effects,features_.particles,features_.lasers);
  }
  if(masked&&!nativePages&&!areaUsed&&(!detail||!detailed_.gpuMasked()))mask_.apply(out->pixels);
  if(bolts&&!gpuEffectsDone&&!region.active){nativefeedback::WorldView projection{-geo.offsetX*30,geo.offsetY*15,geo.rawWidth*30,geo.rawHeight*15,frame.frame};auto status=nativefeedback::effects(frame.effects->bolts,*out,projection,masked?&mask_.alpha:nullptr,projectionScale);if(!status.complete)return {};out->boltsRendered=true;}
  if(!beacons.empty()&&!gpuEffectsDone){sceneextras::RadarCPU sink(*out);for(const auto&b:beacons)sink.uiImage(b.draw.image,b.rgba,(b.draw.x-effectView.x)*width/effectView.width,(b.draw.y-effectView.y)*height/effectView.height,width/effectView.width,height/effectView.height);}
  if(radarExtras&&!gpuEffectsDone){sceneextras::RadarCPU sink(*out);sceneextras::radarDecorations(frame,width,height,read,assets,extraLayers_,sink);}
  if(areaGpuMode&&staticView){areaCachedGpu_=areaUsed;if(masked)maskedBaseScene_=out;else baseScene_=out;}
  if(reusePlan&&(detailed_.gpuCompleted()||cpuReusable))gpuFrameReuse_.completed(*reusePlan,out);else gpuFrameReuse_.reset();
  stage(stageStats_.effectsMs,stageStats_.effectReads);return out;
 }
};
}}
