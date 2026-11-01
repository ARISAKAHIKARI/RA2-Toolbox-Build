#pragma once
#include <algorithm>
#include <cstdlib>
#include "weapon_policy.h"
namespace assist { namespace combatquery {
struct Choice {int index=-1,slot=-1;bool close=false;};
// Existing supported raw weapon-selection policy. This is an approximation of
// the engine's live selector, not a reproduction of MOD virtual methods.
inline Choice choose(const planner::Actor&source,const planner::Actor&target){
 Choice out{};const int index=planner::weaponIndex(source,target);
 if(index<0||index>1||target.armor<0||target.armor>10)return out;
 const auto&w=source.weapons[index];
 if(w.slot<0||w.slot>=18||w.range<=0||w.range>=65536||w.minimum<0||w.minimum>w.range)return out;
 const int64_t dx=int64_t(source.x)-target.x,dy=int64_t(source.y)-target.y;
 const int64_t dz=(source.flags&planner::Airborne)?0:int64_t(source.z)-target.z;
 if(std::abs(dx)>1048576||std::abs(dy)>1048576||std::abs(dz)>1048576)return out;
 const int64_t range=int64_t(w.range)+((target.flags&planner::Airborne)?source.airRangeBonus:0);
 if(range<=0||range>=65536)return out;
 out.index=index;out.slot=w.slot;
 const int64_t distance=dx*dx+dy*dy+dz*dz;
 out.close=distance<=range*range&&distance>=int64_t(w.minimum)*w.minimum;
 return out;
}
}}
