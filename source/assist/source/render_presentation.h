#pragma once
#include "../../external_reader/source/main_viewport.h"
#include "../../shared/source/overlay_composition.h"
#include <algorithm>
#include <cmath>
// Publication policy is separate from rendering: a pending/failed attempt is
// not a new image. Keep the last completed image for a bounded interval, while
// every successful job still recomposes CURRENT lighting and palette data.
namespace renderpresentation {
constexpr uint32_t SampleMaxAge=2500,CompletionMaxAge=1500,ViewReadGrace=120;
// A small source border lets camera motion crop a completed image immediately.
// Keep its cost bounded: no resolution reduction and no full-frame UI copies.
constexpr int ScrollBorder=96;
struct Window {int paddingX=0,paddingY=0,width=0,height=0;};
inline Window window(int width,int height,int nativeWidth,int nativeHeight){
 if(width<1||height<1||width>8192||height>8192||uint64_t(width)*height>16*1024*1024||nativeWidth<1||nativeHeight<1)return {};
 Window out{int(std::ceil(double(ScrollBorder)*width/nativeWidth)),int(std::ceil(double(ScrollBorder)*height/nativeHeight)),width,height};
 auto fits=[&]{return int64_t(width)+2ll*out.paddingX<=8192&&int64_t(height)+2ll*out.paddingY<=8192&&
  (uint64_t(width)+2ull*out.paddingX)*(uint64_t(height)+2ull*out.paddingY)<=16*1024*1024;};
 while(!fits()&&(out.paddingX||out.paddingY)){out.paddingX/=2;out.paddingY/=2;}
 if(!fits())return {};
 out.width=width+2*out.paddingX;out.height=height+2*out.paddingY;return out;
}
inline bool sameProjection(external::mainview::View a,const external::mainview::View&b){
 a.cameraX=b.cameraX;a.cameraY=b.cameraY;return external::mainview::sameViewport(a,b);
}
inline bool recent(uint32_t now,uint32_t tick,uint32_t limit){return uint32_t(now-tick)<limit;}
// Bias only the source crop inside the existing symmetric overscan. The
// observations remain actual game cameras; no unit state, game camera or frame
// time is advanced. A stopped/reversed camera therefore stops/reverses the
// bias on its very next observation instead of continuing an old velocity.
class CameraPrefetch {
 external::mainview::View previous_{};uint32_t tick_=0;bool observed_=false;
public:
 void reset(){observed_=false;}
 external::mainview::View center(const external::mainview::View&actual,uint32_t now,double lastRenderMs,double marginX,double marginY){
  auto out=actual;
  if(!actual.valid()||!std::isfinite(lastRenderMs)||lastRenderMs<=0||lastRenderMs>CompletionMaxAge||
    !std::isfinite(marginX)||!std::isfinite(marginY)||marginX<0||marginY<0){reset();return out;}
  const uint32_t dt=now-tick_;
  const int64_t dx=int64_t(actual.cameraX)-previous_.cameraX,dy=int64_t(actual.cameraY)-previous_.cameraY;
  const bool measured=observed_&&sameProjection(previous_,actual)&&actual.frame>=previous_.frame&&
   dt>0&&dt<=250&&std::abs(dx)<=4*ScrollBorder&&std::abs(dy)<=4*ScrollBorder;
  previous_=actual;tick_=now;observed_=true;
  if(!measured)return out;
  auto bias=[&](int64_t delta,double margin){
   const int limit=int(std::floor(std::min(64.,margin*(2./3.))));
   return int(std::lround(std::clamp(double(delta)*lastRenderMs/dt,-double(limit),double(limit))));
  };
  out.cameraX+=bias(dx,marginX);out.cameraY+=bias(dy,marginY);
  return out.valid()?out:actual;
 }
};
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
template<class Request>auto gpuFlag(const Request&r,int)->decltype(bool(r.gpu)){return r.gpu;}
template<class Request>bool gpuFlag(const Request&,long){return false;}
template<class Request>bool compatible(const Request&a,const Request&b){
 return a.pid==b.pid&&a.match==b.match&&a.context.token==b.context.token&&
  a.context.window==b.context.window&&a.context.width==b.context.width&&a.context.height==b.context.height&&
  a.reveal==b.reveal&&a.generation==b.generation&&a.materialRefresh==b.materialRefresh&&
  a.detail==b.detail&&a.colorPrecision==b.colorPrecision&&a.paletteRange==b.paletteRange&&
  a.pixelScaleMode==b.pixelScaleMode&&gpuFlag(a,0)==gpuFlag(b,0)&&a.width==b.width&&a.height==b.height&&
  external::mainview::sameViewport(a.view,b.view)&&b.view.frame>=a.view.frame;
}
template<class Frame,class Request>bool usable(const Frame&frame,const Request&request,uint32_t now){
 return frame.image&&compatible(frame.request,request)&&
  recent(now,frame.sampledTick,SampleMaxAge)&&recent(now,frame.completedTick,CompletionMaxAge);
}
template<class Request>bool scrollCompatible(Request a,const Request&b){
 a.view.cameraX=b.view.cameraX;a.view.cameraY=b.view.cameraY;return compatible(a,b);
}
struct Shift {int x=0,y=0;};
inline std::shared_ptr<const std::vector<overlaycomposition::Rect>> sceneCutouts(
 const std::shared_ptr<const std::vector<overlaycomposition::Rect>>&spans,const Shift&shift){
 if(!spans||(!shift.x&&!shift.y))return spans;
 auto out=std::make_shared<std::vector<overlaycomposition::Rect>>(*spans);
 for(auto&r:*out){r.x-=shift.x;r.y-=shift.y;}return out;
}
template<class Frame,class Request>bool scrollUsable(const Frame&frame,const Request&request,uint32_t now,Shift&shift){
 if(!frame.image||!scrollCompatible(frame.request,request)||
   !recent(now,frame.sampledTick,SampleMaxAge)||!recent(now,frame.completedTick,CompletionMaxAge)||
   frame.paddingX<0||frame.paddingY<0||
   frame.image->width!=frame.request.width+2*frame.paddingX||frame.image->height!=frame.request.height+2*frame.paddingY)return false;
 const double sx=double(request.width)/request.view.width,sy=double(request.height)/request.view.height;
 const double dx=double(int64_t(request.view.cameraX)-frame.request.view.cameraX)*sx;
 const double dy=double(int64_t(request.view.cameraY)-frame.request.view.cameraY)*sy;
 if(!std::isfinite(dx)||!std::isfinite(dy)||std::abs(dx)>frame.paddingX||std::abs(dy)>frame.paddingY)return false;
 shift={-frame.paddingX-int(std::lround(dx)),-frame.paddingY-int(std::lround(dy))};return true;
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
