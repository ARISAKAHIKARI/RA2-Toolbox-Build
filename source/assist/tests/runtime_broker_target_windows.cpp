#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cwchar>
#include "runtime_broker_fixture.h"
static_assert(sizeof(void*)==4,"Synthetic hook target must be x86");
static brokerfixture::Probe*probe;
static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 if(message==brokerfixture::Message||message==brokerfixture::SentMessage){InterlockedExchange(&probe->ack,LONG(wp));return 0;}
 if(message==WM_CLOSE){DestroyWindow(window);return 0;}
 if(message==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProcW(window,message,wp,lp);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR command,int){
 wchar_t*end=nullptr;DWORD owner=DWORD(std::wcstoul(command,&end,10));if(!owner||!end||*end)return 2;
 wchar_t name[96];brokerfixture::name(name,owner);HANDLE mapping=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
 probe=mapping?(brokerfixture::Probe*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(*probe)):nullptr;
 if(!probe||probe->owner!=owner)return 3;
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=windowProc;wc.lpszClassName=L"RA2RuntimeBrokerTargetFixture";
 if(!RegisterClassW(&wc))return 4;
 HWND window=CreateWindowW(wc.lpszClassName,L"Broker test target",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,instance,nullptr);if(!window)return 5;
 probe->pid=GetCurrentProcessId();probe->thread=GetCurrentThreadId();InterlockedExchange((volatile LONG*)&probe->window,LONG(uintptr_t(window)));
 MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 UnmapViewOfFile(probe);CloseHandle(mapping);return 0;
}
