// A hardware exception is already being handled. Do not call the reporting
// filter here: argument classification may fail closed without allocation,
// another engine entry, logging or swallowing unrelated debug exceptions.
extern "C" int __cdecl AssistTechUiParameter(void*,unsigned);
extern "C" int __cdecl GuardedTechUiParameter(void*context,unsigned tag){
 __try {return AssistTechUiParameter(context,tag);}
 __except((_exception_code()==0xC0000005u||_exception_code()==0xC0000006u)?1:0){return 0;}
}
