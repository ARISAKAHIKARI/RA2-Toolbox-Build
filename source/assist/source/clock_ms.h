#pragma once
#include "protocol.h"
namespace assist {
inline uint32_t counterTick(uint32_t tick,uint64_t start,uint64_t frequency,uint64_t current,uint32_t fallback){
 if(!frequency||current<start)return fallback;
 uint64_t delta=current-start;return tick+uint32_t((delta/frequency)*1000+(delta%frequency)*1000/frequency);
}
#ifdef _WIN32
// Authoritative command, heartbeat, and receipt clock: always shares the same
// GetTickCount() timebase across host and bridge so sub-16ms QPC drift can
// never underflow unsigned elapsed checks into -7 (Expired).
inline uint32_t sampleClock(const Shared*){
 return GetTickCount();
}
// Display time is host-local and independent of injection/handshake. Command
// time remains GetTickCount above. Sample admission and ACK TTL must also use
// GetTickCount: a producer timestamp cannot be aged against this interpolation clock.
inline uint32_t sampleRenderClock(const Shared*){
 struct Origin {LARGE_INTEGER start{},frequency{};uint32_t tick=GetTickCount();
  Origin(){if(!QueryPerformanceFrequency(&frequency)||!QueryPerformanceCounter(&start))frequency.QuadPart=0;}
 };
 static const Origin origin;LARGE_INTEGER current{};
 const uint32_t fallback=GetTickCount();
 if(origin.frequency.QuadPart>0&&QueryPerformanceCounter(&current))
  return counterTick(origin.tick,uint64_t(origin.start.QuadPart),uint64_t(origin.frequency.QuadPart),uint64_t(current.QuadPart),fallback);
 return fallback;
}
#endif
}
