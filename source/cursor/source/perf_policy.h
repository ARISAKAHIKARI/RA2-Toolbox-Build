#pragma once
#include <stdint.h>
namespace cursorperf {
// Match the native publisher's maximum of 250 Hz. Restoration commands bypass
// the budget even when multiple messages arrive in the same millisecond.
inline bool sampleDue(uint32_t now,uint32_t last,bool command,unsigned interval=4) {
 return command || uint32_t(now-last)>=interval;
}
inline int prefetchCount(int count) { return count<3?count:3; }
inline unsigned targetRate(unsigned configured,unsigned monitor,bool native){
 unsigned rate=configured?configured:monitor;
 if(rate<30)rate=30;if(rate>500)rate=500;
 // Game-thread delivery is capped at 250 Hz; show the effective limit.
 return native&&rate>250?250:rate;
}
inline unsigned sampleInterval(unsigned hz){
 unsigned interval=1000/(hz?hz:60);
 return interval<4?4:interval;
}
inline int64_t nextFrameTick(int64_t scheduled,int64_t completed,int64_t frequency,unsigned hz){
 int64_t step=(frequency+hz-1)/hz;if(step<1)step=1;
 int64_t next=scheduled+step;
 return next<=completed?completed+step:next;
}
// Physical button edges must not depend on a low configured drawing rate.
// Reuse the same waitable timer; 500 Hz drawing still keeps its 2 ms deadline.
inline int64_t inputDeadline(int64_t frame,int64_t now,int64_t frequency,bool enabled){
 if(!enabled||frequency<=0)return frame;int64_t input=now+(frequency+249)/250;
 return input<frame?input:frame;
}
}
