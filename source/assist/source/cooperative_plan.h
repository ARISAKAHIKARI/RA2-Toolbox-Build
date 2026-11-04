#pragma once
#include "cooperative_types.h"
#include <algorithm>
#include <vector>
#include <numeric>
#include <cmath>
namespace assist { namespace cooperative {
struct Unit {Source source;uint32_t type;int x,y,slots;uint32_t reservedTarget=0;};
struct Target {uint32_t id,type;int x,y,free,used;Quota quota;int prior=0,hp=0;double priorDamage=0;int kind=-1;bool enemy=false,airborne=false;};
struct Input {int mode=Enter;uint32_t clicked=0;std::vector<Unit> units;std::vector<Target> targets;std::vector<uint8_t> compatible;std::vector<double> expectedDamage;int centerX=0,centerY=0;};
inline int64_t distance(int x,int y,int a,int b){int64_t dx=int64_t(x)-a,dy=int64_t(y)-b;if(dx< -2097152||dx>2097152||dy< -2097152||dy>2097152)return INT64_MAX;return dx*dx+dy*dy;}
inline bool validDamage(double value){return std::isfinite(value)&&value>0&&value<=MaximumExpectedDamage;}
// Both host and Runtime replan only the unprocessed suffix. Already accepted
// orders retain their SNAPSHOT estimate, even if ammo/weapon/HP changes later.
inline void reserveAccepted(Input&w,const std::vector<Assignment>&accepted){
 if(w.mode==Enter)return;
 for(auto&t:w.targets)for(const auto&o:accepted)if(o.target==t.id&&o.type==t.type){
  if(t.prior<MaxSources)++t.prior;
  if(damageMode(w.mode)&&validDamage(o.expectedDamage))t.priorDamage=std::min(MaximumExpectedDamage*MaxSources,t.priorDamage+o.expectedDamage);
 }
}
// The controller ranks only immutable input values. There is no game callback,
// game pointer dereference, native RNG, pathfinder or event submission here.
inline std::vector<Assignment> allocate(const Input&w){
 std::vector<Assignment> out;size_t ns=w.units.size(),nt=w.targets.size();
 if(ns>MaxSources||nt>MaxTargets||w.compatible.size()!=ns*nt||w.mode<Enter||w.mode>AirSingle)return out;
 // A repeated identity makes the compatibility matrix/capacity ambiguous.
 // Reject the snapshot rather than issuing twice or counting one target twice.
 for(size_t i=0;i<ns;++i)for(size_t k=0;k<i;++k)if(w.units[i].source.id&&w.units[i].source.id==w.units[k].source.id)return out;
 for(size_t j=0;j<nt;++j)for(size_t k=0;k<j;++k)if(w.targets[j].id&&w.targets[j].id==w.targets[k].id)return out;
 if(damageMode(w.mode)){
  if(w.expectedDamage.size()!=ns*nt||w.centerX< -1048576||w.centerX>1048576||w.centerY< -1048576||w.centerY>1048576)return out;
  std::vector<double> committed(nt);std::vector<int> degree(ns),order(ns);std::iota(order.begin(),order.end(),0);
  auto targetValid=[&](size_t j){const auto&t=w.targets[j];return t.id&&t.type&&t.hp>0&&t.enemy&&t.x>=-1048576&&t.x<=1048576&&t.y>=-1048576&&t.y<=1048576&&(w.mode==AirSingle?t.id==w.clicked:t.kind==2&&!t.airborne&&distance(w.centerX,w.centerY,t.x,t.y)<=int64_t(DamageRadius)*DamageRadius);};
  auto edgeValid=[&](size_t i,size_t j){return targetValid(j)&&w.compatible[i*nt+j]&&validDamage(w.expectedDamage[i*nt+j]);};
  for(size_t j=0;j<nt;++j){double prior=w.targets[j].priorDamage;if(!std::isfinite(prior)||prior<0||prior>MaximumExpectedDamage*MaxSources)return out;committed[j]=prior;}
  for(size_t i=0;i<ns;++i)for(size_t j=0;j<nt;++j)degree[i]+=edgeValid(i,j);
  std::sort(order.begin(),order.end(),[&](int a,int b){if(degree[a]!=degree[b])return degree[a]<degree[b];
   if(w.mode==AirSingle){double da=0,db=0;for(size_t j=0;j<nt;++j)if(w.targets[j].id==w.clicked){if(edgeValid(a,j))da=w.expectedDamage[size_t(a)*nt+j];if(edgeValid(b,j))db=w.expectedDamage[size_t(b)*nt+j];break;}if(da!=db)return da>db;}
   return w.units[a].source.id<w.units[b].source.id;});
  for(int i:order){const auto&u=w.units[i];if(!u.source.id||u.x< -1048576||u.x>1048576||u.y< -1048576||u.y>1048576)continue;
   int best=-1;bool bestCovered=true;double bestCoverage=0;int64_t bestDistance=INT64_MAX;
   for(size_t j=0;j<nt;++j){if(!edgeValid(i,j))continue;const auto&t=w.targets[j];
    // Allow the shot that crosses 120%; only later aircraft prefer another
    // attackable uncovered target. Covered targets remain a final fallback.
    bool covered=committed[j]*5>=double(t.hp)*6;if(covered&&w.mode==AirSingle)continue;double coverage=committed[j]/t.hp;int64_t d=distance(u.x,u.y,t.x,t.y);
    bool clicked=t.id==w.clicked,oldClicked=best>=0&&w.targets[best].id==w.clicked;
    bool betterTie=(clicked&&!oldClicked)||(clicked==oldClicked&&(d<bestDistance||(d==bestDistance&&t.id<(best>=0?w.targets[best].id:UINT32_MAX))));
    if(best<0||(!covered&&bestCovered)||(covered==bestCovered&&((covered&&coverage<bestCoverage)||((!covered||coverage==bestCoverage)&&betterTie)))){best=int(j);bestCovered=covered;bestCoverage=coverage;bestDistance=d;}
   }
   if(best<0)continue;const auto&t=w.targets[best];double damage=w.expectedDamage[size_t(i)*nt+best];committed[best]+=damage;out.push_back({u.source.id,u.source.token,t.id,t.type,damage});
  }
  return out;
 }
 std::vector<int> order(ns),reserved(nt),counts(nt),degree(ns);std::iota(order.begin(),order.end(),0);
 for(size_t j=0;j<nt;++j){if(w.targets[j].prior<0||w.targets[j].prior>MaxSources)return out;counts[j]=w.mode==Enter?0:w.targets[j].prior;}
 for(size_t i=0;i<ns;++i)for(size_t j=0;j<nt;++j)degree[i]+=w.compatible[i*nt+j]!=0;
 std::sort(order.begin(),order.end(),[&](int a,int b){if(w.mode==Enter&&w.units[a].slots!=w.units[b].slots)return w.units[a].slots>w.units[b].slots;if(degree[a]!=degree[b])return degree[a]<degree[b];return w.units[a].source.id<w.units[b].source.id;});
 for(int i:order){const auto&u=w.units[i];if(!u.source.id||degree[i]==0||u.x< -1048576||u.x>1048576||u.y< -1048576||u.y>1048576||(w.mode==Enter&&(u.slots<=0||u.slots>128)))continue;
  // A native click may already have radio-reserved this tank's bunker. That
  // place is included in used, not free: retain its owner without opening it
  // to another unit or consuming the same place a second time.
  bool retained=false;if(w.mode==Enter&&u.reservedTarget)for(size_t j=0;j<nt;++j){const auto&t=w.targets[j];
   if(t.id==u.reservedTarget&&t.used>=u.slots&&w.compatible[size_t(i)*nt+j]){out.push_back({u.source.id,u.source.token,t.id,t.type});retained=true;break;}}
  if(retained)continue;
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
