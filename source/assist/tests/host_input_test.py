"""Run the production host subscription/scheduling code across two TUs."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
header=r'''
#pragma once
#include <cstdint>
#include <cstddef>
using BOOL=int;using SIZE_T=size_t;using DWORD_PTR=uintptr_t;using WPARAM=uintptr_t;using LPARAM=intptr_t;using DWORD=uint32_t;using LONG=int32_t;using UINT=unsigned;using BOOLEAN=unsigned char;using HWND=void*;using HHOOK=void*;using HMODULE=void*;using HANDLE=void*;using HOOKPROC=void*;
#define CALLBACK
constexpr int WH_GETMESSAGE=3,WH_CALLWNDPROCRET=12,ERROR_INVALID_PARAMETER=87,ERROR_ALREADY_EXISTS=183,ERROR_NOT_ENOUGH_MEMORY=8,ERROR_ACCESS_DENIED=5,ERROR_INVALID_HANDLE=6,ERROR_REVISION_MISMATCH=1306,ERROR_BUSY=170,ERROR_NOT_READY=21,ERROR_SUCCESS=0,ERROR_PARTIAL_COPY=299;
constexpr unsigned INPUT_MOUSE=0,MOUSEEVENTF_LEFTDOWN=2,MOUSEEVENTF_LEFTUP=4,SYNCHRONIZE=1,WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,PAGE_READWRITE=4,FILE_MAP_ALL_ACCESS=1,WT_EXECUTEDEFAULT=0,WM_NULL=0;
constexpr bool FALSE=false;
constexpr unsigned WM_SETCURSOR=32,WM_MOUSEMOVE=512,HTCLIENT=1,SMTO_ABORTIFHUNG=2,SMTO_BLOCK=1,SMTO_ERRORONEXIT=32;
#define MAKELPARAM(a,b) (LPARAM(uint32_t(a)|(uint32_t(b)<<16)))
bool SendMessageTimeoutW(HWND,UINT,WPARAM,LPARAM,UINT,UINT,DWORD_PTR*);
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
constexpr int VK_LBUTTON=1,VK_MBUTTON=4,VK_XBUTTON1=5,VK_XBUTTON2=6,VK_RBUTTON=2,VK_CONTROL=17;
struct LARGE_INTEGER{int64_t QuadPart;};bool QueryPerformanceCounter(LARGE_INTEGER*);bool QueryPerformanceFrequency(LARGE_INTEGER*);
constexpr unsigned TIMERR_NOERROR=0;unsigned timeBeginPeriod(unsigned);unsigned timeEndPeriod(unsigned);bool ChangeTimerQueueTimer(HANDLE,HANDLE,DWORD,DWORD);
struct POINT{LONG x,y;};struct RECT{LONG left,top,right,bottom;};
HWND WindowFromPoint(POINT);short GetAsyncKeyState(int);bool IsIconic(HWND);bool GetCursorPos(POINT*);bool ScreenToClient(HWND,POINT*);bool GetClientRect(HWND,RECT*);
struct INPUT{unsigned type=0;struct {unsigned dwFlags=0;uintptr_t dwExtraInfo=0;}mi;};
constexpr unsigned PROCESS_QUERY_LIMITED_INFORMATION=4096,PROCESS_QUERY_INFORMATION=1024,PROCESS_VM_READ=16,STILL_ACTIVE=259;
struct MEMORY_BASIC_INFORMATION{DWORD State=0,Protect=0;};
HANDLE OpenProcess(DWORD,bool,DWORD);bool GetExitCodeProcess(HANDLE,DWORD*);
bool QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*,DWORD*);
bool ReadProcessMemory(HANDLE,const void*,void*,size_t,SIZE_T*);
SIZE_T VirtualQueryEx(HANDLE,const void*,MEMORY_BASIC_INFORMATION*,size_t);
DWORD GetWindowThreadProcessId(HWND,DWORD*);HWND GetForegroundWindow();void SetLastError(DWORD);DWORD GetLastError();
HHOOK SetWindowsHookExW(int,HOOKPROC,HMODULE,DWORD);bool UnhookWindowsHookEx(HHOOK);UINT SendInput(UINT,INPUT*,int);
DWORD GetModuleFileNameW(HMODULE,wchar_t*,DWORD);HMODULE LoadLibraryW(const wchar_t*);void* GetProcAddress(HMODULE,const char*);bool FreeLibrary(HMODULE);
DWORD GetTickCount();DWORD GetCurrentProcessId();bool PostMessageW(HWND,UINT,uintptr_t,intptr_t);
HANDLE OpenThread(DWORD,bool,DWORD);DWORD WaitForSingleObject(HANDLE,DWORD);bool CloseHandle(HANDLE);
HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD,DWORD,const wchar_t*);void* MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,size_t);bool UnmapViewOfFile(void*);
bool CreateTimerQueueTimer(HANDLE*,HANDLE,void(*)(void*,BOOLEAN),void*,DWORD,DWORD,DWORD);
inline LONG InterlockedExchange(volatile LONG*p,LONG v){return __atomic_exchange_n(p,v,__ATOMIC_SEQ_CST);}
inline LONG InterlockedCompareExchange(volatile LONG*p,LONG v,LONG c){__atomic_compare_exchange_n(p,&c,v,false,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return c;}
inline LONG InterlockedOr(volatile LONG*p,LONG v){return __atomic_fetch_or(p,v,__ATOMIC_SEQ_CST);}
'''
body=r'''
#include "host_game_input.h"
#include "../../assist/source/pointer_intent.h"
#include <cassert>
#include <string>
#include <cstring>
#include <cwchar>
#include <thread>
extern gameinput::Host* otherHost();extern bool otherRelease(gameinput::Subscription);
static DWORD error=0,fg=42,clockMs=100,windowThread=12;static int installs=0,unhooks=0,sends=0,loads=0,posts=0;static UINT sentCount=2;static bool failUnhook=false,failPost=false,oldDead=false,failPostHook=false;
static uint64_t preciseOverride=0;static int timerBalance=0,timerPeriod=4;
bool QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=preciseOverride?preciseOverride:uint64_t(clockMs)*1000;return true;}bool QueryPerformanceFrequency(LARGE_INTEGER*p){p->QuadPart=1000000;return true;}
unsigned timeBeginPeriod(unsigned p){assert(p==1);++timerBalance;return 0;}unsigned timeEndPeriod(unsigned p){assert(p==1);--timerBalance;return 0;}bool ChangeTimerQueueTimer(HANDLE,HANDLE,DWORD due,DWORD period){assert(due==period);timerPeriod=period;return true;}
static bool keyDown[256]{};static uint32_t nativeHwnd=42;static bool readable=true;
HANDLE OpenProcess(DWORD,bool,DWORD pid){return (HANDLE)(uintptr_t)pid;}
bool GetExitCodeProcess(HANDLE,DWORD*p){*p=STILL_ACTIVE;return true;}
bool QueryFullProcessImageNameW(HANDLE,DWORD,wchar_t*,DWORD*){return false;}
bool ReadProcessMemory(HANDLE,const void*at,void*out,size_t n,SIZE_T*got){assert(uintptr_t(at)==assist::messagequery::YrppGameHwndAddr&&n==4);*got=readable?4:0;if(readable)memcpy(out,&nativeHwnd,4);return readable;}
SIZE_T VirtualQueryEx(HANDLE,const void*,MEMORY_BASIC_INFORMATION*,size_t){return 0;}

short GetAsyncKeyState(int k){return keyDown[k]?short(0x8000):0;}bool IsIconic(HWND){return false;}
bool GetCursorPos(POINT*p){*p={100,100};return true;}bool ScreenToClient(HWND,POINT*){return true;}bool GetClientRect(HWND,RECT*p){*p={0,0,800,600};return true;}
static runtime::Control block{};static int cursorRefreshes=0;static bool refreshSucceeds=true;
bool SendMessageTimeoutW(HWND w,UINT msg,WPARAM wp,LPARAM lp,UINT flags,UINT timeout,DWORD_PTR*){
 assert(w==(HWND)42&&wp==uintptr_t(w)&&msg==WM_SETCURSOR&&lp==MAKELPARAM(HTCLIENT,WM_MOUSEMOVE));
 assert(timeout==10&&flags==(SMTO_ABORTIFHUNG|SMTO_BLOCK|SMTO_ERRORONEXIT));
 bool free=false;std::thread check([&]{free=gameinput::host().mutex.try_lock();if(free)gameinput::host().mutex.unlock();});check.join();assert(free);
 ++cursorRefreshes;return refreshSucceeds;
}
DWORD GetWindowThreadProcessId(HWND w,DWORD*p){*p=(w==(HWND)142||w==(HWND)242)?42:uint32_t(uintptr_t(w));return w?(w==(HWND)142?windowThread+1:windowThread):0;}
HWND WindowFromPoint(POINT){return reinterpret_cast<HWND>(uintptr_t(fg));}
HWND GetForegroundWindow(){return reinterpret_cast<HWND>(uintptr_t(fg));}
void SetLastError(DWORD e){error=e;}DWORD GetLastError(){return error;}
HHOOK SetWindowsHookExW(int kind,HOOKPROC,HMODULE,DWORD){++installs;if(failPostHook&&kind==WH_CALLWNDPROCRET){error=1428;return nullptr;}return reinterpret_cast<HHOOK>(uintptr_t(installs));}
bool UnhookWindowsHookEx(HHOOK){++unhooks;error=0;return !failUnhook;}
DWORD GetModuleFileNameW(HMODULE,wchar_t*p,DWORD){std::wcscpy(p,L"C:\\Toolbox\\RA2Toolbox.exe");return std::wcslen(p);}
HMODULE LoadLibraryW(const wchar_t*p){assert(std::wcscmp(p,L"C:\\Toolbox\\GameRuntime.dll")==0);++loads;return (HMODULE)1;}
void* GetProcAddress(HMODULE,const char*n){static uint32_t info[]={4,sizeof(runtime::Control)},entry[]={3,108};if(!strcmp(n,"GameRuntimeInfo"))return info;if(!strcmp(n,"GameEntryManagerInfo"))return entry;return (void*)1;}
bool FreeLibrary(HMODULE){return true;}DWORD GetTickCount(){return clockMs;}DWORD GetCurrentProcessId(){return 456;}
bool PostMessageW(HWND window,UINT msg,uintptr_t tag,intptr_t owner){assert(window==(HWND)(uintptr_t)nativeHwnd&&msg==WM_NULL&&tag==runtime::WakeTag&&owner==456);++posts;if(failPost)error=5;return !failPost;}
HANDLE OpenThread(DWORD,bool,DWORD tid){return (HANDLE)(uintptr_t)tid;}DWORD WaitForSingleObject(HANDLE,DWORD){return oldDead?WAIT_OBJECT_0:WAIT_TIMEOUT;}bool CloseHandle(HANDLE){return true;}
HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD,DWORD n,const wchar_t*){assert(n==sizeof(block));return (HANDLE)1;}
void* MapViewOfFile(HANDLE,DWORD,DWORD,DWORD,size_t){return &block;}bool UnmapViewOfFile(void*){return true;}
bool CreateTimerQueueTimer(HANDLE*p,HANDLE,void(*)(void*,BOOLEAN),void*,DWORD,DWORD,DWORD){*p=(HANDLE)1;return true;}
UINT SendInput(UINT n,INPUT*input,int size){assert(size==sizeof(INPUT));++sends;if(n==2){assert(input[0].mi.dwFlags==2&&input[1].mi.dwFlags==4);fg=99;error=5;return sentCount;}assert(n==1&&input[0].mi.dwFlags==4);return 1;}
int main(){
 using namespace gameinput;using gameentry::Client;assert(otherHost()==&host());auto win=(HWND)42;
 assert(!install(Client::Assist,13,42,win)&&installs==0);assert(!install(Client::Assist,12,43,win)&&installs==0);
 nativeHwnd=0;assert(!install(Client::Assist,12,42,win)&&GetLastError()==ERROR_NOT_READY&&installs==0);assert(std::wstring(installationStage())==L"native-window-check");NativeWindowProbe probe;assert(!nativeWindow(42,12,&probe)&&probe.read&&probe.raw==0);
 nativeHwnd=142;assert(!install(Client::Cursor,12,42,win)&&GetLastError()==ERROR_NOT_READY&&installs==0); // live helper thread cannot bind
 nativeHwnd=42;readable=false;assert(!install(Client::Assist,12,42,win)&&installs==0);readable=true;
 auto a=install(Client::Assist,12,42,win);auto c=install(Client::Cursor,12,42,win);assert(a&&c&&a!=c&&installs==2&&loads==1&&block.enabled==3);
 block.callbacks=7;block.callbackThread=12;block.stage=runtime::Connected;block.hookRva=123;
 auto diagnostic=health(a);assert(diagnostic.diagnosticValid&&diagnostic.callbacks==7&&diagnostic.callbackThread==12&&diagnostic.stage==2&&diagnostic.hookRva==123);
 block.diagnosticSeq=1;block.callbacks=99;block.stage=-4;auto busy=health(a);
 assert(!busy.diagnosticValid&&busy.diagnosticCached&&busy.callbacks==7&&busy.stage==2);
 block.diagnosticSeq=2;block.faultSerial=1;block.faultCode=0xc0000005;block.faultAddress=0x12345678;
 auto fault=health(a);assert(fault.diagnosticValid&&!fault.diagnosticCached&&fault.faultSerial==1&&fault.faultCode==0xc0000005&&fault.faultAddress==0x12345678);
 assert(posts==1&&block.ready==3);for(int i=0;i<100;++i)wake(a);assert(posts==1);
 block.wakePending=0;block.ready=0;clockMs+=4;pulse(nullptr,0);assert(block.ready==3&&posts==2);
 block.wakePending=0;block.ready=0;clockMs+=4;pulse(nullptr,0);assert(block.ready==2); // cursor due, tactical not yet
 assert(!release(Client::Cursor,a)&&unhooks==0);
 std::thread t1([&]{for(int i=0;i<100;++i){auto q=install(Client::Assist,12,42,win);assert(q);assert(release(Client::Assist,q));}});
 std::thread t2([&]{for(int i=0;i<100;++i){auto q=install(Client::Cursor,12,42,win);assert(q);assert(release(Client::Cursor,q));}});t1.join();t2.join();assert(installs==2);
 PointerPress press{};DWORD dropped=0;
 // Loading/menu polling has no active capture context, even while focused.
 keyDown[VK_MBUTTON]=true;pulse(nullptr,0);assert(!takePointer(a,press,dropped));
 setPointerContext(a,1,true);pulse(nullptr,0);assert(!takePointer(a,press,dropped));
 keyDown[VK_MBUTTON]=false;pulse(nullptr,0);
 setFastTactical(a,true);block.wakePending=0;block.ready=0;clockMs+=4;pulse(nullptr,0);assert(block.ready==3);
 assert(timerBalance==1&&timerPeriod==1);
 preciseOverride=uint64_t(clockMs)*1000+1000;block.wakePending=0;block.ready=0;pulse(nullptr,0);assert(block.ready==runtime::Tactical); // coarse GetTickCount unchanged
 preciseOverride=0;
 setFastTactical(a,false);assert(timerBalance==0&&timerPeriod==4);block.wakePending=0;block.ready=0;clockMs+=4;pulse(nullptr,0);assert(block.ready==2);
 keyDown[VK_MBUTTON]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped)&&press.operation==1&&press.serial==pointerCancelSerial(a));
 pulse(nullptr,0);assert(!takePointer(a,press,dropped)); // held button never repeats
 keyDown[VK_MBUTTON]=false;pulse(nullptr,0);
 keyDown[VK_CONTROL]=true;keyDown[VK_RBUTTON]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped)&&press.operation==5);
 keyDown[VK_CONTROL]=false;keyDown[VK_RBUTTON]=false;pulse(nullptr,0);assert(takePointer(a,press,dropped)&&press.operation==4&&press.keys[VK_CONTROL]);
 keyDown[VK_XBUTTON2]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped)&&press.operation==3&&!press.keys[VK_CONTROL]);
 {using namespace assist::pointerinput;Bindings config{0,0,0,0,0,0,17,17,512};
  assert(!(resolveBindings(press.keys,press.operation,config)&Control));config.sell=0;assert(resolveBindings(press.keys,press.operation,config)&Control);
  config.enabled=0;assert(!(resolveBindings(press.keys,press.operation,config)&Control));}
 keyDown[VK_XBUTTON2]=false;pulse(nullptr,0);
 keyDown[VK_CONTROL]=true;keyDown[VK_XBUTTON2]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped)&&press.operation==3);
 keyDown[VK_CONTROL]=false;keyDown[VK_XBUTTON2]=false;pulse(nullptr,0);
 for(int i=0;i<20;++i){keyDown[VK_MBUTTON]=true;pulse(nullptr,0);keyDown[VK_MBUTTON]=false;pulse(nullptr,0);}
 assert(takePointer(a,press,dropped)&&dropped==4);
 fg=99;pulse(nullptr,0);assert(!takePointer(a,press,dropped));fg=42;keyDown[VK_MBUTTON]=true;pulse(nullptr,0);assert(!takePointer(a,press,dropped));keyDown[VK_MBUTTON]=false;pulse(nullptr,0);
 // A held right button from another window is not a new native click.
 fg=99;keyDown[VK_RBUTTON]=true;pulse(nullptr,0);fg=42;pulse(nullptr,0);
 keyDown[VK_RBUTTON]=false;pulse(nullptr,0);assert(!takePointer(a,press,dropped));
 // A valid down followed by focus loss is also disarmed.
 keyDown[VK_RBUTTON]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped)&&press.operation==5);
 fg=99;pulse(nullptr,0);fg=42;pulse(nullptr,0);keyDown[VK_RBUTTON]=false;pulse(nullptr,0);assert(!takePointer(a,press,dropped));
 // A queued click belongs to its capture match and cannot cross epochs.
 keyDown[VK_MBUTTON]=true;pulse(nullptr,0);setPointerContext(a,2,true);
 assert(!takePointer(a,press,dropped));pulse(nullptr,0);
 keyDown[VK_MBUTTON]=false;pulse(nullptr,0);keyDown[VK_MBUTTON]=true;pulse(nullptr,0);
 assert(takePointer(a,press,dropped)&&press.match==2);keyDown[VK_MBUTTON]=false;pulse(nullptr,0);
 // Right down before a menu/match transition must not become a release intent.
 keyDown[VK_RBUTTON]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped));
 auto serial=pointerCancelSerial(a);setPointerContext(a,2,false);assert(pointerCancelSerial(a)!=serial);
 setPointerContext(a,3,true);pulse(nullptr,0);keyDown[VK_RBUTTON]=false;pulse(nullptr,0);
 assert(!takePointer(a,press,dropped));
 // UI starvation expires capture; re-arming never interprets a held button.
 keyDown[VK_RBUTTON]=true;pulse(nullptr,0);assert(takePointer(a,press,dropped));
 clockMs+=151;pulse(nullptr,0);keyDown[VK_RBUTTON]=false;pulse(nullptr,0);assert(!takePointer(a,press,dropped));
 setPointerContext(a,3,true);pulse(nullptr,0);keyDown[VK_MBUTTON]=true;pulse(nullptr,0);
 assert(takePointer(a,press,dropped)&&press.match==3);keyDown[VK_MBUTTON]=false;pulse(nullptr,0);
 assert(otherRelease(a)&&unhooks==0&&block.enabled==2);assert(wake(c));assert(!wake(a)&&!release(Client::Assist,a));
 failPost=true;block.wakePending=0;wake(c);assert(block.wakePending==0&&health(c).postError==5);failPost=false;wake(c);assert(block.wakePending==1&&health(c).postError==0);
 int before=posts;clockMs+=251;wake(c);assert(posts==before+1); // bounded lost-wake recovery
 windowThread=13;block.wakePending=0;before=posts;wake(c);assert(posts==before&&health(c).postError==ERROR_INVALID_PARAMETER&&block.wakePending==0);assert(!install(Client::Assist,13,42,win));oldDead=true;auto migrated=install(Client::Assist,13,42,win);assert(migrated&&installs==4&&block.enabled==3);oldDead=false;
 assert(release(Client::Assist,migrated)&&block.enabled==2);
 failUnhook=true;assert(release(Client::Cursor,c)&&block.enabled==0);assert(host().targets[0].post);failUnhook=false;pulse(nullptr,0);assert(!host().targets[0].pid);
 failPostHook=true;assert(!install(Client::Cursor,13,42,win));assert(GetLastError()==1428);assert(std::wstring(installationStage())==L"WH_CALLWNDPROCRET");assert(!host().targets[0].pid);failPostHook=false;
 auto fresh=install(Client::Cursor,13,42,win);assert(fresh&&fresh!=c);assert(!release(Client::Cursor,c));assert(release(Client::Cursor,fresh));
 nativeHwnd=242;auto rebound=install(Client::Assist,13,42,win);assert(rebound&&host().targets[0].window==(HWND)242);
 nativeHwnd=42;auto sameThread=install(Client::Cursor,13,42,win);assert(sameThread&&host().targets[0].window==win);assert(release(Client::Assist,rebound));assert(release(Client::Cursor,sameThread));
 assert(!refreshCursor(win,43)&&!refreshCursor(nullptr,42)&&cursorRefreshes==0);
 fg=99;assert(!refreshCursor(win,42)&&cursorRefreshes==0);fg=42;
 assert(refreshCursor(win,42)&&cursorRefreshes==1);refreshSucceeds=false;assert(!refreshCursor(win,42)&&cursorRefreshes==2);
 fg=99;assert(clickPair(win,42).sent==0&&sends==0);fg=42;sentCount=1;auto partial=clickPair(win,42);assert(partial.sent==1&&partial.error==5&&partial.released&&sends==2);
 fg=42;sentCount=0;assert(clickPair(win,42).sent==0&&sends==3);fg=42;sentCount=2;auto success=clickPair(win,42);assert(success.sent==2&&success.error==0&&success.released&&sends==4);
}
'''
other='''#include "host_game_input.h"
gameinput::Host* otherHost(){return &gameinput::host();}
bool otherRelease(gameinput::Subscription h){return gameinput::release(gameentry::Client::Assist,h);}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'windows.h').write_text(header);(p/'mmsystem.h').write_text('');(p/'main.cpp').write_text(body);(p/'other.cpp').write_text(other)
 subprocess.run(['g++','-std=c++17','-pthread','-I'+temp,'-I'+str(root/'shared/source'),str(p/'main.cpp'),str(p/'other.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS shared host: one hook pair; 200 concurrent subscriptions; independent release; cadence/coalescing; failed post/unhook recovery; dead-thread migration; stale tokens; balanced input')

print('PASS cursor refresh uses host input manager, validates target/focus, times out without holding scheduler mutex')

print("PASS window-addressed wake, post error diagnostics, stale HWND/thread rejected without losing ready bits")

print("PASS pointer capture: loading suppressed; capture epoch retained; cross-match/menu clicks discarded; held release disarmed; context timeout bounded")

print("PASS startup target: zero/unreadable native HWND and live helper thread rejected before hooks; same-thread HWND replacement updates all wake subscribers")
