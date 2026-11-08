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
// An ejected passenger may briefly inherit an engine Move/Attack mission.
// For automatic ejection only, allow a short grace after landing, but never
// override explicit user Stop (13) or Enter (7).
constexpr int ScatterFrames=180;
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
