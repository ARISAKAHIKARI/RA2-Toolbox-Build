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
 // Stress 1024 active leases: no more than 256 index checks per game
 // frame, repeated message dispatch in that frame does no extra sweep work.
 LeaseArbiter<1024> many;
 for(uint32_t id=1;id<=1024;++id)
  assert(many.record(30,100,id,0x10000u+id,1,Owner::AutoLoad,id));
 unsigned probes=0;
 for(int f=101;f<=104;++f){
  const unsigned before=probes;
  many.sweepBudgeted(30,f,[&](uint32_t){++probes;return std::make_pair(0u,0u);});
  assert(probes-before==256);
  many.sweepBudgeted(30,f,[&](uint32_t){assert(false);return std::make_pair(0u,0u);});
 }
 assert(many.count==0&&probes==1024);
 // Valid leases are not silently dropped; token/pointer change can still
 // immediately supersede an unswept lease via canIssue()/record().
 LeaseArbiter<4> retained;
 assert(retained.record(40,200,88,0xABCD,7,Owner::AutoLoad,401));
 unsigned keepReads=0;
 retained.sweepBudgeted(40,201,[&](uint32_t id){
  assert(id==88);++keepReads;return std::make_pair(0xABCDu,7u);
 });
 assert(keepReads==1&&retained.count==1);
 assert(retained.canIssue(40,201,88,0xABCD,8,Owner::Ore));
 assert(retained.record(40,201,88,0xABCD,8,Owner::Ore,402));
 retained.sweepBudgeted(40,201,[&](uint32_t){
  assert(false);return std::make_pair(0u,0u);
 });
 assert(retained.leases[0].serial==402);
 retained.sweepBudgeted(40,202,[&](uint32_t){
  ++keepReads;return std::make_pair(0xABCDu,8u);
 });
 assert(keepReads==2&&retained.count==1);
 retained.clear();
 assert(retained.record(40,202,89,0xBCDE,0,Owner::Ore,403));
 retained.sweepBudgeted(40,202,[&](uint32_t){++keepReads;return std::make_pair(0u,0u);});
 assert(keepReads==3&&retained.count==0);
 FrameArbiter<4> frame;frame.begin(10,120);assert(frame.record(7,1,Owner::Combat));
 assert(!frame.canIssue(7,1));frame.begin(10,121);assert(frame.canIssue(7,1));
}
