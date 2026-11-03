#pragma once
extern "C" int __cdecl AssistNativeViewAction(unsigned,unsigned);
extern "C" void __cdecl AssistNativeViewFailed();
extern "C" void __cdecl AssistNativeViewUnlockOne();
extern "C" int __cdecl GuardedNativeViewAction(unsigned action,unsigned argument){
 const unsigned long savedError=GetLastError();int result=0;const unsigned token=BeginAssistCapture();
 if(!token){SetLastError(savedError);return 0;}
 __try {
  __try {result=AssistNativeViewAction(action,argument);}
  __except(AssistExceptionFilter(_exception_info(),22)){AssistNativeViewFailed();}
 }__finally {
  for(unsigned n=0;n<2;++n){
   __try {AssistNativeViewUnlockOne();}
   __except(AssistExceptionFilter(_exception_info(),22)){AssistNativeViewFailed();}
  }
  EndAssistCapture(token);SetLastError(savedError);
 }return result;
}
// Pure bounded caller-output/Runtime-state operations. They do not invoke the
// engine or mutate global game memory, so another client's Capture cannot
// force a scaled mouse point to silently fall back to physical coordinates.
extern "C" unsigned __cdecl AssistNativeViewWndEnter(unsigned,unsigned);
extern "C" unsigned __cdecl AssistNativeViewWndLeave();
extern "C" int __cdecl AssistNativeViewMousePoint(void*);
extern "C" unsigned __cdecl GuardedNativeViewWndEnter(unsigned argument,unsigned returnAddress){
 const unsigned long savedError=GetLastError();unsigned result=~0u;
 __try {result=AssistNativeViewWndEnter(argument,returnAddress);}
 __except((_exception_code()==0xC0000005u||_exception_code()==0xC0000006u)?1:0){result=~0u;}
 SetLastError(savedError);return result;
}
extern "C" unsigned __cdecl GuardedNativeViewWndLeave(){
 const unsigned long savedError=GetLastError();const unsigned result=AssistNativeViewWndLeave();SetLastError(savedError);return result;
}
extern "C" int __cdecl GuardedNativeViewMousePoint(void*point){
 const unsigned long savedError=GetLastError();int result=0;
 __try {result=AssistNativeViewMousePoint(point);}
 __except((_exception_code()==0xC0000005u||_exception_code()==0xC0000006u)?1:0){result=0;}
 SetLastError(savedError);return result;
}
