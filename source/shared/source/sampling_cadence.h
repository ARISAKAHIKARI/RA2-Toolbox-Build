#pragma once
#include <cstdint>

namespace samplingcadence {
constexpr uint32_t NormalPulseMs=4, FastLocalPulseMs=2;

// Observe the existing control frame counter. Rendering FPS and observation
// sample FPS are independent of the actual simulation rate.
class LocalRate {
 uint32_t match_=0,tick_=0;
 int frame_=-1;
 bool fast_=false;
public:
 bool update(bool localActive,uint32_t match,int frame,uint32_t now){
  if(!localActive||!match||frame<0){*this={};return false;}
  if(match_!=match||frame<frame_){match_=match;tick_=now;frame_=frame;fast_=false;return false;}
  const uint32_t elapsed=now-tick_;
  if(elapsed<250)return fast_;
  const uint64_t framesPerSecondScaled=uint64_t(frame-frame_)*1000;
  const uint32_t threshold=fast_?100:140;
  fast_=elapsed<=1000&&framesPerSecondScaled>=uint64_t(threshold)*elapsed;
  tick_=now;frame_=frame;
  return fast_;
 }
};
}
