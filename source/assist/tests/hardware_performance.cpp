// The production value-only diagnostic reducer. No Windows or game timings
// are inferred from these tests; exact counts, wrap and publication are checked.
#ifdef NDEBUG
#error Hardware performance checks require assertions enabled
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#include "../../shared/source/hardware_performance.h"
using namespace assist::hardwareperf;
static unsigned checks=0;
static void check(bool value){++checks;assert(value);}
int main(){
 Window window;Counters totals{100,90,70,10,12,8,20,40};Snapshot out;
 check(window.observe(100,1,10,totals,out));check(out.version==Version&&out.serial==1&&out.match==1&&!out.intervalMs);
 check(out.firstLogicFrame==10&&out.lastLogicFrame==10&&out.observedLogicFrames==1);
 check(!std::memcmp(&out.cumulative,&totals,sizeof totals));const Counters zero{};check(!std::memcmp(&out.delta,&zero,sizeof zero));
 const Snapshot original=out;
 for(unsigned i=1;i<1000;++i){++totals.exceptions;++totals.control;++totals.admissionChecks;
  check(!window.observe(100+i,1,10,totals,out));check(!std::memcmp(&out,&original,sizeof out));
 }
 ++totals.exceptions;++totals.control;++totals.admissionChecks;
 check(window.observe(1100,1,12,totals,out));check(out.serial==2&&out.intervalMs==1000&&out.tick==1100);
 check(out.delta.exceptions==1000&&out.delta.control==1000&&out.delta.admissionChecks==1000&&!out.delta.changed);
 check(out.firstLogicFrame==10&&out.lastLogicFrame==12&&out.observedLogicFrames==2); // skipped frame 11 is not invented
 ++totals.worldCaptures;check(!window.observe(1200,1,13,totals,out));
 totals.full+=5;totals.enhancement+=6;totals.map+=7;totals.changed+=8;totals.exceptions+=9;
 check(window.observe(6100,1,30,totals,out));check(out.intervalMs==5000&&out.observedLogicFrames==3);
 check(out.delta.worldCaptures==1&&out.delta.full==5&&out.delta.enhancement==6&&out.delta.map==7&&out.delta.changed==8&&out.delta.exceptions==9);
 check(window.observe(6150,2,1,totals,out));check(out.match==2&&!out.intervalMs&&out.serial==4&&!std::memcmp(&out.delta,&zero,sizeof zero));
 ++totals.exceptions;check(window.observe(6200,2,0,totals,out));check(!out.intervalMs&&out.firstLogicFrame==0&&out.serial==5);
 Window wrapped;Counters before{0xfffffff0u,0xfffffff1u,0xfffffff2u,0xfffffff3u,0xfffffff4u,0xfffffff5u,0xfffffff6u,0xfffffff7u};
 check(wrapped.observe(0xfffffff0u,7,100,before,out));const Counters after{16,17,18,19,20,21,22,23};
 check(wrapped.observe(0x3d8u,7,100,after,out));check(out.intervalMs==1000&&out.observedLogicFrames==1);
 const Counters expected{32,32,32,32,32,32,32,32};check(!std::memcmp(&out.delta,&expected,sizeof expected));
 check(wrapped.observe(0x7c0u,7,100,after,out));check(out.intervalMs==1000&&!std::memcmp(&out.delta,&zero,sizeof zero));
 Window idle;check(idle.observe(0,0,-1,{},out));check(!out.observedLogicFrames);
 check(!idle.observe(999,0,-1,{},out));check(idle.observe(1000,0,-1,{},out)&&!out.observedLogicFrames);
 check(!idle.observe(1100,0,0,{},out));check(idle.observe(2000,0,0,{},out)&&out.observedLogicFrames==1);
 std::printf("{\"pass\":true,\"checks\":%u,\"snapshot_bytes\":%zu,\"counter_bytes\":%zu,\"minimum_window_ms\":%u,\"windows_executed\":false,\"game_executed\":false,\"timing_measured\":false}\n",checks,sizeof(Snapshot),sizeof(Counters),IntervalMs);
}
