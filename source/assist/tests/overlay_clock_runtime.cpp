#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cassert>
using DWORD=uint32_t;using LONG=int32_t;using UINT=unsigned;using HANDLE=void*;
using HWND=void*;using LONGLONG=int64_t;
#define WINAPI
constexpr DWORD WAIT_OBJECT_0=0,WAIT_FAILED=0xFFFFFFFF,INFINITE=0xFFFFFFFF,TIMER_ALL_ACCESS=1,WM_APP=0x8000;
constexpr bool FALSE=false,TRUE=true;
struct LARGE_INTEGER{int64_t QuadPart=0;};
static unsigned mode=0,waitsMade=0,controls=0,frames=0,inputs=0,closed=0;static uint64_t ticks=0,dueTicks=0;
HANDLE CreateWaitableTimerExW(void*,void*,unsigned,unsigned){return mode?reinterpret_cast<HANDLE>(3):nullptr;}
HANDLE CreateWaitableTimerW(void*,bool,void*){return nullptr;}
bool SetWaitableTimer(HANDLE,LARGE_INTEGER*due,unsigned,void*,void*,bool){assert(due->QuadPart<0);dueTicks=uint64_t(-due->QuadPart);return mode==2;}
bool CancelWaitableTimer(HANDLE h){assert(h);return true;}
bool CloseHandle(HANDLE h){assert(h);++closed;return true;}
bool SetEvent(HANDLE){return true;}
HANDLE CreateEventW(void*,bool,bool,void*){return reinterpret_cast<HANDLE>(1);}
HANDLE CreateThread(void*,unsigned,DWORD(*)(void*),void*,unsigned,void*){return nullptr;}
DWORD WaitForSingleObject(HANDLE,DWORD){return WAIT_OBJECT_0;}
LONG InterlockedCompareExchange(volatile LONG*p,LONG next,LONG old){auto result=*p;if(result==old)*p=next;return result;}
LONG InterlockedExchange(volatile LONG*p,LONG next){auto result=*p;*p=next;return result;}
bool QueryPerformanceCounter(LARGE_INTEGER*p){p->QuadPart=int64_t(ticks);return true;}
bool QueryPerformanceFrequency(LARGE_INTEGER*p){p->QuadPart=10000000;return true;}
uint64_t GetTickCount64(){return ticks/10000;}
DWORD WaitForMultipleObjects(DWORD n,const HANDLE*,bool,DWORD timeout){
 assert(n==(mode==2?3u:2u));if(mode==2){assert(timeout==INFINITE);ticks+=dueTicks;}else{assert(timeout>=1&&timeout<=16);ticks+=uint64_t(timeout)*10000;}
 return ++waitsMade==200?WAIT_OBJECT_0:WAIT_OBJECT_0+2;
}
bool PostMessageW(HWND,UINT,unsigned,unsigned);
namespace visual{constexpr int DefaultFPS=60;}
static HWND ui=nullptr;static bool enabled=true,shiftClickEnabled=true;static int shiftClickMode=0;
static bool foregroundGame(){return true;}
static struct{int renderStatus=1;bool chat=false;}snap;
using namespace std;
// Win32 uint64_t is unsigned long long; model that ABI type on Linux LP64.
using WinClockWord=unsigned long long;
#define uint64_t WinClockWord
#include "../source/pacing.h"
using namespace assist;
#include FRAME_CLOCK_HEADER
#undef uint64_t
bool PostMessageW(HWND,UINT id,unsigned,unsigned){if(id==ControlMessage){++controls;controlPending=0;}else if(id==FrameMessage){++frames;framePending=0;}else if(id==InputMessage){++inputs;inputPending=0;}else assert(false);return true;}
int main(){for(mode=0;mode<3;++mode){waitsMade=controls=frames=inputs=closed=0;ticks=dueTicks=0;clockActive=1;clockRate=240;inputClockActive=1;
 frameClock(nullptr);assert(waitsMade==200&&controls>20&&frames>70&&inputs>70);assert(closed==(mode?1u:0u));
 std::printf("PASS actual frameClock mode=%u waits=%u control=%u frame=%u input=%u; controlled Win32 timer failure\n",mode,waitsMade,controls,frames,inputs);
}}
