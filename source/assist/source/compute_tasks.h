#pragma once
#include "autoload.h"
#include <cstring>
#include "tactical_decisions.h"
#include "cooperative_types.h"
namespace assist { namespace compute {
struct QuotaEntry {char id[24]{};cooperative::Quota quota;};
struct QuotaConfig {cooperative::Quota defaults;int count=0;QuotaEntry entries[512];};
struct LoadInput {
 uint32_t match=0,tick=0;int frame=-1,ns=0,nv=0,mode=0,radius=30;
 loading::Infantry soldiers[loading::MaxInfantry];loading::Vehicle vehicles[loading::MaxVehicles];
 uint32_t tokens[loading::MaxInfantry]{};loading::Composition composition;
 uint8_t compatible[loading::MaxInfantry][loading::MaxVehicles]{};
};
// Runtime execution needs identities and source fields, never the assignment
// matrix or composition workspace. Keep this small observation in FrameHistory.
struct LoadObservation {
 uint32_t match=0,tick=0;int frame=-1,ns=0,nv=0,mode=0,radius=30;
 loading::Infantry soldiers[loading::MaxInfantry];loading::Vehicle vehicles[loading::MaxVehicles];
 uint32_t tokens[loading::MaxInfantry]{};
};
inline bool valid(const LoadObservation&i){return i.ns>=0&&i.ns<=loading::MaxInfantry&&i.nv>=0&&i.nv<=loading::MaxVehicles;}
inline void observe(LoadObservation&to,const LoadInput&from){
 to.match=from.match;to.tick=from.tick;to.frame=from.frame;to.ns=from.ns;to.nv=from.nv;to.mode=from.mode;to.radius=from.radius;
 if(!valid(to))return;
 std::memcpy(to.soldiers,from.soldiers,size_t(to.ns)*sizeof(to.soldiers[0]));
 std::memcpy(to.vehicles,from.vehicles,size_t(to.nv)*sizeof(to.vehicles[0]));
 std::memcpy(to.tokens,from.tokens,size_t(to.ns)*sizeof(to.tokens[0]));
}
struct LoadResult {uint32_t match=0,tick=0;int frame=-1,count=0;int assignment[loading::MaxInfantry]{};};
struct RepairCandidate {uint32_t id=0;decisions::RepairView view;};
struct RepairInput {uint32_t match=0,tick=0;int frame=-1,count=0;RepairCandidate items[8192];};
struct RepairResult {uint32_t match=0,tick=0;int frame=-1,count=0;uint32_t ids[8192]{};};
inline bool valid(const LoadInput&i){return i.ns>=0&&i.ns<=loading::MaxInfantry&&i.nv>=0&&i.nv<=loading::MaxVehicles;}
inline LoadResult load(LoadInput&i,loading::Workspace&work){LoadResult r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;
 if(!valid(i))return r;r.count=i.ns;
 loading::assign(i.soldiers,i.ns,i.vehicles,i.nv,i.mode,i.radius,i.composition,work,r.assignment,[&](const loading::Infantry&p,const loading::Vehicle&v){return i.compatible[&p-i.soldiers][&v-i.vehicles]!=0;});return r;
}
inline RepairResult repair(const RepairInput&i){RepairResult r;r.match=i.match;r.tick=i.tick;r.frame=i.frame;if(i.count<0||i.count>8192)return r;
 for(int n=0;n<i.count;++n)if(i.items[n].id&&decisions::shouldRepair(i.items[n].view))r.ids[r.count++]=i.items[n].id;return r;
}
} }
