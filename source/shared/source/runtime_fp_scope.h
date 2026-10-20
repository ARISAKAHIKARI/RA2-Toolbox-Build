#pragma once
#include <stdint.h>
// A callback must not inherit a renderer's SSE rounding/denormal mode or a
// live x87 stack. Restore the complete caller state on exit; this is scoped
// isolation, not a global game FP-state patch. Native x87 control is retained.
struct alignas(16) RuntimeFPScope {
 uint8_t saved[512];
 uint32_t mxcsr=0x1F80;
 DWORD lastError=GetLastError();
 RuntimeFPScope(){
  auto* state=saved;auto* control=&mxcsr;
  __asm {
   mov eax,state
   fxsave [eax]
   fninit
   fldcw [eax]
   mov eax,control
   ldmxcsr [eax]
  }
 }
 ~RuntimeFPScope(){auto* state=saved;__asm {
  mov eax,state
  fxrstor [eax]
 }SetLastError(lastError);}
 RuntimeFPScope(const RuntimeFPScope&)=delete;
 RuntimeFPScope&operator=(const RuntimeFPScope&)=delete;
};
