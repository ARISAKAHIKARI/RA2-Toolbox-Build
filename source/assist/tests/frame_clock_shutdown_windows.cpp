#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include "../source/pacing.h"

namespace visual {constexpr int DefaultFPS=60;}
using namespace assist;
static HWND ui=nullptr;
static bool enabled=false,shiftClickEnabled=false;
static int shiftClickMode=0;
static bool foregroundGame(){return false;}
#include "../source/frame_clock.h"

static DWORD WINAPI resumeLater(void* handle){Sleep(1250);ResumeThread(HANDLE(handle));return 0;}
static int fail(const char* reason){std::printf("FAIL frame clock: %s\n",reason);return 1;}

int main(){
 startClock();
 if(!clockThread||!clockStop||!clockChanged)return fail("failed to start");
 HANDLE resumeHandle=nullptr;
 if(!DuplicateHandle(GetCurrentProcess(),clockThread,GetCurrentProcess(),&resumeHandle,0,FALSE,DUPLICATE_SAME_ACCESS))return fail("duplicate worker");
 if(SuspendThread(clockThread)==DWORD(-1))return fail("suspend worker");
 HANDLE resumer=CreateThread(nullptr,0,resumeLater,resumeHandle,0,nullptr);
 if(!resumer){ResumeThread(clockThread);return fail("start resumer");}
 DWORD began=GetTickCount();
 stopClock();
 DWORD elapsed=GetTickCount()-began;
 WaitForSingleObject(resumer,INFINITE);CloseHandle(resumer);CloseHandle(resumeHandle);
 if(elapsed<1100)return fail("closed event handles before the worker exited");
 if(clockThread||clockStop||clockChanged)return fail("handles not reset");
 for(int i=0;i<24;++i){
  startClock();if(!clockThread||!clockStop||!clockChanged)return fail("restart failed");
  clockState(true,144);Sleep(1);stopClock();
  if(clockThread||clockStop||clockChanged)return fail("restart leaked handles");
 }
 std::puts("PASS frame clock: delayed worker joined before event destruction; repeated start/stop");
 return 0;
}
