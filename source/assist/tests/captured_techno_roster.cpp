#include "../../shared/source/world_frame_capture.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

int main(){
 auto scratch=std::make_unique<worldframe::CaptureScratch>();
 constexpr uint32_t items=0x12345000u;
 worldframe::Key key{};key.pid=41;key.owner=9;key.match=312;
 key.frame=724;key.serial=190;key.tick=8801;key.house=0xA10100;
 key.scenario=0x987000;key.cells=0x456000;key.session=3;
 scratch->technoKey=key;scratch->technoItems=items;
 scratch->technoCount=16384;scratch->technoRoster.resize(16384);
 for(uint32_t i=0;i<16384;++i)scratch->technoRoster[i]=0x10000u+i*0x200u;
 const uint32_t*got=scratch->capturedTechnoRoster(key,items,16384);
 assert(got==scratch->technoRoster.data()); // must NOT materialize a copied list
 assert(got[0]==0x10000u&&got[16383]==0x10000u+16383u*0x200u);
 auto wrong=key;
 ++wrong.serial;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 wrong=key;++wrong.frame;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 wrong=key;++wrong.match;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 wrong=key;++wrong.house;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 wrong=key;++wrong.pid;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 wrong=key;++wrong.generationLow;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 wrong=key;++wrong.tick;assert(!scratch->capturedTechnoRoster(wrong,items,16384));
 assert(!scratch->capturedTechnoRoster(key,items+4,16384));
 assert(!scratch->capturedTechnoRoster(key,items,16383));
 assert(!scratch->capturedTechnoRoster(key,items,16385));
 scratch->reset();
 assert(!scratch->capturedTechnoRoster(key,items,16384));
 assert(scratch->technoRoster.empty()&&scratch->technoCount==-1);
 std::puts("PASS 16K Techno pointer roster O(1) direct borrow; serial/frame/world/header mismatches denied and reset invalidates stale bytes");
}
