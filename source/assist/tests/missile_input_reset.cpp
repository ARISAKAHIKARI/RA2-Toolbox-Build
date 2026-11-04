// Production Input defaults/reset, with heap storage and field-wise comparison.
#include "../source/missile_compute.h"

#ifdef MISSILE_RESET_CODEGEN
// Compile these two equivalent operations with the release x86 Windows target.
// The pointer escapes so neither initialization can be removed as unused.
extern "C" __attribute__((noinline)) void missileResetOldAggregate(assist::missilecompute::Input*value){
 *value={};asm volatile(""::"r"(value):"memory");
}
extern "C" __attribute__((noinline)) void missileResetProduction(assist::missilecompute::Input*value){
 assist::missilecompute::reset(*value);asm volatile(""::"r"(value):"memory");
}
#else
#ifdef NDEBUG
#error Assertions must remain enabled
#endif
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
using namespace assist::missilecompute;
static unsigned checks=0,comparisons=0,passes=0;
static void check(bool okay){++checks;if(!okay){std::fprintf(stderr,"missile reset check %u failed\n",checks);std::abort();}}
template<class T>static void equal(T actual,T expected){++comparisons;check(actual==expected);}
struct Guarded {
 uint64_t before=0x6721c309549aebdfULL;
 Input input{};
 uint64_t after=0x91db357af0e8462cULL;
};
static void guards(const Guarded&value){check(value.before==0x6721c309549aebdfULL);check(value.after==0x91db357af0e8462cULL);}
static void pollute(Input&i,unsigned round){
 // Poison padding too; comparisons below deliberately inspect only fields.
 std::memset(&i,int(0x90+round),sizeof i);
 i.match=111+round;i.tick=222+round;i.frame=333+int(round);i.count=MaxSources;i.targetCount=MaxTargets;i.mode=ReserveSleep|Even;
 for(int n=0;n<MaxSources;++n){auto&s=i.sources[n];
  s.id=1000+n;s.pointer=2000+n;s.token=3000+n;s.current=4000+n;s.pending=5000+n;
  s.cooldown=6000+n;s.held=7000+n;s.pendingMission=8000+n;s.advanceFrames=9000+n;s.releaseFrames=10000+n;s.managed=11000+n;
  s.committedTarget=12000+n;s.committedDamage=13000+n;
 }
 for(int n=0;n<MaxTargets;++n){auto&t=i.targets[n];t.id=14000+n;t.pointer=15000+n;t.hp=16000+n;t.threatening=17000+n;}
 for(int n=0;n<MaxSources;++n)for(int m=0;m<MaxTargets;++m){auto&e=i.edges[n][m];e.damage=18000+n+m;e.distance=19000+n+m;e.eta=20000+n+m;e.close=-21000-n-m;}
}
static void compare(const Input&i,const Input&fresh){
 equal(i.match,fresh.match);equal(i.tick,fresh.tick);equal(i.frame,fresh.frame);equal(i.count,fresh.count);equal(i.targetCount,fresh.targetCount);equal(i.mode,fresh.mode);
 for(int n=0;n<MaxSources;++n){const auto&a=i.sources[n];const auto&b=fresh.sources[n];
  equal(a.id,b.id);equal(a.pointer,b.pointer);equal(a.token,b.token);equal(a.current,b.current);equal(a.pending,b.pending);
  equal(a.cooldown,b.cooldown);equal(a.held,b.held);equal(a.pendingMission,b.pendingMission);equal(a.advanceFrames,b.advanceFrames);equal(a.releaseFrames,b.releaseFrames);equal(a.managed,b.managed);
  equal(a.committedTarget,b.committedTarget);equal(a.committedDamage,b.committedDamage);
  check(a.pendingMission==-1&&a.advanceFrames==-1);
 }
 for(int n=0;n<MaxTargets;++n){const auto&a=i.targets[n];const auto&b=fresh.targets[n];equal(a.id,b.id);equal(a.pointer,b.pointer);equal(a.hp,b.hp);equal(a.threatening,b.threatening);}
 for(int n=0;n<MaxSources;++n)for(int m=0;m<MaxTargets;++m){const auto&a=i.edges[n][m];const auto&b=fresh.edges[n][m];equal(a.damage,b.damage);equal(a.distance,b.distance);equal(a.eta,b.eta);equal(a.close,b.close);check(a.close==1);}
 check(i.frame==-1&&i.mode==Smart);
 // Last slot includes the final scalar in Input, not merely the active prefix.
 check(i.edges[MaxSources-1][MaxTargets-1].close==1);
 ++passes;
}
int main(){
 auto actual=std::make_unique<Guarded>();auto fresh=std::make_unique<Guarded>();Input*const address=&actual->input;
 for(unsigned round=0;round<3;++round){
  pollute(actual->input,round);guards(*actual);reset(actual->input);check(&actual->input==address);guards(*actual);guards(*fresh);compare(actual->input,fresh->input);
  // Repeated reset must retain all defaults and never advance the storage.
  reset(actual->input);check(&actual->input==address);guards(*actual);compare(actual->input,fresh->input);
 }
 std::printf("{\"pass\":true,\"checks\":%u,\"field_comparisons\":%u,\"passes\":%u,\"input_bytes\":%zu,\"padding_compared\":false,\"heap_inputs\":2,\"game_process_tested\":false}\n",checks,comparisons,passes,sizeof(Input));
}
#endif
