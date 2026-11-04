#include "../../shared/source/overlay_present_rate.h"
#include <cassert>
#include <cstdio>
#include <vector>
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
 // Drive the same production cadence with controlled composition workloads.
 // Alternating short/long jobs must neither append a whole idle frame nor
 // issue a short catch-up upload that the display cannot show separately.
 using Clock=std::chrono::steady_clock;using Micros=std::chrono::microseconds;
 for(int hz:{60,144,240})for(auto pattern:{std::vector<uint64_t>{0},std::vector<uint64_t>{2000},std::vector<uint64_t>{4000},std::vector<uint64_t>{25000},std::vector<uint64_t>{2000,18000},std::vector<uint64_t>{2000,2000,18000,2000}}){
  Cadence cadence;Clock::time_point now{},previous{};const auto period=periodMicros(hz);uint64_t expected=0;
  for(int frame=0;frame<1000;++frame){
   const auto work=pattern[size_t(frame)%pattern.size()];now+=Micros(work);
   const auto ready=now;now=cadence.deadline(now,hz);assert(now>=ready);
   if(frame){const auto gap=std::chrono::duration_cast<Micros>(now-previous).count();assert(uint64_t(gap)>=period);assert(uint64_t(gap)==std::max(work,period));expected+=std::max(work,period);}
   else expected=work;
   cadence.submitted(now);previous=now;
  }
  assert(uint64_t(std::chrono::duration_cast<Micros>(now.time_since_epoch()).count())==expected);
 }
 Cadence changed;Clock::time_point base{};changed.submitted(base);
 assert(changed.deadline(base+Micros(2000),144)==base+Micros(6945));
 assert(changed.deadline(base+Micros(2000),60)==base+Micros(16667));
 const auto resumed=base+std::chrono::seconds(5);assert(changed.deadline(resumed,144)==resumed);changed.submitted(resumed);
 assert(changed.deadline(resumed,144)==resumed+Micros(6945));
 std::puts("PASS: compositor upload rate is min(requested, confirmed monitor Hz), unknown keeps requested; ceil microsecond periods respect the cap");
 std::puts("PASS: 18000 controlled frames include alternating 2/18ms work: monitor cap retained, no appended idle period or catch-up upload burst");
}
