#pragma once
#include <stdint.h>
// Pure, platform-independent cursor visibility state transitions.
// HideCursor decrements the known game cursor's reference count by exactly 1.
// If the game later calls ShowCursor, the count rises above our hidden target.
// Re-hide ONLY the increase, so one final paired ShowCursor can restore safely.
namespace hidewatch {
constexpr int32_t maxReference=32;
constexpr int32_t maxPerTick=3;
inline int32_t required(int32_t observed,int32_t hidden){
 if(observed< -maxReference||observed>maxReference)return -1;
 if(hidden < -maxReference||hidden>=maxReference)return -1;
 if(observed<=hidden)return 0;
 int32_t drift=observed-hidden;
 return drift>maxPerTick?maxPerTick:drift;
}
// This MUST be called only after the real game HideCursor invocation; it
// validates the observed decrement and prevents unbounded repeated hides.
inline bool oneHideWasPaired(int32_t before,int32_t after){return after==before-1;}
}
