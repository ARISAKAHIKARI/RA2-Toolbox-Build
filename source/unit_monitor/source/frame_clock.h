#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#endif

namespace umframe {
// Deadlines use 100 ns units, independent of the 16 ms control timer and the
// 250 ms statistics sampler. A late presentation starts one fresh interval.
struct Step {bool frame=false;uint64_t wait=160000;};
class Schedule {
 uint64_t next_=0;int rate_=0;bool active_=false,initialized_=false;
public:
 Step step(uint64_t now,bool active,int rate){
  rate=std::clamp(rate,5,500);
  if(!initialized_||active!=active_||rate!=rate_)next_=now;
  initialized_=true;active_=active;rate_=rate;
  if(!active)return {};
  Step out;
  if(now>=next_){out.frame=true;next_=now+10000000ull/uint64_t(rate);}
  out.wait=next_>now?next_-now:1;return out;
 }
};
class Pending {
 std::atomic<bool> pending_{false};
public:
 bool claim(){return !pending_.exchange(true);}
 void complete(){pending_.store(false);}
};

#ifdef _WIN32
class Clock {
 HWND window_=nullptr;UINT message_=0;HANDLE stop_=nullptr,changed_=nullptr,thread_=nullptr;
 std::atomic<bool> active_{false};std::atomic<int> rate_{60};Pending pending_;
 static DWORD WINAPI entry(void* self){static_cast<Clock*>(self)->run();return 0;}
 void run(){
  HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
  if(!timer)timer=CreateWaitableTimerW(nullptr,FALSE,nullptr);
  HANDLE waits[]={stop_,changed_,timer};Schedule schedule;
  LARGE_INTEGER start{},frequency{},counter{};QueryPerformanceCounter(&start);QueryPerformanceFrequency(&frequency);
  for(;;){
   bool active=active_.load();QueryPerformanceCounter(&counter);
   uint64_t elapsed=counter.QuadPart>=start.QuadPart?uint64_t(counter.QuadPart-start.QuadPart):0;
   uint64_t now=frequency.QuadPart>0?(elapsed/uint64_t(frequency.QuadPart))*10000000ull+(elapsed%uint64_t(frequency.QuadPart))*10000000ull/uint64_t(frequency.QuadPart):GetTickCount64()*10000ull;
   auto step=schedule.step(now,active,rate_.load());
   if(step.frame&&pending_.claim()&&!PostMessageW(window_,message_,0,0))pending_.complete();
   DWORD count=2,timeout=INFINITE;
   if(active){
    LARGE_INTEGER due{};due.QuadPart=-LONGLONG(step.wait);
    if(timer&&SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))count=3;
    else timeout=DWORD(std::max<uint64_t>(1,(step.wait+9999)/10000));
   }else if(timer)CancelWaitableTimer(timer);
   DWORD result=WaitForMultipleObjects(count,waits,FALSE,timeout);
   if(result==WAIT_OBJECT_0||result==WAIT_FAILED)break;
  }
  if(timer){CancelWaitableTimer(timer);CloseHandle(timer);}
 }
public:
 ~Clock(){shutdown();}
 bool start(HWND window,UINT message){
  if(thread_)return true;window_=window;message_=message;
  stop_=CreateEventW(nullptr,TRUE,FALSE,nullptr);changed_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
  if(stop_&&changed_)thread_=CreateThread(nullptr,0,entry,this,0,nullptr);
  if(!thread_){shutdown();return false;}return true;
 }
 void state(bool active,int rate){
  bool changed=active_.exchange(active)!=active;
  changed|=rate_.exchange(std::clamp(rate,5,500))!=std::clamp(rate,5,500);
  if(changed&&changed_)SetEvent(changed_);
 }
 void completed(){pending_.complete();}
 bool running()const{return thread_!=nullptr;}
 void shutdown(){
  if(stop_)SetEvent(stop_);
  // The clock never calls into a UI thread or game code; the stop event wakes
  // its only blocking wait before HWND and presentation resources are freed.
  if(thread_){WaitForSingleObject(thread_,INFINITE);CloseHandle(thread_);}
  if(changed_)CloseHandle(changed_);if(stop_)CloseHandle(stop_);
  thread_=stop_=changed_=nullptr;pending_.complete();
 }
};
#endif
}
