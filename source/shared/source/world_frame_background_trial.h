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
struct BackgroundTrialResult {
 BackgroundTrialStatus status=BackgroundTrialStatus::InvalidBoundary;
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
BackgroundTrialResult backgroundTrial(Stamp stamp,Read read,uint32_t demand,
                                       const std::vector<uint32_t>&requests={}){
 BackgroundTrialResult out{};
 out.before=stamp();++out.frameChecks;
 if(!out.before.pid||out.before.house<0x10000||out.before.scenario<0x10000||
    out.before.cells<0x10000||!out.before.match||out.before.frame<0)return out;
 // Real production CaptureGraph executes on the calling worker thread.
 // The worker snapshot remains private; no command token escapes this call.
 Snapshot sample;
 sample.reset(out.before);
 CaptureScratch scratch;
 CaptureGraph<Read> graph(sample,read,scratch);
 const bool complete=graph.capture(demand,requests);
 const bool sealed=sample.seal();
 out.counters=sample.counters;out.pages=uint32_t(sample.pages.size());
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
} // namespace worldframe
