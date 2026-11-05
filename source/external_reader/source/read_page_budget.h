#pragma once
#include <atomic>
#include <cstddef>
namespace external {namespace readpages {
// Page storage grows only on demand. This CPU read cache is one portion of
// the toolbox memory budget, shared by all sessions rather than per view.
constexpr size_t PageBytes=4096;
constexpr size_t SessionPages=(sizeof(void*)>=8?256u:64u)*1024*1024/PageBytes;
constexpr size_t ProcessPages=(sizeof(void*)>=8?1024u:256u)*1024*1024/PageBytes;
class Budget {
 const size_t limit_;
 std::atomic<size_t> used_{0};
 std::atomic<size_t> pressure_{0};
public:
 explicit Budget(size_t pages):limit_(pages){}
 Budget(const Budget&)=delete;Budget&operator=(const Budget&)=delete;
 bool acquire(){
  auto used=used_.load(std::memory_order_relaxed);
  while(used<limit_){if(used_.compare_exchange_weak(used,used+1,std::memory_order_relaxed))return true;}
  pressure_.fetch_add(1,std::memory_order_relaxed);return false;
 }
 void release(size_t pages){if(pages)used_.fetch_sub(pages,std::memory_order_relaxed);}
 size_t used()const{return used_.load(std::memory_order_relaxed);}
 size_t limit()const{return limit_;}
 // Monotonic tickets avoid a lost-denial race from clearing a shared flag.
 size_t pressure()const{return pressure_.load(std::memory_order_relaxed);}
};
inline Budget& sharedBudget(){static Budget budget(ProcessPages);return budget;}
}}
