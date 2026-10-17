#pragma once
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <cwctype>
#include <stdint.h>
#include <string>
#include <unordered_set>
#include "../../shared/source/message_query.h"

// Discover candidate game processes before looking for a window. No handle
// injection, no privilege adjustment, no writing of game memory.
// Supports YRpp's authoritative Game::hWnd (0xB73550) resolution alongside
// EnumWindows fallback for Alt-Tab (切屏) and minimized (最小化) states.
namespace external { namespace discovery {
constexpr uint32_t YrppGameHwndAddress = assist::messagequery::YrppGameHwndAddr;

inline bool isGameName(const wchar_t* text){
 if(!text)return false;
 std::wstring name(text);
 for(auto& c:name)c=static_cast<wchar_t>(towlower(c));
 return name==L"gamemd.exe"||name==L"gamemd-spawn.exe"||
        name==L"gamemd-ares.exe"||name==L"gamemd.dat"||
        name==L"gamemd-spawn.dat";
}
struct Candidate {HWND window=nullptr;DWORD pid=0,thread=0;long long score=-1;};
struct Scan {Candidate best{};DWORD processCount=0,windowCount=0;DWORD snapshotError=0;
 // A single known process can be sampled without a visible game HWND.
 // Multiple windowless processes remain ambiguous; never guess a PID.
 bool uniqueWindowless=false;
};
struct WindowContext {Scan* scan;const std::unordered_set<DWORD>* ids;DWORD preferredPid=0;};
inline BOOL CALLBACK checkWindow(HWND w,LPARAM value){
 auto* ctx=reinterpret_cast<WindowContext*>(value);
 DWORD pid=0;DWORD thread=GetWindowThreadProcessId(w,&pid);
 if(!pid||!ctx->ids->count(pid)||(ctx->preferredPid&&pid!=ctx->preferredPid))return TRUE;
 if(GetWindowLongW(w,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)return TRUE;
 RECT r{};if(!GetClientRect(w,&r))return TRUE;
 long long width=r.right-r.left,height=r.bottom-r.top;
 // Fullscreen DirectDraw and minimized YR windows report 0x0 client rects during
 // Alt-Tab (切屏) or minimize (最小化). Keep tracking unowned top-level windows.
 if((width<320||height<200)&&(IsIconic(w)||!GetWindow(w,GW_OWNER))){width=800;height=600;}
 if(width<320||height<200||width>16384||height>16384)return TRUE;
 ++ctx->scan->windowCount;
 // Prefer the actual visible game window; still discover a hidden/minimized
 // one for diagnosis and read-only sampling. Ownership is a tie-breaker.
 long long score=(IsWindowVisible(w)?1LL<<42:0)+
                  (IsIconic(w)?1LL<<41:0)+
                  (GetWindow(w,GW_OWNER)?0:1LL<<40)+width*height;
 if(score>ctx->scan->best.score)ctx->scan->best={w,pid,thread,score};
 return TRUE;
}
// Authoritative YRpp Game::hWnd (0xB73550) resolver:
// Reads DEFINE_REFERENCE(HWND, hWnd, 0xB73550u) via the process reader so the
// exact game window is preserved across fullscreen Alt-Tab and minimize.
template <class ProcessReader>
inline HWND resolveYrppWindow(ProcessReader& reader,DWORD pid,HWND fallback=nullptr,DWORD* outThread=nullptr,bool* fromYrpp=nullptr){
 if(fromYrpp)*fromYrpp=false;
 uint32_t rawHwnd=0;
 if(pid&&reader.read(YrppGameHwndAddress,&rawHwnd,sizeof rawHwnd)&&rawHwnd){
  HWND w=reinterpret_cast<HWND>(static_cast<uintptr_t>(rawHwnd));
  DWORD wpid=0;DWORD wtid=GetWindowThreadProcessId(w,&wpid);
  if(wtid&&wpid==pid){
   if(outThread)*outThread=wtid;
   if(fromYrpp)*fromYrpp=true;
   return w;
  }
 }
 if(fallback){
  DWORD wpid=0;DWORD wtid=GetWindowThreadProcessId(fallback,&wpid);
  if(wtid&&(!pid||wpid==pid)){
   if(outThread)*outThread=wtid;
   return fallback;
  }
 }
 if(outThread)*outThread=0;
 return nullptr;
}
// Fallback for restricted snapshots: validate an existing window's owner by
// its executable basename. This is diagnostic compatibility, not a bypass.
struct FallbackContext { std::unordered_set<DWORD>* ids; };
inline BOOL CALLBACK checkFallbackWindow(HWND w,LPARAM value){
 auto* ctx=reinterpret_cast<FallbackContext*>(value);
 DWORD pid=0;GetWindowThreadProcessId(w,&pid);
 if(!pid||ctx->ids->count(pid)||pid==GetCurrentProcessId())return TRUE;
 HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
 if(!h)return TRUE;
 wchar_t path[2048]{};DWORD n=2048;
 if(QueryFullProcessImageNameW(h,0,path,&n)){
  const wchar_t* file=path;
  for(const wchar_t*p=path;*p;++p)if(*p==L'\\'||*p==L'/')file=p+1;
  if(isGameName(file))ctx->ids->insert(pid);
 }
 CloseHandle(h);return TRUE;
}
inline Scan scan(DWORD preferredPid=0){
 Scan result{};
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
 if(snapshot==INVALID_HANDLE_VALUE)result.snapshotError=GetLastError();
 PROCESSENTRY32W entry{};entry.dwSize=sizeof entry;
 std::unordered_set<DWORD> ids;
 if(snapshot!=INVALID_HANDLE_VALUE&&Process32FirstW(snapshot,&entry))do{
  if(isGameName(entry.szExeFile)&&entry.th32ProcessID!=GetCurrentProcessId())ids.insert(entry.th32ProcessID);
 }while(Process32NextW(snapshot,&entry));
 else if(snapshot!=INVALID_HANDLE_VALUE)result.snapshotError=GetLastError();
 if(snapshot!=INVALID_HANDLE_VALUE)CloseHandle(snapshot);
 if(ids.empty()){
  FallbackContext ctx{&ids};EnumWindows(checkFallbackWindow,reinterpret_cast<LPARAM>(&ctx));
 }
 result.processCount=static_cast<DWORD>(ids.size());
 if(!ids.empty()){
  WindowContext ctx{&result,&ids,preferredPid};
  EnumWindows(checkWindow,reinterpret_cast<LPARAM>(&ctx));
  if(!result.best.window){
   DWORD candidate=0;
   if(preferredPid && ids.count(preferredPid))candidate=preferredPid;
   else if(!preferredPid&&ids.size()==1)candidate=*ids.begin();
   if(candidate){result.best.pid=candidate;result.best.thread=0;
    result.uniqueWindowless=ids.size()==1;}
  }
 }
 return result;
}
} }
