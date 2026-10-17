#pragma once
#include <stdint.h>
namespace scheduler {
enum Mode : uint32_t { Message=0, GameFrame=1 };
enum Status : int32_t { Pending=0, MessageActive=1, FrameActive=2, FrameInstalled=3, FrameStalled=4,
                         FrameUnsupported=-1, FrameConflict=-2,
                         FrameAllocationFailed=-3, FramePatchFailed=-4, FrameWrongThread=-5 };
// Frames choose the commit point for host-granted work. Message is the backup
// if the game pauses/changes modes and frame ticks are no longer observed.
inline bool messageMayPoll(Mode requested, Status status, uint32_t now, uint32_t lastFrameTick) {
    return requested==Message || status!=FrameActive || uint32_t(now-lastFrameTick)>250u;
}
inline bool wantsFrame(Mode requested, bool verified, bool stopping) {
    return requested==GameFrame && verified && !stopping;
}
}
