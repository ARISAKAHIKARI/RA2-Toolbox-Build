#pragma once
extern "C" int __cdecl AssistNativePipAction(unsigned,unsigned);
extern "C" int __cdecl AssistNativePipRestore();
extern "C" void __cdecl AssistNativePipUnlock();
extern "C" void __cdecl AssistNativePipFailed();
extern "C" void __cdecl AssistNativePipEnd();
// Keep each recovery handler in its own frame. Nested __except scopes inside
// an x86 MSVC __finally can invalidate its parent-frame access after a second
// fault (e.g. the borrowed Tactical page is still read-only during cleanup).
__declspec(noinline) static int nativePipGuardRun(unsigned action,unsigned argument){
 __try {return AssistNativePipAction(action,argument);}
 __except(AssistExceptionFilter(_exception_info(),33)){AssistNativePipFailed();return 0;}
}
__declspec(noinline) static int nativePipGuardRestore(){
 __try {return AssistNativePipRestore();}
 __except(AssistExceptionFilter(_exception_info(),33)){return 0;}
}
__declspec(noinline) static int nativePipGuardUnlock(){
 __try {AssistNativePipUnlock();return 1;}
 __except(AssistExceptionFilter(_exception_info(),33)){return 0;}
}
extern "C" int __cdecl GuardedNativePipAction(unsigned action,unsigned argument){
 static unsigned depth=0;const unsigned long error=GetLastError();const unsigned token=BeginAssistCapture();if(!token){SetLastError(error);return 0;}
 if(depth){EndAssistCapture(token);SetLastError(error);return 0;}++depth;int result=0;
 __try {result=nativePipGuardRun(action,argument);}
 __finally {
  if(!nativePipGuardRestore()){AssistNativePipFailed();result=0;}
  // One failed Unlock must not strand the other surface or capture token.
  for(unsigned i=0;i<2;++i)if(!nativePipGuardUnlock()){AssistNativePipFailed();result=0;}
  AssistNativePipEnd();--depth;EndAssistCapture(token);SetLastError(error);
 }return result;
}
