#pragma once
#include "radar_map.h"
#include "native_canvas.h"
#include "object_canvas.h"
#include "viewport_canvas.h"
#include "radar_live_shroud.h"
#include "scene_frame_reuse.h"
#include "gpu_effect_plan.h"
#include "feedback_scene.h"
#include "chrono_scene.h"
#include "radar_decoration.h"
#include "render_profile.h"
#include "material_motion.h"
#include "native_radial_indicators.h"
#include "scene_order.h"
#include "../../shared/source/frame_pool.h"
#include <map>
namespace external { namespace worldscene {
// GPL-3.0. Presentation-stage adaptation of Vera20K 3d42426:
// app/presentation/render/{mod,build_instances,draw_passes}.rs and render/batch.rs.
// Retained world inputs precede camera-dependent drawing. The camera is an
// input to every pass, never a translation of a previously rendered picture.
// The existing external reader supplies immutable state instead of Vera's sim.
struct ScreenSprite {camera::ObjectCanvas::Draw draw;std::shared_ptr<const std::vector<uint8_t>>rgba;};
struct Snapshot {
 std::vector<ScreenSprite> beacons;
 camera::NativeCanvas ground;
 radar::Frame state;
 std::vector<camera::ObjectCanvas::Draw> instances;
 std::vector<radar::Marker> missing;
 std::map<std::pair<uint32_t,uint32_t>,std::array<int,4>> bounds;
 std::array<std::array<std::shared_ptr<const voxel::Image>,47>,2> shroud{};
 std::map<uint32_t,std::shared_ptr<const voxel::Image>> alphas;
 presentation::Profile profile;
 unsigned samplingMode=0;
 colorprecision::Mode precision=colorprecision::Mode::NativeSteps;
 uint64_t terrainRevision=0;
 double zscale=.5;
 bool masked=false;
 bool feedbackBounds(const radar::Marker&m,int&x,int&y,int&w,int&h)const{
  auto found=bounds.find({m.object,m.id});if(found==bounds.end())return false;
  const auto&b=found->second;x=b[0];y=b[1];w=b[2];h=b[3];return w>0&&h>0;
 }
};
// Camera-independent actor ordering belongs to the immutable world. Keep its
// owner, not a frame-number key: chrono can select a different retained world.
class InstanceOrder {
 std::shared_ptr<const Snapshot> owner_;
 std::vector<camera::ObjectCanvas::Draw> draws_;
public:
 explicit InstanceOrder(std::shared_ptr<const Snapshot> world):owner_(std::move(world)){
  if(owner_){draws_=owner_->instances;tetherdraw::replay(draws_,owner_->state.markers);}
 }
 const std::vector<camera::ObjectCanvas::Draw>*forWorld(const std::shared_ptr<const Snapshot>&world)const{return owner_==world?&draws_:nullptr;}
};
class Pipeline {
 materialmotion::Timeline motion_;
 radar::Frame presented_;
 std::shared_ptr<const Snapshot> presentedWorld_;
 chronoscene::History<Snapshot> chrono_;
 camera::ViewportCanvas canvas_;
 radar::shroudmask::Live visibility_;
 radar::shroudmask::Snapshot visible_;
 std::shared_ptr<const nativefx::Math> radialMath_;
 gpushroud::Builder masks_;
 std::shared_ptr<const gpushroud::Plan> mask_;
 std::shared_ptr<const std::vector<vulkancompute::TileJob>> tiles_;
 std::shared_ptr<std::vector<uint32_t>> base_;
 std::shared_ptr<const Snapshot> world_;
 mainview::View view_{};
 int width_=0,height_=0;
 camera::FrameReuse<radar::Scene> reuse_;
 asyncwork::FramePool<radar::Scene> outputs_;
 std::array<double,4>timings_{};bool reused_=false;
public:
 const radar::Frame&presentedState()const{return presentedWorld_?presentedWorld_->state:presented_;}
 const std::array<double,4>&timings()const{return timings_;}bool reused()const{return reused_;}
 void reset(){motion_.reset();presented_={};presentedWorld_.reset();chrono_.reset();canvas_.reset();visibility_.reset();masks_.reset();mask_.reset();tiles_.reset();base_.reset();world_.reset();visible_={};radialMath_.reset();view_={};width_=height_=0;reuse_.reset();outputs_.clear();}
 template<class Read>std::shared_ptr<const radar::Scene> draw(std::shared_ptr<const Snapshot>world,const mainview::View&inputView,int width,int height,Read read,const radar::Frame*latest=nullptr,uint32_t uiTick=0,const InstanceOrder*ordered=nullptr){
  timings_={};reused_=false;auto stageAt=std::chrono::steady_clock::now();auto stage=[&](unsigned n){auto now=std::chrono::steady_clock::now();timings_[n]=std::chrono::duration<double,std::milli>(now-stageAt).count();stageAt=now;};
  auto view=inputView;
  if(!world||!world->state.terrain||!view.valid()||width<1||height<1)return {};
  chrono_.select(world,view,width,height,read);
  const auto&geo=world->state.terrain->geo;
  if(geo.house!=view.house)return {};
  const bool live=!chrono_.active()&&latest&&latest->terrain&&latest->markerScanValid&&latest->frame>=world->state.frame&&latest->terrain->geo.same(geo)&&latest->terrain->reveal==world->state.terrain->reveal;
  if(live){motion_.observe(*latest,materialmotion::now());presented_=motion_.sampleLatest(materialmotion::now(),world->profile.motion,false);presentedWorld_.reset();}
  else presentedWorld_=world;
  const auto&presented=presentedState();
  const auto crop=mainview::crop(view,geo.offsetX,geo.offsetY);
  const bool cameraChanged=!mainview::sameViewport(view_,view)||width_!=width||height_!=height;
  const bool groundChanged=!world_||world_->terrainRevision!=world->terrainRevision||!world_->state.terrain->geo.same(geo)||world_->profile!=world->profile||world_->samplingMode!=world->samplingMode||world_->precision!=world->precision;
  if(cameraChanged||groundChanged){
   tiles_.reset();base_=std::make_shared<std::vector<uint32_t>>();std::vector<vulkancompute::TileJob> jobs;
   if(world->ground.gpuRegion(width,height,crop.x,crop.y,crop.width,crop.height,world->samplingMode,jobs)&&!jobs.empty())tiles_=std::make_shared<const std::vector<vulkancompute::TileJob>>(std::move(jobs));
  }
  auto fallback=[world,width,height,crop,cache=base_](std::vector<uint32_t>&pixels){
   if(cache->size()==size_t(width)*height){if(&pixels!=cache.get())pixels=*cache;return true;}
   auto&gpu=xbrzadapter::gpuState();struct Restore{bool&flag;bool prior;~Restore(){flag=prior;}}restore{gpu.requested,gpu.requested};gpu.requested=false;
   if(!world->ground.presentRegion(width,height,*cache,crop.x,crop.y,crop.width,crop.height,world->samplingMode))return false;
   if(&pixels!=cache.get())pixels=*cache;return true;
  };
  if(!tiles_&&!fallback(*base_))return {};
  if(!canvas_.prepare(world->ground,*base_,width,height,crop.x,crop.y,crop.width,crop.height,world->terrainRevision,
    world->precision,world->samplingMode,base_,tiles_,fallback,world->terrainRevision+1))return {};
  if(world->masked&&!(chrono_.active()&&mask_&&!cameraChanged&&!groundChanged)){
   radar::shroudmask::Snapshot now;if(!visibility_.capture(read,geo,crop.x,crop.y,crop.width,crop.height,now))return {};
   if(cameraChanged||groundChanged||!mask_||!visible_.sameMask(now)){
    if(!masks_.build(geo,now.cells,width,height,crop.width,crop.height,now.fog,[&](bool fog,int index,voxel::Image&out){
      if(index<0||index>=47||!world->shroud[fog][index])return false;out=*world->shroud[fog][index];return true;
     },mask_,crop.x,crop.y))return {};
   }
   visible_=std::move(now);
  }else if(!world->masked){mask_.reset();visible_={};}
  // Translate retained instances, including their shadows and depth anchors.
  stage(0);
  // This lane does no asset reads/decoding or VXL geometry reconstruction.
  using Key=std::pair<uint32_t,uint32_t>;std::map<Key,const radar::Marker*> old,current;
  if(live){for(const auto&m:world->state.markers)old[{m.object,m.id}]=&m;for(const auto&m:presented.markers)current[{m.object,m.id}]=&m;}
  const auto*sharedOrder=!live&&ordered?ordered->forWorld(world):nullptr;
  std::vector<camera::ObjectCanvas::Draw> mutableInstances;
  if(!sharedOrder)mutableInstances=world->instances;
  if(live){mutableInstances.erase(std::remove_if(mutableInstances.begin(),mutableInstances.end(),[&](auto&item){
    if(!item.object)return false;const Key key{item.object,item.id};auto a=old.find(key),b=current.find(key);
    if(a==old.end()||b==current.end()||!materialmotion::identity(*a->second,*b->second))return true;
    const auto&p=*a->second;const auto&q=*b->second;
    const double sum=(double(q.x)+q.y)-(double(p.x)+p.y),height=screenHeight(q.z,world->zscale)-screenHeight(p.z,world->zscale);
    const float dx=float(((double(q.x)-q.y)-(double(p.x)-p.y))*30/256.),dy=float(sum*15/256.-height);
    item.x+=dx;item.y+=dy;item.base+=float(sum*15/256.);item.drop=std::max(0.f,item.drop+float(height));
    auto material=q.material;material.mixStrength=item.material.mixStrength;material.clipBottom=q.waterline>0?float(q.waterline-geo.offsetY*15)-item.y:1e7f;item.material=material;return false;
   }),mutableInstances.end());
  }
  if(!sharedOrder)tetherdraw::replay(mutableInstances,presented.markers);
  const auto&instances=sharedOrder?*sharedOrder:mutableInstances;
  std::vector<radar::Marker> mutableMissing;
  if(live){mutableMissing=world->missing;mutableMissing.erase(std::remove_if(mutableMissing.begin(),mutableMissing.end(),[&](auto&m){auto found=current.find({m.object,m.id});if(found==current.end()||!materialmotion::identity(m,*found->second))return true;m=*found->second;return false;}),mutableMissing.end());}
  const auto&missing=live?mutableMissing:world->missing;
  for(const auto&item:instances)if(item.image&&canvas_.visible(*item.image,item.x,item.y,item.drop))canvas_.blit(*item.image,item.x,item.y,1,1,item.base,item.light,item.lift,item.drop,item.image,{},item.material);
  for(const auto&m:missing){const int x=int(((double(m.x)-m.y)*30/256.-view.cameraX)*width/view.width),y=int(((double(m.x)+m.y)*15/256.-view.cameraY)*height/view.height);canvas_.annotationDot(x,y,m.color);}
  worldfx::View effectView;effectView.x=crop.x;effectView.y=crop.y;effectView.width=crop.width;effectView.height=crop.height;effectView.pixelsW=width;effectView.pixelsH=height;effectView.offsetX=geo.offsetX;effectView.offsetY=geo.offsetY;effectView.zscale=world->zscale;
  const auto effects=presented.effects;const bool hasWorld=effects&&effects->world&&!effects->world->empty();
  const bool bolts=world->profile.lasers&&effects&&effects->boltsValid&&!effects->bolts.empty();
  gpueffects::Builder builder(effectView);
  if(hasWorld){if(world->profile.particles)for(const auto&a:effects->world->alphas){auto image=world->alphas.find(a.image);if(image!=world->alphas.end())builder.alpha(a,image->second);}builder.paint(*effects->world,world->profile.particles,world->profile.lasers);}
  if(bolts)builder.bolts(effects->bolts,world->state.frame);
  for(const auto&b:world->beacons)builder.uiImage(b.draw.image,b.rgba,(b.draw.x-crop.x)*width/crop.width,(b.draw.y-crop.y)*height/crop.height,width/crop.width,height/crop.height);
  const bool sparkles=world->profile.particles&&world->state.extras&&!world->state.extras->sparkles.empty();
  if(sparkles)for(const auto&s:world->state.extras->sparkles)builder.sparkle(s,world->precision==colorprecision::Mode::SmoothRGB);
  if(chrono_.active())builder.chrono(chrono_.state(),chrono_.table(),view.width,view.height);
  // Main-camera local ranges share destination pixels with the world. A
  // rejected producer leaves the world transaction intact, never stale rays.
  std::vector<nativefeedback::Object>radialObjects;for(const auto&m:presented.markers)if(m.kind==3)radialObjects.push_back({m.object,m.id,m.type,m.owner,{},m.kind});
  std::vector<radialindicators::Draw>radials;std::array<int,6>pixelFormat{};const bool radial555=read(0x8A0DD0,pixelFormat.data(),sizeof pixelFormat)&&pixelFormat==std::array<int,6>{{10,3,0,3,5,3}};
  if(chrono_.active()||!radialindicators::capture(read,view,radialObjects,uiTick,radials))radials.clear();
  if(!radials.empty()&&!radialMath_)radialMath_=nativefx::math(read);if(!radialMath_)radials.clear();
  const auto radialMath=radialMath_;
  if(!radials.empty())radialindicators::geometry(radials,view,world->zscale,*radialMath,builder,width,height,radial555);
  auto plan=builder.finish();canvas_.gpuEffects((hasWorld||bolts||sparkles||chrono_.active()||!world->beacons.empty()||!radials.empty())?plan:nullptr);
  const auto*inputs=plan->valid?canvas_.completeGpuPlan({},mask_):nullptr;
  world_=world;view_=view;width_=width;height_=height;
  stage(1);if(inputs)if(auto prior=reuse_.get(*inputs)){if(!prior->gpu||gpupresent::available()){xbrzadapter::gpuState().used=bool(prior->gpu);reused_=true;return prior;}}
  auto out=outputs_.acquire();out->width=width;out->height=height;out->gpu.reset();out->guides.reset();out->boltsRendered=false;
  if(!canvas_.finish(out->pixels,true,{},mask_,nullptr,&out->gpu))return {};
  stage(2);
  if(canvas_.gpuCompleted())out->boltsRendered=world->profile.lasers&&effects&&effects->boltsValid;
  else{
   std::vector<uint8_t> alpha;if(mask_&&!gpushroud::reference(*mask_,alpha))return {};
   for(const auto&m:missing){const int x=int(((double(m.x)-m.y)*30/256.-view.cameraX)*width/view.width),y=int(((double(m.x)+m.y)*15/256.-view.cameraY)*height/view.height);
    for(int yy=std::max(0,y-1);yy<std::min(height,y+2);++yy)for(int xx=std::max(0,x-1);xx<std::min(width,x+2);++xx)out->pixels[size_t(yy)*width+xx]=0xff000000u|(m.color&0xffffffu);
   }
   if(hasWorld){worldfx::Painter painter(out->pixels,effectView,canvas_.nativeScene()?canvas_.nativeAlpha().get():nullptr,canvas_.nativeScene()?&canvas_.nativeDepth():nullptr);if(world->profile.particles&&!canvas_.nativeScene())for(const auto&a:effects->world->alphas){auto image=world->alphas.find(a.image);if(image!=world->alphas.end())painter.alpha(a,*image->second);}painter.paint(*effects->world,world->profile.particles,world->profile.lasers);}
   if(mask_&&!canvas_.nativeScene())for(size_t n=0;n<out->pixels.size();++n){auto c=out->pixels[n];uint32_t color=c&0xff000000u;for(int shift:{0,8,16})color|=((((c>>shift)&255u)*alpha[n]+63u)/127u)<<shift;out->pixels[n]=color;}
   if(bolts){auto status=nativefeedback::effects(effects->bolts,*out,view,mask_?&alpha:nullptr,world->zscale,0,0,0,0,canvas_.nativeScene()?&canvas_.nativeDepth():nullptr,canvas_.nativeScene()?canvas_.nativeAlpha().get():nullptr);if(!status.complete)return {};out->boltsRendered=true;}

  }
  if(!canvas_.gpuCompleted()){
   sceneextras::RadarCPU sink(*out);for(const auto&b:world->beacons)sink.uiImage(b.draw.image,b.rgba,(b.draw.x-crop.x)*width/crop.width,(b.draw.y-crop.y)*height/crop.height,width/crop.width,height/crop.height);
   if(sparkles)for(const auto&s:world->state.extras->sparkles)chronoscene::sparkle(s,effectView,out->pixels,world->precision==colorprecision::Mode::SmoothRGB);
   chrono_.apply(out->pixels,width,height,view.width,view.height);
   if(!radials.empty()){worldfx::Painter painter(out->pixels,effectView,canvas_.nativeScene()?canvas_.nativeAlpha().get():nullptr,canvas_.nativeScene()?&canvas_.nativeDepth():nullptr);radialindicators::geometry(radials,view,world->zscale,*radialMath,painter,width,height,radial555);}
  }
  // Reuse a completed CPU frame too, only after comparing the full current
  // ordered draw/material/Z/A/effect inputs. Game-frame equality is not a key.
  stage(3);if(inputs)reuse_.completed(*inputs,out);else reuse_.reset();
  return out;
 }
};
}}
