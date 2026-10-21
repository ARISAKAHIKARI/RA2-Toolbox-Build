#pragma once
#include <cstdint>
namespace assist { namespace scrollrefresh {
// Rate-based native scroll amounts are specified per 60 Hz update. Fractional
// pixels carry across samples, so higher refresh does not multiply speed.
struct Rate {uint32_t at=0;int remainder=0,direction=-1;};
inline int amount(Rate&r,int pixels,int direction,uint32_t now){
 if(pixels<=0||pixels>4096)return 0;
 uint32_t elapsed=r.at?now-r.at:8;r.at=now;
 if(elapsed>50){elapsed=8;r.remainder=0;}
 if(direction!=r.direction){r.remainder=0;r.direction=direction;}
 int value=pixels*int(elapsed)*60+r.remainder;r.remainder=value%1000;return value/1000;
}
inline unsigned period(unsigned hz){return 1000/(hz<60?60:hz>240?240:hz);}
}}
