// Exact product boundary, built with the MSVC x86 SEH ABI.
extern "C" __declspec(dllimport) unsigned long __stdcall GetLastError();
extern "C" __declspec(dllimport) void __stdcall SetLastError(unsigned long);
extern "C" unsigned long _exception_code();
#include "../source/tech_ui_breakpoint_guard.h"
