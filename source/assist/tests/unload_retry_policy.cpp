#include "../source/unload_retry_policy.h"
#include "../source/unload_admission.h"
#include <cassert>
#include <cstdio>
using namespace assist;
struct Ticket {
 unloadadmission::Queue queue{};
 int deployCalls=0,accepted=0;
 static constexpr uint32_t Carrier=0x10000u,Id=11u;
 Ticket(){queue.begin(101,0x20000,1000);assert(queue.add({Carrier,Id,9}));}
 bool step(int nativeResult,uint32_t now){
  const int slot=queue.find(Carrier,Id);
  if(slot<0)return false;
  const auto original=queue.carriers[slot];
  const bool first=unloadretry::maySendInitialDeploy(original.attempted);
  if(first)++deployCalls;
  auto outcome=unloadretry::classify(nativeResult);
  if(unloadretry::committed(outcome)){
   int index=queue.find(Carrier,Id);
   assert(index>=0);
   queue.carriers[index].attempted=true;
   queue.carriers[index].attemptedAt=now;
   ++accepted;
  }
  if(outcome==unloadretry::Outcome::QueuedDeploy)queue.lastProgress=now;
  else if(outcome==unloadretry::Outcome::Enrolled||outcome==unloadretry::Outcome::Finished)
   queue.cancel(Carrier,Id);
  return first;
 }
};
int main(){
 using unloadretry::Outcome;
 assert(unloadretry::classify(-6)==Outcome::Deferred);
 assert(unloadretry::classify(-12)==Outcome::Deferred);
 assert(unloadretry::classify(0)==Outcome::Finished);
 assert(unloadretry::classify(1)==Outcome::Enrolled);
 assert(unloadretry::classify(2)==Outcome::QueuedDeploy);
 static Ticket t;
 // Game-side queue full or stale: the hotkey is saved, not consumed.
 for(int k=0;k<6;++k){assert(t.step(-6,1001+k));assert(t.accepted==0);
  assert(t.queue.count==1&&!t.queue.carriers[0].attempted);}
 // A later frame can enqueue the SAME original hotkey's first native Deploy.
 assert(t.step(2,1020)&&t.accepted==1&&t.queue.count==1);
 assert(t.queue.carriers[0].attempted&&t.queue.carriers[0].attemptedAt==1020);
 // No duplicate native Deploy while waiting for late passengers.
 assert(!t.step(-6,1021));
 assert(!t.step(1,1022)&&t.queue.count==0);
 assert(!t.step(2,1023));
 static Ticket nativeFirst;assert(nativeFirst.step(1,1002));
 assert(nativeFirst.queue.count==0&&nativeFirst.accepted==1);
 static Ticket expired;expired.queue.clear();assert(!expired.step(2,5000));
 // Scene transitions retire pending requests; never revive them.
 static Ticket changed;changed.queue.begin(102,0x20000,1005);
 assert(changed.queue.count==0);
 std::puts("PASS Ctrl+D durable one-shot native admission: failed enqueues retry; committed events do not duplicate; native-first and scene retirement safe");
}
