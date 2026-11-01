#pragma once
#include "../../external_reader/source/material_motion.h"
namespace watchscene {
class Observation {
 std::shared_ptr<const external::radar::Frame> source_;
 external::materialmotion::Timeline timeline_;
public:
 void reset(){source_.reset();timeline_.reset();}
 bool observe(const std::shared_ptr<const external::radar::Frame>&source,double at){
  // Equal game frame numbers do not prove equal membership/poses. Only the
  // SAME immutable publication can skip copying its entire actor inventory.
  if(!source||source==source_)return false;
  timeline_.observe(*source,at);source_=source;return true;
 }
 external::radar::Frame sampleLatest(double at,bool motion,bool fine)const{return timeline_.sampleLatest(at,motion,fine);}
};
}
