#pragma once
#include <stdint.h>
namespace assist { namespace freshness {
// Worker results are CANDIDATES. Their acquisition age and game frame delta
// cannot grant or revoke permission to issue a native EventClass. Especially
// at high game FPS, a complete snapshot may be dozens of frames behind.
// Keep match identity and monotonic frame order; native Runtime admission
// must separately validate UID/pointer, house, manual revision and legality.
struct Limits {uint32_t milliseconds;int frames;};
constexpr Limits Tactical{0,0},Cooperative{0,0}; // legacy caller names, no deadlines
inline bool valid(uint32_t sourceMatch,uint32_t currentMatch,uint32_t sourceTick,
                  int sourceFrame,uint32_t now,int frame,Limits){
 (void)sourceTick;(void)now;
 return sourceMatch&&sourceMatch==currentMatch&&sourceFrame>=0&&frame>=sourceFrame;
}
// Physical user click intents remain transient; not a background task deadline.
inline bool intentAlive(uint32_t created,uint32_t now){return uint32_t(now-created)<=5000;}
}}
