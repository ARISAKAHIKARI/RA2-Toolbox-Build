#pragma once
#include <windows.h>
#include <mutex>
#include <unordered_map>
#include "game_image_identity.h"

namespace external { namespace imageidentity {
inline bool fileMatches(const wchar_t* path){
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(file==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size)&&size.QuadPart>=0;
 if(ok)ok=matches([&](uint32_t at,void*out,size_t n){
  LARGE_INTEGER pos{};pos.QuadPart=at;DWORD got=0;
  return SetFilePointerEx(file,pos,nullptr,FILE_BEGIN)&&ReadFile(file,out,DWORD(n),&got,nullptr)&&got==n;
 },uint64_t(size.QuadPart));
 CloseHandle(file);return ok;
}
struct Cached {uint64_t creation=0;std::wstring path;DWORD tick=0;bool matched=false;};
inline bool processMatches(DWORD pid){
 // No VM_READ, injection or privilege changes during discovery. A cache entry
 // is bound to process creation time and path, never just a recyclable PID.
 HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!h)return false;
 wchar_t path[32768]{};DWORD n=32768;FILETIME created{},exit{},kernel{},user{};
 bool ok=QueryFullProcessImageNameW(h,0,path,&n)&&GetProcessTimes(h,&created,&exit,&kernel,&user);
 CloseHandle(h);if(!ok)return false;
 const uint64_t creation=uint64_t(created.dwHighDateTime)<<32|created.dwLowDateTime;
 const DWORD now=GetTickCount();
 static std::mutex mutex;static std::unordered_map<DWORD,Cached> cache;
 {std::lock_guard<std::mutex> guard(mutex);auto i=cache.find(pid);
  if(i!=cache.end()&&i->second.creation==creation&&i->second.path==path&&DWORD(now-i->second.tick)<3000)return i->second.matched;}
 // Slow EXE reads must not hold up another module's cached discovery result.
 bool matched=fileMatches(path);
 {std::lock_guard<std::mutex> guard(mutex);auto i=cache.find(pid);
  if(i!=cache.end()&&int32_t(i->second.tick-now)>0)return matched;
  // Bounded negative cache; retry files unavailable during startup.
  if(cache.size()>=256)cache.clear();cache[pid]={creation,path,now,matched};}
 return matched;
}
} }
