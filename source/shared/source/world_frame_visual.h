#pragma once
#include "world_frame_capture.h"
#include "world_frame_pool.h"

namespace worldframe {
// This cache runs in the external Host. Game-thread bytes are copied first
// and never overwritten: the collector reads only missing display ranges.
// Publication tick/frame continue to describe that control observation;
// mutable display data has its own acquisition stamp and no command authority.
struct VisualStamp {
 uint32_t scenario=0,cells=0,house=0,count=0;int frame=-1,session=-1;
 std::array<int,6>bounds{};int rawWidth=0,rawHeight=0;bool valid=false;
 bool sameWorld(const VisualStamp&o)const{return valid&&o.valid&&scenario==o.scenario&&cells==o.cells&&house==o.house&&count==o.count&&session==o.session&&bounds==o.bounds&&rawWidth==o.rawWidth&&rawHeight==o.rawHeight;}
 bool same(const VisualStamp&o)const{return sameWorld(o)&&frame==o.frame;}
};
template<class Live>VisualStamp visualStamp(Live live,const Snapshot&base){
 VisualStamp out;uint8_t active=0;bool ok=true;
 auto get=[&](uint32_t at,auto&value){ok=live(at,&value,sizeof value)&&ok;};
 get(0xA8B230,out.scenario);get(0x87F7E8+0x13C,out.cells);get(0x87F7E8+0x140,out.count);
 get(0xA83D4C,out.house);get(0xA8ED84,out.frame);get(0xA8B238,out.session);get(0xA8E9A0,active);
 get(0x87F7E8+0xF4,out.bounds);get(0x87F7E8+0x1240,out.rawWidth);get(0x87F7E8+0x1244,out.rawHeight);
 // Full command-authoritative snapshots may carry geometry bytes that can
 // be compared directly. A display-only Key intentionally carries ZERO
 // game-thread pages: compare geometry across the two independent Host reads
 // (before/after) instead, and require the same map/House/session/frame epoch.
 const auto&key=base.key;
 bool sourceGeometry=true;
 if(!boundaryOnlySource(key)){
  std::array<int,6>bounds{};int rw=0,rh=0;
  sourceGeometry=base.read(0x87F7E8+0xF4,bounds.data(),sizeof bounds)&&
    base.read(0x87F7E8+0x1240,&rw,4)&&
    base.read(0x87F7E8+0x1244,&rh,4)&&
    bounds==out.bounds&&rw==out.rawWidth&&rh==out.rawHeight;
 }
 out.valid=ok&&sourceGeometry&&
  active==1&&out.scenario==key.scenario&&out.cells==key.cells&&
  out.house==key.house&&out.count==key.cellCount&&
  out.session==key.session&&out.frame>=key.frame;
 return out;
}
// Only a Host display capture may span logic frames. At hundreds of game
// ticks per second a multi-page RPM read cannot fit inside one tick.
// This does NOT relax command snapshots, object identities or world changes.
// Reject any rewind/identity change or an excessively long mixed observation.
inline bool coherentVisualCapture(const VisualStamp&before,const VisualStamp&after,uint32_t durationMs,uint32_t maxDurationMs=150){
 return before.same(after)||(before.sameWorld(after)&&after.frame>=before.frame&&durationMs<=maxDurationMs);
}
class VisualCache {
 Token base_,latest_;std::shared_ptr<SnapshotPool>pool_=std::make_shared<SnapshotPool>();
 CaptureScratch scratch_;DemandLeases leases_;std::vector<uint32_t>requests_;
 uint64_t revision_=0,builtRevision_=0,serial_=0;uint32_t builtDemand_=0,attemptedAt_=0;Token attempted_;
 std::array<uint32_t,6>demandTicks_{};uint32_t demanded_=0;
public:
 struct Statistics {uint64_t captures=0,reused=0,failed=0;};Statistics statistics;
 void request(const std::vector<uint32_t>&pages,uint32_t now,bool retry){
  bool added=false;for(uint32_t page:pages){const auto count=leases_.size();if(leases_.retain(page,now)&&leases_.size()!=count)added=true;}
  if(added||retry)++revision_;
 }
 template<class Live>Token acquire(const Token&source,uint32_t demand,uint32_t now,Live live){
  if(!source||source->controlSource||!source->sealed())return {};
  if((base_&&!base_->key.sameWorld(source->key))||(!base_&&attempted_&&!attempted_->key.sameWorld(source->key))){latest_.reset();base_.reset();attempted_.reset();leases_.clear();revision_=builtRevision_=0;builtDemand_=demanded_=0;}
  // Host visual profiles are NOT Runtime command demand. Only this
  // read-only Host cache expands them into actor/passenger/combat graph
  // ranges; the published game-thread boundary remains small.
  const uint32_t visualDetails=demand&uint32_t(HostVisualPassengers|HostVisualCombat|HostVisualOre);
  demand=(demand&~uint32_t(Resources|HostVisualPassengers|HostVisualCombat|HostVisualOre))|Base;
  if(visualDetails&HostVisualPassengers)demand|=Passengers;
  if(visualDetails&HostVisualCombat)demand|=Combat;
  if(visualDetails&HostVisualOre)demand|=Ore;
  uint32_t combined=0;for(unsigned bit=0;bit<6;++bit){const uint32_t flag=1u<<bit;if(demand&flag){demanded_|=flag;demandTicks_[bit]=now;}if((demanded_&flag)&&uint32_t(now-demandTicks_[bit])<=250)combined|=flag;}
  if(latest_&&base_==source&&builtRevision_==revision_&&(demand&~builtDemand_)==0){++statistics.reused;return latest_;}
  if(attempted_==source&&uint32_t(now-attemptedAt_)<16)return {};
  attempted_=source;attemptedAt_=now;
  const uint32_t started=GetTickCount();
  const auto before=visualStamp(live,*source);if(!before.valid){++statistics.failed;return {};}
  auto next=SnapshotPool::acquire(pool_);if(!next->extendVisual(source)){++statistics.failed;return {};}
  requests_.clear();leases_.append(now,requests_);std::sort(requests_.begin(),requests_.end());
  CaptureGraph<Live>capture(*next,live,scratch_);
  const bool collected=capture.capture(combined,requests_);
  const auto after=visualStamp(live,*source);
  if(!collected||!coherentVisualCapture(before,after,uint32_t(GetTickCount()-started))||!next->seal()){++statistics.failed;return {};}
  next->visualFrame=after.frame;next->visualTick=now;next->visualSerial=++serial_;
  base_=source;latest_=std::move(next);builtRevision_=revision_;builtDemand_=combined;++statistics.captures;return latest_;
 }
};
}
