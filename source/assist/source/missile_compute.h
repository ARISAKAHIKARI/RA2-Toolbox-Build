#pragma once
#include <cstdint>
#include <climits>
namespace assist { namespace missilecompute {
constexpr int MaxSources=64,MaxTargets=64,MaxOrders=8;
struct Source {uint32_t id=0,pointer=0,token=0,current=0,pending=0;};
struct Target {uint32_t id=0,pointer=0;int hp=0;uint32_t threatening=0;};
// Range/weapon legality is sampled through the original virtual methods by
// Runtime. The pure allocator never guesses a planar or spherical AA radius.
struct Edge {int damage=0;uint32_t distance=0;};
struct Input {uint32_t match=0,tick=0;int frame=-1,count=0,targetCount=0;
 Source sources[MaxSources];Target targets[MaxTargets];Edge edges[MaxSources][MaxTargets];};
struct Assignment {int source=0,target=0;};
struct Result {uint32_t match=0,tick=0;int frame=-1,count=0;Assignment orders[MaxOrders];};
inline bool valid(const Input&i){return i.frame>=0&&i.count>=0&&i.count<=MaxSources&&i.targetCount>=0&&i.targetCount<=MaxTargets;}
inline Result calculate(const Input&i){
 Result r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(!valid(i))return r;
 int64_t assigned[MaxTargets]{};bool used[MaxSources]{};
 // Keep existing locks first, up to one estimated volley of coverage. A
 // pending native event reserves its shooter until observed or timed out.
 for(int s=0;s<i.count;++s){const auto&a=i.sources[s];uint32_t target=a.pending?a.pending:a.current;
  for(int t=0;t<i.targetCount;++t)if(target==i.targets[t].id&&i.edges[s][t].damage>0){
   if(a.pending||assigned[t]<i.targets[t].hp){assigned[t]+=i.edges[s][t].damage;used[s]=true;}break;
  }
  if(a.pending)used[s]=true;
 }
 for(int s=0;s<i.count&&r.count<MaxOrders;++s){if(used[s])continue;int best=-1;
  for(int t=0;t<i.targetCount;++t){const auto&e=i.edges[s][t];const auto&target=i.targets[t];
   if(e.damage<=0||target.hp<=0||assigned[t]>=target.hp)continue;
   if(best<0||target.threatening>i.targets[best].threatening||
      (target.threatening==i.targets[best].threatening&&(assigned[t]<assigned[best]||
       (assigned[t]==assigned[best]&&(e.distance<i.edges[s][best].distance||
        (e.distance==i.edges[s][best].distance&&target.id<i.targets[best].id))))))best=t;
  }
  if(best<0)continue;assigned[best]+=i.edges[s][best].damage;
  if(i.sources[s].current!=i.targets[best].id)r.orders[r.count++]={s,best};
 }
 return r;
}
} }
