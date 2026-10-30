#pragma once
#include "radar_map.h"
#include "native_canvas.h"
#include "object_canvas.h"
#include "viewport_canvas.h"
#include "radar_live_shroud.h"
#include "scene_frame_reuse.h"
#include "gpu_effect_plan.h"
#include "feedback_scene.h"
#include "render_profile.h"
#include "../../shared/source/frame_pool.h"
#include <map>
namespace external { namespace worldscene {
// GPL-3.0. Presentation-stage adaptation of Vera20K 3d42426:
// app/presentation/render/{mod,build_instances,draw_passes}.rs and render/batch.rs.
// Retained world inputs precede camera-dependent drawing. The camera is an
// input to every pass, never a translation of a previously rendered picture.
// The existing external reader supplies immutable state instead of Vera's sim.
struct Snapshot {
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
 void reset(){canvas_.reset();visibility_.reset();masks_.reset();mask_.reset();tiles_.reset();base_.reset();world_.reset();visible_={};view_={};width_=height_=0;reuse_.reset();outputs_.clear();}
 template<class Read>std::shared_ptr<const radar::Scene> draw(std::shared_ptr<const Snapshot>world,const mainview::View&view,int width,int height,Read read){
  if(!world||!world->state.terrain||!view.valid()||width<1||height<1)return {};
  const auto&geo=world->state.terrain->geo;
  if(geo.house!=view.house)return {};
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
  if(world->masked){
   radar::shroudmask::Snapshot now;if(!visibility_.capture(read,geo,crop.x,crop.y,crop.width,crop.height,now))return {};
   if(cameraChanged||groundChanged||!mask_||!visible_.sameMask(now)){
    if(!masks_.build(geo,now.cells,width,height,crop.width,crop.height,now.fog,[&](bool fog,int index,voxel::Image&out){
      if(index<0||index>=47||!world->shroud[fog][index])return false;out=*world->shroud[fog][index];return true;
     },mask_,crop.x,crop.y))return {};
   }
   visible_=std::move(now);
  }else{mask_.reset();visible_={};}
  for(const auto&item:world->instances)if(item.image&&canvas_.visible(*item.image,item.x,item.y,item.drop))canvas_.blit(*item.image,item.x,item.y,1,1,item.base,item.light,item.lift,item.drop,item.image);
  for(const auto&m:world->missing){const int x=int(((double(m.x)-m.y)*30/256.-view.cameraX)*width/view.width),y=int(((double(m.x)+m.y)*15/256.-view.cameraY)*height/view.height);canvas_.annotationDot(x,y,m.color);}
  worldfx::View effectView;effectView.x=crop.x;effectView.y=crop.y;effectView.width=crop.width;effectView.height=crop.height;effectView.pixelsW=width;effectView.pixelsH=height;effectView.offsetX=geo.offsetX;effectView.offsetY=geo.offsetY;effectView.zscale=world->zscale;
  const auto effects=world->state.effects;const bool hasWorld=effects&&effects->world&&!effects->world->empty();
  const bool bolts=world->profile.lasers&&effects&&effects->boltsValid&&!effects->bolts.empty();
  gpueffects::Builder builder(effectView);
  if(hasWorld){if(world->profile.particles)for(const auto&a:effects->world->alphas){auto image=world->alphas.find(a.image);if(image!=world->alphas.end())builder.alpha(a,image->second);}builder.paint(*effects->world,world->profile.particles,world->profile.lasers);}
  if(bolts)builder.bolts(effects->bolts,world->state.frame);
  auto plan=builder.finish();canvas_.gpuEffects((hasWorld||bolts)?plan:nullptr);
  const auto*inputs=!hasWorld&&!bolts?canvas_.completeGpuPlan({},mask_):nullptr;
  world_=world;view_=view;width_=width;height_=height;
  if(inputs)if(auto prior=reuse_.get(*inputs)){if(!prior->gpu||gpupresent::available()){xbrzadapter::gpuState().used=true;return prior;}}
  auto out=outputs_.acquire();out->width=width;out->height=height;out->gpu.reset();out->guides.reset();out->boltsRendered=false;
  if(!canvas_.finish(out->pixels,true,{},mask_,nullptr,&out->gpu))return {};
  if(canvas_.gpuCompleted())out->boltsRendered=world->profile.lasers&&effects&&effects->boltsValid;
  else{
   std::vector<uint8_t> alpha;if(mask_&&!gpushroud::reference(*mask_,alpha))return {};
   for(const auto&m:world->missing){const int x=int(((double(m.x)-m.y)*30/256.-view.cameraX)*width/view.width),y=int(((double(m.x)+m.y)*15/256.-view.cameraY)*height/view.height);
    for(int yy=std::max(0,y-1);yy<std::min(height,y+2);++yy)for(int xx=std::max(0,x-1);xx<std::min(width,x+2);++xx)out->pixels[size_t(yy)*width+xx]=0xff000000u|(m.color&0xffffffu);
   }
   if(hasWorld){worldfx::Painter painter(out->pixels,effectView);if(world->profile.particles)for(const auto&a:effects->world->alphas){auto image=world->alphas.find(a.image);if(image!=world->alphas.end())painter.alpha(a,*image->second);}painter.paint(*effects->world,world->profile.particles,world->profile.lasers);}
   if(mask_)for(size_t n=0;n<out->pixels.size();++n){auto c=out->pixels[n];uint32_t color=c&0xff000000u;for(int shift:{0,8,16})color|=((((c>>shift)&255u)*alpha[n]+63u)/127u)<<shift;out->pixels[n]=color;}
   if(bolts){auto status=nativefeedback::effects(effects->bolts,*out,view,mask_?&alpha:nullptr,world->zscale);if(!status.complete)return {};out->boltsRendered=true;}
  }
  if(inputs&&canvas_.gpuCompleted())reuse_.completed(*inputs,out);else reuse_.reset();
  return out;
 }
};
}}
