#pragma once
#include <stdint.h>
// Define exactly once in the GNU Runtime TU, and separately in its Windows
// fixture. The MSVC SEH wrapper must call an opaque, non-noexcept function:
// plain inline loads alone can lose their __try region under optimization.
// Do not call this leaf without GuardedReadObjectIdentity's access boundary.
extern "C" unsigned AssistReadObjectIdentity(unsigned object){
 return *reinterpret_cast<const volatile unsigned*>(uintptr_t(object)+0x10u);
}
