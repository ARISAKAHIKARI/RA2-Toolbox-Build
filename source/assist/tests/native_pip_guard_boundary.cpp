extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
extern "C" __declspec(dllimport) void __stdcall SetLastError(unsigned long);
extern "C" void* _exception_info();
extern "C" unsigned BeginAssistCapture();
extern "C" void EndAssistCapture(unsigned);
extern "C" int AssistExceptionFilter(void*,unsigned);
#include "../source/native_pip_guard.h"
