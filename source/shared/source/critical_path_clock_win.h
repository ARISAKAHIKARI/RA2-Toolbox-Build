#pragma once
#include <windows.h>
#include <stdint.h>
#include "critical_path_timing.h"
// X86 game-thread only. Never block or query game state.
namespace criticalpath {
inline uint64_t ticks(){LARGE_INTEGER q{};return QueryPerformanceCounter(&q)&&q.QuadPart>0?uint64_t(q.QuadPart):0;}
inline uint32_t elapsedUs(uint64_t start){
 if(!start)return 0;const uint64_t stop=ticks();if(stop<start)return 0;
 static uint64_t frequency=[](){LARGE_INTEGER f{};return QueryPerformanceFrequency(&f)&&f.QuadPart>0?uint64_t(f.QuadPart):0;}();
 if(!frequency)return 0;
 const uint64_t difference=stop-start;
 const uint64_t us=(difference/frequency)*1000000u+(difference%frequency)*1000000u/frequency;
 return uint32_t(std::min<uint64_t>(us,UINT32_MAX));
}
}
