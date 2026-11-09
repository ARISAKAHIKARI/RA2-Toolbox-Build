#pragma once
#include <cstdint>
#include <climits>
#include <algorithm>
#include <memory>
#include <new>
namespace assist { namespace missilecompute {
constexpr int MaxSources=256,MaxTargets=256,MaxOrders=256,LookAhead=30,CoveragePercent=120,MaxAdvanceFrames=60,AdvanceLeash=1024,ReleaseGraceFrames=30;
enum Mode { Smart=0, Even=1 };
constexpr int ReserveSleep=2,TransientBusy=-2;
constexpr int ShotReserveFrames=12;
// Advance is a planner action: mobile Attack or passive building reservation.
enum Action { Hold=0,Attack=1, Release=5, Advance=32 };
struct Source {uint32_t id=0,pointer=0,token=0,current=0,pending=0;int cooldown=0,held=0,pendingMission=-1,advanceFrames=-1,releaseFrames=0,managed=0;uint32_t committedTarget=0;int committedDamage=0;};
struct Target {uint32_t id=0,pointer=0;int hp=0;uint32_t threatening=0;};
// eta is prediction only; close is the original IsCloseEnough result.
struct Edge {int damage=0;uint32_t distance=0;int eta=0,close=1;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0,targetCount=0,mode=Smart;
 Source sources[MaxSources];Target targets[MaxTargets];Edge edges[MaxSources][MaxTargets];};
inline void reset(Input& input){
 // Construct directly in the existing storage. Input{} assignment can put
 // a >1 MiB temporary on the game's default 1 MiB stack before an early-out.
 // Placement value-initialization allocates nothing and preserves all DMI.
 ::new(static_cast<void*>(&input)) Input{};
}
struct Assignment {int source=0,target=-1,action=Attack;};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Assignment orders[MaxOrders];};
// One workspace belongs to one calculation lane. Keep the 64 KiB candidate
// storage out of the game's stack and reuse it without retaining input facts.
struct Workspace {
 uint8_t sources[MaxTargets][MaxSources];
 uint8_t targets[MaxTargets];
 uint16_t count[MaxTargets];
};
static_assert(MaxSources<=256&&MaxTargets<=256,"candidate indices must fit in one byte");
inline bool valid(const Input&i){return i.frame>=0&&i.count>=0&&i.count<=MaxSources&&i.targetCount>=0&&i.targetCount<=MaxTargets&&i.mode>=0&&i.mode<=3;}
inline Result calculateWithWorkspace(const Input&i,Workspace&work){
 Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(!valid(i))return r;
 int assigned[MaxSources],number[MaxTargets]{},options[MaxSources]{};int64_t damage[MaxTargets]{};
 bool busy[MaxSources]{};
 std::fill(assigned,assigned+MaxSources,-1);
 if(i.mode&ReserveSleep)for(int s=0;s<i.count;++s)if(i.sources[s].committedDamage>0)
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].id==i.sources[s].committedTarget&&i.targets[t].hp>0)damage[t]+=i.sources[s].committedDamage;
 for(int s=0;s<i.count;++s){
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].hp>0&&i.edges[s][t].damage>0){
   if(std::max(i.edges[s][t].eta,i.sources[s].cooldown)<=LookAhead)++options[s];
  }
  // Pending events reserve their shooters, including attacks still approaching.
  busy[s]=i.sources[s].pendingMission!=-1;
  if(busy[s]&&(i.sources[s].pendingMission==Attack||i.sources[s].pendingMission==0))for(int t=0;t<i.targetCount;++t)
   if(i.sources[s].pending==i.targets[t].id&&i.edges[s][t].damage>0){assigned[s]=t;++number[t];if(!(i.mode&ReserveSleep)||i.sources[s].committedTarget!=i.targets[t].id)damage[t]+=i.edges[s][t].damage;break;}
 }
 // A target's damage/count changes during allocation, but every source for
 // that target shares those keys. Its remaining source order is therefore
 // fixed. Heapify each column once, then remove consumed sources at its head.
 // A second heap orders the column heads. An assigned head is an optimistic
 // old key: removing it can only worsen that column, so repair it only when
 // it reaches the global head. Every edge is removed at most once, giving
 // O(S*T*(log(S)+log(T))) worst-case work instead of O(S*S*T).
 auto later=[&](int t,uint8_t a,uint8_t b){
   const auto&x=i.edges[a][t];const auto&y=i.edges[b][t];
   const int ar=std::max(x.eta,i.sources[a].cooldown),br=std::max(y.eta,i.sources[b].cooldown);
   if(ar!=br)return ar>br;
   if(options[a]!=options[b])return options[a]>options[b];
   const bool ac=i.sources[a].current==i.targets[t].id,bc=i.sources[b].current==i.targets[t].id;
   if(ac!=bc)return bc;
   if(x.distance!=y.distance)return x.distance>y.distance;
   if(i.sources[a].id!=i.sources[b].id)return i.sources[a].id>i.sources[b].id;
   return a>b;
 };
 int targetCount=0;
 for(int t=0;t<i.targetCount;++t){
  work.count[t]=0;
  if(i.targets[t].hp<=0||((i.mode&1)==Smart&&damage[t]*100>=int64_t(i.targets[t].hp)*CoveragePercent))continue;
  auto&order=work.sources[t];
  for(int s=0;s<i.count;++s)if(!busy[s]&&i.edges[s][t].damage>0&&std::max(i.edges[s][t].eta,i.sources[s].cooldown)<=LookAhead)order[work.count[t]++]=uint8_t(s);
  std::make_heap(order,order+work.count[t],[&](uint8_t a,uint8_t b){return later(t,a,b);});
  if(work.count[t])work.targets[targetCount++]=uint8_t(t);
 }
 auto clean=[&](int t){
   auto&count=work.count[t];auto&order=work.sources[t];
   while(count&&assigned[order[0]]>=0){std::pop_heap(order,order+count,[&](uint8_t a,uint8_t b){return later(t,a,b);});--count;}
 };
 auto betterTarget=[&](int t,int bt){
    const int s=work.sources[t][0],bs=work.sources[bt][0];
    const auto&e=i.edges[s][t];const auto&target=i.targets[t];
    const int64_t left=(i.mode&1)==Even?number[t]:damage[t]*i.targets[bt].hp;
    const int64_t right=(i.mode&1)==Even?number[bt]:damage[bt]*target.hp;
    if(left!=right)return left<right;
    if(target.threatening!=i.targets[bt].threatening)return target.threatening>i.targets[bt].threatening;
    const int ready=std::max(e.eta,i.sources[s].cooldown),br=std::max(i.edges[bs][bt].eta,i.sources[bs].cooldown);
    if(ready!=br)return ready<br;
    if(options[s]!=options[bs])return options[s]<options[bs];
    if((i.sources[s].current==target.id)!=(i.sources[bs].current==i.targets[bt].id))return i.sources[s].current==target.id;
    if(e.distance!=i.edges[bs][bt].distance)return e.distance<i.edges[bs][bt].distance;
    if(target.id!=i.targets[bt].id)return target.id<i.targets[bt].id;
    if(i.sources[s].id!=i.sources[bs].id)return i.sources[s].id<i.sources[bs].id;
    // The original source-major scan keeps its first exact tie, even for
    // duplicate IDs in a value input. Column traversal must preserve that.
    return s<bs||(s==bs&&t<bt);
 };
 auto laterTarget=[&](uint8_t a,uint8_t b){return betterTarget(b,a);};
 auto reinsert=[&](int t){if(work.count[t]){work.targets[targetCount++]=uint8_t(t);std::push_heap(work.targets,work.targets+targetCount,laterTarget);}};
 std::make_heap(work.targets,work.targets+targetCount,laterTarget);
 for(int pass=0;pass<i.count;++pass){int bt=-1;
  while(targetCount){
   // Remove a target before changing its source head or its damage/count key.
   // The keys of every target still in the global heap remain unchanged.
   std::pop_heap(work.targets,work.targets+targetCount,laterTarget);const int t=work.targets[--targetCount];
   if(assigned[work.sources[t][0]]>=0){clean(t);reinsert(t);continue;}
   bt=t;break;
  }
  if(bt<0)break;const int bs=work.sources[bt][0];assigned[bs]=bt;++number[bt];damage[bt]+=i.edges[bs][bt].damage;
  if((i.mode&1)!=Smart||damage[bt]*100<int64_t(i.targets[bt].hp)*CoveragePercent){clean(bt);reinsert(bt);}
 }
 // Only immediately usable shots justify sending a ready gun ahead. Future
 // assignments/cooldowns are planning reservations, not guaranteed damage.
 int64_t covering[MaxTargets]{};
 if(i.mode&ReserveSleep)for(int s=0;s<i.count;++s)if(i.sources[s].committedDamage>0)
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].id==i.sources[s].committedTarget)covering[t]+=i.sources[s].committedDamage;
 for(int s=0;s<i.count;++s){int t=assigned[s];if(t>=0&&i.edges[s][t].close&&i.sources[s].cooldown<=3&&i.sources[s].pendingMission!=0)covering[t]+=i.edges[s][t].damage;}
 int actions[MaxSources]{};
 for(int s=0;s<i.count;++s){if(busy[s])continue;int t=assigned[s],fallback=-1,future=-1;
  for(int n=0;n<i.targetCount;++n){const auto&e=i.edges[s][n];if(e.damage<=0||i.targets[n].hp<=0)continue;
   if(!e.close&&e.eta<=LookAhead){if(future<0||e.eta<i.edges[s][future].eta)future=n;continue;}
   if(!e.close)continue;
   if(fallback<0||i.targets[n].threatening>i.targets[fallback].threatening||
      (i.targets[n].threatening==i.targets[fallback].threatening&&covering[n]*i.targets[fallback].hp<covering[fallback]*i.targets[n].hp))fallback=n;
  }
  int action=Release;
  if(i.mode&ReserveSleep){
   if(t>=0)action=i.edges[s][t].close&&i.sources[s].cooldown<=3?Attack:Hold;
   else if(fallback>=0){
    t=fallback;bool covered=covering[t]*100>=int64_t(i.targets[t].hp)*CoveragePercent;
    action=i.sources[s].cooldown>3||covered&&i.targets[t].threatening<2?Hold:Attack;
    if(action==Attack)covering[t]+=i.edges[s][t].damage;
   }else if(future>=0){t=future;action=Hold;}
   actions[s]=action;assigned[s]=t;continue;
  }
  if(t>=0&&i.edges[s][t].close)action=Attack;
  else if(fallback>=0){
   bool covered=covering[fallback]*100>=int64_t(i.targets[fallback].hp)*CoveragePercent;
   bool ahead=future>=0&&covered&&i.targets[fallback].threatening<2&&!i.sources[s].releaseFrames&&i.sources[s].cooldown<=LookAhead;
   if(ahead){action=Advance;if(t<0)t=future;}
   else{action=Attack;t=fallback;if(i.sources[s].cooldown<=3)covering[t]+=i.edges[s][t].damage;}
  }else if(future>=0&&!i.sources[s].releaseFrames&&i.sources[s].cooldown<=LookAhead){action=Advance;if(t<0)t=future;}
  // No eligible target: release only an automatic order we still own.
  actions[s]=action;assigned[s]=t;
 }
 // Covering attacks/wakes precede advances if the gateway runs out of budget.
 for(int phase=0;phase<2;++phase)for(int s=0;s<i.count;++s){if(busy[s])continue;
  int t=assigned[s],action=actions[s];if((phase==1)!=(action==Advance||action==Hold))continue;
  if(action==Release&&!i.sources[s].held&&!i.sources[s].managed)continue;
  // held records ownership from enqueue, not proof the game entered Sleep.
  // Runtime deduplicates only an observed Sleep with an empty target, so a
  // timed-out or rejected hold remains eligible for another native event.
  if(action!=Release&&i.sources[s].current==i.targets[t].id&&
     ((action==Attack&&!i.sources[s].held)||(action==Advance&&i.sources[s].managed&&i.sources[s].advanceFrames>=0)))continue;
  r.orders[r.count++]={s,t,action};
 }
 return r;
}
// Convenience entry for standalone value callers. Production Runtime and Host
// pass their own reusable workspace, so neither allocates once per calculation.
inline Result calculate(const Input&i){
 if(!valid(i)){Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;return r;}
 std::unique_ptr<Workspace>work(new Workspace);
 return calculateWithWorkspace(i,*work);
}
} }


