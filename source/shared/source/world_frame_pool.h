#pragma once
#include "world_frame.h"
#include <atomic>

namespace worldframe {
// Recycle storage only after the publication's last strong token is released.
// Each acquire creates a NEW shared_ptr control block: weak tokens are used as
// publication identities and must expire, even when the raw address is reused.
class SnapshotPool {
 std::array<std::atomic<Snapshot*>,3> idle_;
 Snapshot*take(){
  for(auto&slot:idle_)if(auto*frame=slot.exchange(nullptr,std::memory_order_acquire))return frame;
  return new Snapshot;
 }
 void give(Snapshot*frame)noexcept{
  for(auto&slot:idle_){Snapshot*empty=nullptr;if(slot.compare_exchange_strong(empty,frame,std::memory_order_release,std::memory_order_relaxed))return;}
  delete frame;
 }
 struct Return {
  std::shared_ptr<SnapshotPool> pool;
  void operator()(Snapshot*frame)noexcept{
   // Weak tokens retain their old control block (including its deleter). Do
   // not let those expired tokens keep this pool and all idle pages alive.
   auto owner=std::move(pool);owner->give(frame);
  }
 };
public:
 SnapshotPool(){for(auto&slot:idle_)slot.store(nullptr,std::memory_order_relaxed);}
 ~SnapshotPool(){clear();}
 SnapshotPool(const SnapshotPool&)=delete;SnapshotPool&operator=(const SnapshotPool&)=delete;
 static std::shared_ptr<Snapshot> acquire(std::shared_ptr<SnapshotPool>pool){
  auto*frame=pool->take();return std::shared_ptr<Snapshot>(frame,Return{std::move(pool)});
 }
 void clear()noexcept{for(auto&slot:idle_)delete slot.exchange(nullptr,std::memory_order_acquire);}
};
}
