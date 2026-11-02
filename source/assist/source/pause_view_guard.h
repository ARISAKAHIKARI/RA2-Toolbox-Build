
// The native pause mini-loop pumps Windows messages. Do not retain a Capture
// lease across it: the normal Runtime message gateway must remain available.
extern "C" unsigned __cdecl AssistPauseViewMainGate();
extern "C" void AssistPauseViewFailed();
extern "C" unsigned __cdecl GuardedPauseViewMainGate(){
 const unsigned long savedError=GetLastError();unsigned result=0;
 __try {result=AssistPauseViewMainGate();}
 __except(AssistExceptionFilter(_exception_info(),18)){AssistPauseViewFailed();}
 SetLastError(savedError);return result;
}

extern "C" unsigned __cdecl AssistPauseViewNative(unsigned,unsigned,unsigned,unsigned);
extern "C" unsigned __cdecl GuardedPauseViewNative(unsigned op,unsigned key,unsigned x,unsigned y){
 const unsigned long savedError=GetLastError();unsigned result=0;
 const unsigned token=BeginAssistCapture();if(!token){SetLastError(savedError);return 0;}
 __try {
  __try {result=AssistPauseViewNative(op,key,x,y);}
  __except(AssistExceptionFilter(_exception_info(),18)){AssistPauseViewFailed();}
 }__finally {EndAssistCapture(token);SetLastError(savedError);}
 return result;
}
