#pragma once
#include <algorithm>
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
}
