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

extern "C" unsigned __cdecl BeginNativeViewQuery();
extern "C" void __cdecl EndNativeViewQuery(unsigned);
extern "C" int __cdecl GuardedNativeViewQuery(void*point,void*cell,void*coord,void*object,void*flagA,void*flagB){
 const unsigned long savedError=GetLastError();const unsigned token=BeginNativeViewQuery();
 if(!token){SetLastError(savedError);return 0;}int result=0;
 __try {
  __try {((void(__thiscall*)(void*,void*,void*,void*,void*,void*,void*))0x692300)((void*)0x87F7E8,point,cell,coord,object,flagA,flagB);result=1;}
  __except(AssistExceptionFilter(_exception_info(),21)){result=0;}
 }__finally {EndNativeViewQuery(token);SetLastError(savedError);}
 return result;
}


#include "view_zoom_native_guard.h"
