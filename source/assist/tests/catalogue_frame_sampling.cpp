// Executes the production sampler, page leases, capture graph and transport.
// Only Win32 calls and game bytes come from the existing frame-core fixture.
#define WORLD_FRAME_FIXTURE_ONLY
#include "world_frame_core.cpp"
#include "../../external_reader/source/shared_reader.h"
#include "../../external_reader/source/tactical_sampler.h"
namespace fixture {
void catalogueSampling(bool baseline){
 currentPid=HostPid;clock=100;Memory memory;setup(memory);gameMemory=&memory;
 constexpr uint32_t unused=0x310100;
 memory.vector(0xA8B218,0x620000,{unused});memory.vector(0xA8E348,0x621000,{Type});
 memory.vector(0xA83CE0,0x622000,{});memory.vector(0xA83C68,0x623000,{});
 for(auto p:{unused,Type}){memory.put(p+0x24,0x4B4E4154u);memory.put(p+0x634,1);memory.put(p+0x6CC,1u);memory.put(p+0x6D6,0x504853u);}
 memory.put(0xA8E378,uint8_t(1));memory.put(0x887324,0x450000u);memory.put(0x8A00A4,800);memory.put(0x8A00A8,600);memory.put(0xB0CD48,0.0866025403784);
 for(auto p:{ActorA,ActorB}){memory.put(p+0x74,uint8_t(1));memory.put(p+0x90,uint8_t(1));memory.put(p+0x9C,1024);memory.put(p+0xA0,512);}
 external::ProcessReader reader;CHECK(reader.open(GamePid));CHECK(!reader.beginWorldFrame(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));
 worldframe::CaptureScratch scratch;worldframe::Snapshot raw;int frame=50;unsigned failures=0,complete=0;
 external::TacticalSampler<external::SharedReadView> sampler({&reader});sampler.worldEpoch(7);assist::Snapshot stable{};
 auto cycle=[&](bool active=true){
  currentPid=GamePid;auto k=key(++frame);k.serial=frame;k.tick=clock;k.generationLow=777;
  k.connectionLow=uint32_t(reader.generation());k.connectionHigh=uint32_t(reader.generation()>>32);memory.put(0xA8ED84,frame);
  raw.reset(k);worldframe::CaptureGraph<Read> graph(raw,Read{&memory},scratch);CHECK(graph.capture(worldframe::Base,server.requests()));CHECK(raw.seal());
  server.observe(k);server.publish(raw);currentPid=HostPid;CHECK(reader.beginWorldFrame(7));
  if(active){const auto&candidate=sampler.sample(clock,assist::Capacity,false,0,uint32_t(frame));
   if(reader.worldFrameComplete()){sampler.commitSample();stable=candidate;++complete;}
   else{sampler.invalidateSample(&stable);++failures;}}
  reader.endWorldFrame();clock+=16;
 };
 // Initial discovery may require several publications; warmup is separate
 // from repeated expiry after a previously complete catalogue is available.
 for(unsigned i=0;i<32;++i)cycle();CHECK(stable.typeCount==2);
 const auto warmFailures=failures,initialComplete=complete;
 for(unsigned i=0;i<1000;++i)cycle();
 const auto periodic=failures-warmFailures;
 for(const auto&call:memory.calls)CHECK(call.success);
 if(baseline){CHECK(periodic>0);std::printf("NEGATIVE CONTROL: %u incomplete / 1000 post-warmup frames; actual memory read failures=0\n",periodic);return;}
 CHECK(periodic==0&&complete-initialComplete==1000);CHECK(stable.typeCount==2);
 std::puts("PASS 1000 post-warmup logic frames: no repeated catalogue miss, real memory read failures=0");
 // The Host's same-publication camera path must keep dependency membership
 // while the simulation is paused, without inventing a newer capture tick.
 const auto pausedKey=raw.key;const auto beforePauseCalls=memory.calls.size();
 for(unsigned i=0;i<160;++i){currentPid=GamePid;server.observe(pausedKey);currentPid=HostPid;CHECK(reader.beginWorldFrame(7));
  CHECK(reader.worldFrameToken()->key.tick==pausedKey.tick);sampler.refreshCamera(clock);CHECK(reader.worldFrameComplete());reader.endWorldFrame();clock+=16;}
 CHECK(memory.calls.size()>beforePauseCalls); // Only the independent camera UI read.
 for(unsigned i=0;i<40;++i)cycle();CHECK(failures==warmFailures);
 std::puts("PASS paused camera-only path retains catalogue dependencies; resumed frames stay complete");
 // Retaining page membership must still capture changed metadata.
 memory.put(unused+0x6CC,0u);for(unsigned i=0;i<40;++i)cycle();CHECK(stable.typeCount==1&&failures==warmFailures);
 memory.put(unused+0x6CC,1u);for(unsigned i=0;i<40;++i)cycle();CHECK(stable.typeCount==2&&failures==warmFailures);
 std::puts("PASS retained pages observe changed catalogue metadata within its normal refresh cadence");
 // Retention must not suppress a real invalid range or certify stale bytes.
 memory.denied.push_back({unused&~4095u,4096});const auto beforeFault=failures;
 for(unsigned i=0;i<40;++i)cycle();CHECK(failures>beforeFault);memory.denied.clear();
 for(unsigned i=0;i<40;++i)cycle();const auto recovered=failures;
 for(unsigned i=0;i<80;++i)cycle();CHECK(failures==recovered&&stable.typeCount==2);
 std::puts("PASS actual unreadable catalogue still rejects the sample, then recovers");
 sampler.reset();for(unsigned i=0;i<24;++i)cycle(false);
 currentPid=GamePid;const auto&requests=server.requests();CHECK(std::find(requests.begin(),requests.end(),unused&~4095u)==requests.end());
 std::puts("PASS reset/inactive sampler releases catalogue pages through the unchanged 250ms lease");
}
}
int main(int argc,char**argv){fixture::catalogueSampling(argc>1&&std::string(argv[1])=="--expect-periodic-miss");std::printf("PASS catalogue sampling checks=%zu\n",fixture::checks);}
