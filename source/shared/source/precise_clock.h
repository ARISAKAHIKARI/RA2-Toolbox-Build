#pragma once
#include <cstdint>
// Display/sampling cadence only. Command timestamps and heartbeat TTLs must
// retain the shared GetTickCount timebase in clock_ms.h.
namespace preciseclock {
inline uint64_t micros(){
 // Callers are serialized by the host scheduler mutex or the Runtime's
 // game-thread Capture. Constant initialization avoids a CRT guard in DLL.
 static uint64_t frequency=0;
 if(!frequency){LARGE_INTEGER f{};if(QueryPerformanceFrequency(&f)&&f.QuadPart>0)frequency=uint64_t(f.QuadPart);}
 LARGE_INTEGER t{};
 if(frequency&&QueryPerformanceCounter(&t)&&t.QuadPart>=0){const uint64_t v=uint64_t(t.QuadPart);return (v/frequency)*1000000+(v%frequency)*1000000/frequency;}
 return uint64_t(GetTickCount())*1000;
}
}
