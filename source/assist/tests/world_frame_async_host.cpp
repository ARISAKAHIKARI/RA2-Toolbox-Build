#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cwchar>
using Start=int(__cdecl*)(HANDLE);using Fn=int(__cdecl*)();
static void until(Fn fn,int expected){const DWORD begin=GetTickCount();for(;;){const int value=fn();assert(value>=0);if(value==expected)return;assert(GetTickCount()-begin<5000);Sleep(1);}}
int main(int argc,char**argv){
 assert(argc==2);SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);std::setvbuf(stdout,nullptr,_IONBF,0);
 HANDLE gate=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(gate);HMODULE module=LoadLibraryA(argv[1]);assert(module);
 auto start=(Start)GetProcAddress(module,"Start");auto entered=(Fn)GetProcAddress(module,"Entered");auto advance=(Fn)GetProcAddress(module,"Advance");auto latest=(Fn)GetProcAddress(module,"ReadLatest");auto stop=(Fn)GetProcAddress(module,"Stop");auto resume=(Fn)GetProcAddress(module,"Resume");auto recover=(Fn)GetProcAddress(module,"RecoverPublisher");auto idle=(Fn)GetProcAddress(module,"Idle");assert(start&&entered&&advance&&latest&&stop&&resume&&recover&&idle);assert(idle()==1);
 assert(start(gate));until(entered,1);const DWORD begin=GetTickCount();assert(advance()==1001);std::printf("Game-side 1000 captures/submissions completed in %lu ms while actual publication worker remained blocked\n",GetTickCount()-begin);
 assert(entered()==1);assert(SetEvent(gate));until(latest,1001);Sleep(20);assert(resume()==1);until(latest,1001);until(recover,1);until(latest,1001);std::puts("PASS paused frame restarts publisher after worker exit despite a still-current descriptor");until(stop,1);assert(latest()==0);
 // Restart, then remove the loader's reference while the worker is blocked.
 // Its independent module reference must keep every function valid until exit.
 assert(start(gate));until(entered,1);assert(stop()==0);assert(FreeLibrary(module));assert(GetModuleHandleW(L"async-publication.dll"));assert(SetEvent(gate));
 const DWORD deadline=GetTickCount();while(GetModuleHandleW(L"async-publication.dll")){assert(GetTickCount()-deadline<5000);Sleep(1);}CloseHandle(gate);
 std::puts("PASS real Windows DLL: no idle worker without subscriber, background named-mapping publication, pinned source bytes, latest pending state, paused same-frame re-export without live reads/restamp, nonblocking retire/stop, restart, module reference while copying and automatic safe unload; no game process loaded");
}
