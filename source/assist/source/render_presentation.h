#pragma once
#include "../../external_reader/source/main_viewport.h"
#include "../../shared/source/overlay_composition.h"
// Publication policy is separate from rendering: a pending/failed attempt is
// not a new image. Keep the last completed image for a bounded interval, while
// every successful job still recomposes CURRENT lighting and palette data.
namespace renderpresentation {
constexpr uint32_t SampleMaxAge=2500,CompletionMaxAge=1500,ViewReadGrace=120;
inline bool recent(uint32_t now,uint32_t tick,uint32_t limit){return uint32_t(now-tick)<limit;}
inline bool sampleAdvance(int first,int sampled,int last){
 return first>=0&&sampled>=first&&last>=sampled&&int64_t(last)-sampled<=2;
}
template<class Request>bool compatible(const Request&a,const Request&b){
 return a.pid==b.pid&&a.match==b.match&&a.context.token==b.context.token&&
  a.context.window==b.context.window&&a.context.width==b.context.width&&a.context.height==b.context.height&&
  a.reveal==b.reveal&&a.generation==b.generation&&a.materialRefresh==b.materialRefresh&&
  a.detail==b.detail&&a.colorPrecision==b.colorPrecision&&a.paletteRange==b.paletteRange&&
  a.pixelScaleMode==b.pixelScaleMode&&a.width==b.width&&a.height==b.height&&
  external::mainview::sameViewport(a.view,b.view)&&b.view.frame>=a.view.frame;
}
template<class Frame,class Request>bool usable(const Frame&frame,const Request&request,uint32_t now){
 return frame.image&&compatible(frame.request,request)&&
  recent(now,frame.sampledTick,SampleMaxAge)&&recent(now,frame.completedTick,CompletionMaxAge);
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
 bool retain(uintptr_t window,uint32_t pid,uint32_t match,int width,int height,uint32_t now)const{
  return valid_&&key.window==window&&key.pid==pid&&key.match==match&&key.width==width&&key.height==height&&recent(now,tick_,ViewReadGrace);
 }
 const external::mainview::View&view()const{return view_;}
};
}
