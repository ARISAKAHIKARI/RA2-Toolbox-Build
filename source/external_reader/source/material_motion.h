#pragma once
#include "radar_map.h"
#include <cmath>
#include <map>
// Read-only presentation timeline. Interpolate two OBSERVED states, with one
// observation interval of visual latency. Never predict movement, advance an
// unobserved SHP frame, change a simulation clock or submit a game command.
namespace external { namespace materialmotion {
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
class Timeline {
 radar::Frame previous_,current_;double observed_=0,interval_=0;bool ready_=false;
 using Key=std::pair<uint32_t,uint32_t>;std::map<Key,radar::Marker> prior_;
public:
 void reset(){previous_={};current_={};observed_=interval_=0;ready_=false;prior_.clear();}
 bool observe(const radar::Frame&frame,double now){
  if(!frame.terrain||!frame.markerScanValid||frame.frame<0||!std::isfinite(now))return false;
  if(ready_&&frame.frame==current_.frame){current_=frame;return false;} // refresh current membership without advancing the interpolation clock
  bool continuous=ready_&&frame.frame>current_.frame&&
   now>observed_&&now-observed_<=250&&frame.terrain->geo.same(current_.terrain->geo)&&
   frame.terrain->reveal==current_.terrain->reveal;
  prior_.clear();interval_=continuous?std::clamp(now-observed_,1.,250.):0;
  if(continuous){previous_=std::move(current_);for(const auto&m:previous_.markers)prior_[{m.object,m.id}]=m;}
  else previous_={};
  current_=frame;observed_=now;ready_=true;return true;
 }
 bool ready()const{return ready_;}
 double interval()const{return interval_;}
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
 for(const auto&m:frame.markers){for(uint32_t v:{m.object,m.id,m.type,m.owner,uint32_t(m.x),uint32_t(m.y),uint32_t(m.z),uint32_t(m.pose.frame),uint32_t(m.pose.body),uint32_t(m.pose.turret),uint32_t(m.pose.bodyFine),uint32_t(m.pose.turretFine),uint32_t(m.pose.frameFrom),uint32_t(m.pose.frameWeight)})add(v);add(voxelposture::signature(m.pose.posture));}
 return h;
}
}}
