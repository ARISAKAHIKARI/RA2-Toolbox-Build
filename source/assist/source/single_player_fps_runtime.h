#pragma once
// Included by the existing game-thread bridge. One logical tag participates
// in its shared four-slot hardware service; no instruction bytes are written.
#include "single_player_fps.h"
#include "tech_ui_breakpoint_core.h"
namespace assist { namespace singlefps {
static Clock clock;static HANDLE timer;static uint64_t frequency;
static uint32_t scenario;static int lastFrame=-1;
static bool desired=false,enabled=false;static int failure=0;
static techbp::SignatureSnapshot<36> native;
static void disable(){enabled=false;clock.reset();if(sh)sh->singlePlayerFPSActive=0;}
static bool fresh(){return sh&&!sh->stop&&!fault&&DWORD(sampleClock(sh)-sh->heartbeat)<=500&&sh->singlePlayerFPS>=10&&valid(sh->singlePlayerFPS);}
static bool live(){return fresh()&&session(get<int>(0xA8B238))&&get<bool>(0xA8E9A0)&&scenario&&get<uint32_t>(0xA8B230)==scenario&&get<int>(0xA8ED84)>=lastFrame;}
static void maintain(){
 desired=false;failure=0;
 if(!fresh()){disable();return;}
 const int mode=get<int>(0xA8B238);
 if(!session(mode)){disable();failure=-6;return;}
 const uint32_t current=get<uint32_t>(0xA8B230);const int frame=get<int>(0xA8ED84);
 if(!current||frame<0||!get<bool>(0xA8E9A0)){disable();return;}
 if(current!=scenario||frame<lastFrame)disable();scenario=current;lastFrame=frame;
 if(!native.refresh(0x55D79E,0xA0B5F183u,read)){disable();failure=techbp::Signature;return;}
 if(!frequency){LARGE_INTEGER hz{};if(!QueryPerformanceFrequency(&hz)||hz.QuadPart<240){disable();failure=techbp::System;return;}frequency=uint64_t(hz.QuadPart);}
 if(!timer){timer=CreateWaitableTimerExW(nullptr,nullptr,2u,TIMER_ALL_ACCESS);if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);}
 if(!timer){disable();failure=techbp::System;return;}
 desired=true;
}
static bool parameter(CONTEXT&context){
 if(!enabled||context.Eip!=Entry||!live()) {disable();return false;}
 if(!native.matches(reinterpret_cast<const volatile uint8_t*>(0x55D79E))){disable();failure=techbp::Signature;return false;}
 const unsigned rate=sh->singlePlayerFPS;LARGE_INTEGER now{};
 if(!QueryPerformanceCounter(&now)){disable();failure=techbp::System;return false;}
 const uint64_t due=clock.deadline(uint64_t(now.QuadPart),rate,frequency);
 // No message pumping or engine entry inside the exception callback. Short
 // waits observe an off request, Host loss or a world change before writing.
 while(uint64_t(now.QuadPart)<due){
  if(!live()||sh->singlePlayerFPS!=rate){disable();return false;}
  uint64_t ticks=(due-uint64_t(now.QuadPart))*10000000u/frequency;if(ticks>40000)ticks=40000;if(!ticks)ticks=1;
  LARGE_INTEGER delay{};delay.QuadPart=-LONGLONG(ticks);
  if(!SetWaitableTimer(timer,&delay,0,nullptr,nullptr,FALSE)||WaitForSingleObject(timer,20)!=WAIT_OBJECT_0||!QueryPerformanceCounter(&now)){disable();failure=techbp::System;return false;}
 }
 if(!live()||sh->singlePlayerFPS!=rate||!native.matches(reinterpret_cast<const volatile uint8_t*>(0x55D79E))){disable();return false;}
 // Substitute precisely this MOV's data destination, preserving ESI and all
 // arithmetic flags. The next native iteration overwrites the timer again.
 *reinterpret_cast<volatile uint32_t*>(TimerDuration)=0;context.Eip=Resume;
 sh->singlePlayerFPSActive=rate;techbp::countOwner(sh->singlePlayerFPSHits);return true;
}
static void release(){disable();desired=false;failure=0;if(timer){CloseHandle(timer);timer=nullptr;}scenario=0;lastFrame=-1;}
static unsigned activeRate(){return enabled&&live()&&sh->singlePlayerFPSActive==sh->singlePlayerFPS?sh->singlePlayerFPSActive:0;}
}}
