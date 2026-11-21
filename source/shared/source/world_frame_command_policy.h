#pragma once
#include "world_frame.h"

namespace worldframe {
// Only the original game dispatcher may consume a completed capture. This
// identity/age/revision check intentionally disallows even one frame of slack:
// worker data from a later or earlier simulation tick is not command authority.
inline bool acceptCommandCapture(const Key&captured,const Key&current,
 uint32_t capturedDemand,uint32_t requiredDemand,
 uint64_t capturedRevision,uint64_t currentRevision,
 uint32_t now,uint32_t maxAgeMs=80){
 return captured.pid&&captured.owner&&captured.match&&
        captured.sameFrame(current)&&
        (requiredDemand&~capturedDemand)==0&&
        capturedRevision==currentRevision&&
        uint32_t(now-captured.tick)<=maxAgeMs;
}
// OBSERVATION ONLY. A completed background graph may span more than one
// game frame; that makes it useful for candidate discovery but NEVER a native
// EventClass authorization. It must not be passed to acceptCommandCapture,
// nor used as proof that an actor still exists. Individual native actions must
// re-read current UID, owner, manual version and eligibility.
// A mutable Host dependency revision does not change candidate provenance;
// it still invalidates the strict command authority above.
inline bool acceptObservationCandidate(const Key&captured,const Key&current,
 uint32_t capturedDemand,uint32_t requiredDemand,uint32_t now,
 uint32_t maxAgeMs=500){
 return captured.pid&&captured.owner&&captured.match&&
        captured.sameWorld(current)&&captured.frame<=current.frame&&
        (requiredDemand&~capturedDemand)==0&&
        uint32_t(now-captured.tick)<=maxAgeMs;
}
// Only suppress redundant *work submission*, not permission to reuse a
// captured graph. A worker still busy on the same world/revision must not be
// cancelled every 16ms when the Game frame advances. Exact-frame take() remains
// strict; this can reduce background churn but cannot authorize late events.
inline bool coalesceInFlightWorld(const Key&previous,const Key&current,
 uint32_t previousDemand,uint32_t demand,uint64_t previousRevision,uint64_t revision,
 bool previousGlobal,bool global,uint32_t elapsed,bool outstanding,uint32_t windowMs=80){
 return outstanding&&previous.pid&&previous.sameWorld(current)&&
        previousDemand==demand&&previousRevision==revision&&
        previousGlobal==global&&elapsed<windowMs;
}
// Dispatcher-side throttling: skip a repeat before rebuilding a potentially
// 16K-page request vector. This is only a request coalescer, not capture
// authority. New frame/identity, demand, revision or global-candidate mode
// always dispatches immediately; failed submission is never recorded.
struct CommandRequestGate {
 Key key{};
 uint32_t demand=0;
 uint64_t revision=0;
 bool globalVehicles=false;
 void reset(){key={};demand=0;revision=0;globalVehicles=false;}
 bool defer(const Key&current,uint32_t required,uint64_t currentRevision,
            bool currentGlobal,uint32_t now,uint32_t retryMs=16)const{
  return key.pid&&key.sameFrame(current)&&demand==required&&
         revision==currentRevision&&globalVehicles==currentGlobal&&
         uint32_t(now-key.tick)<retryMs;
 }
 void submitted(const Key&current,uint32_t required,uint64_t currentRevision,
                bool currentGlobal){
  key=current;demand=required;revision=currentRevision;globalVehicles=currentGlobal;
 }
};
} // namespace worldframe
