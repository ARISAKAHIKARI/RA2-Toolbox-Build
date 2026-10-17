#pragma once
#include <stdint.h>
// Game-thread observer. Identity uses only fields verified by the target
// game's current adapter. It catches observable transitions, NOT an
// undetected start/end cycle whose pointers, mode and frame never change.
namespace assist { namespace sessionepoch {
enum class Transition : uint8_t { None, Start, HouseChanged, FrameRestart, ModeChanged, Stop };
struct Tracker {
 uint32_t epoch=0,house=0; int mode=-1,frame=-1; bool active=false;
 Transition update(bool present,uint32_t nextHouse,int nextMode,int nextFrame){
  if(!present){bool was=active;active=false;house=0;mode=-1;frame=-1;
   return was?Transition::Stop:Transition::None;}
  Transition reason=Transition::None;
  if(!active)reason=Transition::Start;
  else if(nextHouse!=house)reason=Transition::HouseChanged;
  else if(nextMode!=mode)reason=Transition::ModeChanged;
  else if(nextFrame<frame)reason=Transition::FrameRestart;
  active=true;house=nextHouse;mode=nextMode;frame=nextFrame;
  if(reason!=Transition::None){if(++epoch==0)++epoch;}
  return reason;
 }
};
}}
