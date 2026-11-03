#pragma once
#include "../../external_reader/source/main_viewport.h"
#include <algorithm>
#include <cmath>
#include <chrono>
namespace assist { namespace nativeviewhost {
using Transform=nativeviewportprojection::Transform;
struct ProcessIdentity {uint32_t pid=0;uint64_t creation=0;};
inline bool sameProcess(ProcessIdentity a,ProcessIdentity b){return a.pid==b.pid&&a.creation==b.creation;}
inline uint64_t probeClock(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
class Lifecycle {
 ProcessIdentity process_{};bool activeSeen_=false,unknown_=false;uint64_t leaseEpoch_=0;
 bool probed_=false;uint64_t probeAt_=0;
public:
 static constexpr uint64_t ProbeInterval=250;
 // A reader reconnect counter or a transient pid=0 is not process death.
 // Only an attested different PID/creation pair can retire a lost active R.
 void bind(ProcessIdentity next){
  if(!next.pid)return;
  if(!process_.pid){process_=next;probed_=false;++leaseEpoch_;return;}
  if(next.pid==process_.pid){if(next.creation&&process_.creation&&next.creation!=process_.creation){process_=next;activeSeen_=unknown_=false;probed_=false;++leaseEpoch_;}else if(next.creation)process_.creation=next.creation;return;}
  if(!next.creation){unknown_=activeSeen_||unknown_;probed_=false;++leaseEpoch_;return;}
  process_=next;activeSeen_=unknown_=false;probed_=false;++leaseEpoch_;
 }
 ProcessIdentity identity()const{return process_;}
 // No-mapping identity is a bounded observation, never a permanent lease:
 // another Host may create the named mapping after an earlier Absent result.
 bool probeDue(uint64_t now=probeClock())const{return process_.pid&&(!probed_||now<probeAt_||now-probeAt_>=ProbeInterval);}
 bool probeAbsent(ProcessIdentity expected,uint64_t now=probeClock()){
  if(!process_.pid||!sameProcess(process_,expected))return false;probed_=true;probeAt_=now;return true;
 }
 bool probeUnknown(ProcessIdentity expected,uint64_t now=probeClock()){
  if(!probeAbsent(expected,now))return false;unknown_=true;return true;
 }
 template<class Shared>bool probeExisting(ProcessIdentity expected,const Shared*s,uint64_t now=probeClock()){
  if(!process_.pid||!sameProcess(process_,expected))return false;
  const uint32_t serial=s?uint32_t(s->viewportSerial):0;Transform next;
  if(!s||!serial||(serial&1)||!nativeviewportprojection::sample(s,next)||serial!=uint32_t(s->viewportSerial)||!next.generation||s->viewZoomBusy||next.active){return probeUnknown(expected,now);}
  activeSeen_=unknown_=false;return probeAbsent(expected,now);
 }
 template<class Shared>bool sample(const Shared*s,Transform&out,uint64_t now=probeClock()){
  if(!s){if(activeSeen_)unknown_=true;if(unknown_||probeDue(now))return false;out={};return true;}
  const uint32_t serial=uint32_t(s->viewportSerial);Transform next;if(!nativeviewportprojection::sample(s,next)||serial!=uint32_t(s->viewportSerial))return false;
  if(next.active){if(process_.pid&&!probed_)unknown_=true;activeSeen_=true;if(unknown_)return false;}
  else{if((process_.pid||activeSeen_||unknown_||(s->viewportHostCapabilities&nativeviewportprojection::HostHandshake))&&(!serial||!next.generation||s->viewZoomBusy)){unknown_=true;return false;}activeSeen_=unknown_=false;if(serial&&next.generation){probed_=true;probeAt_=now;}}
  out=next;return true;
 }
 template<class Shared>void lost(const Shared*s){
  probed_=false;if(!s)return;++leaseEpoch_;Transform departure;
  // Probe BEFORE the current mapping lease is dropped, including an active
  // publication that no paint has seen yet. Unstable is possible ownership.
  const uint32_t serial=uint32_t(s->viewportSerial);
  if(!(s->viewportHostCapabilities&nativeviewportprojection::RenderCapability)&&nativeviewportprojection::sample(s,departure)&&serial==uint32_t(s->viewportSerial)&&!departure.active&&!s->viewZoomBusy&&
     (!(process_.pid||activeSeen_||unknown_||(s->viewportHostCapabilities&nativeviewportprojection::HostHandshake))||(serial&&departure.generation))){activeSeen_=unknown_=false;return;}
  activeSeen_=unknown_=true;
 }
 bool unknown()const{return unknown_;}
 uint64_t leaseEpoch()const{return leaseEpoch_;}
};
inline Lifecycle&lifecycle(){static Lifecycle state;return state;}
inline void bindProcess(uint32_t pid,uint64_t creation){lifecycle().bind({pid,creation});}
inline ProcessIdentity processIdentity(){return lifecycle().identity();}
inline bool probeDue(uint64_t now=probeClock()){return lifecycle().probeDue(now);}
inline bool probeAbsent(ProcessIdentity expected,uint64_t now=probeClock()){return lifecycle().probeAbsent(expected,now);}
inline bool probeUnknown(ProcessIdentity expected,uint64_t now=probeClock()){return lifecycle().probeUnknown(expected,now);}
template<class Shared>bool probeExisting(ProcessIdentity expected,const Shared*s,uint64_t now=probeClock()){return lifecycle().probeExisting(expected,s,now);}
template<class Shared>void mappingLost(const Shared*s){lifecycle().lost(s);}
template<class Shared>bool sampleTransform(const Shared*s,Transform&out){return lifecycle().sample(s,out);}
template<class Shared>bool stable(const Shared*s,const Transform&expected){Transform current;return sampleTransform(s,current)&&current==expected;}
struct CapabilityLease {const void*mapping=nullptr;uint64_t epoch=0;bool granted=false;};
inline CapabilityLease&capabilityLease(){static CapabilityLease lease;return lease;}
// Decision only: the caller owns the eventual Shared write. A fresh zeroed
// mapping is not a Runtime acknowledgement that private R resources retired.
// Permission is first granted from a published, idle inactive state; an active
// viewport can only retain its already granted permission, never acquire it.
template<class Shared>uint32_t capabilityRequest(const Shared*s,bool requested=true){
 const uint32_t presence=s?(s->viewportHostCapabilities&nativeviewportprojection::HostHandshake):0;
 auto&lease=capabilityLease();auto&state=lifecycle();auto deny=[&]{lease={};return presence;};
 if(!s||!requested||state.unknown()){
  // A published inactive remap can clear the loss latch. No zeroed or active
  // remap can do so, even if its address was reused by Windows.
  if(s&&requested){Transform restored;if(!sampleTransform(s,restored))return deny();}else return deny();
 }
 const uint64_t epoch=state.leaseEpoch();const uint32_t prior=s->viewportHostCapabilities;
 const bool priorReady=(prior&nativeviewportprojection::RequiredCapabilities)==nativeviewportprojection::RequiredCapabilities&&
  !(prior&~(nativeviewportprojection::HostHandshake|nativeviewportprojection::RequiredCapabilities));
 const bool sameGrant=lease.granted&&lease.mapping==s&&lease.epoch==epoch;
 // Runtime owns busy1 -> odd -> commit -> even -> busy0. Preserve an already
 // authorized SAME lease through that mutation; drawing/input still reject
 // odd. A fault deliberately left odd with busy0 is not an in-progress lease.
 if(sameGrant&&priorReady&&s->viewportSerial&&s->viewportGeneration&&s->viewZoomBusy==1){
  std::atomic_thread_fence(std::memory_order_acquire);
  if(!state.unknown()&&state.leaseEpoch()==epoch&&s->viewportHostCapabilities==prior&&s->viewZoomBusy==1&&s->viewportSerial&&s->viewportGeneration)return prior;
 }
 Transform before;if(!sampleTransform(s,before))return deny();
 const uint32_t serial=uint32_t(s->viewportSerial);
 if(!serial||(serial&1)||!before.generation||s->viewZoomBusy||
    (before.active&&(!sameGrant||!priorReady)))return deny();
 Transform after;if(!sampleTransform(s,after)||after!=before||serial!=uint32_t(s->viewportSerial)||s->viewZoomBusy||prior!=s->viewportHostCapabilities||epoch!=state.leaseEpoch())return deny();
 lease={s,epoch,true};
 return presence|nativeviewportprojection::RequiredCapabilities;
}
// Only the UI thread reads the mapped Shared lease. Workers carry immutable
// transforms and validate native R; publication rechecks the live generation.
template<class Shared,class Read>bool sample(const Shared*s,Read read,external::mainview::View&out,const Transform&transform){
 if(!stable(s,transform))return false;
 external::mainview::View view;if(!external::mainview::sample(read,view,transform)||!stable(s,transform))return false;
 out=view;return true;
}
template<class Shared,class Read>bool sample(const Shared*s,Read read,external::mainview::View&out){Transform transform;return sampleTransform(s,transform)&&sample(s,read,out,transform);}
inline uint64_t key(const Transform&t){
 if(!t.active&&!t.generation)return 0;
 uint64_t h=1469598103934665603ull;auto add=[&](uint32_t value){for(int n=0;n<4;++n){h^=uint8_t(value>>(n*8));h*=1099511628211ull;}};
 add(t.active);add(t.generation);for(auto r:{t.physical,t.logical})for(int n:{r.x,r.y,r.w,r.h})add(uint32_t(n));return h?h:1;
}
inline bool localPoint(const Transform&t,double&x,double&y){
 if(!std::isfinite(x)||!std::isfinite(y))return false;if(!t.active)return true;
 if(!nativeviewportprojection::valid(t.physical)||!nativeviewportprojection::valid(t.logical))return false;
 // Native CoordsToClient returns map-local R, without R.x/R.y.
 x=t.physical.x+x*t.physical.w/t.logical.w;y=t.physical.y+y*t.physical.h/t.logical.h;
 return std::isfinite(x)&&std::isfinite(y)&&std::abs(x)<=1000000&&std::abs(y)<=1000000;
}
inline bool localPoint(const Transform&t,float&x,float&y){double a=x,b=y;if(!localPoint(t,a,b))return false;x=float(a);y=float(b);return true;}
inline bool localPoint(const Transform&t,int&x,int&y){double a=x,b=y;if(!localPoint(t,a,b))return false;x=int(std::lround(a));y=int(std::lround(b));return true;}
inline nativeviewportprojection::Rect clip(const Transform&t,int width,int height){return t.active?t.physical:nativeviewportprojection::Rect{0,0,width,height};}
template<class Shared,class Read>bool radarOcclusion(const Shared*s,Read read,int screenW,int screenH,nativeviewportprojection::Rect&out){
 out={};if(!s)return true;const auto status=s->sidebarStatus;const auto mode=s->sidebarMode;
 if(s->viewZoomBusy||status==2||status<0)return false;if(status!=3)return true;
 int width=0,height=0;uint8_t side=0;
 if(mode!=1||screenW<640||screenH<240||!read(0x7F5BF8,&width,4)||!read(0x886F94,&height,4)||!read(0xA8EB7C,&side,1)||
    width<64||width>512||height<32||height>512||width>screenW||height+5>screenH)return false;
 int againWidth=0,againHeight=0;uint8_t againSide=0;
 if(!read(0x7F5BF8,&againWidth,4)||!read(0x886F94,&againHeight,4)||!read(0xA8EB7C,&againSide,1)||againWidth!=width||againHeight!=height||againSide!=side||
    s->sidebarStatus!=status||s->sidebarMode!=mode||s->viewZoomBusy)return false;
 out={side?screenW-width:0,0,width,height+5};return true;
}
}}
