#include "../../shared/source/overlay_present_rate.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace overlaypresentrate;
 for(int requested:{5,30,60,120,144,165,240,360,500}){
  for(int hz:{5,24,59,60,75,120,144,165,240,360,500}){
   const int applied=limit(requested,hz);assert(applied==std::min(requested,hz));
   assert(periodMicros(applied)*uint64_t(applied)>=1000000);
  }
  for(int unknown:{-1,0,1,2,4})assert(limit(requested,unknown)==requested);
 }
 assert(confirmed(0)==0&&confirmed(1)==0&&confirmed(4)==0&&confirmed(59)==59&&confirmed(1000)==500);
 assert(limit(1000,144)==144&&limit(2,144)==5&&limit(30,500)==30);
 assert(periodMicros(60)==16667&&periodMicros(144)==6945&&periodMicros(500)==2000);
 std::puts("PASS: compositor upload rate is min(requested, confirmed monitor Hz), unknown keeps requested; ceil microsecond periods respect the cap");
}
