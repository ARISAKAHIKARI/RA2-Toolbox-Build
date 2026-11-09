#pragma once
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include "deadline_wake.h"
#include "worker_failure.h"
namespace asyncwork {
struct SignalWait {
 std::shared_ptr<void> event;
 std::chrono::milliseconds maintenance{50};
};
// One immutable latest configuration, one running job, and its own clock.
// UI submissions update configuration/lease; they do not schedule each frame.
// Snapshot retirement must not hold the lock needed by UI get/submit calls.
template<class Input,class Output>class Recurring {
 using Clock=std::chrono::steady_clock;
 std::mutex mutex_;DeadlineWake cv_;std::thread thread_;
 std::shared_ptr<const Input> input_;std::shared_ptr<const Output> latest_;
 std::function<std::shared_ptr<const Output>(const Input&)> work_;
 std::function<void()> completed_;
 std::function<SignalWait()> signalSource_;
 std::function<bool(const Input&,const Input&)> configurationChanged_;
 Clock::time_point due_{},expires_{};int rate_=60;uint64_t epoch_=0,schedule_=0;bool stop_=false;
 Clock::time_point retryAfter_{};WorkerFailureState failures_;
 void run(){SignalWait source;for(;;){std::shared_ptr<const Input> input;uint64_t epoch,schedule;Clock::time_point started;
  {
   std::unique_lock<std::mutex>lock(mutex_);
   for(;;){
    if(stop_)return;
    if(!input_){source={};cv_.wait(lock,[&]{return stop_||input_;});continue;}
    auto now=Clock::now();
    if(now>=expires_){auto input=std::move(input_);auto output=std::move(latest_);lock.unlock();input.reset();output.reset();lock.lock();continue;}
    const auto ready=std::max(due_,retryAfter_);
    if(now<ready){
     // A notification coalesces to one latest-state job. During exception
     // backoff it cannot bypass the retry deadline or turn a failure into spin.
     if(cv_.wait_until(lock,std::min(ready,expires_),now>=retryAfter_?source.event.get():nullptr))due_=Clock::now();
     continue;
    }
    input=input_;epoch=epoch_;schedule=schedule_;started=now;
    due_=now+std::chrono::nanoseconds(1000000000ll/rate_);break;
   }
  }
  std::shared_ptr<const Output> result;
  SignalWait nextSource;
  try { result=work_(*input);if(signalSource_&&cv_.canWaitForSignal())nextSource=signalSource_(); }
  catch(...) {
   failures_.workFailed();
   std::lock_guard<std::mutex>lock(mutex_);
   // A persistent decode/allocation failure must not spin at the 1000 Hz
   // observation rate. An invalidated old job cannot delay the new world.
   if(epoch==epoch_)retryAfter_=Clock::now()+std::chrono::milliseconds(100);
   continue;
  }
  bool published=false;
  {std::lock_guard<std::mutex>lock(mutex_);
   if(epoch==epoch_){
    source=std::move(nextSource);
    if(schedule==schedule_)due_=started+(source.event?
     std::chrono::duration_cast<Clock::duration>(std::max(std::chrono::milliseconds(1),source.maintenance)):
     std::chrono::duration_cast<Clock::duration>(std::chrono::nanoseconds(1000000000ll/rate_)));
   }
   if(!stop_&&epoch==epoch_&&input_&&Clock::now()<expires_&&result){latest_.swap(result);published=true;}
  }
  if(published&&completed_)try { completed_(); }catch(...) { failures_.notificationFailed(); }
 }}
public:
 explicit Recurring(std::function<std::shared_ptr<const Output>(const Input&)> work,std::function<void()> completed={},
  std::function<SignalWait()> signalSource={},std::function<bool(const Input&,const Input&)> configurationChanged={}):
  work_(std::move(work)),completed_(std::move(completed)),signalSource_(std::move(signalSource)),configurationChanged_(std::move(configurationChanged)){}
 ~Recurring(){shutdown();}
 void submit(Input input,int rate,std::chrono::milliseconds lease=std::chrono::milliseconds(500)){
  submitShared(std::make_shared<const Input>(std::move(input)),rate,lease);
 }
 // Multiple consumers may share one immutable configuration. Lease renewals
 // and replacement inputs at the same cadence are consumed at the next due
 // time; waking a timer for each UI paint only makes it re-arm the same wait.
 void submitShared(std::shared_ptr<const Input> next,int rate,std::chrono::milliseconds lease=std::chrono::milliseconds(500)){
  if(!next)return;
  std::lock_guard<std::mutex>lock(mutex_);if(stop_)return;
  rate=std::clamp(rate,5,1000);auto now=Clock::now();
  const bool restart=!input_||now>=expires_||rate!=rate_||(configurationChanged_&&configurationChanged_(*input_,*next));
  const auto expires=now+lease;const bool wake=restart||expires<expires_;
  if(restart){due_=now;++schedule_;}
  input_.swap(next);rate_=rate;expires_=expires;
  if(!thread_.joinable())thread_=std::thread([this]{run();});
  if(wake)cv_.notify_one();
 }
 std::shared_ptr<const Output> get(){std::lock_guard<std::mutex>lock(mutex_);return latest_;}
 WorkerFailures failures()const noexcept{return failures_.get();}
 void invalidate(){std::shared_ptr<const Input> input;std::shared_ptr<const Output> output;{std::lock_guard<std::mutex>lock(mutex_);++epoch_;retryAfter_={};input=std::move(input_);output=std::move(latest_);cv_.notify_one();}}
 void shutdown(){
  std::shared_ptr<const Input> input;std::shared_ptr<const Output> output;
  {std::lock_guard<std::mutex>lock(mutex_);stop_=true;++epoch_;input=std::move(input_);output=std::move(latest_);cv_.notify_one();}
  input.reset();output.reset();
  if(thread_.joinable())thread_.join();
 }
};
}
