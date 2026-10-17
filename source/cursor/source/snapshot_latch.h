#pragma once
#include <cstdint>
#include "sequence.h"

namespace cursorsnap {
// Only verified samples from the current game/bridge session are retained.
// A frame doesn't become invalid merely because no new game GUI messages ran.
struct Sample {
 int frame=-1, first=-1, count=0, hotX=-1, hotY=-1, gameW=0, gameH=0, cursorID=-1;
 std::uint32_t tick=0;
 std::int32_t seq=0;
};
class Latch {
 Sample current_{};
 bool has_=false;
public:
 void clear(){current_=Sample{};has_=false;}
 bool has()const{return has_;}
 const Sample& value()const{return current_;}
 std::uint32_t age(std::uint32_t now)const{return has_?now-current_.tick:0xffffffffu;}
 // Frame identity and sequence must be verified; a hotspot may be unknown.
 // Do not discard a valid changing frame merely because the platform publishes
 // a sentinel/missing hotspot. Presentation resolves hotspot independently.
 bool observe(const Sample& s,int frameCount,int canvasW,int canvasH){
  if(!cursorseq::committed(uint32_t(s.seq))||s.frame<0||s.frame>=frameCount||
     !s.tick)return false;
  if(has_&&!cursorseq::currentOrNewer(uint32_t(s.seq),uint32_t(current_.seq)))return false;
  current_=s;has_=true;return true;
 }
};
}
