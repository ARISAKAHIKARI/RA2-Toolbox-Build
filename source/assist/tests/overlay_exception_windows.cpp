#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <stdexcept>
#include <thread>
static std::atomic<HWND> fixtureGame{nullptr};
static std::atomic<DWORD> rendererThread{0};static DWORD mainThread=0;
static std::atomic<bool> allocationFailure{false},throwDiagnostics{false};
static std::atomic<int> uploadFailure{0};
static std::atomic<unsigned> allocationFaults{0},uploadFaults{0},attempts{0},successful{0},diagnostics{0};
static std::atomic<uint32_t> submittedPixel{0};
void*operator new(size_t n){if(GetCurrentThreadId()==rendererThread&&allocationFailure.exchange(false)){++allocationFaults;throw std::bad_alloc();}if(void*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
static HWND WINAPI fixtureForeground(){if(GetCurrentThreadId()!=mainThread)rendererThread=GetCurrentThreadId();return fixtureGame.load();}
static BOOL WINAPI fixtureUpload(HWND w,HDC d,POINT*p,SIZE*s,HDC source,POINT*origin,COLORREF c,BLENDFUNCTION*b,DWORD f){
 ++attempts;const int mode=uploadFailure;
 if(mode==1){uploadFailure=0;++uploadFaults;throw std::runtime_error("controlled upload failure");}
 if(mode==2){++uploadFaults;throw std::bad_alloc();}
 const BOOL result=UpdateLayeredWindow(w,d,p,s,source,origin,c,b,f);
 if(result){DIBSECTION section{};auto bitmap=GetCurrentObject(source,OBJ_BITMAP);if(GetObjectW(bitmap,sizeof section,&section)==sizeof section&&section.dsBm.bmBits)submittedPixel=static_cast<const uint32_t*>(section.dsBm.bmBits)[0];++successful;}return result;
}
#define GetForegroundWindow fixtureForeground
#define UpdateLayeredWindow fixtureUpload
#ifndef OVERLAY_COMPOSITOR_HEADER
#define OVERLAY_COMPOSITOR_HEADER "../../shared/source/overlay_compositor.h"
#endif
#include OVERLAY_COMPOSITOR_HEADER
#undef GetForegroundWindow
#undef UpdateLayeredWindow
static void log(const std::wstring&s){if(s.find(L"compositor-exception")==0){++diagnostics;if(throwDiagnostics)throw std::bad_alloc();}}
static void check(bool ok,int line){if(!ok){std::fprintf(stderr,"FAIL line%d Win32=%lu alloc=%u upload=%u good=%u logs=%u\n",line,GetLastError(),allocationFaults.load(),uploadFaults.load(),successful.load(),diagnostics.load());ExitProcess(2);}}
#define CHECK(v) check(bool(v),__LINE__)
int main(){
 mainThread=GetCurrentThreadId();std::set_terminate([]{ExitProcess(86);});SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(done);std::thread watchdog([done]{if(WaitForSingleObject(done,20000)!=WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),124);});
 HWND game=CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"Exception target",WS_POPUP,20,20,64,48,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);CHECK(game);fixtureGame=game;
 overlaycompositor::Host host;host.setDiagnosticsSink(log);auto context=host.configure(uintptr_t(game),GetCurrentProcessId(),1,64,48,0,0,true,true,240);
 auto surface=[&](uint32_t color){return overlaycompositor::Surface{1,10,context,{0,0,64,48},false,255,std::make_shared<const std::vector<uint32_t>>(64*48,color)};};
 auto beat=[&]{host.configure(uintptr_t(game),GetCurrentProcessId(),1,64,48,0,0,true,true,240);host.touch(1,uintptr_t(game),GetCurrentProcessId());MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}Sleep(2);};
 auto pump=[&](auto condition){DWORD at=GetTickCount();while(!condition()&&DWORD(GetTickCount()-at)<5000)beat();CHECK(condition());};
 CHECK(host.publish(surface(0xff123456)));pump([&]{return successful>0&&IsWindowVisible(host.window());});const HWND only=host.window();const auto first=host.uploadStatistics();
 // Throw in the real worker's first layer-vector allocation under its mutex.
 // The log sink also throws during recovery; neither exception may escape.
 throwDiagnostics=true;allocationFailure=true;CHECK(host.publish(surface(0xff234567)));pump([&]{return allocationFaults==1&&diagnostics>0;});
 pump([&]{return submittedPixel==0xff234567&&host.lastError()==0&&IsWindowVisible(only);});CHECK(host.window()==only&&host.layerCount()==1&&host.uploadStatistics().failures>first.failures);throwDiagnostics=false;
 uploadFailure=1;CHECK(host.publish(surface(0xff345678)));pump([&]{return uploadFaults>0;});pump([&]{return submittedPixel==0xff345678&&host.lastError()==0;});CHECK(host.window()==only);
 // Repeated failures plus new revisions/heartbeats cannot create a retry spin.
 uploadFailure=2;CHECK(host.publish(surface(0xff456789)));pump([&]{return host.lastError()==ERROR_NOT_ENOUGH_MEMORY;});const unsigned before=uploadFaults;DWORD at=GetTickCount();unsigned changed=0;
 while(DWORD(GetTickCount()-at)<250){CHECK(host.publish(surface(0xff500000+(++changed))));beat();}
 CHECK(uploadFaults-before>=1&&uploadFaults-before<=4);uploadFailure=0;CHECK(host.publish(surface(0xffabcdef)));pump([&]{return submittedPixel==0xffabcdef&&host.lastError()==0&&IsWindowVisible(only);});
 // Stop interrupts backoff; no process restart or lost window ownership.
 uploadFailure=2;CHECK(host.publish(surface(0xff777777)));pump([&]{return host.lastError()==ERROR_NOT_ENOUGH_MEMORY;});at=GetTickCount();host.shutdown();CHECK(DWORD(GetTickCount()-at)<500&&host.window()==nullptr);fixtureGame=nullptr;DestroyWindow(game);
 SetEvent(done);watchdog.join();CloseHandle(done);
 std::puts("PASS real Windows compositor exception recovery: layer allocation, throwing diagnostics, upload exception, bounded retry despite new frames, latest-pixel recovery, one HWND and interruptible shutdown; no game/DWM claim");
}
