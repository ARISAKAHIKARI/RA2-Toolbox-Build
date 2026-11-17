#pragma once
#include <stdint.h>
#include "critical_path_timing.h"
// The production stopwatch is Windows-only. The portable DR service fixture
// extracts the real Service with a mocked Win32 boundary, and never calls
// the timing sink; stub clocks keep that fixture's exception logic identical.
#ifdef _WIN32
#include <windows.h>
namespace criticalpath {
inline uint64_t ticks(){LARGE_INTEGER q{};return QueryPerformanceCounter(&q)&&q.QuadPart>0?uint64_t(q.QuadPart):0;}
inline uint32_t elapsedUs(uint64_t start){
 if(!start)return 0;const uint64_t stop=ticks();if(stop<start)return 0;
 // Zero-initialized scalar: no local-static guard, mutex or CRT lazy
 // initialization inside the first sampled hardware exception callback.
 static uint64_t frequency=0;
 if(!frequency){LARGE_INTEGER f{};if(QueryPerformanceFrequency(&f)&&f.QuadPart>0)frequency=uint64_t(f.QuadPart);}
 if(!frequency)return 0;
 const uint64_t difference=stop-start;
 const uint64_t us=(difference/frequency)*1000000u+(difference%frequency)*1000000u/frequency;
 return uint32_t(std::min<uint64_t>(us,UINT32_MAX));
}
}
#else
namespace criticalpath {
inline uint64_t ticks(){return 0;}
inline uint32_t elapsedUs(uint64_t){return 0;}
}
#endif
