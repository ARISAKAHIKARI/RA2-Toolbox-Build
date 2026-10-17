#pragma once
#include <stdint.h>
namespace assist { namespace freshness {
// Source age includes acquisition, planning, transport AND queueing. A publish
// or retry may never renew a stamp without acquiring a new snapshot.
struct Limits {uint32_t milliseconds;int frames;};
constexpr Limits Tactical{150,6},Cooperative{250,12};
inline bool valid(uint32_t sourceMatch,uint32_t currentMatch,uint32_t sourceTick,
                  int sourceFrame,uint32_t now,int frame,Limits limit){
 return sourceMatch && sourceMatch==currentMatch && sourceFrame>=0 && frame>=sourceFrame &&
        int64_t(frame)-sourceFrame<=limit.frames && uint32_t(now-sourceTick)<=limit.milliseconds;
}
inline bool intentAlive(uint32_t created,uint32_t now){return uint32_t(now-created)<=5000;}
}}
