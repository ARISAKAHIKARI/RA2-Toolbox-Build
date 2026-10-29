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
  std::lock_guard<std::mutex>lock(mutex_);pid_=pid;match_=match;world_=world;frame_=std::move(frame);
 }
 std::shared_ptr<const Frame> get(uint32_t pid,uint32_t match,uint32_t reveal,uint32_t now,bool worldOnly=false){
  std::lock_guard<std::mutex>lock(mutex_);
  return frame_&&pid_==pid&&match_==match&&frame_->terrain->reveal==bool(reveal)&&
   uint32_t(now-frame_->tick)<=500&&(!worldOnly||world_)?frame_:nullptr;
 }
};
inline SceneBank&sceneBank(){static SceneBank bank;return bank;}
}}
