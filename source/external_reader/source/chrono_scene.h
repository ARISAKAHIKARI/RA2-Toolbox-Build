#pragma once
#include "native_scene_extras.h"
#include "world_effects.h"
#include "main_viewport.h"
#include "native_packed_color.h"
namespace external { namespace chronoscene {
// Vera20K chrono_screen.{rs,wgsl}: two retained scenes, one at each side of
// Action127. The native phase/counter is the only clock. No screenshot, game
// call, RNG, audio or input-lock mutation belongs to this presentation owner.
template<class World>class History {
 std::shared_ptr<const World>previous_,retained_;
 mainview::View previousView_{},retainedView_{};
 std::shared_ptr<const std::vector<uint8_t>>trig_;
 sceneextras::Chrono state_{};int width_=0,height_=0,lastFrame_=-1;
public:
 void reset(){previous_.reset();retained_.reset();trig_.reset();state_={};previousView_={};retainedView_={};width_=height_=0;lastFrame_=-1;}
 const sceneextras::Chrono&state()const{return state_;}
 auto table()const{return trig_;}
 bool active()const{return bool(retained_)&&state_.phase>0&&bool(trig_);}
 template<class Read>void select(std::shared_ptr<const World>&world,mainview::View&view,int w,int h,Read read){
  const auto incoming=world;const auto inputView=view;
  auto next=world->state.extras&&world->state.extras->chronoValid?world->state.extras->chrono:sceneextras::Chrono{};
  const bool context=previousView_.scenario&&previousView_.scenario==view.scenario&&previousView_.house==view.house&&previousView_.tactical==view.tactical&&view.frame>=lastFrame_;
  if(!context){retained_.reset();previous_.reset();state_={};trig_.reset();}
  const bool requested=next.phase>0&&next.duration>0;
  if(requested&&!trig_){auto data=std::make_shared<std::vector<uint8_t>>(10241*4);if(read(0x84F084,data->data(),data->size())){bool valid=true;for(size_t n=0;n<10241;++n){float f=0;std::memcpy(&f,data->data()+n*4,4);valid&=std::isfinite(f)&&std::abs(f)<=1.001f;}if(valid)trig_=std::move(data);}}
  if(!requested||!trig_)retained_.reset();
  else if(!retained_||state_.source!=next.source||(next.phase==1&&state_.phase!=1&&state_.phase!=2)||(next.phase==3&&state_.phase!=3)||width_!=w||height_!=h){
   const bool prior=next.phase==1&&previous_&&width_==w&&height_==h;retained_=prior?previous_:incoming;retainedView_=prior?previousView_:inputView;
  }
  state_=next;width_=w;height_=h;lastFrame_=inputView.frame;previous_=incoming;previousView_=inputView;
  if(retained_){world=retained_;view=retainedView_;}
 }
 // CPU fallback is restricted to this frame's host scene; GPU uses the same
 // retained world and a destination snapshot in its ordered effect dispatch.
 void apply(std::vector<uint32_t>&pixels,int w,int h,int nativeW,int nativeH)const{
  if(!active()||pixels.size()!=size_t(w)*h||nativeW<1||nativeH<1)return;
  if(state_.phase==2){std::fill(pixels.begin(),pixels.end(),0xffffffffu);return;}
  const auto source=pixels;int step=std::clamp(state_.phase==1?state_.duration-state_.remaining:state_.remaining,0,state_.duration);double velocity=0,amp=0;for(int n=0;n<step;++n){velocity+=.002;amp+=velocity;}
  float amplitude=float(amp),cx=float(nativeW/2),cy=float(nativeH/2),length=std::max(1.f,cx*cx+cy*cy);unsigned weight=unsigned(int64_t(step)*255/state_.duration)&65535u;
  auto trig=[&](int n){float f;std::memcpy(&f,trig_->data()+n*4,4);return f;};
  for(int y=0;y<h;++y)for(int x=0;x<w;++x){float dx=float(int(int64_t(x)*nativeW/w))-cx,dy=float(int(int64_t(y)*nativeH/h))-cy;float radial=(1-(dx*dx+dy*dy)/length)*5,angle=radial*radial*amplitude,rawFloat=angle*2607.594482421875f;
   int raw=rawFloat<2147483520.f&&rawFloat>=-2147483648.f?int(rawFloat):INT32_MIN,index=(raw/2)%8192;if(index<0)index+=8192;int cosine=index+2048;if(raw&1){if(index<8191)++index;if(cosine<10239)++cosine;}
   float sn=trig(index),cs=trig(cosine);int xx=std::clamp(int(sn*dy+cs*dx+cx),0,nativeW-1),yy=std::clamp(int(cs*dy-sn*dx+cy),0,nativeH-1);uint32_t c=source[size_t(int64_t(yy)*h/nativeH)*w+int64_t(xx)*w/nativeW];
   unsigned channels[3]={(c>>19)&31,(c>>10)&63,(c>>3)&31},maximum[3]={31,63,31};uint32_t result=0xff000000u;for(int n=0;n<3;++n){channels[n]+=((maximum[n]-channels[n])*weight)>>8;result|=((channels[n]*255+maximum[n]/2)/maximum[n])<<(16-n*8);}pixels[size_t(y)*w+x]=result;
  }
 }
};
inline void sparkle(const sceneextras::Sparkle&s,const worldfx::View&v,std::vector<uint32_t>&pixels,bool smooth){
 if(pixels.size()!=size_t(v.pixelsW)*v.pixelsH)return;unsigned phase=unsigned(s.phase),lerp=phase&4095;if(phase&4096)lerp=4096-lerp;uint32_t color=0xff000000u;for(int sh:{0,8,16})color|=(((((s.base>>sh)&255)*(4096-lerp))+(((s.peak>>sh)&255)*lerp))>>12)<<sh;
 if(!smooth){unsigned r=(color>>19)&31,g=(color>>10)&63,b=(color>>3)&31;color=0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}
 double x=((s.x-s.y+v.offsetX)*30.+s.dx-v.x)*v.pixelsW/v.width,y=((s.x+s.y-v.offsetY-s.level)*15.+15+s.dy-v.y)*v.pixelsH/v.height;
 int l=std::clamp(int(std::floor(x)),0,v.pixelsW),t=std::clamp(int(std::floor(y)),0,v.pixelsH),r=std::clamp(int(std::max(std::floor(x)+1,std::ceil(x+v.pixelsW/v.width))),0,v.pixelsW),b=std::clamp(int(std::max(std::floor(y)+1,std::ceil(y+v.pixelsH/v.height))),0,v.pixelsH);
 for(int yy=t;yy<b;++yy)for(int xx=l;xx<r;++xx)pixels[size_t(yy)*v.pixelsW+xx]=color;
}
}}
