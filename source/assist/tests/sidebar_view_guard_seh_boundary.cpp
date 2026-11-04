// Build this boundary with the same MSVC x86 SEH ABI as production guard.cpp.
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
extern "C" __declspec(dllimport) void __stdcall SetLastError(unsigned long);
extern "C" void* _exception_info();
extern "C" unsigned long _exception_code();
extern "C" int AssistExceptionFilter(void*,unsigned);
extern "C" unsigned BeginAssistCapture();
extern "C" void EndAssistCapture(unsigned);
#include "../source/sidebar_view_guard.h"
