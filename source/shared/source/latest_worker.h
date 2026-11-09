#pragma once
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include "worker_failure.h"
namespace asyncwork {
// One running job and one replaceable pending snapshot. No caller waits for
// computation, and invalidation rejects an in-flight old-world completion.
// Retire potentially large snapshots after releasing the scheduling mutex.
template<class Input,class Output>class Latest {
 std::mutex mutex_;std::condition_variable cv_;std::thread thread_;
 std::unique_ptr<Input> pending_;std::shared_ptr<const Output> latest_;
 std::function<std::shared_ptr<const Output>(const Input&)> work_;
 std::function<void()> completed_;
 uint64_t epoch_=0;bool stop_=false;
 WorkerFailureState failures_;
 void run(){for(;;){std::unique_ptr<Input> input;uint64_t epoch;
  {std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return stop_||pending_;});if(stop_)return;input=std::move(pending_);epoch=epoch_;}
  std::shared_ptr<const Output> result;
  try { result=work_(*input); }
  catch(...) { failures_.workFailed(); continue; }
  bool published=false;
  {std::lock_guard<std::mutex> lock(mutex_);if(!stop_&&epoch==epoch_&&result){latest_.swap(result);published=true;}}
  if(published&&completed_)try { completed_(); }catch(...) { failures_.notificationFailed(); }
 }}
public:
 explicit Latest(std::function<std::shared_ptr<const Output>(const Input&)> work,std::function<void()> completed={}):work_(std::move(work)),completed_(std::move(completed)){}
 ~Latest(){shutdown();}
 void submit(Input input){auto next=std::make_unique<Input>(std::move(input));std::lock_guard<std::mutex> lock(mutex_);if(stop_)return;pending_.swap(next);if(!thread_.joinable())thread_=std::thread([this]{run();});cv_.notify_one();}
 std::shared_ptr<const Output> get(){std::lock_guard<std::mutex> lock(mutex_);return latest_;}
 WorkerFailures failures()const noexcept{return failures_.get();}
 void invalidate(){std::unique_ptr<Input> input;std::shared_ptr<const Output> output;{std::lock_guard<std::mutex> lock(mutex_);++epoch_;input=std::move(pending_);output=std::move(latest_);}}
 void shutdown(){std::unique_ptr<Input> input;std::shared_ptr<const Output> output;{std::lock_guard<std::mutex> lock(mutex_);stop_=true;input=std::move(pending_);output=std::move(latest_);++epoch_;cv_.notify_one();}input.reset();output.reset();if(thread_.joinable())thread_.join();}
};
}
