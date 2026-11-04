#pragma once
#if defined(_WIN64) || defined(__LP64__)
using RA2GpuAddress=unsigned long long;
#else
using RA2GpuAddress=unsigned;
#endif
struct RA2GpuFault {unsigned code=0;RA2GpuAddress address=0;};
#if defined(_WIN32) && defined(RA2_GPU_FAULT_GUARD)
extern "C" int RA2GuardGpuCall(int(*call)(void*),void*context,RA2GpuFault*fault);
#else
inline int RA2GuardGpuCall(int(*call)(void*),void*context,RA2GpuFault*){return call(context);}
#endif
