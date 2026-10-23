#pragma once
#include <cstdint>
#include <climits>
#include <algorithm>
namespace assist { namespace missilecompute {
constexpr int MaxSources=64,MaxTargets=64,MaxOrders=64,LookAhead=30,CoveragePercent=120,MaxHoldFrames=30,ReleaseGraceFrames=30;
enum Mode { Smart=0, Even=1 };
enum Action { Attack=1, Hold=0, Release=5 };
struct Source {uint32_t id=0,pointer=0,token=0,current=0,pending=0;int cooldown=0,held=0,pendingMission=-1,holdFrames=0,releaseFrames=0;};
struct Target {uint32_t id=0,pointer=0;int hp=0;uint32_t threatening=0;};
// eta is prediction only; close is the original IsCloseEnough result.
struct Edge {int damage=0;uint32_t distance=0;int eta=0,close=1;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0,targetCount=0,mode=Smart;
 Source sources[MaxSources];Target targets[MaxTargets];Edge edges[MaxSources][MaxTargets];};
struct Assignment {int source=0,target=-1,action=Attack;};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Assignment orders[MaxOrders];};
inline bool valid(const Input&i){return i.frame>=0&&i.count>=0&&i.count<=MaxSources&&i.targetCount>=0&&i.targetCount<=MaxTargets&&(i.mode==Smart||i.mode==Even);}
inline Result calculate(const Input&i){
 Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(!valid(i))return r;
 int assigned[MaxSources],number[MaxTargets]{},options[MaxSources]{};int64_t damage[MaxTargets]{};
 bool busy[MaxSources]{};
 std::fill(assigned,assigned+MaxSources,-1);
 for(int s=0;s<i.count;++s){
  for(int t=0;t<i.targetCount;++t)if(i.targets[t].hp>0&&i.edges[s][t].damage>0){
   if(std::max(i.edges[s][t].eta,i.sources[s].cooldown)<=LookAhead)++options[s];
  }
  // Pending events reserve their shooters. Existing auto-acquired targets do
  // not: excess shooters need an explicit native Sleep to preserve ammunition.
  busy[s]=i.sources[s].pendingMission>=0;
  if(busy[s]&&i.sources[s].pendingMission==Attack)for(int t=0;t<i.targetCount;++t)
   if(i.sources[s].pending==i.targets[t].id&&i.edges[s][t].damage>0){assigned[s]=t;++number[t];damage[t]+=i.edges[s][t].damage;break;}
 }
 for(int pass=0;pass<i.count;++pass){int bs=-1,bt=-1,br=INT_MAX;
  for(int s=0;s<i.count;++s)if(!busy[s]&&assigned[s]<0)for(int t=0;t<i.targetCount;++t){
   const auto&e=i.edges[s][t];const auto&target=i.targets[t];int ready=std::max(e.eta,i.sources[s].cooldown);
   if(e.damage<=0||target.hp<=0||ready>LookAhead||(i.mode==Smart&&damage[t]*100>=int64_t(target.hp)*CoveragePercent))continue;
   bool better=bt<0;
   if(!better){
    const int64_t left=i.mode==Even?number[t]:damage[t]*i.targets[bt].hp;
    const int64_t right=i.mode==Even?number[bt]:damage[bt]*target.hp;
    if(left!=right)better=left<right;
    else if(target.threatening!=i.targets[bt].threatening)better=target.threatening>i.targets[bt].threatening;
    else if(ready!=br)better=ready<br;
    else if(options[s]!=options[bs])better=options[s]<options[bs];
    else if((i.sources[s].current==target.id)!=(i.sources[bs].current==i.targets[bt].id))better=i.sources[s].current==target.id;
    else if(e.distance!=i.edges[bs][bt].distance)better=e.distance<i.edges[bs][bt].distance;
    else if(target.id!=i.targets[bt].id)better=target.id<i.targets[bt].id;
    else better=i.sources[s].id<i.sources[bs].id;
   }
   if(better){bs=s;bt=t;br=ready;}
  }
  if(bs<0)break;assigned[bs]=bt;++number[bt];damage[bt]+=i.edges[bs][bt].damage;
 }
 // Only immediately usable shots justify holding a ready gun. Future
 // assignments/cooldowns are planning reservations, not guaranteed damage.
 int64_t covering[MaxTargets]{};
 for(int s=0;s<i.count;++s){int t=assigned[s];if(t>=0&&i.edges[s][t].close&&i.sources[s].cooldown<=3)covering[t]+=i.edges[s][t].damage;}
 int actions[MaxSources]{};
 for(int s=0;s<i.count;++s){if(busy[s])continue;int t=assigned[s],fallback=-1,future=-1;
  for(int n=0;n<i.targetCount;++n){const auto&e=i.edges[s][n];if(e.damage<=0||i.targets[n].hp<=0)continue;
   if(!e.close&&e.eta<=LookAhead){if(future<0||e.eta<i.edges[s][future].eta)future=n;continue;}
   if(!e.close)continue;
   if(fallback<0||i.targets[n].threatening>i.targets[fallback].threatening||
      (i.targets[n].threatening==i.targets[fallback].threatening&&covering[n]*i.targets[fallback].hp<covering[fallback]*i.targets[n].hp))fallback=n;
  }
  int action=Release;
  if(t>=0&&i.edges[s][t].close)action=Attack;
  else if(fallback>=0){
   bool covered=covering[fallback]*100>=int64_t(i.targets[fallback].hp)*CoveragePercent;
   bool wait=future>=0&&covered&&i.targets[fallback].threatening<2&&i.sources[s].holdFrames<MaxHoldFrames&&!i.sources[s].releaseFrames;
   if(wait){action=Hold;t=future;}
   else{action=Attack;t=fallback;if(i.sources[s].cooldown<=3)covering[t]+=i.edges[s][t].damage;}
  }
  // Without a current in-range target, leave ordinary guard/search running.
  // A previously held source is explicitly woken if its reason disappears.
  actions[s]=action;assigned[s]=t;
 }
 // Establish covering attacks/wakes before holds. If the gateway exhausts
 // its budget, it must not put every spare gun to sleep before coverage is sent.
 for(int phase=0;phase<2;++phase)for(int s=0;s<i.count;++s){if(busy[s])continue;
  int t=assigned[s],action=actions[s];if((phase==1)!=(action==Hold))continue;
  if(action==Hold&&i.sources[s].held)continue;
  if(action==Release&&!i.sources[s].held)continue;
  if(action==Attack&&!i.sources[s].held&&i.sources[s].current==i.targets[t].id)continue;
  r.orders[r.count++]={s,t,action};
 }
 return r;
}
} }
