#include "../source/repair_delivery.h"
#include <cassert>
#include <cstdio>
using namespace assist::repairdelivery;
int main(){
 Pending<4> p;
 assert(p.queued(1,100)&&p.queued(5,500)); // deliberate same hash bucket
 assert(p.contains(1,100)&&p.contains(5,500)&&p.count==2);
 for(int i=0;i<10000;++i)p.observe([](uint32_t,uint32_t){return Observation{true,false,false};});
 assert(p.contains(1,100)&&p.contains(5,500)); // elapsed time never creates a retry
 p.observe([](uint32_t id,uint32_t){return Observation{true,id==1,false};});
 assert(!p.contains(1,100)&&p.contains(5,500)); // tombstone preserves collision chain
 assert(p.queued(9,900));
 p.observe([](uint32_t id,uint32_t){return Observation{id!=5,false,false};});
 assert(!p.contains(5,500)&&p.contains(9,900));
 assert(!p.contains(9,901));assert(p.queued(9,901)&&p.count==1);
 p.observe([](uint32_t,uint32_t){return Observation{true,false,true};});assert(!p.count);
 for(int i=1;i<=4;++i)assert(p.queued(i,i*100));
 assert(!p.room(5)&&!p.queued(5,500));
 p.clear();assert(p.count==0&&!p.contains(1,100)&&p.room(5));
 puts("PASS: repair delivery waits for evidence; no timeout retry/collision overwrite; identity, health, capacity, reset");
}
