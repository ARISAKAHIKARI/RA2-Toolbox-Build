#pragma once
#include <algorithm>
#include <stdint.h>
namespace assist { namespace pacing {
struct Step {bool control=false,frame=false;uint64_t wait=1;};
// Time is in 100 ns units. Control and drawing have separate deadlines, and a
// delayed UI never creates a burst of old catch-up messages.
class Clock {
 uint64_t nextControl_=0,nextFrame_=0;int rate_=0;bool active_=false,initialized_=false;
public:
 Step step(uint64_t now,bool active,int rate){
  rate=std::clamp(rate,5,500);
  if(!initialized_){nextControl_=now;nextFrame_=now;initialized_=true;}
  if(active!=active_||rate!=rate_)nextFrame_=now;
  active_=active;rate_=rate;Step out;
  if(now>=nextControl_){out.control=true;nextControl_=now+160000;}
  if(active&&now>=nextFrame_){out.frame=true;nextFrame_=now+uint64_t(10000000/rate);}
  uint64_t due=active?std::min(nextControl_,nextFrame_):nextControl_;
  out.wait=due>now?due-now:1;return out;
 }
};
}}
