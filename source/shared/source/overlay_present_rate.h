#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
namespace overlaypresentrate {
constexpr int MinHz=5,MaxHz=500;
// 0/1 in DEVMODE mean unspecified/default. An unknown monitor never silently
// changes the user's requested rate to 60 Hz.
inline int confirmed(uint32_t hz){return hz<MinHz?0:int(std::min(hz,uint32_t(MaxHz)));}
inline int limit(int requested,int monitorHz){
 const int wanted=std::clamp(requested,MinHz,MaxHz);
 return monitorHz>=MinHz?std::min(wanted,std::min(monitorHz,MaxHz)):wanted;
}
inline uint64_t periodMicros(int hz){
 const uint64_t rate=uint64_t(std::clamp(hz,MinHz,MaxHz));
 return (1000000ull+rate-1)/rate;
}
// Prepare the next immutable image immediately, then wait only before upload.
// Anchor to the last actual attempt, never a backlog of missed deadlines.
class Cadence {
 using Clock=std::chrono::steady_clock;
 Clock::time_point last_{};bool valid_=false;
public:
 Clock::time_point deadline(Clock::time_point now,int hz)const{
  return valid_?std::max(now,last_+std::chrono::microseconds(periodMicros(hz))):now;
 }
 void submitted(Clock::time_point now){last_=now;valid_=true;}
};
}
