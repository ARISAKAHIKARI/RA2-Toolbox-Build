#include "../source/unit_task_arbiter.h"
#include <cassert>
#include <memory>
#include <vector>
using namespace assist::taskarbiter;
int main(){
 FrameArbiter<2> a;a.begin(3,120);
 assert(a.canIssue(10,1));assert(a.record(10,1,Owner::AutoLoad));
 assert(!a.canIssue(10,1));assert(!a.canIssue(10,2));
 assert(a.record(11,0,Owner::Ore));assert(!a.canIssue(12,0));
 a.begin(3,121);assert(a.canIssue(10,1));
 assert(a.record(10,2,Owner::Unload));a.begin(4,121);
 assert(a.canIssue(10,2));a.clear();assert(a.canIssue(10,0));
 // Manual native input replaces its earlier automatic claim even when full.
 FrameArbiter<2> native;native.begin(7,200);
 assert(native.record(14,0,Owner::Ore)&&native.record(15,0,Owner::Unload));
 assert(native.replaceExisting(14,4,Owner::Generic));
 assert(native.actors[native.find(14)].manualVersion==4);
 assert(!native.replaceExisting(100,4,Owner::Generic)&&!native.record(16,0,Owner::Ore));
 // Indexed duplicate admission in a full 8192-unit command cohort.
 auto full=std::make_unique<FrameArbiter<8192>>();
 full->begin(42,100);
 for(uint32_t id=1;id<=8192;++id)assert(full->record(id,0,Owner::Ore));
 for(uint32_t id=1;id<=8192;++id)
  assert(full->find(id)>=0&&!full->canIssue(id,0)&&!full->record(id,1,Owner::Combat));
 assert(full->count==8192&&!full->record(8193,0,Owner::Ore));
 assert(full->replaceExisting(4096,19,Owner::Generic));
 assert(full->actors[full->find(4096)].manualVersion==19);
 full->begin(42,101);
 assert(full->count==0&&full->canIssue(4096,20));
 assert(full->record(8193,0,Owner::Combat)&&!full->canIssue(8193,0));
 // Also prove the rare 32-bit generation wrap invalidates every prior key.
 full->generation=UINT32_MAX;full->begin(43,101);
 assert(full->record(8194,0,Owner::Combat)&&!full->canIssue(8194,0));
 // Deliberate hash collisions must not alias another actor or skip an entry.
 FrameArbiter<8> collisions;collisions.begin(44,1);
 std::vector<uint32_t> keys;
 for(uint32_t id=1;id<200000&&keys.size()<8;++id)
  if(FrameArbiter<8>::hash(id)==0)keys.push_back(id);
 assert(keys.size()==8);
 for(uint32_t id:keys)assert(collisions.record(id,0,Owner::Ore));
 for(uint32_t id:keys)assert(collisions.find(id)>=0&&!collisions.canIssue(id,1));
 collisions.begin(44,2);
 for(uint32_t id:keys)assert(collisions.record(id,1,Owner::Generic));
}
