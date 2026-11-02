// Native layout/surface work may fail independently of the command dispatcher.
extern "C" int __cdecl AssistSidebarViewAction(unsigned,unsigned);
extern "C" void AssistSidebarViewFailed();
extern "C" int __cdecl GuardedSidebarViewAction(unsigned action,unsigned argument){
 const unsigned long savedError=GetLastError();int result=0;
 const unsigned token=BeginAssistCapture();if(!token){SetLastError(savedError);return 0;}
 __try {
  __try {result=AssistSidebarViewAction(action,argument);}
  __except(AssistExceptionFilter(_exception_info(),19)){AssistSidebarViewFailed();}
 }__finally {EndAssistCapture(token);SetLastError(savedError);}
 return result;
}
