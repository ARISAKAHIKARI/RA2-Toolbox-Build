#include "gpu_fault_guard.h"
extern "C" void* _exception_info();
struct GpuExceptionRecord {unsigned code,flags;void*nested;void*address;unsigned count;RA2GpuAddress information[15];};
struct GpuExceptionPointers {GpuExceptionRecord*record;void*context;};
static_assert(sizeof(void*)==8&&sizeof(GpuExceptionRecord)==152,"Windows x64 SEH ABI");
// Keep SEH in a small MSVC-ABI C wrapper. No C++ object with a destructor
// lives in its guarded scope. A failed driver is quarantined, never retried.
static int captureGpuFault(void*raw,RA2GpuFault*fault){
 const auto*pointers=static_cast<GpuExceptionPointers*>(raw);const auto*record=pointers?pointers->record:nullptr;if(!record)return 0;const unsigned code=record->code;
 if(code!=0xC0000005u&&code!=0xC0000006u&&code!=0xC000001Du)return 0;
 if(fault){fault->code=code;fault->address=reinterpret_cast<RA2GpuAddress>(record->address);}return 1;
}
extern "C" int RA2GuardGpuCall(int(*call)(void*),void*context,RA2GpuFault*fault){
 __try{return call(context);}__except(captureGpuFault(_exception_info(),fault)){return -1;}
}
