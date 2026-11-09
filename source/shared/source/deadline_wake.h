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
 bool canWaitForSignal()const noexcept{
#ifdef _WIN32
  return changed_!=nullptr;
#else
  return false;
#endif
 }
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
 // An optional auto-reset publication event belongs to one observing worker.
 // Its owner must retain the handle across this wait. The return value only
 // reports a notification; callers still validate the latest publication.
 bool wait_until(std::unique_lock<std::mutex>&lock,std::chrono::steady_clock::time_point deadline,void* publication=nullptr){
#ifdef _WIN32
  if(changed_){
   const auto remaining=std::chrono::duration_cast<std::chrono::nanoseconds>(deadline-std::chrono::steady_clock::now()).count();
   if(remaining<=0)return false;
   LARGE_INTEGER due{};due.QuadPart=-((remaining+99)/100);
   const bool armed=timer_&&SetWaitableTimer(timer_,&due,0,nullptr,nullptr,FALSE);
   HANDLE handles[]{changed_,publication,timer_};DWORD count=publication?2:1;
   if(armed)handles[count++]=timer_;
   const DWORD timeout=armed?INFINITE:DWORD((remaining+999999)/1000000);
   lock.unlock();const DWORD result=WaitForMultipleObjects(count,handles,FALSE,timeout);lock.lock();
   if(result!=WAIT_FAILED)return publication&&result==WAIT_OBJECT_0+1;
  }
#endif
  cv_.wait_until(lock,deadline);
  return false;
 }
};
}
