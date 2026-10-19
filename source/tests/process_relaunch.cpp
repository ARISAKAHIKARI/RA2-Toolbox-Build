#include "../external_reader/source/process_discovery.h"
#include <cassert>
#include <cstring>
#include <map>
#include <vector>
#include <iostream>
struct Window {DWORD pid,thread;bool iconic=false;int width=1920,height=1080;};
static std::map<uintptr_t,Window>windows;static std::vector<DWORD> processes;static size_t processAt;
extern "C" {
DWORD GetCurrentProcessId(){return 999;}HANDLE CreateToolhelp32Snapshot(DWORD,DWORD){processAt=0;return (HANDLE)1;}
BOOL Process32FirstW(HANDLE,PROCESSENTRY32W*e){if(processes.empty())return FALSE;e->th32ProcessID=processes[0];std::wcscpy(e->szExeFile,L"gamemd-spawn.dat");return TRUE;}
BOOL Process32NextW(HANDLE,PROCESSENTRY32W*e){if(++processAt>=processes.size())return FALSE;e->th32ProcessID=processes[processAt];std::wcscpy(e->szExeFile,L"gamemd.exe");return TRUE;}
BOOL CloseHandle(HANDLE){return TRUE;}DWORD GetLastError(){return 0;}BOOL EnumWindows(BOOL(*fn)(HWND,LPARAM),LPARAM arg){for(auto&e:windows)fn((HWND)e.first,arg);return TRUE;}
DWORD GetWindowThreadProcessId(HWND w,DWORD*p){auto it=windows.find(uintptr_t(w));if(it==windows.end()){*p=0;return 0;}*p=it->second.pid;return it->second.thread;}
BOOL GetClientRect(HWND w,RECT*r){auto it=windows.find(uintptr_t(w));if(it==windows.end())return FALSE;*r={0,0,it->second.width,it->second.height};return TRUE;}
BOOL IsWindowVisible(HWND){return TRUE;}BOOL IsIconic(HWND w){return windows[uintptr_t(w)].iconic;}LONG GetWindowLongW(HWND,int){return 0;}HWND GetWindow(HWND,unsigned){return nullptr;}
HANDLE OpenProcess(DWORD,BOOL,DWORD p){return (HANDLE)uintptr_t(p);}BOOL QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*out,DWORD*n){std::wcscpy(out,L"C:\\YR\\gamemd.exe");*n=17;return TRUE;}
}
struct Reader {uint32_t cached=0,current=0;int cachedReads=0,freshReads=0;bool read(uint32_t,void*out,size_t n){++cachedReads;memcpy(out,&cached,n);return true;}bool readFresh(uint32_t,void*out,size_t n){++freshReads;memcpy(out,&current,n);return true;}};
int main(){using namespace external::discovery;
 processes={42};windows[10]={42,100};Reader r{10,0};DWORD tid=0;bool official=false;
 auto first=scan();assert(first.best.pid==42&&first.best.window==(HWND)10);
 assert(resolveYrppWindow(r,42,(HWND)10,&tid,&official)==(HWND)10&&tid==100&&!official);
 // Main game HWND appears while the initial launcher/helper HWND stays valid.
 windows[11]={42,101};r.current=11;assert(resolveYrppWindow(r,42,(HWND)10,&tid,&official)==(HWND)11&&tid==101&&official);
 assert(r.cachedReads==0&&r.freshReads==2);
 // Old process exits; the OS may reuse its HWND for the next game's process.
 processes={43};windows.clear();windows[10]={43,200};r.current=10;
 assert(!resolveYrppWindow(r,42,(HWND)10,&tid)&&tid==0);
 auto next=scan();assert(next.best.pid==43&&next.best.thread==200);
 assert(resolveYrppWindow(r,43,next.best.window,&tid)==(HWND)10&&tid==200);
 // Recheck the authoritative handle even when the new fallback remains valid.
 windows[12]={43,201};r.current=12;assert(resolveYrppWindow(r,43,(HWND)10,&tid)==(HWND)12&&tid==201);
 windows.clear();windows[12]={43,201,true,0,0};assert(scan().best.pid==43);
 assert(isGameName(L"gamemd-spawn.dat")&&isGameName(L"GAMEMD.DAT"));
 std::cout<<"PASS: new-process discovery, reused HWND rejection, delayed authoritative HWND with live helper, uncached reads, minimized .dat launchers\n";
}
