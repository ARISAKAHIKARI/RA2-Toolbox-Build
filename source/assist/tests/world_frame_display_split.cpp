// Production CaptureGraph: DisplayOnly producer is a small sealed scene
// boundary; Host VisualCache reconstructs the full non-command object graph.
// Control consumers must still receive an ordinary complete Base graph.
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
#include <cassert>
#include <cstdio>
#include "../../shared/source/world_frame_visual.h"
int main(){
 using namespace fixture;
 Memory memory;setup(memory);Read read{&memory};
 auto shallow=std::make_shared<worldframe::Snapshot>();
 auto k=key(50);k.tick=100;
 shallow->reset(k);
 worldframe::CaptureScratch scratch;
 worldframe::CaptureGraph<Read> graph(*shallow,read,scratch);
 assert(graph.capture(worldframe::Base|worldframe::HostDisplayOnly,{}));
 assert(shallow->seal());
 const auto shallowCalls=shallow->counters.calls;
 const auto shallowBytes=shallow->counters.bytes;
 const auto shallowPages=shallow->pages.size();
 int hp=-1;uint32_t actorType=0;
 assert(!shallow->read(ActorA+0x6C,&hp,sizeof hp));
 assert(!shallow->read(Type,&actorType,sizeof actorType));
 assert(shallow->read(0x87F924,&actorType,sizeof actorType)&&actorType==Cells);
 assert(shallow->read(0xA8B230,&actorType,sizeof actorType)&&actorType==Scenario);
 // The source sample has already sealed; displaying the actor must not
 // mutate those bytes or create command authority from the Host's copy.
 const auto baseSerial=shallow->key.serial;
 worldframe::VisualCache host;
 auto display=host.acquire(shallow,worldframe::Visual|worldframe::Passengers,104,read);
 assert(display&&display->sealed()&&display->controlSource==shallow);
 assert(display->read(ActorA+0x6C,&hp,sizeof hp)&&hp==100);
 assert(display->read(Type,&actorType,sizeof actorType));
 assert(!shallow->read(ActorA+0x6C,&hp,sizeof hp));
 assert(shallow->key.serial==baseSerial);
 // A steady-state visual reader already knows these three Type pages from
 // the previous observation. Warm the run before discovery, not after
 // actor() has made three separate page calls. No authority or bytes change.
 worldframe::VisualCache warmHost;
 const uint32_t typePage=Type&~4095u;
 warmHost.request({typePage,typePage+4096,typePage+8192},104,false);
 auto warmDisplay=warmHost.acquire(shallow,worldframe::Visual|worldframe::Passengers,104,read);
 assert(warmDisplay&&warmDisplay->controlSource==shallow);
 assert(warmDisplay->counters.calls+2<=display->counters.calls);
 assert(warmDisplay->counters.bytes==display->counters.bytes);
 assert(warmDisplay->pages.size()==display->pages.size());
 assert(warmDisplay->read(ActorA+0x6C,&hp,sizeof hp)&&hp==100);
 assert(warmDisplay->read(Type,&actorType,sizeof actorType));
 assert(!shallow->read(Type,&actorType,sizeof actorType));
 std::printf("PASS Host leased Type warmup: %u -> %u reads, equal bytes/pages\\n",
   display->counters.calls,warmDisplay->counters.calls);
 // Raw Base requests never enter the boundary-only lane. Command readers
 // receive their own full actor and object type data in the native sample.
 worldframe::Snapshot full;
 full.reset(k);
 worldframe::CaptureScratch fullScratch;
 worldframe::CaptureGraph<Read> fullGraph(full,read,fullScratch);
 assert(fullGraph.capture(worldframe::Base,{}));assert(full.seal());
 assert(full.read(ActorA+0x6C,&hp,sizeof hp)&&hp==100);
 assert(full.read(Type,&actorType,sizeof actorType));
 assert(shallowCalls<full.counters.calls);
 assert(shallowBytes<full.counters.bytes);
 assert(shallowPages<full.pages.size());
 std::printf("PASS display/control separation: game-thread %u reads/%u bytes/%zu pages vs full %u reads/%u bytes/%zu pages; Host recovered actor and Type without mutating command source\n",
   shallowCalls,shallowBytes,shallowPages,
   full.counters.calls,full.counters.bytes,full.pages.size());
}
