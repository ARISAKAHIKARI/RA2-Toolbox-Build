#pragma once
// Controlled Win32 events/timers over real threads. This models wait semantics,
// not Windows scheduling precision, DWM, or game latency.
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
using HANDLE=void*;using DWORD=uint32_t;using BOOL=int;
struct LARGE_INTEGER {int64_t QuadPart=0;};
constexpr BOOL FALSE=0,TRUE=1;
constexpr DWORD INFINITE=0xffffffff,WAIT_FAILED=0xffffffff,WAIT_OBJECT_0=0,WAIT_TIMEOUT=258,TIMER_ALL_ACCESS=1;
namespace winwait {
using Clock=std::chrono::steady_clock;
struct Object {bool timer=false,manual=false,set=false;Clock::time_point due=Clock::time_point::max();};
inline std::mutex mutex;inline std::condition_variable changed;
inline std::atomic<int> mode{0},live{0},failedWait{0};
inline std::atomic<unsigned> waits{0},timeouts{0};
}
inline HANDLE CreateEventW(void*,BOOL manual,BOOL initial,const wchar_t*){++winwait::live;return new winwait::Object{false,bool(manual),bool(initial)};}
inline HANDLE CreateWaitableTimerExW(void*,const wchar_t*,DWORD,DWORD){if(winwait::mode==1)return nullptr;++winwait::live;return new winwait::Object{true};}
inline BOOL SetEvent(HANDLE event){std::lock_guard<std::mutex> lock(winwait::mutex);assert(event);static_cast<winwait::Object*>(event)->set=true;winwait::changed.notify_all();return TRUE;}
inline BOOL SetWaitableTimer(HANDLE timer,const LARGE_INTEGER*due,DWORD,void*,void*,BOOL){
 if(winwait::mode==2)return FALSE;std::lock_guard<std::mutex> lock(winwait::mutex);assert(timer&&due->QuadPart<0);
 static_cast<winwait::Object*>(timer)->due=winwait::Clock::now()+std::chrono::nanoseconds(-due->QuadPart*100);winwait::changed.notify_all();return TRUE;
}
inline BOOL CloseHandle(HANDLE event){std::lock_guard<std::mutex> lock(winwait::mutex);assert(event);delete static_cast<winwait::Object*>(event);--winwait::live;return TRUE;}
inline DWORD WaitForMultipleObjects(DWORD count,const HANDLE*events,BOOL all,DWORD timeout){
 assert(!all&&count>=1&&count<=3);++winwait::waits;
 if(winwait::failedWait.exchange(0))return WAIT_FAILED;
 std::unique_lock<std::mutex> lock(winwait::mutex);
 const auto end=timeout==INFINITE?winwait::Clock::time_point::max():winwait::Clock::now()+std::chrono::milliseconds(timeout);
 for(;;){auto next=end;const auto now=winwait::Clock::now();
  for(DWORD n=0;n<count;++n){auto*object=static_cast<winwait::Object*>(events[n]);assert(object);
   if(object->set||(object->timer&&now>=object->due)){if(!object->manual){object->set=false;object->due=winwait::Clock::time_point::max();}return WAIT_OBJECT_0+n;}
   if(object->timer)next=std::min(next,object->due);
  }
  if(now>=end){++winwait::timeouts;return WAIT_TIMEOUT;}
  if(next==winwait::Clock::time_point::max())winwait::changed.wait(lock);else winwait::changed.wait_until(lock,next);
 }
}
