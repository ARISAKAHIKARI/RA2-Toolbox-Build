#pragma once
#include <chrono>
#include <condition_variable>
#include <mutex>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace asyncwork {
// Windows condition-variable timeouts can round a short frame deadline to a
// scheduler tick. Use a high-resolution one-shot timer, interruptible by any
// new configuration/invalidation/shutdown. No polling or catch-up burst.
class DeadlineWake {
 std::condition_variable cv_;
#ifdef _WIN32
 HANDLE changed_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 HANDLE timer_=CreateWaitableTimerExW(nullptr,nullptr,0x2,TIMER_ALL_ACCESS);
#endif
public:
 ~DeadlineWake(){
#ifdef _WIN32
  if(timer_)CloseHandle(timer_);if(changed_)CloseHandle(changed_);
#endif
 }
 void notify_one(){cv_.notify_one();
#ifdef _WIN32
  if(changed_)SetEvent(changed_);
#endif
 }
 template<class Predicate>void wait(std::unique_lock<std::mutex>&lock,Predicate predicate){cv_.wait(lock,predicate);}
 template<class Predicate>bool wait_until(std::unique_lock<std::mutex>&lock,std::chrono::steady_clock::time_point deadline,Predicate predicate){
  while(!predicate()){if(std::chrono::steady_clock::now()>=deadline)return false;wait_until(lock,deadline);}return true;
 }
 void wait_until(std::unique_lock<std::mutex>&lock,std::chrono::steady_clock::time_point deadline){
#ifdef _WIN32
  if(timer_&&changed_){
   const auto remaining=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-std::chrono::steady_clock::now()).count();
   if(remaining<=0)return;
   LARGE_INTEGER due{};due.QuadPart=-((remaining+99)/100);
   if(SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE)){
    HANDLE handles[]{changed_,timer_};lock.unlock();
    const DWORD result=WaitForMultipleObjects(2,handles,FALSE,INFINITE);lock.lock();
    if(result!=WAIT_FAILED)return;
   }
  }
#endif
  cv_.wait_until(lock,deadline);
 }
};
}
