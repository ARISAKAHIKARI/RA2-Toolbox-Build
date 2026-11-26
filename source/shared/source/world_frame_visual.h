#pragma once
#include "world_frame_capture.h"
#include "world_frame_pool.h"
#include <chrono>

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
 // Coalesce contiguous guard fields without sampling them from a page
 // cache. Both bookends still read the live game independently.
 std::array<uint32_t,2>cellHeader{};std::array<int,2>dimensions{};
 get(0xA8B230,out.scenario);get(0x87F7E8+0x13C,cellHeader);
 out.cells=cellHeader[0];out.count=cellHeader[1];
 get(0xA83D4C,out.house);get(0xA8ED84,out.frame);get(0xA8B238,out.session);get(0xA8E9A0,active);
 get(0x87F7E8+0xF4,out.bounds);get(0x87F7E8+0x1240,dimensions);
 out.rawWidth=dimensions[0];out.rawHeight=dimensions[1];
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
// Size-based pacing, not the monitor refresh rate. A light display graph
// can supply up to 100 observations/s; large graphs retain the existing
// ~60/s cap. The capture cost and native frame arrival may lower both rates.
// Failed attempts keep the independent 16 ms recovery throttle.
inline uint32_t visualReuseWindow(size_t pages){return pages<=256?10u:16u;}
class VisualCache {
 Token base_,latest_;std::weak_ptr<const Snapshot> validatedReuseSource_;std::shared_ptr<SnapshotPool>pool_=std::make_shared<SnapshotPool>();
 CaptureScratch scratch_;DemandLeases leases_;std::vector<uint32_t>requests_;
 VisualStamp latestStamp_,failedBuildStamp_;
 uint64_t failedBuildRevision_=0;uint32_t failedBuildDemand_=0;bool failedBuild_=false;
 uint64_t revision_=0,builtRevision_=0,serial_=0;uint32_t builtDemand_=0,attemptedAt_=0;Token attempted_;
 static constexpr std::array<uint32_t,8> VisualGraphDemands{Base,Combat,Passengers,Ore,Bullets,Visual,Resources,MonitorStats};
 std::array<uint32_t,VisualGraphDemands.size()>demandTicks_{};uint32_t demanded_=0;
public:
 struct Statistics {
  uint64_t captures=0,reused=0,failed=0;
  uint64_t buildInitial=0,buildRevision=0,buildProfile=0,buildRefresh=0,buildSameSource=0;
  uint64_t coveredLeases=0,repairedReports=0,retiredReports=0,deferredFailedBuilds=0;
 };Statistics statistics;
 // Report whether this request invalidated the built visual observation.
 // A full lease table can evict an expired page and admit a new one without
 // changing size; the insertion status, not the count, proves new demand.
 bool request(const std::vector<uint32_t>&pages,uint32_t now,bool retry,const Token&reported={}){
  // A reader can finish after the cache has moved to a different world.
  // Ignore its obsolete visual dependencies before touching the lease table.
  // Keep explicit requests and reports without an older visual serial intact.
  if(reported&&reported->controlSource&&latest_&&
     reported->visualSerial<latest_->visualSerial&&!reported->key.sameWorld(latest_->key)){++statistics.retiredReports;return false;}
  bool added=false;
  const bool sameVisualWorld=reported&&reported->controlSource&&latest_&&
    reported->key.sameWorld(latest_->key);
  for(uint32_t page:pages){
   if(leases_.retainStatus(page,now)!=DemandLeases::RetainResult::Added)continue;
   // A newly retained page may already be fully covered by graph discovery.
   // Keep its lease for future observations without rebuilding this one.
   // Explicit requests without a visual token retain the previous behavior.
   const auto*covered=sameVisualWorld?latest_->find(page):nullptr;
   if(!covered||!covered->full)added=true;else ++statistics.coveredLeases;
  }
  // A delayed display reader may report a miss after another reader has
  // already repaired it. Suppress only retry reports from an older sealed
  // visual token in this world, with positive full-page coverage in latest_.
  // Missing/partial pages, current-source reports and explicit retries keep
  // their original invalidation semantics.
  if(retry&&!pages.empty()&&reported&&reported->controlSource&&latest_&&
     reported->visualSerial<latest_->visualSerial&&reported->key.sameWorld(latest_->key)){
   bool repaired=false,covered=true;
   for(uint32_t page:pages){const auto*old=reported->find(page);const auto*next=latest_->find(page);
    repaired|=!old||!old->full;covered&=next&&next->full;if(!covered)break;
   }
   if(repaired&&covered){retry=false;++statistics.repairedReports;}
  }
  if(added||retry){++revision_;return true;}
  return false;
 }
 template<class Live>Token acquire(const Token&source,uint32_t demand,uint32_t now,Live live){
  if(!source||source->controlSource||!source->sealed())return {};
  if((base_&&!base_->key.sameWorld(source->key))||(!base_&&attempted_&&!attempted_->key.sameWorld(source->key))){latest_.reset();base_.reset();attempted_.reset();validatedReuseSource_.reset();leases_.clear();revision_=builtRevision_=0;builtDemand_=demanded_=0;failedBuild_=false;}
  // Host visual profiles are NOT Runtime command demand. Only this
  // read-only Host cache expands them into actor/passenger/combat graph
  // ranges; the published game-thread boundary remains small.
  const uint32_t visualDetails=demand&uint32_t(HostVisualPassengers|HostVisualCombat|HostVisualOre);
  demand=(demand&~uint32_t(HostVisualPassengers|HostVisualCombat|HostVisualOre|HostVisualBase))|Base;
  if(visualDetails&HostVisualPassengers)demand|=Passengers;
  if(visualDetails&HostVisualCombat)demand|=Combat;
  if(visualDetails&HostVisualOre)demand|=Ore;
  // Resources and MonitorStats are explicit Host graph profiles, not Runtime demand.
  // Retain it in the same lease union so repeated panel reads do not fault
  // the same factory/count pages back into the next visual capture.
  uint32_t combined=0;for(size_t bit=0;bit<VisualGraphDemands.size();++bit){const uint32_t flag=VisualGraphDemands[bit];if(demand&flag){demanded_|=flag;demandTicks_[bit]=now;}if((demanded_&flag)&&uint32_t(now-demandTicks_[bit])<=250)combined|=flag;}
  const bool compatible=latest_&&builtRevision_==revision_&&(demand&~builtDemand_)==0;
  if(compatible&&base_==source){++statistics.reused;return latest_;}
  // Only zero-page read-only boundaries may share a successful guard result.
  // Do not extend the original visual age or retain another full snapshot.
  if(compatible&&boundaryOnlySource(source->key)&&validatedReuseSource_.lock()==source&&
     uint32_t(now-latest_->visualTick)<visualReuseWindow(latest_->pages.size())){++statistics.reused;return latest_;}
  validatedReuseSource_.reset();
  if(attempted_==source&&uint32_t(now-attemptedAt_)<16)return {};
  const auto started=std::chrono::steady_clock::now();
  const auto before=visualStamp(live,*source);
  if(!before.valid){attempted_=source;attemptedAt_=now;failedBuild_=false;++statistics.failed;return {};}
  // Display sampling is deliberately lower priority than Runtime commands.
   // Reuse a coherent same-world visual snapshot across nearby Runtime frames;
   // never retag its original frame or treat it as command authority.
  if(compatible&&base_&&base_->key.sameWorld(source->key)&&
      source->key.frame>=base_->key.frame&&before.frame>=latestStamp_.frame&&
      uint32_t(now-latest_->visualTick)<visualReuseWindow(latest_->pages.size())&&latestStamp_.sameWorld(before)){
   if(boundaryOnlySource(source->key))validatedReuseSource_=source;
   ++statistics.reused;return latest_;
  }
  // A failed read-only build must not retry a full graph on every new
  // boundary token at high frame rates. Preserve immediate attempts for
  // changed geometry, world, demand or explicit dependency revisions.
  if(failedBuild_&&attempted_&&boundaryOnlySource(attempted_->key)&&
     boundaryOnlySource(source->key)&&attempted_->key.sameWorld(source->key)&&
     source->key.frame>=attempted_->key.frame&&before.frame>=failedBuildStamp_.frame&&
     failedBuildStamp_.sameWorld(before)&&revision_==failedBuildRevision_&&
     (combined&~failedBuildDemand_)==0&&uint32_t(now-attemptedAt_)<16){
   ++statistics.deferredFailedBuilds;return {};
  }
  // Actual graph build attempts: mutually exclusive reasons, with same-source
  // counted independently. Guard failures before this point are not builds.
  if(!latest_)++statistics.buildInitial;
  else if(builtRevision_!=revision_)++statistics.buildRevision;
  else if(demand&~builtDemand_)++statistics.buildProfile;
  else ++statistics.buildRefresh;
  if(base_==source)++statistics.buildSameSource;
  attempted_=source;attemptedAt_=now;
  failedBuild_=true;failedBuildStamp_=before;failedBuildRevision_=revision_;failedBuildDemand_=combined;
  auto next=SnapshotPool::acquire(pool_);if(!next->extendVisual(source)){++statistics.failed;return {};}
  requests_.clear();leases_.append(now,requests_);std::sort(requests_.begin(),requests_.end());
  CaptureGraph<Live>capture(*next,live,scratch_);
  // Recently demanded visual pages often overlap the graph's Type/House/
  // Cell discovery. Warm their contiguous runs BEFORE pointer traversal so
  // those pages do not each incur a separate RPM followed by a cache hit.
  // Only this Host-owned unsealed visual capture uses the warmup. Failed
  // speculative spans publish no bytes; graph and final requested-page
  // recovery still use the existing per-page/partial-range policy.
  next->prefetchPages(live,requests_,scratch_.pageScratch.data(),scratch_.pageScratch.size(),false,32);
  const bool collected=!next->overflow()&&capture.capture(combined,requests_);
  const auto after=visualStamp(live,*source);
  if(!collected||!coherentVisualCapture(before,after,uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count()))||!next->seal()){++statistics.failed;return {};}
  next->visualFrame=after.frame;next->visualTick=now;next->visualSerial=++serial_;
  failedBuild_=false;base_=source;latest_=std::move(next);latestStamp_=after;builtRevision_=revision_;builtDemand_=combined;++statistics.captures;return latest_;
 }
};
}
