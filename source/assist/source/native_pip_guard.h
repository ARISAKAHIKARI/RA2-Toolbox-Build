#pragma once
extern "C" int __cdecl AssistNativePipAction(unsigned,unsigned);
extern "C" int __cdecl AssistNativePipRestore();
extern "C" void __cdecl AssistNativePipUnlock();
extern "C" void __cdecl AssistNativePipFailed();
extern "C" void __cdecl AssistNativePipEnd();
extern "C" int __cdecl GuardedNativePipAction(unsigned action,unsigned argument){
 static unsigned depth=0;const unsigned long error=GetLastError();const unsigned token=BeginAssistCapture();if(!token){SetLastError(error);return 0;}
 if(depth){EndAssistCapture(token);SetLastError(error);return 0;}++depth;int result=0;
 __try {
  __try {result=AssistNativePipAction(action,argument);}
  __except(AssistExceptionFilter(_exception_info(),33)){AssistNativePipFailed();}
 }__finally {
  __try {if(!AssistNativePipRestore())AssistNativePipFailed();}
  __except(AssistExceptionFilter(_exception_info(),33)){AssistNativePipFailed();}
  // Each surface has its own exception boundary: one failing Unlock must not
  // strand the other surface or skip capture/depth cleanup.
  for(unsigned i=0;i<2;++i){
   __try {AssistNativePipUnlock();}
   __except(AssistExceptionFilter(_exception_info(),33)){AssistNativePipFailed();}
  }
  AssistNativePipEnd();--depth;EndAssistCapture(token);SetLastError(error);
 }return result;
}
