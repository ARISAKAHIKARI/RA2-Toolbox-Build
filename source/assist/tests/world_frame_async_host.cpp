#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cwchar>
#include <cstring>
using Start=int(__cdecl*)(HANDLE);using Fn=int(__cdecl*)();
static void until(Fn fn,int expected){const DWORD begin=GetTickCount();for(;;){const int value=fn();assert(value>=0);if(value==expected)return;assert(GetTickCount()-begin<5000);Sleep(1);}}
int main(int argc,char**argv){
 assert(argc==2||(argc==3&&!std::strcmp(argv[2],"async")));
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);std::setvbuf(stdout,nullptr,_IONBF,0);
 HANDLE gate=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(gate);HMODULE module=LoadLibraryA(argv[1]);assert(module);
 if(argc==3){
  // This is the real production captureWorldFrame entry with the default-on
  // Win32 command worker. The separate no-async mode still tests the pinned
  // publisher and read-only recovery contracts below.
  auto workerStart=(Fn)GetProcAddress(module,"AsyncStart");
  auto workerPoll=(Fn)GetProcAddress(module,"AsyncPoll");
  auto workerStop=(Fn)GetProcAddress(module,"AsyncStop");
  assert(workerStart&&workerPoll&&workerStop);
  assert(workerStart()==1);until(workerPoll,1);until(workerStop,1);
  assert(FreeLibrary(module));CloseHandle(gate);
  std::puts("PASS default-on Win32 Runtime captureWorldFrame x86/x64: nonblocking request, sealed same-frame worker adoption, native roster and pinned shutdown");
  return 0;
 }
 auto start=(Start)GetProcAddress(module,"Start");auto entered=(Fn)GetProcAddress(module,"Entered");auto advance=(Fn)GetProcAddress(module,"Advance");auto latest=(Fn)GetProcAddress(module,"ReadLatest");auto stop=(Fn)GetProcAddress(module,"Stop");auto resume=(Fn)GetProcAddress(module,"Resume");auto idle=(Fn)GetProcAddress(module,"Idle");
 auto probeBegin=(Fn)GetProcAddress(module,"ProbeBegin");
 auto probeReady=(Fn)GetProcAddress(module,"ProbeReady");
 auto shadowBegin=(Fn)GetProcAddress(module,"ShadowBegin");
 auto shadowReady=(Fn)GetProcAddress(module,"ShadowReady");
 assert(start&&entered&&advance&&latest&&stop&&resume&&idle&&probeBegin&&probeReady&&shadowBegin&&shadowReady);
 assert(idle()==1);
 assert(start(gate));until(entered,1);const DWORD begin=GetTickCount();assert(advance()==1001);std::printf("Game-side 1000 captures/submissions completed in %lu ms while actual publication worker remained blocked\n",GetTickCount()-begin);
 assert(entered()==1);assert(SetEvent(gate));until(latest,1001);Sleep(20);assert(resume()==1);until(latest,1001);
 assert(probeBegin()==1);until(probeReady,1);
 std::puts("PASS real background recovery: Win32 read ran off-game-thread; sealed command sample unchanged");
 assert(shadowBegin()==1);until(shadowReady,1);
 std::puts("PASS pinned shadow graph: production CaptureGraph executed off-game-thread; no command snapshot adopted");
 until(stop,1);assert(latest()==0);
 // Restart, then remove the loader's reference while the worker is blocked.
 // Its independent module reference must keep every function valid until exit.
 assert(start(gate));until(entered,1);
 // A disconnected and fully retired shadow worker must restart in a new
 // connection, even while the separate publication worker is blocked.
 assert(shadowBegin()==1);until(shadowReady,1);
 assert(stop()==0);assert(FreeLibrary(module));assert(GetModuleHandleW(L"async-publication.dll"));assert(SetEvent(gate));
 const DWORD deadline=GetTickCount();while(GetModuleHandleW(L"async-publication.dll")){assert(GetTickCount()-deadline<5000);Sleep(1);}CloseHandle(gate);
 std::puts("PASS real Windows DLL: no idle worker without subscriber, background named-mapping publication, pinned source bytes, latest pending state, paused same-frame re-export without live reads/restamp, nonblocking retire/stop, restart, module reference while copying and automatic safe unload; no game process loaded");
}
