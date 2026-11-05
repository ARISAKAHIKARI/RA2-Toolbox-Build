#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
namespace parallelrows {
// Only disjoint rows and immutable inputs enter this team. Each submitter
// retains its callback until every claimed range has finished.
class Pool {
 struct Job {
  void*context;void(*work)(void*,size_t,size_t);size_t end,grain;
  std::atomic<size_t>next{0};size_t active=0;std::exception_ptr error;
  Job(void*c,void(*w)(void*,size_t,size_t),size_t n,size_t g):context(c),work(w),end(n),grain(g){}
 };
 std::mutex state_;std::condition_variable ready_,done_;
 std::vector<std::thread>threads_;std::vector<Job*>jobs_;
 size_t cursor_=0,activeWorkers_=0,busyCallers_=0;bool stop_=false;
 inline static thread_local bool inside_=false;
 struct Inside {bool prior=inside_;Inside(){inside_=true;}~Inside(){inside_=prior;}};
 static unsigned workerCount(){const unsigned n=std::thread::hardware_concurrency();return n>3?std::min(7u,n-3):n>2?1u:0u;}
 bool runnable()const{
  if(jobs_.empty())return false;
  if(activeWorkers_+busyCallers_>=threads_.size()+1)return false;
  for(const auto*job:jobs_)if(job->next.load(std::memory_order_relaxed)<job->end)return true;return false;
 }
 void failure(Job&job,std::exception_ptr error){if(!job.error)job.error=std::move(error);job.next.store(job.end,std::memory_order_relaxed);}
 void run(){Inside inside;for(;;){Job*job=nullptr;size_t first=0,last=0;
  {std::unique_lock<std::mutex>lock(state_);ready_.wait(lock,[&]{return stop_||runnable();});if(stop_)return;
   for(size_t n=0;n<jobs_.size();++n){const size_t index=(cursor_+n)%jobs_.size();auto*candidate=jobs_[index];
    first=candidate->next.fetch_add(candidate->grain,std::memory_order_relaxed);if(first>=candidate->end)continue;
    job=candidate;last=std::min(job->end,first+job->grain);++job->active;++activeWorkers_;cursor_=(index+1)%jobs_.size();break;}
  }
  if(!job)continue;std::exception_ptr error;try{job->work(job->context,first,last);}catch(...){error=std::current_exception();}
  {std::lock_guard<std::mutex>lock(state_);if(error)failure(*job,std::move(error));--job->active;--activeWorkers_;done_.notify_all();ready_.notify_all();}
 }}
public:
 explicit Pool(unsigned workers=workerCount()){
  try{for(unsigned i=0;i<std::min(7u,workers);++i)threads_.emplace_back([this]{run();});}
  catch(...){ {std::lock_guard<std::mutex>lock(state_);stop_=true;ready_.notify_all();}for(auto&t:threads_)t.join();throw;}
 }
 ~Pool(){{std::lock_guard<std::mutex>lock(state_);stop_=true;ready_.notify_all();}for(auto&t:threads_)t.join();}
 template<class Work>void rows(size_t count,size_t pixels,Work&&work){
  if(inside_||threads_.empty()||count<32||pixels<262144){work(0,count);return;}
  Job job(&work,[](void*p,size_t a,size_t b){(*static_cast<typename std::remove_reference<Work>::type*>(p))(a,b);},count,std::max<size_t>(16,(count+threads_.size())/(threads_.size()+1)));
  {std::unique_lock<std::mutex>lock(state_);
   // The bounded queue never retains a renderer or creates more OS threads.
   if(jobs_.size()>=16){lock.unlock();Inside inside;work(0,count);return;}
   jobs_.push_back(&job);++busyCallers_;ready_.notify_all();
  }
  {Inside inside;try{for(size_t first;(first=job.next.fetch_add(job.grain,std::memory_order_relaxed))<count;)work(first,std::min(count,first+job.grain));}
   catch(...){std::lock_guard<std::mutex>lock(state_);failure(job,std::current_exception());}}
  std::unique_lock<std::mutex>lock(state_);--busyCallers_;ready_.notify_all();
  done_.wait(lock,[&]{return job.next.load(std::memory_order_relaxed)>=count&&!job.active;});
  jobs_.erase(std::find(jobs_.begin(),jobs_.end(),&job));cursor_=jobs_.empty()?0:cursor_%jobs_.size();ready_.notify_all();
  auto error=job.error;lock.unlock();if(error)std::rethrow_exception(error);
 }
};
inline Pool&pool(){static Pool value;return value;}
template<class Work>void rows(size_t count,size_t pixels,Work&&work){
 if(count<32||pixels<262144){work(0,count);return;}pool().rows(count,pixels,std::forward<Work>(work));
}
}
