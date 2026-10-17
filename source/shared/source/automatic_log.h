#pragma once
#include <windows.h>
#include <string>
#include <mutex>
#include <cstdio>
#include "release_version.h"

// Host-only, bounded critical diagnostics. Never included by GameRuntime.
// Logging preserves GetLastError, including failed rotation/fallback attempts.
namespace autolog {
constexpr LONGLONG Limit=2*1024*1024;
struct ErrorGuard {DWORD value=GetLastError();~ErrorGuard(){SetLastError(value);}};
struct State {std::mutex mutex;std::wstring preferred,fallback,active;};
inline State& state(){static State value;return value;}
inline void initialize(const std::wstring& root){
 ErrorGuard error;auto&s=state();std::lock_guard<std::mutex> lock(s.mutex);
 s.preferred=root+L"\\logs\\Runtime.log";
 CreateDirectoryW((root+L"\\logs").c_str(),nullptr);
 wchar_t local[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
 if(n&&n<32768){
  std::wstring base=std::wstring(local)+L"\\RA2Toolbox";CreateDirectoryW(base.c_str(),nullptr);
  base+=L"\\logs";CreateDirectoryW(base.c_str(),nullptr);s.fallback=base+L"\\Runtime.log";
 }
}
inline bool append(const std::wstring& path,const std::string& data){
 if(path.empty())return false;
 HANDLE f=CreateFileW(path.c_str(),FILE_APPEND_DATA|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(f==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};
 if(!GetFileSizeEx(f,&size)){CloseHandle(f);return false;}
 if(size.QuadPart+LONGLONG(data.size())>Limit){
  CloseHandle(f);const auto first=path+L".1",second=path+L".2";
  // Never continue appending to an oversized file when rotation is blocked.
  if(GetFileAttributesW(first.c_str())!=INVALID_FILE_ATTRIBUTES&&
     !MoveFileExW(first.c_str(),second.c_str(),MOVEFILE_REPLACE_EXISTING))return false;
  if(!MoveFileExW(path.c_str(),first.c_str(),MOVEFILE_REPLACE_EXISTING))return false;
  f=CreateFileW(path.c_str(),FILE_APPEND_DATA|FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
  if(f==INVALID_HANDLE_VALUE)return false;
 }
 DWORD written=0;bool ok=WriteFile(f,data.data(),DWORD(data.size()),&written,nullptr)&&written==data.size();
 CloseHandle(f);return ok;
}
inline bool write(const std::wstring& message){
 ErrorGuard error;auto&s=state();std::lock_guard<std::mutex> lock(s.mutex);
 SYSTEMTIME time{};GetLocalTime(&time);wchar_t prefix[160];
 swprintf(prefix,160,L"%04u-%02u-%02u %02u:%02u:%02u.%03u v%ls host=%lu tid=%lu ",
  time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds,
  RA2_RELEASE_VERSION_W,GetCurrentProcessId(),GetCurrentThreadId());
 std::wstring line=prefix;line+=message.substr(0,4096);line+=L"\r\n";
 int size=WideCharToMultiByte(CP_UTF8,0,line.data(),int(line.size()),nullptr,0,nullptr,nullptr);
 if(size<=0)return false;std::string bytes(size,'\0');
 if(!WideCharToMultiByte(CP_UTF8,0,line.data(),int(line.size()),&bytes[0],size,nullptr,nullptr))return false;
 const auto& selected=s.active.empty()?s.preferred:s.active;
 if(append(selected,bytes)){s.active=selected;return true;}
 if(selected!=s.fallback&&append(s.fallback,bytes)){s.active=s.fallback;return true;}
 return false;
}
}
