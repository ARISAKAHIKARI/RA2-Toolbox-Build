#pragma once
#include <cstdint>
namespace inputtransport {
// A live helper process is not proof that its Win32 hooks still receive input.
// Watch callback progress, not gameplay frames (paused games still pump input).
class Recovery {
 uint32_t callbacks_=0,progressAt_=0,attemptAt_=0,observedAt_=0;bool seen_=false,attempted_=false;
public:
 bool due(uint32_t callbacks,uint32_t now,bool usable){
  if(!usable){seen_=false;return false;}
  const bool gap=seen_&&uint32_t(now-observedAt_)>1000;observedAt_=now;
  if(!seen_||gap||callbacks!=callbacks_){seen_=true;callbacks_=callbacks;progressAt_=now;return false;}
  if(uint32_t(now-progressAt_)<3000||(attempted_&&uint32_t(now-attemptAt_)<10000))return false;
  attempted_=true;attemptAt_=now;return true;
 }
};
}
