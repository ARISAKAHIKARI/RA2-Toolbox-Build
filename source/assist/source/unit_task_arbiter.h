#pragma once
#include <stdint.h>
// Conservative per-simulation-frame ownership. A successful original EventClass
// for an actor blocks another automatic EventClass in that SAME game frame.
// This intentionally is not a long-lived lease and does not intercept the
// player's own native orders. Feature-specific follow-up remains responsible
// for longer task cancellation, business completion and manual intervention.
namespace assist {namespace taskarbiter {
enum class Owner : uint8_t {
 Generic, AutoLoad, Ore, Unload, Cooperative, Combat, Magnetron,
 Repair, Sell, Production, SuperWeapon, MissileDefense, BurstRefresh, Dodge, Formation, MagnetronDrop, HarvesterDefense, BombBoard
};
// Synthetic actor identities for player-level events (Production / SuperWeapon)
// so they can participate in unified gateway validation and same-frame arbitration.
constexpr uint32_t syntheticProductionActor(int kind,int index){
 return 0x80000000u|((uint32_t(kind)&0xFFu)<<16)|(uint32_t(index)&0xFFFFu);
}
constexpr uint32_t syntheticSuperWeaponActor(int swIndex){
 return 0x90000000u|(uint32_t(swIndex)&0xFFFFu);
}
struct Claim {uint32_t actor=0,manualVersion=0;Owner owner=Owner::Generic;};
template<int Capacity=8192> struct FrameArbiter {
 Claim actors[Capacity]{};int count=0,frame=-1;uint32_t match=0;
 void clear(){count=0;frame=-1;match=0;}
 void begin(uint32_t m,int f){if(match!=m||frame!=f){count=0;match=m;frame=f;}}
 bool canIssue(uint32_t actor,uint32_t revision)const{
  if(!actor||count>=Capacity)return false;
  for(int i=0;i<count;++i)if(actors[i].actor==actor)return false;
  return true;
 }
 bool record(uint32_t actor,uint32_t revision,Owner owner){
  if(!canIssue(actor,revision))return false;
  actors[count++]={actor,revision,owner};return true;
 }
};
// Game-thread only. Lease prevents competing autonomous planners from stealing
// an actor across frames. It is NOT a replacement for per-frame event gating.
// Revisions and raw object pointers guard against manual takeover and actor-ID
// reuse. A timeout avoids permanent locks when observation is unavailable.
struct Lease {
 uint32_t actor=0,object=0,manualVersion=0,match=0,serial=0;
 int issuedFrame=0; Owner owner=Owner::Generic;
};
constexpr int LeaseDurationFrames=45;
inline int priority(Owner o){
 switch(o){
 case Owner::Sell:return 8;
 case Owner::MagnetronDrop:return 7; // explicit global hazard-release policy
 case Owner::Formation:case Owner::Generic:case Owner::SuperWeapon:case Owner::Production:return 6;
 case Owner::Dodge:case Owner::Unload:case Owner::BombBoard:return 5;
 case Owner::Magnetron:case Owner::HarvesterDefense:return 4;
 case Owner::BurstRefresh:case Owner::Combat:case Owner::MissileDefense:return 4;
 case Owner::Cooperative:return 3;
 case Owner::AutoLoad:case Owner::Repair:return 2;
 case Owner::Ore:return 1;
 }
 return 0;
}
template<int Capacity=8192> struct LeaseArbiter {
 Lease leases[Capacity]{};int count=0;
 // Cleanup is bookkeeping; command admission independently checks live
 // actor/pointer/manual token and TTL. Never scan all 8K leases in one frame.
 static constexpr unsigned MaxSweepPerFrame=256;
 int sweepCursor=0,lastSweepFrame=0;
 uint32_t lastSweepMatch=0;
 bool sweepStamped=false;
 void clear(){count=0;sweepCursor=0;sweepStamped=false;}
 int find(uint32_t actor)const{for(int i=0;i<count;++i)if(leases[i].actor==actor)return i;return -1;}
 void remove(int i){
  if(i>=0&&i<count){
   leases[i]=leases[--count];
   if(sweepCursor>=count)sweepCursor=0;
  }
 }
 bool current(const Lease& l,uint32_t m,int frame,uint32_t object,uint32_t revision)const{
  return l.match==m&&l.object==object&&l.manualVersion==revision&&
   frame>=l.issuedFrame&&frame-l.issuedFrame<=LeaseDurationFrames;
 }
 bool canIssue(uint32_t m,int frame,uint32_t actor,uint32_t object,uint32_t revision,Owner owner)const{
  if(!actor||!object)return false;
  int i=find(actor);if(i<0)return count<Capacity;
  const Lease &l=leases[i];if(!current(l,m,frame,object,revision))return true;
  return l.owner==owner||priority(owner)>priority(l.owner);
 }
 // Commit only AFTER the original EventClass was actually enqueued.
 bool record(uint32_t m,int frame,uint32_t actor,uint32_t object,uint32_t revision,Owner owner,uint32_t serial,Lease* replaced=nullptr){
  if(!canIssue(m,frame,actor,object,revision,owner))return false;
  int i=find(actor);if(replaced)*replaced={};
  if(i<0){if(count>=Capacity)return false;i=count++;}
  else if(replaced&&current(leases[i],m,frame,object,revision)&&leases[i].owner!=owner)
   *replaced=leases[i];
  leases[i]={actor,object,revision,m,serial,frame,owner};return true;
 }
 void release(uint32_t match,uint32_t actor,uint32_t serial){
  int i=find(actor);if(i>=0&&leases[i].match==match&&serial&&leases[i].serial==serial)remove(i);
 }
 void manual(uint32_t actor){remove(find(actor));}
 template<class Observe> void sweep(uint32_t match,int frame,Observe observation){
  for(int i=0;i<count;){auto &l=leases[i];
   // observation returns (current object pointer, manual version), or zero
   // pointer when the actor is no longer valid in this world snapshot.
   auto state=observation(l.actor);
   if(!current(l,match,frame,state.first,state.second))remove(i);else ++i;
  }
 }
 // One bounded pass per unique simulation frame. Timed-out or manually
 // superseded leases are also rejected directly by canIssue()/record(), so
 // deferring cleanup cannot authorize a stale order or steal manual control.
 // 8,192 slots are covered within 32 frames at the default 256-check budget,
 // shorter than the 45-frame lease lifetime.
 template<class Observe> void sweepBudgeted(uint32_t match,int frame,Observe observation,
                                             unsigned budget=MaxSweepPerFrame){
  if(sweepStamped&&lastSweepMatch==match&&lastSweepFrame==frame)return;
  sweepStamped=true;lastSweepMatch=match;lastSweepFrame=frame;
  const unsigned limit=budget<unsigned(count)?budget:unsigned(count);
  for(unsigned visited=0;count&&visited<limit;++visited){
   if(sweepCursor>=count)sweepCursor=0;
   const int i=sweepCursor;
   const auto state=observation(leases[i].actor);
   if(!current(leases[i],match,frame,state.first,state.second))remove(i);
   else if(++sweepCursor>=count)sweepCursor=0;
  }
 }
};

}} // namespace assist::taskarbiter
