#include "../source/command_gateway.h"
#include <cassert>
using namespace assist::gateway;
int main(){
 Context c{12,500,true,false};Intent i{12,480,123,456,0};
 assert(validate(c,i,50)==Failure::None);
 assert(validate({13,500,true,false},i,50)==Failure::WrongMatch);
 assert(validate({12,550,true,false},i,50)==Failure::Stale);
 assert(validate({12,500,true,true},i,50)==Failure::ChatActive);
 assert(!canControl({1,0,0,1})&&canControl({0,1,1,0}));
 assert(chooseVisibleStatus({-1,1,1,0})==1);
 assert(chooseVisibleStatus({0,0,1,0})==1); // stale display must not unregister live command hotkeys
 assert(chooseVisibleStatus({0,-2,1,0})==-2); // real bridge fault still wins
 assert(chooseVisibleStatus({1,-1,0,1})==-1);
 Receipt r;planned(r,123,i);assert(r.phase==Phase::Planned&&!confirmed(r));
 submitted(r,500);assert(r.phase==Phase::Queued&&!confirmed(r));
 finished(r);assert(!confirmed(r));observed(r,502);assert(confirmed(r));finished(r);assert(r.phase==Phase::Completed);
 MissionObserver<4> ring;
 ring.record(12,123,456,7,100,500,{true,5,5,0});
 assert(ring.pending());
 auto before=[](uint32_t){return MissionState{true,5,5,0};};
 assert(ring.poll(12,100,501,before)==0);assert(ring.poll(12,101,501,before)==0);
 auto boarded=[](uint32_t){return MissionState{true,5,5,456};};
 assert(ring.poll(12,102,502,boarded)==1&&!ring.pending());
 ring.record(12,123,0,2,102,503,{true,2,2,0});
 auto same=[](uint32_t){return MissionState{true,2,2,0};};
 assert(ring.poll(12,103,504,same)==0&&ring.pending());
 auto changed=[](uint32_t){return MissionState{true,5,2,0};};
 assert(ring.poll(12,104,505,changed)==0); // old queued=2, no new evidence
 assert(ring.poll(13,105,506,changed)==0&&!ring.pending());
 ring.record(12,123,0,2,105,510,{false,0,0,0});assert(!ring.pending());
 ring.record(12,123,0,2,105,510,{true,5,5,0});
 auto observedMove=[](uint32_t){return MissionState{true,2,2,0};};
 assert(ring.poll(12,106,511,observedMove)==1);
 // A pending no-change order must not starve a later observable one.
 ring.record(12,111,0,2,106,520,{true,2,2,0});
 ring.record(12,222,0,2,106,520,{true,5,5,0});
 auto multi=[](uint32_t id){return id==222?MissionState{true,2,2,0}:MissionState{true,2,2,0};};
 assert(ring.poll(12,107,521,multi,1)==0);
 assert(ring.poll(12,108,522,multi,1)==1);
}
