#pragma once
#include <windows.h>
#include <dbghelp.h>
#include <exception>
#include "release_version.h"

// EXE only. Prepare paths/handles before starting workers; a fatal callback
// never enters the normal logger, its locks, or the game process. Minidumps
// contain the toolbox's stacks, not a full memory image. Recording is best effort.
namespace hostcrash {
using Dump=BOOL(WINAPI*)(HANDLE,DWORD,HANDLE,MINIDUMP_TYPE,
 PMINIDUMP_EXCEPTION_INFORMATION,PMINIDUMP_USER_STREAM_INFORMATION,PMINIDUMP_CALLBACK_INFORMATION);
inline HANDLE logFile=INVALID_HANDLE_VALUE;
inline wchar_t dumpPath[32768]{};
inline Dump writeDump=nullptr;
inline volatile LONG recording=0;
inline LPTOP_LEVEL_EXCEPTION_FILTER previousFilter=nullptr;
inline void line(const char*text)noexcept{if(logFile!=INVALID_HANDLE_VALUE){DWORD n=0;WriteFile(logFile,text,DWORD(lstrlenA(text)),&n,nullptr);}}
inline void number(const char*key,uintptr_t value)noexcept{
 char digits[2*sizeof(value)+4]{};digits[0]='0';digits[1]='x';
 for(unsigned n=0;n<2*sizeof(value);++n)digits[2+n]="0123456789ABCDEF"[(value>>(4*(2*sizeof(value)-1-n)))&15];
 line(key);line(digits);line("\r\n");
}
inline void record(EXCEPTION_POINTERS*exception,const char*kind)noexcept{
 if(InterlockedCompareExchange(&recording,1,0))return;
 line("\r\nRA2Toolbox " RA2_RELEASE_VERSION " Host fatal ");line(kind);line("\r\n");
 SYSTEMTIME time{};GetSystemTime(&time);char stamp[]="utc=0000-00-00T00:00:00.000Z\r\n";
 auto digits=[&](unsigned at,unsigned count,unsigned value){while(count){stamp[at+--count]=char('0'+value%10);value/=10;}};
 digits(4,4,time.wYear);digits(9,2,time.wMonth);digits(12,2,time.wDay);digits(15,2,time.wHour);digits(18,2,time.wMinute);digits(21,2,time.wSecond);digits(24,3,time.wMilliseconds);line(stamp);
 number("pid=",GetCurrentProcessId());number("tid=",GetCurrentThreadId());
 if(exception&&exception->ExceptionRecord){const auto&r=*exception->ExceptionRecord;
  number("exception=",r.ExceptionCode);number("address=",uintptr_t(r.ExceptionAddress));
  MEMORY_BASIC_INFORMATION memory{};
  if(VirtualQuery(r.ExceptionAddress,&memory,sizeof memory)){
   number("moduleBase=",uintptr_t(memory.AllocationBase));number("moduleRva=",uintptr_t(r.ExceptionAddress)-uintptr_t(memory.AllocationBase));
   wchar_t module[1024]{};char utf8[4096]{};
   if(GetModuleFileNameW(HMODULE(memory.AllocationBase),module,1024)&&WideCharToMultiByte(CP_UTF8,0,module,-1,utf8,sizeof utf8,nullptr,nullptr)){line("module=");line(utf8);line("\r\n");}
  }
  if((r.ExceptionCode==EXCEPTION_ACCESS_VIOLATION||r.ExceptionCode==EXCEPTION_IN_PAGE_ERROR)&&r.NumberParameters>=2){number("accessOperation=",r.ExceptionInformation[0]);number("accessAddress=",r.ExceptionInformation[1]);}
 }
 if(logFile!=INVALID_HANDLE_VALUE)FlushFileBuffers(logFile);
 if(writeDump&&dumpPath[0]){
  HANDLE file=CreateFileW(dumpPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(file!=INVALID_HANDLE_VALUE){MINIDUMP_EXCEPTION_INFORMATION info{GetCurrentThreadId(),exception,FALSE};
   const BOOL ok=writeDump(GetCurrentProcess(),GetCurrentProcessId(),file,MiniDumpNormal,exception?&info:nullptr,nullptr,nullptr);
   number("dumpWritten=",ok);if(!ok)number("dumpError=",GetLastError());CloseHandle(file);
  }else number("dumpOpenError=",GetLastError());
 }
 if(logFile!=INVALID_HANDLE_VALUE)FlushFileBuffers(logFile);
}
inline LONG WINAPI filter(EXCEPTION_POINTERS*exception)noexcept{
 record(exception,"unhandled-native");
 return previousFilter?previousFilter(exception):EXCEPTION_EXECUTE_HANDLER;
}
[[noreturn]] inline void terminated()noexcept{
 CONTEXT context{};RtlCaptureContext(&context);EXCEPTION_RECORD exception{};exception.ExceptionCode=0xE0000001;
#if defined(_WIN64)
 exception.ExceptionAddress=reinterpret_cast<void*>(context.Rip);
#else
 exception.ExceptionAddress=reinterpret_cast<void*>(context.Eip);
#endif
 EXCEPTION_POINTERS pointers{&exception,&context};record(&pointers,"std-terminate");
 // GNU's unhandled C++ SEH can reach our filter before terminate(). Keep
 // that original throw dump; append the final termination cause either way.
 line("termination=std-terminate\r\n");number("terminationCode=",exception.ExceptionCode);
 if(logFile!=INVALID_HANDLE_VALUE)FlushFileBuffers(logFile);
 TerminateProcess(GetCurrentProcess(),exception.ExceptionCode);ExitProcess(exception.ExceptionCode);
}
inline bool directory(const wchar_t*path)noexcept{
 if(!path||!path[0]||lstrlenW(path)>32000)return false;
 CreateDirectoryW(path,nullptr);wchar_t file[32768]{};lstrcpyW(file,path);lstrcatW(file,L"\\HostCrash.log");
 HANDLE opened=CreateFileW(file,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(opened==INVALID_HANDLE_VALUE)return false;
 logFile=opened;lstrcpyW(dumpPath,path);lstrcatW(dumpPath,L"\\HostCrash.dmp");return true;
}
inline void install(const wchar_t*testDirectory=nullptr)noexcept{
 static bool installed=false;if(installed)return;installed=true;
 wchar_t path[32768]{};
 if(testDirectory)directory(testDirectory);
 else{
  const DWORD n=GetModuleFileNameW(nullptr,path,32768);
  if(n&&n<32768){wchar_t*slash=nullptr;for(auto*p=path;*p;++p)if(*p==L'\\'||*p==L'/')slash=p;
   if(slash){*slash=0;if(lstrlenW(path)<32000){lstrcatW(path,L"\\logs");directory(path);}}
  }
  if(logFile==INVALID_HANDLE_VALUE){const DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
   if(size&&size<32000){lstrcatW(path,L"\\RA2Toolbox");CreateDirectoryW(path,nullptr);lstrcatW(path,L"\\logs");directory(path);}
  }
 }
 if(auto module=LoadLibraryExW(L"dbghelp.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32))writeDump=reinterpret_cast<Dump>(GetProcAddress(module,"MiniDumpWriteDump"));
 previousFilter=SetUnhandledExceptionFilter(filter);std::set_terminate(terminated);
}
}
