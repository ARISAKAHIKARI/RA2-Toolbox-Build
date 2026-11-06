#pragma once
#include "weapon_policy.h"
#include <algorithm>
#include <cmath>
namespace assist { namespace minerdefense {
constexpr int MaxExtraCells=30;
inline bool position(const planner::Actor&a){return a.x>=0&&a.x<512*256&&a.y>=0&&a.y<512*256&&a.z>=-1048576&&a.z<=1048576;}
inline bool miner(const planner::Actor&a){
 return a.id&&a.kind==2&&position(a)&&(a.flags&planner::Own)&&(a.flags&planner::Harvester)&&
  !(a.flags&(planner::Disabled|planner::Transported|planner::Limbo|planner::OffMap|planner::Airborne|planner::Manipulated|planner::Infested|planner::OrePaused));
}
inline bool threat(const planner::Actor&a){
 return a.id&&a.kind==2&&position(a)&&(a.flags&planner::Enemy)&&(a.flags&planner::ParasiteThreat)&&
  !(a.flags&(planner::Transported|planner::Cloaked|planner::Limbo|planner::OffMap|planner::Airborne|planner::Protected|planner::Manipulated));
}
inline int64_t distance(const planner::Actor&a,const planner::Actor&b){
 const int64_t x=int64_t(a.x)-b.x,y=int64_t(a.y)-b.y,z=int64_t(a.z)-b.z;return x*x+y*y+z*z;
}
// Match the existing raw weapon-selection policy, including the actual selected
// primary/secondary/elite weapon and armor eligibility. Native Attack owns any
// approach or minimum-range reposition; automation never writes a fire timer.
inline bool inRange(const planner::Actor&s,const planner::Actor&t,int extraCells,bool retaining=false){
 if(!miner(s)||!threat(t)||!planner::compatibleTarget(s,t,false))return false;
 const auto&w=s.weapons[planner::weaponIndex(s,t)];
 const int64_t reach=int64_t(w.range)+(std::clamp(extraCells,0,MaxExtraCells)+(retaining?1:0))*256;
 return reach>0&&reach<65536&&distance(s,t)<=reach*reach;
}
inline bool queuedWork(int mission,int queued){return queued==-1||queued==mission||queued==10||queued==12;}
inline bool workingMission(int mission){return mission==10||mission==12||mission==16||mission==7;}
inline bool passiveMission(int mission){return mission==5||mission==10||mission==12||mission==16||mission==7;}
}}
