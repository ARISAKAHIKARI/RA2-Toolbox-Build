#pragma once
#include "radar_map.h"
#include <mutex>
namespace external { namespace radar {
// One full-map immutable source bank, not a scaled minimap screenshot.
class SceneBank {
 std::mutex mutex_;uint32_t pid_=0,match_=0;bool world_=false;
 std::shared_ptr<const Frame> frame_;
public:
 void publish(uint32_t pid,uint32_t match,bool world,std::shared_ptr<const Frame>frame){
  if(!frame||!frame->terrain||frame->terrain->art.empty()||!frame->markerScanValid)return;
  // Releasing the last source can free whole-map art and marker vectors.
  // Keep that work outside the publication lock used by every view.
  std::shared_ptr<const Frame> retired;
  std::lock_guard<std::mutex>lock(mutex_);
  if(frame_&&pid_==pid&&match_==match){
   // Independent view workers finish at different times. Completion order
   // cannot turn an earlier observation into the latest world source. Use
   // the shared monotonic sample clock, with wrap-safe tick fallback for
   // sources without it. Never use max(game frame): a newer observation of
   // a real rewind/load must still retire the consumer's old presentation.
   const bool precise=std::isfinite(frame->observedMs)&&frame->observedMs>0&&std::isfinite(frame_->observedMs)&&frame_->observedMs>0;
   if(precise&&frame->observedMs<frame_->observedMs)return;
   if(!precise||frame->observedMs==frame_->observedMs){
    const int32_t order=int32_t(frame->tick-frame_->tick);
    if(order<0||(order==0&&frame->frame<frame_->frame))return;
   }
  }
  pid_=pid;match_=match;world_=world;retired=std::move(frame_);frame_=std::move(frame);
 }
 std::shared_ptr<const Frame> get(uint32_t pid,uint32_t match,uint32_t reveal,uint32_t now,bool worldOnly=false){
  std::lock_guard<std::mutex>lock(mutex_);
  return frame_&&pid_==pid&&match_==match&&frame_->terrain->reveal==bool(reveal)&&
   uint32_t(now-frame_->tick)<=500&&(!worldOnly||world_)?frame_:nullptr;
 }
};
inline SceneBank&sceneBank(){static SceneBank bank;return bank;}
}}
