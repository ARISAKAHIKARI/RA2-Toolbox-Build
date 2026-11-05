#include "exception_policy.h"
extern "C" void* _exception_info();
// A hardware exception is already being handled. Do not call the reporting
// filter here: argument classification may fail closed without allocation,
// another engine entry, logging or swallowing unrelated debug exceptions.
extern "C" int __cdecl AssistTechUiParameter(void*,unsigned);
extern "C" int __cdecl GuardedTechUiParameter(void*context,unsigned tag){
 const DWORD saved=GetLastError();int result=0;
 __try {__try {result=AssistTechUiParameter(context,tag);}
  __except(assistguard::noReportAccess(_exception_info())){result=0;}}
 __finally {SetLastError(saved);}return result;
}

extern "C" int __stdcall AssistTechEnhancementProbeCall(unsigned,unsigned);
extern "C" unsigned BeginTechEnhancementProbe();extern "C" void EndTechEnhancementProbe(unsigned);
extern "C" int __cdecl GuardedTechEnhancementProbe(unsigned house,unsigned type){
 const unsigned long saved=GetLastError();const unsigned token=BeginTechEnhancementProbe();if(!token){SetLastError(saved);return 0;}int result=0;
 __try {__try {result=AssistTechEnhancementProbeCall(house,type);}
  __except(assistguard::noReportAccess(_exception_info())){result=0;}}
 __finally {EndTechEnhancementProbe(token);SetLastError(saved);}return result;
}
