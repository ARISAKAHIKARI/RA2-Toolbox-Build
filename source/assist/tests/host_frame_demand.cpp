#include "../source/host_frame_demand.h"
#include <cassert>
#include <cstdio>
using namespace assist;
int main(){
 using namespace compute;
 static_assert(Default==0,"new installations should start with host-side pure computation");
 static_assert((All&(1u<<BurstRefresh))!=0,"retired burst slot is ABI-retained");
 HostFrameDemand d{};
 // The old computeApplied != All shortcut produced unconditional native
 // frame notifications whenever any Host slot had been selected, even when
 // every feature was off. Both idle placement masks must now be quiet.
 d.placement=Default;assert(!needsFrameNotification(d));
 d.placement=All-1;assert(!needsFrameNotification(d));
 d.placement=All;assert(!needsFrameNotification(d));
 // Every active host planner lane wakes; the same runtime placement does not.
 constexpr uint32_t flags[]={8192u,262144u,32768u,1073741824u};
 for(auto f:flags){d=HostFrameDemand{};d.flags=f;d.placement=0;assert(needsFrameNotification(d));
  d.placement=All;assert(!needsFrameNotification(d));}
 // Automatic repair, unload, missile, scatter dodge and magnetron auto modes.
 for(int i=0;i<6;++i){
  d={};d.placement=0;switch(i){
   case 0:d.repair=true;break;case 1:d.unload=true;break;
   case 2:d.dodge=true;break;case 3:d.missile=true;break;
   case 4:d.magnetronGlobal=true;break;case 5:d.magnetronDrop=true;break;
  }assert(needsFrameNotification(d));d.placement=All;assert(!needsFrameNotification(d));
 }
 // Pending completions must continue waking even when the user disabled the
 // feature or changed the requested placement. They are independent of the
 // newly selected mask, so no auto-computation can be mistaken for a command.
 for(auto mask:{Default,All}){d={};d.placement=mask;d.pending=true;assert(needsFrameNotification(d));}
 // A single unrelated setting never produces a wake.
 d={};d.flags=1u;assert(!needsFrameNotification(d));
 std::puts("PASS host-first placement and active-demand-only frame notifications");
}
