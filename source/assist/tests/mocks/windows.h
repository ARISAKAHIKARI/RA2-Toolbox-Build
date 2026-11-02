#pragma once
// Unit-test-only Win32 model. Executable-memory mutation APIs deliberately do
// not exist here: reintroducing one into the manager must fail compilation.
#include <cstdint>
#define __declspec(x)
#ifndef __stdcall
#define __stdcall
#endif
using DWORD=uint32_t;
using LONG=int32_t;
using HANDLE=void*;
constexpr DWORD SYNCHRONIZE=0x00100000,WAIT_OBJECT_0=0,WAIT_TIMEOUT=258;
constexpr int FALSE=0;
namespace mockwin {
inline unsigned calls=0;
inline DWORD thread=7;
inline DWORD waitResult=WAIT_TIMEOUT;
}
inline DWORD GetCurrentThreadId(){++mockwin::calls;return mockwin::thread;}
inline LONG InterlockedCompareExchange(volatile LONG*p,LONG value,LONG compare){++mockwin::calls;LONG old=*p;if(old==compare)*p=value;return old;}
inline LONG InterlockedExchange(volatile LONG*p,LONG value){++mockwin::calls;LONG old=*p;*p=value;return old;}
inline LONG InterlockedIncrement(volatile LONG*p){++mockwin::calls;return ++*p;}
inline DWORD WaitForSingleObject(HANDLE,DWORD){++mockwin::calls;return mockwin::waitResult;}
inline int CloseHandle(HANDLE){++mockwin::calls;return 1;}
inline HANDLE OpenThread(DWORD,int,DWORD){++mockwin::calls;return reinterpret_cast<HANDLE>(uintptr_t(1));}
