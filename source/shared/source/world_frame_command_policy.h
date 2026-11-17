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
