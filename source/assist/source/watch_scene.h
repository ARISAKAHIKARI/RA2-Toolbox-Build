#pragma once
#include "../../external_reader/source/watch_slots.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include "../../external_reader/source/scene_bank.h"
#include "../../external_reader/source/scene_poses.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/material_motion.h"
#include "watch_read_view.h"
#include "watch_observation.h"
#include "watch_current_anchor.h"
#include <chrono>
#include "watch_presentation.h"
namespace watchscene {
// A tracking camera is a viewport of the common immutable world, rendered by
// the SAME scene compositor as the main view. Only camera lifetime/navigation
// queries remain local. Sampling is a fallback when no other producer is live.
template<class Reader,class Assets>class Renderer {
 using Read=ReadView<Reader>;
 Reader&reader_;Assets&assets_;Read read_;external::radar::Sampler<Read> sampler_;
 external::radar::SceneRenderer<Assets> preparer_;
 std::array<external::worldscene::Pipeline,3> scenes_;
 uint32_t controls_=0;bool haveControls_=false;int sourceFrame_=-1;bool reveal_=false;
 external::presentation::Profile profile_;Observation motion_;
 static double clock(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
 static constexpr int Width=external::watch::NativeViewWidth,Height=external::watch::NativeViewHeight;
 struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false;std::shared_ptr<const external::radar::Scene> scene;};
 std::array<View,3> views;external::watch::Track tracks[3];external::watch::Target automatic;
 bool autoSuppressed=false,revealMap=false,danger=false;int width=800,height=600;uint32_t process=0,epoch=0,sourceTick=0;
 bool sharedSource=false,gpuAttempted=false,gpuUsed=false;
 struct Timings {double sample=0,prepare=0,total=0;std::array<double,3>draw{};std::array<bool,3>attempted{},reused{};bool prepared=false,sampled=false;} timings;
 std::array<std::array<int,2>,3> outputSizes{{{{320,256}},{{320,256}},{{320,256}}}};
 Renderer(Reader&r,Assets&a):reader_(r),assets_(a),read_{&r},sampler_(read_){}
 void reset(){motion_.reset();preparer_.reset();sampler_.reset();sourceFrame_=-1;haveControls_=false;automatic={};autoSuppressed=false;for(auto&t:tracks)t={};for(auto&v:views)v={};for(auto&s:scenes_)s.reset();}
 void features(const external::presentation::Profile&p){if(p!=profile_){preparer_.reset();for(auto&s:scenes_)s.reset();for(auto&v:views)v.scene.reset();sourceFrame_=-1;profile_=p;}}
 void syncTracks(uint32_t revision,const std::array<external::watch::Track,3>&next,bool suppressed){if(haveControls_&&controls_==revision)return;std::copy(next.begin(),next.end(),tracks);autoSuppressed=suppressed;controls_=revision;haveControls_=true;}
 bool update(uint32_t now){
  timings={};const double started=clock();struct Total {double start;Timings&out;~Total(){out.total=watchpresentation::now()-start;if(!out.sampled)out.sample=out.total;}} total{started,timings};
  auto fresh=[this](uint32_t p,void*out,size_t n){return reader_.readFresh(p,out,n);};
  bool manual=false;for(auto&t:tracks){bool had=bool(t);external::watch::follow(fresh,t);if(had&&!t)autoSuppressed=true;manual|=bool(t);}
  if(!manual&&!autoSuppressed)automatic=external::watch::choose(read_,{},automatic,danger);else automatic={};
  // With automatic tracking explicitly off/suppressed and no pinned target,
  // there is no camera to draw. Do not sample/decode a world for empty panels.
  if(!manual&&(!danger||autoSuppressed)){for(auto&v:views)v={};sourceTick=now;sharedSource=gpuAttempted=gpuUsed=false;return true;}
  auto current=external::radar::publicationContext(fresh);auto bank=external::radar::sceneBank().get(process,epoch,revealMap,now,true);
  if(bank&&(!external::radar::recentMarkers(*bank,current,now)||!external::radar::supportsEffects(*bank,profile_.particles,profile_.lasers)))bank.reset();sharedSource=bool(bank);
  if(!bank){
   if(auto prior=external::radar::sceneBank().get(process,epoch,revealMap,now))sampler_.seedTerrain(*prior);
   sampler_.effects(profile_.particles,profile_.lasers);if(!sampler_.step(revealMap,16384,true))return false;
   auto sampled=sampler_.snapshot(now,current.frame);sampled.observedMs=clock();external::radar::preparePoses(sampled,read_);
   if(!sampled.markerScanValid||!external::radar::recentMarkers(sampled,external::radar::publicationContext(fresh),now))return false;
   bank=std::make_shared<const external::radar::Frame>(std::move(sampled));external::radar::sceneBank().publish(process,epoch,true,bank);
  }
  if(reveal_!=revealMap||bank->frame<sourceFrame_){motion_.reset();preparer_.reset();sourceFrame_=-1;for(auto&s:scenes_)s.reset();for(auto&v:views)v.scene.reset();}reveal_=revealMap;
  sourceFrame_=bank->frame;
  motion_.observe(bank,clock());auto data=motion_.sampleLatest(clock(),profile_.motion,profile_.detail!=0);if(!data.terrain)return false;
  std::array<external::watch::Anchor,3> centers;
  for(int i=0;i<3;++i){auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};if(!target)continue;
   auto prior=views[i].frame.center;if(tracks[i]&&tracks[i].located)prior={tracks[i].spatial,tracks[i].x,tracks[i].y,tracks[i].z,true,tracks[i].spatialKind,tracks[i].discoveryFrame,tracks[i].searched,tracks[i].discoveryCursor};
   bool readable=true;auto checked=[&](uint32_t p,void*out,size_t n){const bool ok=fresh(p,out,n);readable&=ok;return ok;};
   centers[i]=external::watch::anchor(checked,target,prior);if(!readable)centers[i].located=false;
  }
  if(!currentAnchors(data,centers,fresh,profile_.detail!=0))return false;
  sourceTick=bank->tick;data.tick=now;assets_.identity(process,epoch);
  int rasterW=1,rasterH=1;for(const auto&size:outputSizes){rasterW=std::max(rasterW,size[0]);rasterH=std::max(rasterH,size[1]);}
  external::xbrzadapter::gpuPresentation(profile_.gpu);external::presentation::apply(profile_,assets_,Width,Height,rasterW,rasterH);
  const double zscale=external::projectionZScale(fresh);
  timings.sample=clock()-started;timings.sampled=true;
  gpuAttempted=gpuUsed=false;std::shared_ptr<const external::worldscene::Snapshot> world;std::unique_ptr<external::worldscene::InstanceOrder> ordered;bool prepared=false;
  for(int i=0;i<3;++i){auto&v=views[i];auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};
   if(!(target==v.frame.focus))v={};v.shown=bool(target);if(!target)continue;
   auto center=centers[i];if(!center.located)continue;
   auto next=v.frame;
   next.focus=target;next.center=center;next.bounds=data.terrain->geo.bounds;next.zScale=zscale;next.reveal=revealMap;
   double cx=(int64_t(center.x)-center.y)*30./256,cy=(int64_t(center.x)+center.y)*15./256-external::screenHeight(center.z,zscale)+external::camera::anchorY(center.kind);
   if(!next.bounds.clampCamera(cx,cy,Width,Height))continue;next.cameraX=cx;next.cameraY=cy;next.cameraReady=true;
   next.passengers.clear();next.dangerous=false;external::passengers::contents(read_,target.pointer,next.passengers,next.dangerous);
   // Prepare the complete world once for all three cameras. Each camera
   // records the same pass sequence at its own current position; following a
   // fast target never translates pixels or invalidates native terrain pages.
   if(!prepared){prepared=true;timings.prepared=true;const double prepareAt=clock();preparer_.features(profile_);preparer_.guideEnabled(false,controls_);preparer_.prepareWorld(true);preparer_.outputSized(true);
    auto ready=preparer_.render(data,Width,Height,read_,assets_,{cx-Width/2+data.terrain->geo.offsetX*30.,cy-Height/2-data.terrain->geo.offsetY*15.,Width,Height,true},unsigned(profile_.scale),profile_.detail!=0,true,profile_.precision!=0,true,4.);
    if(ready)world=preparer_.preparedWorld();if(world)ordered=std::make_unique<external::worldscene::InstanceOrder>(world);timings.prepare=clock()-prepareAt;
   }
   external::mainview::View camera{0x10000,data.terrain->geo.house,0x10000,data.frame,int(cx)-Width/2,int(cy)-Height/2,Width,Height,0,0,Width,Height};
   // Camera span stays 640x512 game pixels. Only its physical raster follows
   // the largest visible host panel, shared by primary and secondary displays.
   const double drawAt=clock();timings.attempted[i]=bool(world);auto scene=world?scenes_[i].draw(world,camera,outputSizes[i][0],outputSizes[i][1],fresh,nullptr,0,ordered.get()):nullptr;timings.draw[i]=clock()-drawAt;timings.reused[i]=bool(scene)&&scenes_[i].reused();
   gpuAttempted|=external::xbrzadapter::gpuAttempted();gpuUsed|=external::xbrzadapter::gpuUsed();
   if(scene){next.cameraX=camera.cameraX+Width/2;next.cameraY=camera.cameraY+Height/2;v.scene=std::move(scene);v.frame=std::move(next);}
  }
  bool visible[3]{};external::watch::Box boxes[3]{};for(int i=0;i<3;++i)visible[i]=views[i].shown;external::watch::arrange(visible,width,height,boxes);for(int i=0;i<3;++i)views[i].box=boxes[i];
  return true;
 }
 void drawView(Graphics&g,const View&v){if(!v.shown||!v.scene||v.scene->pixels.empty())return;Bitmap bm(v.scene->width,v.scene->height,v.scene->width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(v.scene->pixels.data())));g.DrawImage(&bm,RectF(0,0,float(Width),float(Height)),0.f,0.f,float(v.scene->width),float(v.scene->height),UnitPixel,nullptr);
  Pen border(Color(255,v.frame.dangerous?255:105,v.frame.dangerous?167:195,v.frame.dangerous?62:225),6.f);g.DrawRectangle(&border,3.f,3.f,float(Width-6),float(Height-6));
 }
};
}
