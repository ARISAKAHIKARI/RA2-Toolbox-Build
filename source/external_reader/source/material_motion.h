#pragma once
#include "radar_map.h"
#include <cmath>
#include <map>
#include <chrono>
// Read-only presentation timeline. Interpolate two OBSERVED states, with one
// observation interval of visual latency. Never predict movement, advance an
// unobserved SHP frame, change a simulation clock or submit a game command.
namespace external { namespace materialmotion {
inline double now(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline int angle(uint16_t a,uint16_t b,double phase){
 int delta=int(uint16_t(b-a));if(delta>32767)delta-=65536;
 int raw=int(std::lround(double(a)+delta*phase));return ((raw+128)>>8)&255;
}
inline bool identity(const radar::Marker&a,const radar::Marker&b){
 return a.object&&a.id&&a.object==b.object&&a.id==b.id&&a.type==b.type&&a.owner==b.owner&&
  a.kind==b.kind&&a.parent==b.parent&&a.parentId==b.parentId&&a.layer==b.layer&&a.onBridge==b.onBridge&&
  a.pose.variant==b.pose.variant&&a.pose.buildingFlags==b.pose.buildingFlags;
}
inline bool adjacent(const visual::Pose&a,const visual::Pose&b){
 // Direction/sequence changes and loop boundaries snap. Do not dissolve a
 // walking infantryman into a firing/death frame, production body or damage.
 return a.frame>=0&&b.frame>=0&&b.frame==a.frame+1&&a.body==b.body&&a.turret==b.turret&&
  a.variant==b.variant&&a.buildingFlags==b.buildingFlags&&a.doorFrame==b.doorFrame&&
  a.sequence==b.sequence&&a.turretFrame==b.turretFrame;
}
// Live presenters consume the newest memory snapshot immediately. Rendering
// it again never waits for a new engine frame or moves actors back toward the
// preceding observation. Raw 16-bit facing can still use fine VXL geometry.
inline radar::Frame latest(const radar::Frame&frame,bool fine=true){
 if(!frame.terrain||!frame.markerScanValid||frame.frame<0)return {};
 auto out=frame;
 for(auto&m:out.markers){
  m.pose.frameFrom=-1;m.pose.frameWeight=255;
  if(fine&&m.pose.voxel){
   const auto body=m.pose.rawFacing?m.pose.rawBody:uint16_t(m.pose.body*2048);
   const auto turret=m.pose.rawFacing?m.pose.rawTurret:uint16_t(m.pose.turret*2048);
   m.pose.bodyFine=angle(body,body,1.);m.pose.turretFine=angle(turret,turret,1.);
  }
 }
 return out;
}
class Timeline {
 radar::Frame previous_,current_;double observed_=0,interval_=0,motionInterval_=0;bool ready_=false;
 using Key=std::pair<uint32_t,uint32_t>;std::map<Key,radar::Marker> prior_;
public:
 void reset(){previous_={};current_={};observed_=interval_=motionInterval_=0;ready_=false;prior_.clear();}
 bool observe(const radar::Frame&frame,double now){
  if(frame.observedMs>0)now=frame.observedMs;
  if(!frame.terrain||!frame.markerScanValid||frame.frame<0||!std::isfinite(now))return false;
  if(ready_&&frame.frame==current_.frame&&frame.terrain->geo.same(current_.terrain->geo)&&frame.terrain->reveal==current_.terrain->reveal){current_=frame;return false;} // refresh current membership without advancing the interpolation clock
  bool continuous=ready_&&frame.frame>current_.frame&&
   now>observed_&&now-observed_<=250&&frame.terrain->geo.same(current_.terrain->geo)&&
   frame.terrain->reveal==current_.terrain->reveal;
  prior_.clear();motionInterval_=continuous?now-observed_:0;interval_=continuous?std::clamp(now-observed_,1.,100.):0;
  if(continuous){previous_=std::move(current_);for(const auto&m:previous_.markers)prior_[{m.object,m.id}]=m;}
  else previous_={};
  current_=frame;observed_=now;ready_=true;return true;
 }
 bool ready()const{return ready_;}
 double interval()const{return interval_;}
 // Display-only, zero-buffer motion from the latest two observed frames.
 // The horizon is at most one observed frame and 25 ms. Native poses, game
 // clocks, animation sequences, RNG and command queues are never advanced.
 radar::Frame sampleLatest(double at,bool enabled=true,bool fine=true)const{
  if(!ready_||!std::isfinite(at))return {};
  auto out=latest(current_,fine);
  const double age=at-observed_,horizon=std::min(motionInterval_,25.);
  const double phase=enabled&&motionInterval_>0&&age>=0&&age<=std::max(motionInterval_*2.,50.)?std::min(age,horizon)/motionInterval_:0.;
  const int ticks=previous_.frame>=0?std::max(1,current_.frame-previous_.frame):1;
  for(auto&m:out.markers){auto it=prior_.find({m.object,m.id});if(it==prior_.end()||!identity(it->second,m))continue;
   const auto&old=it->second;const int ox=m.x,oy=m.y,oz=m.z;const double dx=double(m.x)-old.x,dy=double(m.y)-old.y,dz=double(m.z)-old.z;
   const double limit=(m.kind==projectiles::Kind?1536.:256.)*ticks;
   if(phase==0||m.kind==3||m.kind==4||m.kind>6||dx*dx+dy*dy>limit*limit||std::abs(dz)>256.*ticks)continue;
   m.x=int(std::lround(m.x+dx*phase));m.y=int(std::lround(m.y+dy*phase));m.z=int(std::lround(m.z+dz*phase));
   if(m.x<0||m.y<0||m.z<0||m.x>=512*256||m.y>=512*256){m.x=ox;m.y=oy;m.z=oz;continue;}
   out.terrain->geo.project(double(m.x)/256,double(m.y)/256,m.u,m.v);
   if(fine&&m.pose.voxel){auto raw=[](const radar::Marker&v,bool turret){return v.pose.rawFacing?(turret?v.pose.rawTurret:v.pose.rawBody):uint16_t((turret?v.pose.turret:v.pose.body)*2048);};
    auto turn=[&](bool turret){const auto a=raw(old,turret),b=raw(m,turret);int d=int(uint16_t(b-a));if(d>32767)d-=65536;return std::abs(d)<=8192?angle(a,b,1.+phase):angle(b,b,1.);};
    m.pose.bodyFine=turn(false);m.pose.turretFine=turn(true);
   }
  }return out;
 }
 radar::Frame sample(double now,bool interpolate=true,bool fine=true)const{
  if(!ready_||!std::isfinite(now))return {};
  auto out=current_;double phase=interpolate&&interval_>0?std::clamp((now-observed_)/interval_,0.,1.):1.;
  const int ticks=previous_.frame>=0?std::max(1,current_.frame-previous_.frame):1;
  for(auto&m:out.markers){
   auto it=prior_.find({m.object,m.id});const auto*old=it==prior_.end()?nullptr:&it->second;
   bool valid=interpolate&&old&&identity(*old,m);
   if(valid){
    double dx=double(m.x)-old->x,dy=double(m.y)-old->y,dz=double(m.z)-old->z;
    const double limit=(m.kind==projectiles::Kind?1536.:256.)*ticks;
    valid=dx*dx+dy*dy<=limit*limit&&std::abs(dz)<=256.*ticks;
   }
   if(valid){
    const auto&before=*old;
    if(m.kind!=3&&m.kind!=4){
     m.x=int(std::lround(before.x+(m.x-before.x)*phase));m.y=int(std::lround(before.y+(m.y-before.y)*phase));
     m.z=int(std::lround(before.z+(m.z-before.z)*phase));
     out.terrain->geo.project(double(m.x)/256,double(m.y)/256,m.u,m.v);
    }
    if(adjacent(before.pose,m.pose)&&phase<1){m.pose.frameFrom=before.pose.frame;m.pose.frameWeight=int(std::lround(phase*255));}
   }
   if(fine&&m.pose.voxel){
    auto raw=[](const radar::Marker&v,bool turret){return v.pose.rawFacing?(turret?v.pose.rawTurret:v.pose.rawBody):uint16_t((turret?v.pose.turret:v.pose.body)*2048);};
    m.pose.bodyFine=angle(valid?raw(*old,false):raw(m,false),raw(m,false),valid?phase:1.);
    m.pose.turretFine=angle(valid?raw(*old,true):raw(m,true),raw(m,true),valid?phase:1.);
   }
  }
  return out;
 }
};
inline unsigned refreshPeriod(int target,bool highRefresh){return unsigned(1000/std::clamp(target,5,highRefresh?500:60));}
inline uint64_t presentationKey(const radar::Frame&frame){
 uint64_t h=1469598103934665603ull;auto add=[&](uint64_t v){h=(h^v)*1099511628211ull;};
 add(uint32_t(frame.frame));add(frame.markers.size());
 for(const auto&m:frame.markers){for(uint32_t v:{m.object,m.id,m.type,m.owner,uint32_t(m.x),uint32_t(m.y),uint32_t(m.z),uint32_t(m.pose.frame),uint32_t(m.pose.body),uint32_t(m.pose.turret),uint32_t(m.pose.bodyFine),uint32_t(m.pose.turretFine),uint32_t(m.pose.frameFrom),uint32_t(m.pose.frameWeight)})add(v);add(voxelposture::signature(m.pose.posture));add(m.material.translucency);add(m.material.mixStrength);add(uint16_t(m.waterline));add(m.disguised);}
 return h;
}
}}
