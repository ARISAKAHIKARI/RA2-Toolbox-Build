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
} // namespace worldframe
