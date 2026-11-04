// A hardware exception is already being handled. Do not call the reporting
// filter here: argument classification may fail closed without allocation,
// another engine entry, logging or swallowing unrelated debug exceptions.
extern "C" int __cdecl AssistTechUiParameter(void*,unsigned);
extern "C" int __cdecl GuardedTechUiParameter(void*context,unsigned tag){
 __try {return AssistTechUiParameter(context,tag);}
 __except((_exception_code()==0xC0000005u||_exception_code()==0xC0000006u)?1:0){return 0;}
}

extern "C" int __stdcall AssistTechEnhancementProbeCall(unsigned,unsigned);
extern "C" unsigned BeginTechEnhancementProbe();extern "C" void EndTechEnhancementProbe(unsigned);
extern "C" int __cdecl GuardedTechEnhancementProbe(unsigned house,unsigned type){
 const unsigned long saved=GetLastError();const unsigned token=BeginTechEnhancementProbe();if(!token){SetLastError(saved);return 0;}int result=0;
 __try {__try {result=AssistTechEnhancementProbeCall(house,type);}
  __except((_exception_code()==0xC0000005u||_exception_code()==0xC0000006u)?1:0){result=0;}}
 __finally {EndTechEnhancementProbe(token);SetLastError(saved);}return result;
}
