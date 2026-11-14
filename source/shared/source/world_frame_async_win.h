#pragma once
#include "world_frame_publication.h"
#include "world_frame_transport.h"
#include <windows.h>

namespace worldframe {
// Persistent, independent writer. Thread/data/mapping ownership never enters
// the game-entry lock; the game thread only exchanges bounded buffer leases.
template<class Writer=Server>class AsyncPublisher {
 PublicationPool pool_;HANDLE thread_=nullptr,event_=nullptr;HMODULE module_=nullptr;
 std::atomic<bool>stopping_{false};
 std::atomic<uint32_t>attemptSerial_{0},attemptEpoch_{0},attemptAt_{0};
 uint32_t launchAt_=0;bool launchTried_=false;
 static DWORD WINAPI run(void*opaque){
  auto*self=static_cast<AsyncPublisher*>(opaque);
  {
   Writer writer(Server::Role::PublicationOnly);
   while(!self->stopping_.load(std::memory_order_acquire)){
    const DWORD wait=WaitForSingleObject(self->event_,100);if(wait==WAIT_FAILED)break;
    Publication job;
    while(!self->stopping_.load(std::memory_order_acquire)&&self->pool_.take(job)){
     const auto current=[&]{return !self->stopping_.load(std::memory_order_acquire)&&self->pool_.current(job)&&writer.observes(job.key);};
     if(self->pool_.current(job)&&writer.wanted(job.key.owner,GetTickCount()))writer.publish(job,current);
     self->attemptAt_.store(GetTickCount(),std::memory_order_relaxed);self->attemptEpoch_.store(job.epoch,std::memory_order_relaxed);self->attemptSerial_.store(job.key.serial,std::memory_order_release);
     self->pool_.finish(job);
    }
   }
   writer.disconnect();
  }
  // The DLL reference protects this code even after Host removes its hook.
  const HMODULE module=self->module_;
  FreeLibraryAndExitThread(module,0);return 0;
 }
 void reap(){
  if(thread_&&WaitForSingleObject(thread_,0)==WAIT_OBJECT_0){
   CloseHandle(thread_);CloseHandle(event_);thread_=event_=nullptr;module_=nullptr;pool_.invalidate();
   attemptSerial_.store(0,std::memory_order_relaxed);launchTried_=false;
  }
 }
 bool launch(){
  if(thread_){
   if(WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0)return !stopping_.load(std::memory_order_acquire);
   CloseHandle(thread_);CloseHandle(event_);thread_=event_=nullptr;module_=nullptr;pool_.invalidate();
  }
  const uint32_t now=GetTickCount();if(launchTried_&&uint32_t(now-launchAt_)<1000)return false;launchTried_=true;launchAt_=now;
  HMODULE pin=nullptr;if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&run),&pin))return false;
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!event){FreeLibrary(pin);return false;}
  module_=pin;event_=event;stopping_.store(false,std::memory_order_release);
  thread_=CreateThread(nullptr,0,run,this,0,nullptr);
  if(!thread_){CloseHandle(event_);event_=nullptr;module_=nullptr;FreeLibrary(pin);return false;}
  return true;
 }
public:
 // Module-static lifetime. The worker's module reference reaches this
 // destructor only after its body released all leases and mapping handles.
 ~AsyncPublisher(){if(thread_)CloseHandle(thread_);if(event_)CloseHandle(event_);}
 AsyncPublisher()=default;AsyncPublisher(const AsyncPublisher&)=delete;AsyncPublisher&operator=(const AsyncPublisher&)=delete;
 bool begin(Snapshot&frame){return pool_.begin(frame);}
 void invalidate(){pool_.invalidate();if(event_)SetEvent(event_);}
 bool publish(const Snapshot&frame,const Server&control){
  reap();
  const uint32_t epoch=pool_.epoch();
  // A prior success is not sufficient: heartbeat expiry/retirement can remove
  // the advertised descriptor while this immutable game frame stays paused.
  if(control.published(frame))return true;
  if(attemptSerial_.load(std::memory_order_acquire)==frame.key.serial&&attemptEpoch_.load(std::memory_order_relaxed)==epoch&&uint32_t(GetTickCount()-attemptAt_.load(std::memory_order_relaxed))<16)return false;
  if(!launch()||!pool_.submit(frame))return false;SetEvent(event_);return true;
 }
 // Poll only. Ordinary Runtime detach cannot acknowledge stop or unload while
 // the worker still uses a source lease or executes module code. No join here.
 bool stop(){
  stopping_.store(true,std::memory_order_release);pool_.invalidate();if(event_)SetEvent(event_);
  if(!thread_)return true;if(WaitForSingleObject(thread_,0)!=WAIT_OBJECT_0)return false;
  CloseHandle(thread_);CloseHandle(event_);thread_=event_=nullptr;module_=nullptr;launchTried_=false;return true;
 }
};
}
