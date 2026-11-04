#ifdef NDEBUG
#undef NDEBUG
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../../shared/source/host_game_input.h"
#include <cassert>
#include <cstdio>
static LRESULT CALLBACK message(int c,WPARAM w,LPARAM l){return CallNextHookEx(nullptr,c,w,l);}
int main(){
 auto*page=VirtualAlloc(reinterpret_cast<void*>(0xB70000),65536,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);assert(page==reinterpret_cast<void*>(0xB70000));
 HWND a=CreateWindowExW(0,L"STATIC",L"a",0,0,0,1,1,HWND_MESSAGE,nullptr,nullptr,nullptr);
 HWND b=CreateWindowExW(0,L"STATIC",L"b",0,0,0,1,1,HWND_MESSAGE,nullptr,nullptr,nullptr);assert(a&&b);
 auto&h=gameinput::host();auto&t=h.targets[0];t.pid=GetCurrentProcessId();t.thread=GetCurrentThreadId();t.window=a;
 t.message=SetWindowsHookExW(WH_GETMESSAGE,message,nullptr,t.thread);assert(t.message);const HHOOK original=t.message;
 h.leases[0]={1,gameentry::Client::Assist,0};h.leases[1]={2,gameentry::Client::Cursor,0};
 runtime::Control control{};control.enabled=runtime::Tactical|runtime::Cursor;control.wakePending=1;t.control=&control;
 auto*native=reinterpret_cast<uint32_t*>(0xB73550);*native=uint32_t(uintptr_t(b));
 assert(gameinput::rebindWindow(1,b));assert(t.window==b&&t.message==original&&control.enabled==(runtime::Tactical|runtime::Cursor)&&!control.wakePending);
 assert(h.leases[0].id==1&&h.leases[1].id==2);assert(gameinput::rebindWindow(2,b));
 assert(!gameinput::rebindWindow(1,a));assert(!gameinput::rebindWindow(1,nullptr));assert(!gameinput::rebindWindow(99,b));assert(t.window==b&&t.message==original);
 *native=0;assert(!gameinput::rebindWindow(1,b));assert(t.window==b&&t.message==original&&h.leases[0].id==1);
 t.control=nullptr;h.leases[0]={};h.leases[1]={};assert(gameinput::clear(t));
 DestroyWindow(a);DestroyWindow(b);VirtualFree(page,0,MEM_RELEASE);
 puts("PASS: real Win32 same-thread HWND rebind retains HHOOK, both subscriptions and enabled mask; invalid/unknown HWND cannot release transport");
}
