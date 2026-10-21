#pragma once
#include <cstdint>
namespace assist { namespace scrollrefresh {
// Rate-based native scroll amounts are specified per 60 Hz update. Fractional
// pixels carry across samples, so higher refresh does not multiply speed.
struct Rate {uint64_t at=0;int64_t remainder=0;int direction=-1;};
inline int amount(Rate&r,int pixels,int direction,uint64_t now){
 if(pixels<=0||pixels>4096)return 0;
 uint64_t elapsed=r.at&&now>=r.at?now-r.at:8333;r.at=now;
 if(elapsed>50000){elapsed=8333;r.remainder=0;}
 if(direction!=r.direction){r.remainder=0;r.direction=direction;}
 int64_t value=int64_t(pixels)*elapsed*60+r.remainder;r.remainder=value%1000000;return int(value/1000000);
}
struct Cadence {
 uint64_t origin=0,next=0;unsigned rate=0;
 bool ready(uint64_t now,unsigned hz)const{return !rate||rate!=hz||now<origin||now>=next;}
 bool take(uint64_t now,unsigned hz){
  hz=hz<60?60:hz>240?240:hz;
  if(!ready(now,hz))return false;
  if(!rate||rate!=hz||now<origin||now-origin>10000000){origin=now;rate=hz;}
  // Keep phase after late wakes, skip missed slots instead of drawing bursts.
  uint64_t slot=(now-origin)*rate/1000000+1;
  next=origin+(slot*1000000+rate-1)/rate;return true;
 }
};
}}
