#pragma once
#include "../../external_reader/source/watch_slots.h"
#include "../../external_reader/source/radar_scene.h"
#include "../../external_reader/source/radar_publication.h"
#include "../../external_reader/source/scene_bank.h"
#include "../../external_reader/source/scene_poses.h"
#include "../../external_reader/source/feedback_scene.h"
#include "../../external_reader/source/material_motion.h"
#include <functional>
#include <chrono>
namespace watchscene {
// A tracking camera is a viewport of the common immutable world, rendered by
// the SAME scene compositor as the main view. Only camera lifetime/navigation
// queries remain local. Sampling is a fallback when no other producer is live.
template<class Reader,class Assets>class Renderer {
 using Read=std::function<bool(uint32_t,void*,size_t)>;
 Reader&reader_;Assets&assets_;Read read_;external::radar::Sampler<Read> sampler_;
 std::array<external::radar::SceneRenderer<Assets>,3> scenes_;
 external::materialmotion::Timeline timeline_;
 uint32_t controls_=0;bool haveControls_=false;int sourceFrame_=-1;bool reveal_=false;
 external::presentation::Profile profile_;
 static double clock(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
public:
 static constexpr int Width=external::watch::NativeViewWidth,Height=external::watch::NativeViewHeight;
 struct View {external::watch::Frame frame;external::watch::Box box;bool shown=false;std::shared_ptr<const external::radar::Scene> scene;};
 std::array<View,3> views;external::watch::Track tracks[3];external::watch::Target automatic;
 bool autoSuppressed=false,revealMap=false,danger=false;int width=800,height=600;uint32_t process=0,epoch=0,sourceTick=0;
 bool sharedSource=false,gpuAttempted=false,gpuUsed=false;
 Renderer(Reader&r,Assets&a):reader_(r),assets_(a),read_([&r](uint32_t p,void*out,size_t n){return r.read(p,out,n);}),sampler_(read_){}
 void reset(){sampler_.reset();timeline_.reset();sourceFrame_=-1;haveControls_=false;automatic={};autoSuppressed=false;for(auto&t:tracks)t={};for(auto&v:views)v={};for(auto&s:scenes_)s.reset();}
 void features(const external::presentation::Profile&p){if(p!=profile_){for(auto&s:scenes_)s.reset();for(auto&v:views)v.scene.reset();timeline_.reset();sourceFrame_=-1;profile_=p;}}
 void syncTracks(uint32_t revision,const std::array<external::watch::Track,3>&next,bool suppressed){if(haveControls_&&controls_==revision)return;std::copy(next.begin(),next.end(),tracks);autoSuppressed=suppressed;controls_=revision;haveControls_=true;}
 bool update(uint32_t now){
  auto fresh=[this](uint32_t p,void*out,size_t n){return reader_.readFresh(p,out,n);};
  bool manual=false;for(auto&t:tracks){bool had=bool(t);external::watch::follow(fresh,t);if(had&&!t)autoSuppressed=true;manual|=bool(t);}
  if(!manual&&!autoSuppressed)automatic=external::watch::choose(read_,{},automatic,danger);else automatic={};
  auto current=external::radar::publicationContext(fresh);auto bank=external::radar::sceneBank().get(process,epoch,revealMap,now,true);
  if(bank&&(!external::radar::recentMarkers(*bank,current,now)||!external::radar::supportsEffects(*bank,profile_.particles,profile_.lasers)))bank.reset();sharedSource=bool(bank);
  if(!bank){
   if(auto prior=external::radar::sceneBank().get(process,epoch,revealMap,now))sampler_.seedTerrain(*prior);
   sampler_.effects(profile_.particles,profile_.lasers);if(!sampler_.step(revealMap,16384,true))return false;
   auto sampled=sampler_.snapshot(now,current.frame);external::radar::preparePoses(sampled,read_);
   if(!sampled.markerScanValid||!external::radar::freshMarkers(sampled,external::radar::publicationContext(fresh)))return false;
   bank=std::make_shared<const external::radar::Frame>(std::move(sampled));external::radar::sceneBank().publish(process,epoch,true,bank);
  }
  if(reveal_!=revealMap||bank->frame<sourceFrame_){timeline_.reset();sourceFrame_=-1;for(auto&s:scenes_)s.reset();for(auto&v:views)v.scene.reset();}reveal_=revealMap;
  timeline_.observe(*bank,clock()-uint32_t(now-bank->tick));sourceFrame_=bank->frame;
  auto data=timeline_.sample(clock(),profile_.material,profile_.detail!=0);data.effects=bank->effects;if(!data.terrain)return false;
  sourceTick=bank->tick;data.tick=now;assets_.identity(process,epoch);
  external::xbrzadapter::gpuPresentation(profile_.gpu);external::presentation::apply(profile_,assets_,Width,Height,Width,Height);
  const double zscale=external::projectionZScale(fresh);
  gpuAttempted=gpuUsed=false;
  for(int i=0;i<3;++i){auto&v=views[i];auto target=tracks[i]?tracks[i].target:i==0?automatic:external::watch::Target{};
   if(!(target==v.frame.focus))v={};v.shown=bool(target);if(!target)continue;
   auto prior=v.frame.center;if(tracks[i]&&tracks[i].located)prior={tracks[i].spatial,tracks[i].x,tracks[i].y,tracks[i].z,true,tracks[i].spatialKind,tracks[i].discoveryFrame,tracks[i].searched,tracks[i].discoveryCursor};
   auto center=external::watch::anchor(fresh,target,prior);if(!center.located)continue;
   // Match the sampled spatial host (carrier/garrison), not newer live actor
   // coordinates projected over an older world snapshot.
   for(const auto&m:data.markers)if(m.object==center.target.pointer&&m.id==center.target.id){center.x=m.x;center.y=m.y;center.z=m.z;center.kind=m.kind;break;}
   v.frame.focus=target;v.frame.center=center;v.frame.bounds=data.terrain->geo.bounds;v.frame.zScale=zscale;v.frame.reveal=revealMap;
   double cx=(int64_t(center.x)-center.y)*30./256,cy=(int64_t(center.x)+center.y)*15./256-external::screenHeight(center.z,zscale)+external::camera::anchorY(center.kind);
   if(!v.frame.bounds.clampCamera(cx,cy,Width,Height))continue;v.frame.cameraX=cx;v.frame.cameraY=cy;v.frame.cameraReady=true;
   v.frame.passengers.clear();v.frame.dangerous=false;external::passengers::contents(read_,target.pointer,v.frame.passengers,v.frame.dangerous);
   auto&s=scenes_[i];s.features(profile_);s.guideEnabled(false,controls_);
   auto scene=s.render(data,Width,Height,read_,assets_,{cx-Width/2+data.terrain->geo.offsetX*30.,cy-Height/2-data.terrain->geo.offsetY*15.,Width,Height,true},unsigned(profile_.scale),profile_.detail!=0,true,profile_.precision!=0,true,4.);
   gpuAttempted|=external::xbrzadapter::gpuAttempted();gpuUsed|=external::xbrzadapter::gpuUsed();
   if(scene&&profile_.lasers&&data.effects&&data.effects->boltsValid){const std::vector<uint8_t>*mask=nullptr;if(s.feedbackVisibility(mask)){
    external::mainview::View camera{0x10000,data.terrain->geo.house,0x10000,data.frame,int(cx)-Width/2,int(cy)-Height/2,Width,Height,0,0,Width,Height};
    auto decorated=std::make_shared<external::radar::Scene>(*scene);auto status=external::nativefeedback::effects(data.effects->bolts,*decorated,camera,mask,zscale);if(status.complete)scene=std::move(decorated);
   }}
   if(scene)v.scene=std::move(scene);
  }
  bool visible[3]{};external::watch::Box boxes[3]{};for(int i=0;i<3;++i)visible[i]=views[i].shown;external::watch::arrange(visible,width,height,boxes);for(int i=0;i<3;++i)views[i].box=boxes[i];
  return true;
 }
 void drawView(Graphics&g,const View&v){if(!v.shown||!v.scene||v.scene->pixels.empty())return;Bitmap bm(Width,Height,Width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(const_cast<uint32_t*>(v.scene->pixels.data())));g.DrawImage(&bm,RectF(0,0,float(Width),float(Height)),0.f,0.f,float(Width),float(Height),UnitPixel,nullptr);
  Pen border(Color(255,v.frame.dangerous?255:105,v.frame.dangerous?167:195,v.frame.dangerous?62:225),6.f);g.DrawRectangle(&border,3.f,3.f,float(Width-6),float(Height-6));
 }
};
}
