#pragma once
#include <stdint.h>

namespace assist { namespace inputownership {
// Missing button-up messages must not own every keyboard shortcut forever.
// Require a sustained physical release; a delayed native UP still has time
// to commit normally. Recovery only cancels, never synthesizes an order.
class ReleaseWatch {
 uint32_t releasedAt_=0;bool observing_=false;
public:
 void clear(){observing_=false;}
 bool expired(bool captured,bool down,uint32_t now){
  if(!captured||down){clear();return false;}
  if(!observing_){observing_=true;releasedAt_=now;return false;}
  return uint32_t(now-releasedAt_)>=250;
 }
};
constexpr uint32_t LeaseMs=200;
// Zero is the unset sentinel. A deadline which lands exactly on zero is
// moved to one, extending only that wrap boundary by one millisecond.
constexpr uint32_t MaxRemaining=LeaseMs+1;
inline uint32_t afterGesture(uint32_t now){
 const uint32_t deadline=now+LeaseMs;
 return deadline?deadline:1;
}
inline bool ownsFormation(bool active,uint32_t previewMatch,uint32_t currentMatch,
                          uint32_t consumedUntil,uint32_t now){
 if(!currentMatch||previewMatch!=currentMatch)return false;
 if(active)return true;
 const uint32_t remaining=consumedUntil-now;
 return consumedUntil&&remaining&&remaining<=MaxRemaining;
}
}}
