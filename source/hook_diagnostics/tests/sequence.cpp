#include "../source/sequence.h"
#include "../source/catalog.h"
#include "../source/selection.h"
#include "../source/observation_gate.h"
#include <set>
#include <string>
#include <cassert>
#include <cstdio>
int main(){
 using namespace hooktest;Sequence s;s.start(0,100);assert(!s.tick(1000000));
 s.installed(1000000);assert(!s.tick(1029999));assert(s.tick(1030000));
 assert(!s.tick(1090000)&&s.index==0);assert(s.normal());
 assert(!s.restored(false,true,1090000)&&s.index==0);assert(s.restored(true,true,1090000)&&s.index==1);
 s.installed(1100000);s.stop(true);assert(!s.normal()&&!s.restored(true,true,1200000)&&s.index==1);
 s.start(3,0);s.installed(0);assert(s.tick(30000));assert(s.normal());assert(s.restored(true,false,30001));assert(s.phase==Phase::Complete);
 s.start(0,100000);s.installed(100000);assert(!s.tick(90000));
 s.pause(20000);assert(!s.tick(130000));assert(s.tick(150000));
 // Normal may end a verified observation early, never the install itself.
 s.start(0,100);assert(!s.normal()&&!s.skip());s.installed(200);
 assert(s.normal()&&!s.restored(false,true,300));assert(s.restored(true,true,400));
 s.installed(500);assert(s.skip());assert(s.restored(true,true,600));
 s.phase=Phase::Blocked;assert(!s.skip(true));assert(s.skip());
 s.stop(true);assert(!s.skip()&&!s.normal()&&!s.restored(true,true,100000));
 s.start(2,100);s.installed(100);assert(s.tick(30100));s.pause(90000);
 assert(s.phase==Phase::Question&&s.since==90100&&!s.tick(130000));assert(s.skip());
 // No baseline steps by default. A single group is exactly one step;
 // opting into baselines prepends them once for the entire run.
 Selection selection;auto plan=selection.schedule(0,true);assert(plan.size()==19&&plan.front()==2);
 auto withBaselines=selection.schedule(0,true,true);assert(withBaselines.size()==21&&withBaselines[0]==0&&withBaselines[1]==1);
 assert(selection.schedule(2,false)==std::vector<unsigned>{3});
 assert(selection.schedule(2,true).size()==18&&selection.schedule(1,false)==std::vector<unsigned>{2});
 selection.excluded[2]=true;assert(selection.schedule(1,false).empty()&&selection.schedule(0,true).size()==18);
 selection.excluded[2]=false;
 assert(selection.schedule(-1,true).empty()&&selection.schedule(100,true).empty());
 assert(selection.nextChoice(2)==2);
 std::set<std::wstring> keys;
 for(unsigned i=2;i<steps().size();++i){
  assert(keys.insert(selectionKey(steps()[i].kind)).second);
  Selection excluded;excluded.excluded.assign(steps().size(),false);excluded.excluded[i]=true;
  auto filtered=excluded.schedule(0,true);
  assert(std::find(filtered.begin(),filtered.end(),i)==filtered.end());
  for(auto j:filtered)for(auto address:steps()[j].sites)
   assert(std::find(steps()[i].sites.begin(),steps()[i].sites.end(),address)==steps()[i].sites.end());
 }
 selection.excluded.assign(steps().size(),true);assert(selection.schedule(0,true).empty()&&selection.nextChoice(2)==-1);
 assert((selection.schedule(0,true,true)==std::vector<unsigned>{0,1}));
 // Loss of focus freezes observation, not the test. Busy snapshots have
 // bounded retries and focus return has a longer publication grace period.
 ObservationGate gate;assert(gate.focus(true));assert(gate.verify(true,0)==Verification::Ready);
 assert(gate.verify(false,100)==Verification::Waiting&&gate.verify(false,1099)==Verification::Waiting);
 assert(gate.verify(true,1100)==Verification::Ready);
 assert(gate.verify(false,1200)==Verification::Waiting&&gate.verify(false,2200)==Verification::Failed);
 assert(!gate.focus(false)&&!gate.focus(false)&&gate.background&&!gate.mismatch);
 s.start(0,100);s.installed(100);s.pause(600000);assert(!s.tick(600101));
 assert(gate.focus(true)&&gate.recovering&&gate.verify(false,600100)==Verification::Waiting);
 assert(gate.verify(false,605099)==Verification::Waiting&&gate.verify(false,605100)==Verification::Failed);
 assert(gate.verify(true,605101)==Verification::Ready&&!gate.recovering);
 assert(!gate.focus(false)&&gate.focus(true));assert(gate.verify(false,700000)==Verification::Waiting);
 assert(gate.verify(true,700001)==Verification::Ready);
 auto all=allSites();assert(all.size()==51);unsigned active=0,dormantCount=0,denied=0;
 for(auto a:all){assert(covered(a)!=dormant(a));if(covered(a))++active;else ++dormantCount;}
 for(const auto&stage:steps()){std::set<uint32_t>unique;for(auto a:stage.sites){assert(experimentalpatch::size(a)&&!dormant(a));assert(unique.insert(a).second);}}
 for(uint32_t a=0x400000;a<0x800000;++a)if(assist::hookregistry::ownerAt(a)!=assist::hookregistry::Owner::Unknown&&!experimentalpatch::size(a))++denied;
 assert(active==48&&dormantCount==3&&denied==14&&steps().size()==21);
 puts("PASS: all 51 native sites accounted for: 48 scheduled (camera conditional), 3 product-disabled; 14 denied sites never scheduled; 21 stages");
 puts("PASS: timing starts after verified installation; answer required; failed restoration and OOS cannot advance; final stage and clock regression");
 puts("PASS: early normal, manual skip, no default baselines, shared-site exclusions, baseline opt-in, focus pause and bounded snapshot retry");
}
