#pragma once
#include "protocol.h"

namespace assist { namespace commandsampling {
enum class Sample { Exact, LocalCandidate, Missing };

inline bool selectionBound(int command){
 return command==SmartUnload||command==SelectVariant||command==StickyGuard||
        command==CancelAdvanced||(command>=SetSleep&&command<=SetHarmless);
}
inline bool canWait(int command){
 return command!=OpenTech&&command!=AttackOwnGround&&command!=DestructPlayer&&
        command!=PlaceCompletedBuilding;
}
inline bool canAttempt(int command,Sample sample){
 if(sample==Sample::Exact)return true;
 switch(command){
 case Repair: case AutoBoard: // Intake only. Each runs its own admission later.
 case Cheer: case Taunt: case ClearBeacons:
 case SelectSuper: case SelectIron: case SelectChrono:
 case ProduceDeferred: case Produce: case CancelTechProduction:
 case SwitchPlayer: case RestorePlayer: // Currently return unavailable.
  return true;
 case SmartUnload: case ProduceReserved: case AirPathMove:
  return sample==Sample::LocalCandidate;
 default:
  return false;
 }
}

struct SelectionMember {uint32_t pointer=0,id=0,manual=0;};
struct SelectionReceipt {
 static constexpr int Capacity=64;
 SelectionMember members[Capacity]{};
 uint32_t serial=0;
 int count=-1;
 void clear(){count=-1;}
 bool matches(const SelectionMember* live,int size)const{
  if(size<0||size!=count)return false;
  for(int i=0;i<size;++i){
   const auto& a=members[i];const auto& b=live[i];
   if(a.pointer!=b.pointer||a.id!=b.id||a.manual!=b.manual)return false;
  }
  return true;
 }
 uint32_t capture(const SelectionMember* live,int size){
  if(size<0||size>Capacity)return 0;
  if(matches(live,size))return serial;
  for(int i=0;i<size;++i)members[i]=live[i];
  count=size;
  if(++serial>0x7fffffffu)serial=1;
  return serial;
 }
};
}}
