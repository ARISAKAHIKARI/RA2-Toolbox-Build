#pragma once
#include <stdint.h>
namespace assist { namespace outgoing {
constexpr int ReplanFromNewObservation=2;
// Owned by the single game-thread command consumer. A plan is not acknowledged
// while a queue/budget backpressure stop leaves unsent entries behind.
struct Batch {
 uint32_t request=0, acceptedMask=0, rejectedMask=0, inspectedVersion=0;
 int next=0, accepted=0,candidateAt=0;
 uint64_t candidateSliceAt=0;
 void begin(uint32_t serial){*this={};request=serial;}
 bool settled(int i)const{return ((acceptedMask|rejectedMask)&(1u<<i))!=0;}
 void advance(){while(next<32&&settled(next))++next;}
 void acceptAt(int i){if(!settled(i)){acceptedMask|=1u<<i;++accepted;advance();}}
 void rejectAt(int i){if(!settled(i)){rejectedMask|=1u<<i;advance();}}
 void accept(){acceptAt(next);}
 void reject(){rejectAt(next);}
 bool done(int count)const{return next>=count;}
 // A cross-frame candidate can only settle individually proven items. Leave
 // other entries for the strict path. Scan each publication at most once.
 template<class Eligible,class Admit>
 bool candidatePass(int count,int budget,uint64_t now,uint32_t version,Eligible eligible,Admit admit){
  if(count<=0||count>32)return false;
  if(candidateAt==count&&inspectedVersion==version)return true;
  if(candidateSliceAt&&now-candidateSliceAt<4000)return false;
  candidateSliceAt=now;
  if(candidateAt>=count)candidateAt=0;
  while(candidateAt<count&&budget>0&&!done(count)){
   const int i=candidateAt++;
   if(settled(i)||!eligible(i))continue;
   --budget;
   const int result=admit(i);
   if(result>0)acceptAt(i);else if(result<0)rejectAt(i);
  }
  if(candidateAt==count)inspectedVersion=version;
  return candidateAt==count;
 }
};
}} // namespace assist::outgoing
