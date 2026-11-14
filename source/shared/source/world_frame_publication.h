#pragma once
#include "world_frame.h"
#include <atomic>

namespace worldframe {
// The worker borrows ONLY owned page bytes. No game pointers are dereferenced.
struct Publication {
 Key key{};Counters counters{};
 struct Pages {const Page*pointer=nullptr;uint32_t count=0;const Page*data()const{return pointer;}size_t size()const{return count;}}pages;
 uint32_t epoch=0;int slot=-1;bool controlSource=false;
 bool sealed()const{return true;}bool overflow()const{return false;}
};
// One game-thread capture owner, one publication worker, three bounded buffers.
// Swapping PageBuffer ownership never copies pages or frees them on handoff.
class PublicationPool {
 struct Slot {PageBuffer spare;std::atomic<unsigned>pins{0};Publication job;};
 std::array<Slot,3>slots_;
 std::atomic<int>pending_{-1};std::atomic<uint32_t>epoch_{1};int current_=0;
 void release(int slot){slots_[slot].pins.fetch_sub(1,std::memory_order_release);}
public:
 PublicationPool(){slots_[0].pins.store(1,std::memory_order_relaxed);}
 PublicationPool(const PublicationPool&)=delete;PublicationPool&operator=(const PublicationPool&)=delete;
 bool begin(Snapshot&frame){
  if(slots_[current_].pins.load(std::memory_order_acquire)==1)return true;
  for(int i=0;i<3;++i)if(i!=current_){unsigned free=0;
   if(slots_[i].pins.compare_exchange_strong(free,1,std::memory_order_acquire,std::memory_order_relaxed)){
    frame.pages.swap(slots_[current_].spare);frame.pages.swap(slots_[i].spare);
    release(current_);current_=i;return true;
   }
  }return false;
 }
 bool submit(const Snapshot&frame){
  if(!frame.sealed()||frame.overflow()||frame.controlSource)return false;
  auto&slot=slots_[current_];
  // An already pending/in-flight view is immutable; never rewrite its metadata.
  if(slot.pins.load(std::memory_order_acquire)>1)return true;
  slot.job.key=frame.key;slot.job.counters=frame.counters;slot.job.pages={frame.pages.data(),uint32_t(frame.pages.size())};
  slot.job.epoch=epoch_.load(std::memory_order_acquire);slot.job.slot=current_;
  slot.pins.fetch_add(1,std::memory_order_relaxed);
  const int replaced=pending_.exchange(current_,std::memory_order_acq_rel);if(replaced>=0)release(replaced);
  return true;
 }
 bool take(Publication&out){
  const int slot=pending_.exchange(-1,std::memory_order_acq_rel);if(slot<0)return false;
  out=slots_[slot].job;return true;
 }
 void finish(const Publication&job){release(job.slot);}
 bool current(const Publication&job)const{return job.epoch==epoch_.load(std::memory_order_acquire);}
 uint32_t epoch()const{return epoch_.load(std::memory_order_acquire);}
 void invalidate(){
  epoch_.fetch_add(1,std::memory_order_acq_rel);const int slot=pending_.exchange(-1,std::memory_order_acq_rel);if(slot>=0)release(slot);
 }
};
}
