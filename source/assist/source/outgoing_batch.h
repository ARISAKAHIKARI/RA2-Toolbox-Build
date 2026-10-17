#pragma once
#include <stdint.h>
namespace assist { namespace outgoing {
// Owned by the single game-thread command consumer. A plan is not acknowledged
// while a queue/budget backpressure stop leaves unsent entries behind.
struct Batch {
 uint32_t request=0, acceptedMask=0, rejectedMask=0;
 int next=0, accepted=0;
 void begin(uint32_t serial){request=serial;next=accepted=0;acceptedMask=rejectedMask=0;}
 void accept(){acceptedMask|=(1u<<next);++accepted;++next;}
 void reject(){rejectedMask|=(1u<<next);++next;}
 bool done(int count)const{return next>=count;}
};
}} // namespace assist::outgoing
