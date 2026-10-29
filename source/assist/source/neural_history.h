#pragma once
#include <cstdint>
#include <limits>
namespace neuralhistory {
// The scene renderer advances its motion history even when a later stage
// withholds that frame. Neural history advances only after successful inference.
class Gate {
 uint64_t generation_=0,sequence_=0;bool valid_=false;
public:
 void invalidate(){valid_=false;}
 bool reset(uint64_t generation,uint64_t sequence,bool sceneReset)const{
  return sceneReset||!valid_||!sequence||generation!=generation_||
   sequence_==std::numeric_limits<uint64_t>::max()||sequence!=sequence_+1;
 }
 void accepted(uint64_t generation,uint64_t sequence){generation_=generation;sequence_=sequence;valid_=sequence!=0;}
};
}
