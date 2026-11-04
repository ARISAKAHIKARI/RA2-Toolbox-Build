#pragma once
#include <cmath>
#include <cstdint>
namespace renderobservation {
// Same policy as the full observer's existing 50 ms duplicate-frame gate.
// This is only an early rejection: changed/unknown frames still take the full
// checked world/camera/actor path. It never authorizes a displayed frame.
template<class Read>bool duplicate(Read read,int previousFrame,double previousAt,double now){
 const double age=now-previousAt;
 if(previousFrame<0||!std::isfinite(age)||age<0.||age>=50.)return false;
 int frame=-1;
 return read(0xA8ED84,&frame,sizeof frame)&&frame==previousFrame;
}
}
