#pragma once
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
namespace asyncwork {
// One running job and one replaceable pending snapshot. No caller waits for
// computation, and invalidation rejects an in-flight old-world completion.
template<class Input,class Output>class Latest {
 std::mutex mutex_;std::condition_variable cv_;std::thread thread_;
 std::unique_ptr<Input> pending_;std::shared_ptr<const Output> latest_;
 std::function<std::shared_ptr<const Output>(const Input&)> work_;
 uint64_t epoch_=0;bool stop_=false;
 void run(){for(;;){std::unique_ptr<Input> input;uint64_t epoch;
  {std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[&]{return stop_||pending_;});if(stop_)return;input=std::move(pending_);epoch=epoch_;}
  auto result=work_(*input);
  {std::lock_guard<std::mutex> lock(mutex_);if(!stop_&&epoch==epoch_&&result)latest_=std::move(result);}
 }}
public:
 explicit Latest(std::function<std::shared_ptr<const Output>(const Input&)> work):work_(std::move(work)){}
 ~Latest(){shutdown();}
 void submit(Input input){std::lock_guard<std::mutex> lock(mutex_);if(stop_)return;pending_=std::make_unique<Input>(std::move(input));if(!thread_.joinable())thread_=std::thread([this]{run();});cv_.notify_one();}
 std::shared_ptr<const Output> get(){std::lock_guard<std::mutex> lock(mutex_);return latest_;}
 void invalidate(){std::lock_guard<std::mutex> lock(mutex_);++epoch_;pending_.reset();latest_.reset();}
 void shutdown(){{std::lock_guard<std::mutex> lock(mutex_);stop_=true;pending_.reset();latest_.reset();++epoch_;cv_.notify_one();}if(thread_.joinable())thread_.join();}
};
}
