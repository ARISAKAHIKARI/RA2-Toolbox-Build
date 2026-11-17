#pragma once
#include <cstdint>
#include <algorithm>

namespace assist { namespace globalvehicle {
// Native admission (owner/UID/mission/command) remains on the game thread,
// but it must not inspect an unbounded global candidate list every frame.
// Round-robin progress is retained between simulation frames; newly published
// candidate rosters are safe to use because each pointer is revalidated live.
struct Cursor {
 static constexpr unsigned MaxProbesPerFrame=64;
 unsigned next=0;
 void reset(){next=0;}
 unsigned budget(unsigned total)const{return std::min(total,MaxProbesPerFrame);}
 unsigned index(unsigned total,unsigned examined)const{
  return total?((next%total)+examined)%total:0;
 }
 void advance(unsigned total,unsigned examined){
  next=total?((next%total)+examined)%total:0;
 }
};
}} // namespace assist::globalvehicle
