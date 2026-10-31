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
class Pipeline {
 materialmotion::Timeline motion_;
 radar::Frame presented_;
 chronoscene::History<Snapshot> chrono_;
 camera::ViewportCanvas canvas_;
 radar::shroudmask::Live visibility_;
 radar::shroudmask::Snapshot visible_;
 gpushroud::Builder masks_;
 std::shared_ptr<const gpushroud::Plan> mask_;
 std::shared_ptr<const std::vector<vulkancompute::TileJob>> tiles_;
 std::shared_ptr<std::vector<uint32_t>> base_;
 std::shared_ptr<const Snapshot> world_;
 mainview::View view_{};
 int width_=0,height_=0;
 camera::FrameReuse<radar::Scene> reuse_;
 asyncwork::FramePool<radar::Scene> outputs_;
public:
 const radar::Frame&presentedState()const{return presented_;}
 void reset(){motion_.reset();presented_={};chrono_.reset();canvas_.reset();visibility_.reset();masks_.reset();mask_.reset();tiles_.reset();base_.reset();world_.reset();visible_={};view_={};width_=height_=0;reuse_.reset();outputs_.clear();}
 template<class Read>std::shared_ptr<const radar::Scene> draw(std::shared_ptr<const Snapshot>world,const mainview::View&inputView,int width,int height,Read read,const radar::Frame*latest=nullptr){
  auto view=inputView;
  if(!world||!world->state.terrain||!view.valid()||width<1||height<1)return {};
  chrono_.select(world,view,width,height,read);
  const auto&geo=world->state.terrain->geo;
  if(geo.house!=view.house)return {};
  presented_=world->state;
  const bool live=!chrono_.active()&&latest&&latest->terrain&&latest->markerScanValid&&latest->frame>=world->state.frame&&latest->terrain->geo.same(geo)&&latest->terrain->reveal==world->state.terrain->reveal;
  if(live){motion_.observe(*latest,materialmotion::now());presented_=motion_.sampleLatest(materialmotion::now(),world->profile.motion,false);}
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
  // This lane does no asset reads/decoding or VXL geometry reconstruction.
  using Key=std::pair<uint32_t,uint32_t>;std::map<Key,const radar::Marker*> old,current;
  if(live){for(const auto&m:world->state.markers)old[{m.object,m.id}]=&m;for(const auto&m:presented_.markers)current[{m.object,m.id}]=&m;}
  auto instances=world->instances;
  if(live){instances.erase(std::remove_if(instances.begin(),instances.end(),[&](auto&item){
    if(!item.object)return false;const Key key{item.object,item.id};auto a=old.find(key),b=current.find(key);
    if(a==old.end()||b==current.end()||!materialmotion::identity(*a->second,*b->second))return true;
    const auto&p=*a->second;const auto&q=*b->second;
    const double sum=(double(q.x)+q.y)-(double(p.x)+p.y),height=screenHeight(q.z,world->zscale)-screenHeight(p.z,world->zscale);
    const float dx=float(((double(q.x)-q.y)-(double(p.x)-p.y))*30/256.),dy=float(sum*15/256.-height);
    item.x+=dx;item.y+=dy;item.base+=float(sum*15/256.);item.drop=std::max(0.f,item.drop+float(height));
    if(item.material.clipBottom<1e7f)item.material.clipBottom-=dy;return false;
   }),instances.end());
   // Preserve fixed ground/trail passes and the order of an actor's parts.
   for(size_t first=0;first<instances.size();){if(!instances[first].object){++first;continue;}size_t last=first+1;while(last<instances.size()&&instances[last].object)++last;
    std::stable_sort(instances.begin()+first,instances.begin()+last,[&](const auto&a,const auto&b){const auto&x=*current.at({a.object,a.id});const auto&y=*current.at({b.object,b.id});return camera::orderKey(x.layer,x.kind,x.x,x.y,x.sort)<camera::orderKey(y.layer,y.kind,y.x,y.y,y.sort);});first=last;
   }
  }
  auto missing=world->missing;if(live){missing.erase(std::remove_if(missing.begin(),missing.end(),[&](auto&m){auto found=current.find({m.object,m.id});if(found==current.end()||!materialmotion::identity(m,*found->second))return true;m=*found->second;return false;}),missing.end());}
  for(const auto&item:instances)if(item.image&&canvas_.visible(*item.image,item.x,item.y,item.drop))canvas_.blit(*item.image,item.x,item.y,1,1,item.base,item.light,item.lift,item.drop,item.image,{},item.material);
  for(const auto&m:missing){const int x=int(((double(m.x)-m.y)*30/256.-view.cameraX)*width/view.width),y=int(((double(m.x)+m.y)*15/256.-view.cameraY)*height/view.height);canvas_.annotationDot(x,y,m.color);}
  worldfx::View effectView;effectView.x=crop.x;effectView.y=crop.y;effectView.width=crop.width;effectView.height=crop.height;effectView.pixelsW=width;effectView.pixelsH=height;effectView.offsetX=geo.offsetX;effectView.offsetY=geo.offsetY;effectView.zscale=world->zscale;
  const auto effects=presented_.effects;const bool hasWorld=effects&&effects->world&&!effects->world->empty();
  const bool bolts=world->profile.lasers&&effects&&effects->boltsValid&&!effects->bolts.empty();
  gpueffects::Builder builder(effectView);
  if(hasWorld){if(world->profile.particles)for(const auto&a:effects->world->alphas){auto image=world->alphas.find(a.image);if(image!=world->alphas.end())builder.alpha(a,image->second);}builder.paint(*effects->world,world->profile.particles,world->profile.lasers);}
  if(bolts)builder.bolts(effects->bolts,world->state.frame);
  for(const auto&b:world->beacons)builder.uiImage(b.draw.image,b.rgba,(b.draw.x-crop.x)*width/crop.width,(b.draw.y-crop.y)*height/crop.height,width/crop.width,height/crop.height);
  const bool sparkles=world->profile.particles&&world->state.extras&&!world->state.extras->sparkles.empty();
  if(sparkles)for(const auto&s:world->state.extras->sparkles)builder.sparkle(s,world->precision==colorprecision::Mode::SmoothRGB);
  if(chrono_.active())builder.chrono(chrono_.state(),chrono_.table(),view.width,view.height);
  auto plan=builder.finish();canvas_.gpuEffects((hasWorld||bolts||sparkles||chrono_.active()||!world->beacons.empty())?plan:nullptr);
  const auto*inputs=plan->valid?canvas_.completeGpuPlan({},mask_):nullptr;
  world_=world;view_=view;width_=width;height_=height;
  if(inputs)if(auto prior=reuse_.get(*inputs)){if(!prior->gpu||gpupresent::available()){xbrzadapter::gpuState().used=true;return prior;}}
  auto out=outputs_.acquire();out->width=width;out->height=height;out->gpu.reset();out->guides.reset();out->boltsRendered=false;
  if(!canvas_.finish(out->pixels,true,{},mask_,nullptr,&out->gpu))return {};
  if(canvas_.gpuCompleted())out->boltsRendered=world->profile.lasers&&effects&&effects->boltsValid;
  else{
   std::vector<uint8_t> alpha;if(mask_&&!gpushroud::reference(*mask_,alpha))return {};
   for(const auto&m:missing){const int x=int(((double(m.x)-m.y)*30/256.-view.cameraX)*width/view.width),y=int(((double(m.x)+m.y)*15/256.-view.cameraY)*height/view.height);
    for(int yy=std::max(0,y-1);yy<std::min(height,y+2);++yy)for(int xx=std::max(0,x-1);xx<std::min(width,x+2);++xx)out->pixels[size_t(yy)*width+xx]=0xff000000u|(m.color&0xffffffu);
   }
   if(hasWorld){worldfx::Painter painter(out->pixels,effectView);if(world->profile.particles)for(const auto&a:effects->world->alphas){auto image=world->alphas.find(a.image);if(image!=world->alphas.end())painter.alpha(a,*image->second);}painter.paint(*effects->world,world->profile.particles,world->profile.lasers);}
   if(mask_)for(size_t n=0;n<out->pixels.size();++n){auto c=out->pixels[n];uint32_t color=c&0xff000000u;for(int shift:{0,8,16})color|=((((c>>shift)&255u)*alpha[n]+63u)/127u)<<shift;out->pixels[n]=color;}
   if(bolts){auto status=nativefeedback::effects(effects->bolts,*out,view,mask_?&alpha:nullptr,world->zscale);if(!status.complete)return {};out->boltsRendered=true;}
  }
  if(!canvas_.gpuCompleted()){
   sceneextras::RadarCPU sink(*out);for(const auto&b:world->beacons)sink.uiImage(b.draw.image,b.rgba,(b.draw.x-crop.x)*width/crop.width,(b.draw.y-crop.y)*height/crop.height,width/crop.width,height/crop.height);
   if(sparkles)for(const auto&s:world->state.extras->sparkles)chronoscene::sparkle(s,effectView,out->pixels,world->precision==colorprecision::Mode::SmoothRGB);
   chrono_.apply(out->pixels,width,height,view.width,view.height);
  }
  if(inputs&&canvas_.gpuCompleted())reuse_.completed(*inputs,out);else reuse_.reset();
  return out;
 }
};
}}
