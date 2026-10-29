#pragma once
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
namespace asyncwork {
// One immutable latest configuration, one running job, and its own clock.
// UI submissions update configuration/lease; they do not schedule each frame.
template<class Input,class Output>class Recurring {
 using Clock=std::chrono::steady_clock;
 std::mutex mutex_;std::condition_variable cv_;std::thread thread_;
 std::shared_ptr<const Input> input_;std::shared_ptr<const Output> latest_;
 std::function<std::shared_ptr<const Output>(const Input&)> work_;
 std::function<void()> completed_;
 Clock::time_point due_{},expires_{};int rate_=60;uint64_t epoch_=0;bool stop_=false;
 void run(){for(;;){std::shared_ptr<const Input> input;uint64_t epoch;
  {
   std::unique_lock<std::mutex>lock(mutex_);
   for(;;){
    if(stop_)return;
    if(!input_){cv_.wait(lock,[&]{return stop_||input_;});continue;}
    auto now=Clock::now();
    if(now>=expires_){input_.reset();latest_.reset();continue;}
    if(now<due_){cv_.wait_until(lock,std::min(due_,expires_));continue;}
    input=input_;epoch=epoch_;
    due_=now+std::chrono::nanoseconds(1000000000ll/rate_);break;
   }
  }
  auto result=work_(*input);bool published=false;
  {std::lock_guard<std::mutex>lock(mutex_);
   if(!stop_&&epoch==epoch_&&input_&&Clock::now()<expires_&&result){latest_=std::move(result);published=true;}
  }
  if(published&&completed_)completed_();
 }}
public:
 explicit Recurring(std::function<std::shared_ptr<const Output>(const Input&)> work,std::function<void()> completed={}):work_(std::move(work)),completed_(std::move(completed)){}
 ~Recurring(){shutdown();}
 void submit(Input input,int rate,std::chrono::milliseconds lease=std::chrono::milliseconds(500)){
  auto next=std::make_shared<const Input>(std::move(input));std::lock_guard<std::mutex>lock(mutex_);if(stop_)return;
  rate=std::clamp(rate,5,500);auto now=Clock::now();if(!input_||rate!=rate_)due_=now;
  input_=std::move(next);rate_=rate;expires_=now+lease;
  if(!thread_.joinable())thread_=std::thread([this]{run();});cv_.notify_one();
 }
 std::shared_ptr<const Output> get(){std::lock_guard<std::mutex>lock(mutex_);return latest_;}
 void invalidate(){std::lock_guard<std::mutex>lock(mutex_);++epoch_;input_.reset();latest_.reset();cv_.notify_one();}
 void shutdown(){
  {std::lock_guard<std::mutex>lock(mutex_);stop_=true;++epoch_;input_.reset();latest_.reset();cv_.notify_one();}
  if(thread_.joinable())thread_.join();
 }
};
}
