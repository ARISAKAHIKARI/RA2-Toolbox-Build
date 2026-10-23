#include "../source/unit_task_arbiter.h"
#include <cassert>
#include <utility>
using namespace assist::taskarbiter;
int main(){
 LeaseArbiter<4> a;Lease removed{};
 assert(a.canIssue(10,100,7,0x4000,1,Owner::AutoLoad));
 assert(a.record(10,100,7,0x4000,1,Owner::AutoLoad,56));
 assert(!a.canIssue(10,101,7,0x4000,1,Owner::Ore));
 assert(a.canIssue(10,101,7,0x4000,1,Owner::AutoLoad));
 assert(a.canIssue(10,101,7,0x4000,1,Owner::Unload));
 assert(a.record(10,101,7,0x4000,1,Owner::Unload,57,&removed));
 assert(removed.serial==56&&removed.owner==Owner::AutoLoad);
 // A late receipt for superseded 56 may not release active 57.
 a.release(10,7,56);assert(a.find(7)>=0&&a.leases[a.find(7)].serial==57);
 a.release(10,7,57);assert(a.find(7)<0);
 assert(a.record(10,110,7,0x4000,1,Owner::Ore,60));
 assert(a.canIssue(10,111,7,0x4000,2,Owner::AutoLoad)); // manual revision
 assert(a.canIssue(10,111,7,0x5000,1,Owner::AutoLoad)); // pointer reuse
 assert(a.canIssue(11,111,7,0x4000,1,Owner::AutoLoad)); // next match
 assert(a.canIssue(10,156,7,0x4000,1,Owner::AutoLoad)); // lease timeout
 // Sweeping removes an invalid actor and never writes game state.
 a.sweep(10,112,[](uint32_t){return std::make_pair(0u,0u);});assert(a.find(7)<0);
 assert(a.record(10,120,7,0x4000,1,Owner::Combat,63));
 a.manual(7);assert(a.find(7)<0);
 // Owner::Sell supersedes Repair/Combat/Unload leases on the same object
 assert(a.record(10,120,9,0x6000,1,Owner::Repair,70));
 assert(a.canIssue(10,121,9,0x6000,1,Owner::Sell));
 assert(a.record(10,121,9,0x6000,1,Owner::Sell,71,&removed)&&removed.owner==Owner::Repair);
 assert(!a.canIssue(10,122,9,0x6000,1,Owner::MagnetronDrop));
 assert(a.record(10,121,7,0x4000,1,Owner::Generic,72));
 assert(!a.canIssue(10,122,7,0x4000,1,Owner::Magnetron));
 assert(a.canIssue(10,122,7,0x4000,1,Owner::MagnetronDrop));
 assert(syntheticProductionActor(3,5)!=syntheticSuperWeaponActor(5));
 FrameArbiter<4> frame;frame.begin(10,120);assert(frame.record(7,1,Owner::Combat));
 assert(!frame.canIssue(7,1));frame.begin(10,121);assert(frame.canIssue(7,1));
}
