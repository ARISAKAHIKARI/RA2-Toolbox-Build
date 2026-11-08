#pragma once
#include "../../external_reader/source/main_viewport.h"
#include "../../shared/source/overlay_composition.h"
#include <algorithm>
#include <cmath>
// Publication policy is separate from rendering: a pending/failed attempt is
// not a new image. Keep the last completed image for a bounded interval, while
// every successful job still recomposes CURRENT lighting and palette data.
namespace renderpresentation {
// Restore the pre-migration completed-frame window. A 250 ms render must not
// withdraw an otherwise valid front image after 100 ms. Neither retries nor UI
// polls renew these timestamps; menu/focus/world/config invalidation is immediate.
constexpr uint32_t SampleMaxAge=2500,CompletionMaxAge=1500,ViewReadGrace=120;
constexpr int ObservationRate=1000;
inline bool sameProjection(external::mainview::View a,const external::mainview::View&b){
 a.cameraX=b.cameraX;a.cameraY=b.cameraY;return external::mainview::sameViewport(a,b);
}
inline bool recent(uint32_t now,uint32_t tick,uint32_t limit){return uint32_t(now-tick)<limit;}
class StatusCadence {
 uint64_t key_=~uint64_t(0);uint32_t tick_=0;
public:
 bool due(uint64_t key,uint32_t now){
  if(key==key_&&recent(now,tick_,250))return false;
  key_=key;tick_=now;return true;
 }
};
inline bool sampleAdvance(int first,int sampled,int last){
 return first>=0&&sampled>=first&&last>=sampled&&int64_t(last)-sampled<=2;
}
inline bool sampleAdvance(int first,int sampled,int last,uint32_t elapsedMs){
 return first>=0&&sampled>=first&&last>=sampled&&elapsedMs<=120;
}
template<class Request>auto gpuFlag(const Request&r,int)->decltype(bool(r.gpu)){return r.gpu;}
template<class Request>bool gpuFlag(const Request&,long){return false;}
template<class Request>auto neuralFlag(const Request&r,int)->decltype(bool(r.neural)){return r.neural;}
template<class Request>bool neuralFlag(const Request&,long){return false;}
template<class Request>bool compatible(const Request&a,const Request&b){
 return a.pid==b.pid&&a.match==b.match&&a.context.token==b.context.token&&
  a.context.window==b.context.window&&a.context.width==b.context.width&&a.context.height==b.context.height&&a.context.projection==b.context.projection&&
  a.reveal==b.reveal&&a.generation==b.generation&&a.materialRefresh==b.materialRefresh&&
  a.detail==b.detail&&a.colorPrecision==b.colorPrecision&&a.paletteRange==b.paletteRange&&
  a.pixelScaleMode==b.pixelScaleMode&&gpuFlag(a,0)==gpuFlag(b,0)&&neuralFlag(a,0)==neuralFlag(b,0)&&a.width==b.width&&a.height==b.height&&
  external::mainview::sameViewport(a.view,b.view)&&b.view.frame>=a.view.frame;
}
// A sealed world can remain current while the game is paused. Keep its
// original acquisition tick; require a freshly completed draw and an exact
// live/requested logic frame instead of manufacturing a new sample timestamp.
template<class Frame,class Request>auto currentSealedWorld(const Frame&frame,const Request&request,int)
 ->decltype(bool(frame.world->state.sourceFrame),bool(frame.logicFrame==request.view.frame),bool()){
 return frame.world&&frame.world->state.sourceFrame&&frame.logicFrame==request.view.frame&&
  frame.world->state.sourceFrame->key.frame==request.view.frame&&frame.world->state.sourceFrame->key.pid==request.pid&&
  frame.world->state.sourceFrame->key.match==request.match;
}
template<class Frame,class Request>bool currentSealedWorld(const Frame&,const Request&,long){return false;}
template<class Frame,class Request>bool currentSample(const Frame&frame,const Request&request,uint32_t now){
 return recent(now,frame.sampledTick,SampleMaxAge)||currentSealedWorld(frame,request,0);
}

template<class Frame,class Request>bool usable(const Frame&frame,const Request&request,uint32_t now){
 return frame.image&&compatible(frame.request,request)&&
  currentSample(frame,request,now)&&recent(now,frame.completedTick,CompletionMaxAge);
}
template<class Request>bool sameWorldRequest(Request a,const Request&b){
 a.view.cameraX=b.view.cameraX;a.view.cameraY=b.view.cameraY;return compatible(a,b);
}
// A whole frame remains in its recorded camera. Only immutable world inputs
// may be reused for another camera; completed image pixels are never shifted.
template<class Frame,class Request>bool completeUsable(const Frame&frame,const Request&request,uint32_t now){
 return frame.image&&sameWorldRequest(frame.request,request)&&
  currentSample(frame,request,now)&&recent(now,frame.completedTick,CompletionMaxAge)&&
  frame.image->width==request.width&&frame.image->height==request.height;
}
inline std::vector<overlaycomposition::Surface> worldOnly(std::vector<overlaycomposition::Surface>layers,uint64_t id){
 layers.erase(std::remove_if(layers.begin(),layers.end(),[&](const auto&layer){return layer.id!=id;}),layers.end());
 for(auto&layer:layers)layer.cutouts.reset();return layers;
}
template<class Request,class Frame>class GroupLease {
 Request request_{};std::shared_ptr<const Frame> frame_;std::vector<overlaycomposition::Surface> layers_;uint32_t tick_=0;
public:
 void reset(){frame_.reset();layers_.clear();}
 void publish(const Request&request,std::shared_ptr<const Frame>frame,const std::vector<overlaycomposition::Surface>&layers,uint32_t now){
  request_=request;frame_=std::move(frame);layers_=layers;tick_=now;
 }
 bool retain(const Request&request,uint32_t now)const{
  return frame_&&compatible(request_,request)&&usable(*frame_,request,now)&&recent(now,tick_,ViewReadGrace);
 }
 // Hold a complete camera-tagged world while a newer camera is being drawn.
 // The caller retains this context and removes all native cutouts/feedback.
 bool retainWorld(const Request&request,uint32_t now)const{
  return frame_&&sameWorldRequest(request_,request)&&
   currentSample(*frame_,request,now)&&recent(now,frame_->completedTick,CompletionMaxAge);
 }
 const overlaycomposition::Context&context()const{return request_.context;}
 const std::vector<overlaycomposition::Surface>&layers()const{return layers_;}
 const std::shared_ptr<const Frame>&frame()const{return frame_;}
};
class ViewLease {
 overlaycomposition::Context key{};external::mainview::View view_{};uint32_t tick_=0;bool valid_=false;
public:
 void reset(){valid_=false;}
 void observe(uintptr_t window,uint32_t pid,uint32_t match,int width,int height,const external::mainview::View&view,uint32_t now){
  key={0,window,pid,match,width,height};view_=view;tick_=now;valid_=true;
 }
 bool retain(uintptr_t window,uint32_t pid,uint32_t match,int width,int height,uint32_t now,const nativeviewportprojection::Transform&transform={})const{
  return valid_&&key.window==window&&key.pid==pid&&key.match==match&&key.width==width&&key.height==height&&view_.transform==transform&&recent(now,tick_,ViewReadGrace);
 }
 const external::mainview::View&view()const{return view_;}
};
}
