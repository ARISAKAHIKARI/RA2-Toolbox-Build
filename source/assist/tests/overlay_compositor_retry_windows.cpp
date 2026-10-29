#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <thread>
static std::atomic<HWND> fixtureGame{nullptr};
static std::atomic<bool> rejectUploads{false};
static std::atomic<unsigned> attempts{0},successful{0};
static std::atomic<uint32_t> submittedPixel{0};
static HWND WINAPI fixtureForeground(){return fixtureGame.load();}
static BOOL WINAPI fixtureUpload(HWND window,HDC destination,POINT*position,SIZE*size,HDC source,POINT*origin,COLORREF key,BLENDFUNCTION*blend,DWORD flags){
 ++attempts;
 if(rejectUploads){SetLastError(ERROR_RETRY);return FALSE;}
 const BOOL result=UpdateLayeredWindow(window,destination,position,size,source,origin,key,blend,flags);
 if(result){DIBSECTION section{};auto bitmap=GetCurrentObject(source,OBJ_BITMAP);if(GetObjectW(bitmap,sizeof section,&section)==sizeof section&&section.dsBm.bmBits)submittedPixel=static_cast<const uint32_t*>(section.dsBm.bmBits)[0];++successful;}
 return result;
}
#define GetForegroundWindow fixtureForeground
#define UpdateLayeredWindow fixtureUpload
#include "../../shared/source/overlay_compositor.h"
#undef UpdateLayeredWindow
#undef GetForegroundWindow
static void check(bool condition,int line){if(!condition){std::fprintf(stderr,"FAIL compositor retry line%d error%lu attempts%u successful%u\n",line,GetLastError(),attempts.load(),successful.load());ExitProcess(2);}}
#define CHECK(x) check(bool(x),__LINE__)
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(done);
 std::thread watchdog([done]{if(WaitForSingleObject(done,20000)!=WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),124);});
 HWND foreground=GetForegroundWindow();
 HWND game=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Retry target",WS_POPUP,20,20,64,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(game);fixtureGame=game;
 overlaycompositor::Host host;
 auto context=host.configure(uintptr_t(game),GetCurrentProcessId(),1,64,48,0,0,true,true,240);
 auto surface=[&](uint32_t color){auto pixels=std::make_shared<const std::vector<uint32_t>>(64*48,color);return overlaycompositor::Surface{1,10,context,{0,0,64,48},false,255,pixels};};
 bool eligible=true;
 auto pump=[&](auto condition){DWORD start=GetTickCount();while(!condition()&&DWORD(GetTickCount()-start)<5000){if(eligible){host.configure(uintptr_t(game),GetCurrentProcessId(),1,64,48,0,0,true,true,240);host.touch(1,uintptr_t(game),GetCurrentProcessId());}MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}Sleep(2);}CHECK(condition());};
 CHECK(context.token&&host.publish(surface(0xff123456)));pump([&]{return successful.load()>0&&host.window()&&IsWindowVisible(host.window());});HWND only=host.window();CHECK(only&&IsWindowVisible(only)&&submittedPixel==0xff123456);
 rejectUploads=true;CHECK(host.publish(surface(0xff223344)));pump([&]{return host.lastError()==ERROR_RETRY;});
 // A runtime upload error keeps one owner and accepts a newer complete frame.
 // The old implementation returned false here, causing producer HWND fallback.
 CHECK(host.capture(uintptr_t(game),GetCurrentProcessId()).token==context.token);
 CHECK(host.publish(surface(0xff445566))&&host.touch(1,uintptr_t(game),GetCurrentProcessId()));
 CHECK(host.window()==only&&host.layerCount()==1&&IsWindowVisible(only));
 CHECK(host.replaceGroup({2},{overlaycompositor::Surface{2,20,context,{63,47,1,1},false,255,std::make_shared<const std::vector<uint32_t>>(1,0xffffffff)}}));
 unsigned before=successful;rejectUploads=false;pump([&]{return successful>before&&host.lastError()==0&&submittedPixel==0xff445566;});CHECK(host.window()==only&&host.layerCount()==2&&IsWindowVisible(only));
 // Ended eligibility still hides, even after a failed presentation. There is
 // no indefinite stale-frame lease or synthetic foreground activation.
 eligible=false;host.suspend();pump([&]{return !IsWindowVisible(only);});CHECK(!host.capture(uintptr_t(game),GetCurrentProcessId()).token);
 host.shutdown();fixtureGame=nullptr;DestroyWindow(game);CHECK(GetForegroundWindow()==foreground);
 SetEvent(done);watchdog.join();CloseHandle(done);
 std::puts("PASS real layered HWND upload failure/retry: current frames accepted, one passive presentation owner retained, latest DIB pixels recovered, suspend hides; injected foreground query does not activate windows; no DWM/game visual claim");
}
