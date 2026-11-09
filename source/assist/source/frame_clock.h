#include "pacing.h"
// Control messages use the same bounded clock, independently of the paint rate.
static HANDLE clockStop,clockChanged,clockThread;
static volatile LONG clockRate=visual::DefaultFPS,clockActive,framePending,controlPending;
static constexpr UINT FrameMessage=WM_APP+1;
static constexpr UINT ControlMessage=WM_APP+3;
static constexpr UINT InputMessage=WM_APP+4;static volatile LONG inputPending,inputClockActive;
static DWORD WINAPI frameClock(void*){
 HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
 if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
 HANDLE waits[3]={clockStop,clockChanged,timer};
 LARGE_INTEGER start{},frequency{},counter{};QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);
 pacing::Clock deadlines;uint64_t inputDue=0;
 for(;;){
  LONG active=InterlockedCompareExchange(&clockActive,0,0);
  LONG rate=InterlockedCompareExchange(&clockRate,0,0);
  QueryPerformanceCounter(&counter);uint64_t elapsed=counter.QuadPart>=start.QuadPart?uint64_t(counter.QuadPart-start.QuadPart):0;
  uint64_t now=frequency.QuadPart>0?(elapsed/uint64_t(frequency.QuadPart))*10000000ull+(elapsed%uint64_t(frequency.QuadPart))*10000000ull/uint64_t(frequency.QuadPart):GetTickCount64()*10000ull;
  auto step=deadlines.step(now,active!=0,int(rate));
  if(step.control&&!InterlockedCompareExchange(&controlPending,1,0)&&!PostMessageW(ui,ControlMessage,0,0))InterlockedExchange(&controlPending,0);
   // A slow paint keeps its single pending marker until it FINISHES. This
   // leaves room for controls and prevents a perpetual paint-message backlog.
  if(step.frame&&!InterlockedCompareExchange(&framePending,1,0)&&!PostMessageW(ui,FrameMessage,0,0))InterlockedExchange(&framePending,0);
  if(InterlockedCompareExchange(&inputClockActive,0,0)){
   if(now>=inputDue){inputDue=now+40000;if(!InterlockedCompareExchange(&inputPending,1,0)&&!PostMessageW(ui,InputMessage,0,0))InterlockedExchange(&inputPending,0);}
   step.wait=std::min(step.wait,inputDue>now?inputDue-now:1ull);
  }else inputDue=now;
  LARGE_INTEGER due;due.QuadPart=-LONGLONG(step.wait);
  // Timer creation/arming can fail without losing the control, input and
  // paint clocks. The same stop/change events interrupt the timed fallback.
  const bool armed=timer&&SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);
  const DWORD timeout=armed?INFINITE:DWORD(std::max<uint64_t>(1,(step.wait+9999)/10000));
  DWORD result=WaitForMultipleObjects(armed?3:2,waits,FALSE,timeout);if(result==WAIT_OBJECT_0||result==WAIT_FAILED)break;
 }
 if(timer){CancelWaitableTimer(timer);CloseHandle(timer);}return 0;
}
static void clockState(bool active,int rate){
 bool changed=InterlockedExchange(&clockActive,active)!=active;
 // Presentation can be hidden while ordinary game input remains enabled.
 const bool input=enabled&&foregroundGame()&&snap.renderStatus==1&&!snap.chat&&shiftClickEnabled&&shiftClickMode!=2;
 changed|=InterlockedExchange(&inputClockActive,input)!=input;
 changed|=InterlockedExchange(&clockRate,rate)!=rate;
 if(changed&&clockChanged)SetEvent(clockChanged);
}
static void startClock(){
 clockStop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
 clockChanged=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 if(clockStop&&clockChanged)clockThread=CreateThread(nullptr,0,frameClock,nullptr,0,nullptr);
 if(!clockThread){
  if(clockStop)CloseHandle(clockStop);
  if(clockChanged)CloseHandle(clockChanged);
  clockStop=clockChanged=nullptr;
 }
}
static void stopClock(){
 if(clockStop)SetEvent(clockStop);
 // The worker owns its wait array and may still be reading both events.
 // It never waits for the UI thread, so joining it cannot deadlock the owner.
 if(clockThread){WaitForSingleObject(clockThread,INFINITE);CloseHandle(clockThread);}
 if(clockStop)CloseHandle(clockStop);
 if(clockChanged)CloseHandle(clockChanged);
 clockThread=clockStop=clockChanged=nullptr;
}
