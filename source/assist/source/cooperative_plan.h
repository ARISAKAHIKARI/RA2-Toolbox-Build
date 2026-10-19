#pragma once
#include "cooperative_types.h"
#include <algorithm>
#include <vector>
#include <numeric>
namespace assist { namespace cooperative {
struct Unit {Source source;uint32_t type;int x,y,slots;};
struct Target {uint32_t id,type;int x,y,free,used;Quota quota;int prior=0;};
struct Input {int mode=Enter;uint32_t clicked=0;std::vector<Unit> units;std::vector<Target> targets;std::vector<uint8_t> compatible;};
inline int64_t distance(int x,int y,int a,int b){int64_t dx=int64_t(x)-a,dy=int64_t(y)-b;if(dx< -2097152||dx>2097152||dy< -2097152||dy>2097152)return INT64_MAX;return dx*dx+dy*dy;}
// The controller ranks only immutable input values. There is no game callback,
// game pointer dereference, native RNG, pathfinder or event submission here.
inline std::vector<Assignment> allocate(const Input&w){
 std::vector<Assignment> out;size_t ns=w.units.size(),nt=w.targets.size();
 if(ns>MaxSources||nt>MaxTargets||w.compatible.size()!=ns*nt||w.mode<Enter||w.mode>AirSecond)return out;
 // A repeated identity makes the compatibility matrix/capacity ambiguous.
 // Reject the snapshot rather than issuing twice or counting one target twice.
 for(size_t i=0;i<ns;++i)for(size_t k=0;k<i;++k)if(w.units[i].source.id&&w.units[i].source.id==w.units[k].source.id)return out;
 for(size_t j=0;j<nt;++j)for(size_t k=0;k<j;++k)if(w.targets[j].id&&w.targets[j].id==w.targets[k].id)return out;
 std::vector<int> order(ns),reserved(nt),counts(nt),degree(ns);std::iota(order.begin(),order.end(),0);
 for(size_t j=0;j<nt;++j){if(w.targets[j].prior<0||w.targets[j].prior>MaxSources)return out;counts[j]=w.mode==Enter?0:w.targets[j].prior;}
 for(size_t i=0;i<ns;++i)for(size_t j=0;j<nt;++j)degree[i]+=w.compatible[i*nt+j]!=0;
 std::sort(order.begin(),order.end(),[&](int a,int b){if(w.mode==Enter&&w.units[a].slots!=w.units[b].slots)return w.units[a].slots>w.units[b].slots;if(degree[a]!=degree[b])return degree[a]<degree[b];return w.units[a].source.id<w.units[b].source.id;});
 for(int i:order){const auto&u=w.units[i];if(!u.source.id||degree[i]==0||u.x< -1048576||u.x>1048576||u.y< -1048576||u.y>1048576||(w.mode==Enter&&(u.slots<=0||u.slots>128)))continue;
  int best=-1;bool bestPreferred=false;int bestCount=0;int64_t bestDistance=INT64_MAX;
  for(size_t j=0;j<nt;++j){const auto&t=w.targets[j];if(!t.id||t.x< -1048576||t.x>1048576||t.y< -1048576||t.y>1048576||!w.compatible[size_t(i)*nt+j])continue;
   if(w.mode==Enter&&(t.free<0||int64_t(reserved[j])+u.slots>t.free))continue;
   auto quota=bounded(t.quota);if(w.mode!=Enter&&quota.maximum&&counts[j]>=quota.maximum)continue;
   int preferred=w.mode==AirSecond?quota.second:quota.first;bool before=w.mode!=Enter&&counts[j]<preferred;
   int n=w.mode==Enter?(u.slots>1?t.free-reserved[j]-u.slots:0):counts[j];int64_t d=distance(u.x,u.y,t.x,t.y);
   bool clicked=t.id==w.clicked,oldClicked=best>=0&&w.targets[best].id==w.clicked;
   if(best<0||(before&&!bestPreferred)||(before==bestPreferred&&(n<bestCount||(n==bestCount&&((clicked&&!oldClicked)||(clicked==oldClicked&&(d<bestDistance||(d==bestDistance&&t.id<w.targets[best].id)))))))){best=int(j);bestPreferred=before;bestCount=n;bestDistance=d;}
  }
  if(best<0)continue;const auto&t=w.targets[best];if(w.mode==Enter)reserved[best]+=u.slots;++counts[best];out.push_back({u.source.id,u.source.token,t.id,t.type});
 }
 return out;
}
} }
