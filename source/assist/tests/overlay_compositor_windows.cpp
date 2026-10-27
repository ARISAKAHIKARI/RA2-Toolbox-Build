#include "../../shared/source/overlay_compositor.h"
#include <cstdio>
#include <cstdlib>
static void require(bool condition,int line){if(!condition){std::fprintf(stderr,"FAIL compositor line %d win32=%lu\n",line,GetLastError());ExitProcess(2);}}
#define CHECK(v) require(bool(v),__LINE__)
static void pump(unsigned ms){DWORD start=GetTickCount();do{MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}Sleep(1);}while(GetTickCount()-start<ms);}
int main(){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 HANDLE done=CreateEventW(nullptr,TRUE,FALSE,nullptr);CHECK(done);
 std::thread watchdog([done]{if(WaitForSingleObject(done,20000)!=WAIT_OBJECT_0)TerminateProcess(GetCurrentProcess(),124);});
 WNDCLASSW cls{};cls.hInstance=GetModuleHandleW(nullptr);cls.lpfnWndProc=DefWindowProcW;cls.lpszClassName=L"RenderUpgradeTestGame";RegisterClassW(&cls);
 HWND game=CreateWindowExW(WS_EX_NOACTIVATE,cls.lpszClassName,L"",WS_POPUP,20,20,320,200,nullptr,nullptr,cls.hInstance,nullptr);CHECK(game);
 HWND prior=GetForegroundWindow();
 overlaycompositor::Host host;auto c=host.configure(uintptr_t(game),GetCurrentProcessId(),1,320,200,10,20,true,true,60);
 CHECK(c.token);for(int i=0;i<200&&!host.window()&&!host.lastError();++i)pump(5);CHECK(host.window()&&host.lastError()==0);
 LONG_PTR flags=GetWindowLongPtrW(host.window(),GWL_EXSTYLE);
 CHECK((flags&(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE))==(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE));
 CHECK(GetForegroundWindow()==prior);
 auto pixels=std::make_shared<const std::vector<uint32_t>>(32*20,0xff123456);
 for(unsigned i=1;i<=3;++i)CHECK(host.publish({i,int(i*10),c,{0,0,32,20},i!=2,255,pixels}));
 CHECK(host.layerCount()==3);HWND only=host.window();
 auto changed=host.configure(uintptr_t(game),GetCurrentProcessId(),2,320,200,10,20,true,false,60);CHECK(changed.token!=c.token&&host.layerCount()==0);
 CHECK(host.publish({1,10,c,{0,0,32,20},true,255,pixels}));CHECK(host.layerCount()==0&&host.window()==only);
 CHECK(host.publish({1,10,changed,{0,0,32,20},true,255,pixels}));CHECK(host.layerCount()==1);
 host.hide(1);CHECK(host.layerCount()==0);host.suspend();CHECK(!host.capture(uintptr_t(game),GetCurrentProcessId()).token);
 CHECK(!host.publish({1,10,changed,{0,0,32,20},true,255,pixels}));
 host.shutdown();CHECK(!host.window());DestroyWindow(game);
 SetEvent(done);watchdog.join();CloseHandle(done);
 std::puts("PASS: one actual passive HWND; multiple logical surfaces; epoch reset; stale frame rejection; suspend/shutdown; no focus activation");
}
