#include "../../shared/source/critical_path_timing.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>
using namespace criticalpath;
int main(){
 static_assert(sizeof(Snapshot)==4848,"x86/x64 IPC payload fixed");
 static_assert(Samples==128&&StageCount==9,"expected fixed ring");
 Recorder rec;Snapshot output{};
 assert(!rec.publish(1000,7,output));
 assert(!rec.publish(1999,7,output));
 for(uint32_t i=1;i<=100;++i)rec.add(MessageHook,i);
 rec.add(WorldCapture,1500);
 assert(rec.publish(2000,7,output));
 assert(output.version==Version&&output.intervalMs==1000&&output.match==7);
 auto h=summarize(output.rows[MessageHook]);
 assert(h.sampled==100&&h.stored==100&&h.p95Us==95&&h.p99Us==99&&h.maxUs==100);
 assert(h.meanUs==50.5&&h.sumUs==5050);
 auto w=summarize(output.rows[WorldCapture]);
 assert(w.sampled==1&&w.stored==1&&w.p95Us==1500&&w.p99Us==1500);
 assert(!rec.publish(2500,7,output));
 for(uint32_t i=1;i<=512;++i)rec.add(HardwareHit,i);
 assert(rec.publish(3000,7,output));
 auto b=summarize(output.rows[HardwareHit]);
 assert(b.sampled==512&&b.stored==Samples&&b.maxUs==512&&b.meanUs==256.5);
 assert(b.p95Us==506&&b.p99Us==511); // last 128 readings: 385..512
 // A new match never borrows previous game's histogram or window.
 rec.add(SceneScan,3000);
 assert(!rec.publish(3500,8,output));
 rec.add(SceneScan,4000);
 assert(rec.publish(4500,8,output));
 assert(summarize(output.rows[SceneScan]).sampled==1);
 assert(summarize(output.rows[SceneScan]).maxUs==4000);
 for(unsigned i=0;i<StageCount;++i)assert(Stride[i]>=1);
 puts("PASS fixed-size game thread profiler, latest-128 p95/p99 and match reset");
}
