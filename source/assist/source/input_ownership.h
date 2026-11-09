#pragma once
#include <stdint.h>

namespace assist { namespace inputownership {
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
