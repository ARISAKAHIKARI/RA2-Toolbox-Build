#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../external_reader/source/scene_bank.h"
#include <cassert>
#include <cstdio>
using namespace external::radar;
int main(){
 SceneBank bank;
 auto sample=[](int frame,uint32_t tick,double observed,bool reveal=false){auto terrain=std::make_shared<Terrain>();terrain->art.resize(1);terrain->reveal=reveal;auto out=std::make_shared<Frame>();out->terrain=terrain;out->frame=frame;out->tick=tick;out->observedMs=observed;out->markerScanValid=true;return out;};
 auto newest=sample(102,1020,1020.);bank.publish(1,7,true,newest);assert(bank.get(1,7,0,1021,true)==newest);
 // A slower producer sampled earlier, and must not cause watch views to
 // reset merely because its source finishes after a newer worker.
 bank.publish(1,7,true,sample(101,1030,1010.));assert(bank.get(1,7,0,1031,true)==newest);
 bank.publish(1,7,true,sample(101,1010,0));assert(bank.get(1,7,0,1031,true)==newest);
 bank.publish(1,7,true,sample(101,1020,1020.));assert(bank.get(1,7,0,1031,true)==newest);
 // An old reveal-policy job cannot replace the new policy's source either.
 bank.publish(1,7,true,sample(101,1010,1010.,true));assert(bank.get(1,7,0,1031,true)==newest);
 auto rewind=sample(2,1040,1040.);bank.publish(1,7,true,rewind);assert(bank.get(1,7,0,1041,true)==rewind);
 auto loaded=sample(1,1,1.);bank.publish(1,8,true,loaded);assert(bank.get(1,8,0,2,true)==loaded&&!bank.get(1,7,0,1041,true));
 auto other=sample(0,0,0);bank.publish(2,8,true,other);assert(bank.get(2,8,0,1,true)==other);
 auto beforeWrap=sample(100,0xfffffff0u,0);bank.publish(3,1,true,beforeWrap);auto afterWrap=sample(101,8,0);bank.publish(3,1,true,afterWrap);assert(bank.get(3,1,0,9,true)==afterWrap);
 bank.publish(3,1,true,beforeWrap);assert(bank.get(3,1,0,9,true)==afterWrap);
 auto invalid=sample(102,10,10.);invalid->markerScanValid=false;bank.publish(3,1,true,invalid);assert(bank.get(3,1,0,11,true)==afterWrap);
 auto reveal=sample(103,12,12.,true);bank.publish(3,1,true,reveal);assert(bank.get(3,1,1,13,true)==reveal&&!bank.get(3,1,0,13,true));
 assert(!bank.get(3,1,1,513,true));
 std::puts("PASS: production SceneBank rejects late older observations, preserves newer rewinds, separates identities and reveal policy, handles tick wrap and rejects incomplete sources");
}
