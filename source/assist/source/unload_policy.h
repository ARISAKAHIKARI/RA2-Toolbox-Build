#pragma once
#include <cstdint>
namespace assist { namespace unloadpolicy {
// Per-observation pointer lookup. Clear only occupied slots so a small selected
// roster does not zero a large table every admitted tick. The history itself
// retains its order and expiry/manual-token rules.
template<unsigned Capacity=8192> struct RosterLookup {
 static_assert(Capacity&&!(Capacity&(Capacity-1))&&Capacity<=16384,"bounded power-of-two roster required");
 static constexpr unsigned Slots=Capacity*2;
 struct Entry {uint32_t pointer=0;int index=0;};
 Entry entries[Slots]{};uint16_t occupied[Capacity]{};unsigned count=0;
 void clear(){for(unsigned n=0;n<count;++n)entries[occupied[n]]={};count=0;}
 unsigned slot(uint32_t pointer)const{
  uint32_t key=pointer;key^=key>>16;key*=0x7FEB352Du;key^=key>>15;key*=0x846CA68Bu;key^=key>>16;
  unsigned at=key&(Slots-1);
  for(unsigned n=0;n<Slots;++n,at=(at+1)&(Slots-1))if(!entries[at].pointer||entries[at].pointer==pointer)return at;
  return Slots;
 }
 int find(uint32_t pointer)const{if(!pointer)return -1;unsigned at=slot(pointer);return at<Slots&&entries[at].pointer?entries[at].index:-1;}
 bool add(uint32_t pointer,int index){
  if(!pointer)return false;unsigned at=slot(pointer);if(at==Slots)return false;
  if(!entries[at].pointer){if(count==Capacity)return false;occupied[count++]=uint16_t(at);}
  entries[at]={pointer,index};return true;
 }
};
// Carrier-keyed adjacency for candidate-only admission. Replaces sorting up
// to 32K passengers on the game thread with O(N) fixed-storage insertions.
// The mapping does not authorize a command; every passenger is still checked
// live before its native EventClass is queued.
template<unsigned Capacity=32768,unsigned Buckets=65536> struct CarrierGroups {
 static_assert(Capacity&&Buckets>=Capacity&&!(Buckets&(Buckets-1)),"power-of-two buckets required");
 static constexpr unsigned MaxProbe=64;
 struct Bucket {uint32_t carrier=0,epoch=0;int first=-1,last=-1;};
 Bucket buckets[Buckets]{};
 uint32_t epoch=1;
 bool valid=true;
 static unsigned hash(uint32_t p){p^=p>>16;p*=0x7feb352du;p^=p>>15;return p&(Buckets-1);}
 void clear(){
  if(++epoch==0){for(auto&v:buckets)v.epoch=0;epoch=1;}
  valid=true;
 }
 int first(uint32_t carrier)const{
  if(!carrier||!valid)return -1;
  unsigned at=hash(carrier);
  for(unsigned probe=0;probe<MaxProbe;++probe,at=(at+1)&(Buckets-1)){
   const auto&b=buckets[at];
   if(b.epoch!=epoch)return -1;
   if(b.carrier==carrier)return b.first;
  }
  return -1;
 }
 template<class Entry>bool add(uint32_t carrier,int index,Entry*entries){
  if(!carrier||!valid||index<0||index>=int(Capacity)||!entries){valid=false;return false;}
  unsigned at=hash(carrier);
  for(unsigned probe=0;probe<MaxProbe;++probe,at=(at+1)&(Buckets-1)){
   auto&b=buckets[at];
   if(b.epoch!=epoch){b={carrier,epoch,index,index};entries[index].next=-1;return true;}
   if(b.carrier==carrier){
    entries[index].next=-1;
    if(b.last>=0)entries[b.last].next=index;
    else b.first=index;
    b.last=index;return true;
   }
  }
  valid=false;return false; // UNKNOWN: do not publish partial enrolment.
 }
};
// An ejected passenger may briefly inherit an engine Move/Attack mission.
// For automatic ejection only, allow a short grace after landing, but never
// override explicit user Stop (13) or Enter (7).
constexpr int ScatterFrames=180;
inline bool completeIdentityIndex(int indexedFrame,int frame){return indexedFrame==frame;}
// YRpp Sequence::Deploy=27; Deployed/DeployedFire/DeployedIdle=28..30.
inline bool completedDeploy(int sequence){return sequence>=28&&sequence<=30;}
inline bool deployObserved(int sequence){return sequence>=27&&sequence<=30;}
inline bool initialMayContinue(int mission,int mode,int landedAge,bool enteringOldCarrier=false,bool ejectDeployer=false){
 if(mission==2&&(mode==0||ejectDeployer&&mode!=0))return landedAge>=0&&landedAge<=(mode==0?900:ScatterFrames);
 if(mission==0||mission==5||mission==6||mission==11)return true;
 if(mission==7&&enteringOldCarrier)return true;
 if(mission==13||mission==7||mission==16)return false;
 return mode!=0&&landedAge>=0&&landedAge<=8;
}
inline bool shouldKeepWatching(bool feature,bool carrierAlive,bool alreadyTracked){
 return feature&&(carrierAlive||alreadyTracked);
}
} }
