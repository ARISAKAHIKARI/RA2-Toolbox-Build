#include "../source/construction_policy.h"
#include <cassert>
#include <cstdio>
using assist::construction::showReservationChoice;
int main(){
 unsigned cases=0;
 for(int tab=0;tab<4;++tab)
 for(int native=0;native<=1;++native)
 for(int queued=0;queued<=1;++queued)
 for(int future=0;future<=1;++future)
 for(int busy=0;busy<=1;++busy)
 for(int optIn=0;optIn<=1;++optIn){
  // Building/defense retain the original pre-existing native factory-busy
  // behavior. Infantry/vehicle native cameos are hidden by default.
  const bool expected=queued||(!native&&future)||
    (native&&(tab<2?bool(busy):bool(optIn)));
  assert(showReservationChoice(tab,native,queued,future,busy,optIn)==expected);
  ++cases;
 }
 // No permission side effects: this routine controls PANEL VISIBILITY only.
 assert(!showReservationChoice(2,true,false,false,false,false));
 assert(!showReservationChoice(3,true,false,false,false,false));
 assert(showReservationChoice(2,true,false,false,false,true));
 assert(showReservationChoice(3,true,false,false,false,true));
 assert(showReservationChoice(2,true,true,false,false,false));
 assert(showReservationChoice(3,true,true,false,false,false));
 assert(showReservationChoice(2,false,false,true,false,false));
 assert(showReservationChoice(3,false,false,true,false,false));
 assert(showReservationChoice(0,true,false,false,true,false));
 assert(!showReservationChoice(0,true,false,false,false,true));
 std::printf("PASS unit reservation candidate policy: %u tab/native/queued/future/busy/option combinations\n",cases);
}
