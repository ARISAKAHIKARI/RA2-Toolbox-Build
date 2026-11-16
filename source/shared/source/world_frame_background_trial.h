#pragma once
#include "world_frame_capture.h"

// A background Graph is an approximate, ordered DISCOVERY view. It need not
// describe one exact game logic frame: nearby frames may mix while the graph
// is traversed. The *game/world identity* must remain stable, age bounded,
// and results must arrive in chronological order.
//
// This is not an authoritative command source. Consumers that submit native
// events must still confirm live UID, object ownership, manual revisions,
// mission, target and native eligibility at admission time.
namespace worldframe {
struct BackgroundGraphPolicy {
 static constexpr uint32_t MaxCaptureMs=120;
 static constexpr int MaxCaptureFrames=6;
 static constexpr uint32_t MaxDeliveryMs=160;
 static constexpr int MaxDeliveryFrames=8;
};
enum class BackgroundTrialStatus:uint8_t {
 InvalidBoundary, CaptureFailure, WorldChanged, TooOld, OutOfOrder, CandidateOnly
};
// Read-only projection: no native object addresses, UID membership, actions,
// EventClass, or pointers may escape the worker's private Snapshot. The source
// is an approximate cross-frame discovery, never command authority.
struct BackgroundGraphFacts {
 uint32_t visited=0,actors=0,owned=0,other=0,alive=0,dead=0;
 uint32_t missingIdentity=0,unknownVtable=0;
 uint32_t units=0,infantry=0,aircraft=0,buildings=0;
};
inline BackgroundGraphFacts backgroundFacts(const Snapshot&sample,
                                             const CaptureScratch&scratch){
 BackgroundGraphFacts out{};
 if(!sample.sealed()||sample.overflow())return out;
 for(uint32_t p:scratch.queue){
  ++out.visited;
  uint32_t vt=0,uid=0,owner=0;int32_t hp=0;
  uint8_t onMap=0,limbo=0,alive=0;
  if(!sample.read(p,&vt,4)){++out.missingIdentity;continue;}
  const int kind=vt==0x7F5C70u?2:vt==0x7EB058u?1:
                 vt==0x7E3EBCu?3:vt==0x7E22A4u?0:-1;
  if(kind<0){++out.unknownVtable;continue;}
  if(!sample.read(p+0x10,&uid,4)||!sample.read(p+0x21C,&owner,4)||
     !sample.read(p+0x6C,&hp,4)||!sample.read(p+0x74,&onMap,1)||
     !sample.read(p+0x81,&limbo,1)||!sample.read(p+0x90,&alive,1)||
     !uid||owner<0x10000){++out.missingIdentity;continue;}
  ++out.actors;
  if(owner==sample.key.house)++out.owned;else ++out.other;
  if(hp>0&&onMap&&!limbo&&alive)++out.alive;else ++out.dead;
  if(kind==0)++out.infantry;else if(kind==1)++out.units;
  else if(kind==2)++out.aircraft;else ++out.buildings;
 }
 return out;
}
struct BackgroundTrialResult {
 BackgroundTrialStatus status=BackgroundTrialStatus::InvalidBoundary;
 BackgroundGraphFacts facts{};
 Key before{},after{};
 Counters counters{};
 uint32_t pages=0;
 // The caller owns elapsed-clock measurement and schedules the worker.
 uint32_t frameChecks=0;
 bool usableForCommands()const{return false;}
};
// Validate a completed approximate observation against a CURRENT independent
// world stamp. Prevent old workers from overwriting newer graph discoveries.
// Same-frame results need a strictly newer timestamp to advance sequence.
class BackgroundGraphOrder {
 Key last_{};
 bool hasLast_=false;
public:
 void reset(){last_={};hasLast_=false;}
 bool admit(const BackgroundTrialResult&candidate,const Key&current){
  if(candidate.status!=BackgroundTrialStatus::CandidateOnly||
     !candidate.before.sameWorld(candidate.after)||
     !candidate.after.sameWorld(current)||
     current.frame<candidate.after.frame||
     current.frame-candidate.after.frame>BackgroundGraphPolicy::MaxDeliveryFrames||
     uint32_t(current.tick-candidate.after.tick)>BackgroundGraphPolicy::MaxDeliveryMs)return false;
  if(hasLast_){
   if(!last_.sameWorld(current)){reset();}
   else if(candidate.after.frame<last_.frame||
     (candidate.after.frame==last_.frame&&
      (candidate.after.tick==last_.tick||
       uint32_t(candidate.after.tick-last_.tick)>BackgroundGraphPolicy::MaxDeliveryMs)))return false;
  }
  last_=candidate.after;hasLast_=true;return true;
 }
};
template<class Stamp,class Read>
BackgroundTrialResult backgroundTrialWithStorage(Stamp stamp,Read read,uint32_t demand,
                Snapshot&sample,CaptureScratch&scratch,const std::vector<uint32_t>&requests={}){
 BackgroundTrialResult out{};
 out.before=stamp();++out.frameChecks;
 if(!out.before.pid||out.before.house<0x10000||out.before.scenario<0x10000||
    out.before.cells<0x10000||!out.before.match||out.before.frame<0)return out;
 // The worker retains its page capacity and traversal storage across
 // observations. The reset invalidates every previous byte/index, so this
 // is storage reuse, never stale-observation reuse or command authority.
 sample.reset(out.before);
 CaptureGraph<Read> graph(sample,read,scratch);
 const bool complete=graph.capture(demand,requests);
 const bool sealed=sample.seal();
 out.counters=sample.counters;out.pages=uint32_t(sample.pages.size());
 // Derive facts from the sealed read-set, never by touching native memory a
 // second time. Partial fields stay unknown, not zero or a dead object.
 if(complete&&sealed&&!sample.overflow())out.facts=backgroundFacts(sample,scratch);
 out.after=stamp();++out.frameChecks;
 if(!out.before.sameWorld(out.after)){
  out.status=BackgroundTrialStatus::WorldChanged;return out;
 }
 if(out.after.frame<out.before.frame){
  out.status=BackgroundTrialStatus::OutOfOrder;return out;
 }
 // Clock subtraction is wrap-safe for short windows. Rewind or excessive
 // capture duration fails closed, without asking the game thread to wait.
 if(out.after.frame-out.before.frame>BackgroundGraphPolicy::MaxCaptureFrames||
    uint32_t(out.after.tick-out.before.tick)>BackgroundGraphPolicy::MaxCaptureMs){
  out.status=BackgroundTrialStatus::TooOld;return out;
 }
 out.status=complete&&sealed&&!sample.overflow()?
   BackgroundTrialStatus::CandidateOnly:BackgroundTrialStatus::CaptureFailure;
 return out;
}
// Standalone test/one-shot callers still receive private buffers; the
// production long-lived worker calls backgroundTrialWithStorage to avoid
// repeated 4 KiB page-node and traversal-array allocations.
template<class Stamp,class Read>
BackgroundTrialResult backgroundTrial(Stamp stamp,Read read,uint32_t demand,
                                       const std::vector<uint32_t>&requests={}){
 Snapshot sample;
 CaptureScratch scratch;
 return backgroundTrialWithStorage(stamp,read,demand,sample,scratch,requests);
}
} // namespace worldframe
