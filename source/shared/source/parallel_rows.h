#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
namespace parallelrows {
// Only disjoint output rows and immutable input may enter this pool. Remote
// readers, decoders, painter order and mutable caches stay on their owner.
class Pool {
 std::mutex submission_,state_;std::condition_variable ready_,done_;
 std::vector<std::thread> threads_;std::atomic<size_t> next_{0};
 size_t end_=0,grain_=1,remaining_=0;unsigned epoch_=0;bool stop_=false;
 void* context_=nullptr;void(*work_)(void*,size_t,size_t)=nullptr;
 inline static thread_local bool inside_=false;
 void consume(){bool prior=inside_;inside_=true;
  for(size_t begin;(begin=next_.fetch_add(grain_,std::memory_order_relaxed))<end_;)work_(context_,begin,std::min(end_,begin+grain_));
  inside_=prior;
 }
 void run(){unsigned seen=0;for(;;){
  {std::unique_lock<std::mutex> lock(state_);ready_.wait(lock,[&]{return stop_||epoch_!=seen;});if(stop_)return;seen=epoch_;}
  consume();{std::lock_guard<std::mutex> lock(state_);if(!--remaining_)done_.notify_one();}
 }}
public:
 Pool(){const unsigned hardware=std::thread::hardware_concurrency();const unsigned count=hardware>2?std::min(3u,hardware-2):0;
  for(unsigned i=0;i<count;++i)threads_.emplace_back([this]{run();});
 }
 ~Pool(){{std::lock_guard<std::mutex> lock(state_);stop_=true;ready_.notify_all();}for(auto&t:threads_)t.join();}
 template<class Work>void rows(size_t count,size_t pixels,Work&&work){
  if(inside_||threads_.empty()||count<32||pixels<262144){work(0,count);return;}
  std::unique_lock<std::mutex> owner(submission_,std::try_to_lock);
  // Independent renderers never queue behind one another for this CPU team.
  if(!owner.owns_lock()){work(0,count);return;}
  {std::lock_guard<std::mutex> lock(state_);context_=&work;work_=[](void*p,size_t a,size_t b){(*static_cast<typename std::remove_reference<Work>::type*>(p))(a,b);};
   end_=count;grain_=std::max<size_t>(16,(count+threads_.size())/(threads_.size()+1));next_=0;remaining_=threads_.size();++epoch_;}
  ready_.notify_all();consume();
  {std::unique_lock<std::mutex> lock(state_);done_.wait(lock,[&]{return !remaining_;});context_=nullptr;work_=nullptr;}
 }
};
inline Pool& pool(){static Pool value;return value;}
template<class Work>void rows(size_t count,size_t pixels,Work&&work){
 if(count<32||pixels<262144){work(0,count);return;}
 pool().rows(count,pixels,std::forward<Work>(work));
}
}
