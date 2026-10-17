#pragma once
#include "command_intent.h"
// Pure serialization adapter. Calls injected ABI invoker only. It cannot
// enqueue, send, affect simulation state or observe objects.
namespace assist { namespace constructors {
enum class Verdict : uint8_t { Ready, ProbeOnly, InvalidPlayer, InvalidFields, UnknownAddress };
inline Verdict admit(const intent::EventOrder&r,bool diagnosticProbe=false){
 if(!vanilla::address(r.domain))return Verdict::UnknownAddress;
 if(r.player<0||r.player>=32)return Verdict::InvalidPlayer;
 if(vanilla::exposure(r.domain)==vanilla::Exposure::ProbeOnly&&!diagnosticProbe)return Verdict::ProbeOnly;
 switch(r.domain){
 case vanilla::Domain::Object:
  if(r.a<=0 || r.tagA!=vanilla::TargetTag::Techno ||
     (r.eventKind!=vanilla::EventType::Repair&&r.eventKind!=vanilla::EventType::Sell))
   return Verdict::InvalidFields;
  break;
 case vanilla::Domain::MegaMission:
  if(r.a<=0||r.tagA!=vanilla::TargetTag::Techno||r.mission<0||r.mission>255||
     !vanilla::validTargetRef(r.b,r.tagB)||!vanilla::validTargetRef(r.c,r.tagC)||
     !vanilla::validTargetRef(r.d,r.nearestTag))
   return Verdict::InvalidFields;
  break;
 case vanilla::Domain::Production: {
  const bool validProdKind=diagnosticProbe
   ? (r.eventKind==vanilla::EventType::Produce||r.eventKind==vanilla::EventType::Suspend||
      r.eventKind==vanilla::EventType::Abandon||r.eventKind==vanilla::EventType::AbandonAll)
   : (r.eventKind==vanilla::EventType::Produce);
  if(r.a<0||r.b<0||(r.c!=0&&r.c!=1)||!validProdKind)return Verdict::InvalidFields;
  break;
 }
 case vanilla::Domain::SuperWeaponPlace:
  if(r.a<0||r.a>=256||r.eventKind!=vanilla::EventType::SpecialPlace||!vanilla::validCell(r.cell))
   return Verdict::InvalidFields;
  break;
 default:
  if(r.eventKind<0||r.eventKind>255)return Verdict::InvalidFields;
 }
 return Verdict::Ready;
}
// Invoke signature: invoke(uint32_t address, uint32_t self, args...).
// A test double can capture arguments; production uses __thiscall on x86.
template<class Invoke>
inline Verdict fill(vanilla::Packet&packet,const intent::EventOrder&r,Invoke invoke,bool diagnosticProbe=false){
 const auto result=admit(r,diagnosticProbe);if(result!=Verdict::Ready)return result;
 const uint32_t self=uint32_t(reinterpret_cast<uintptr_t>(&packet));
 switch(r.domain){
 case vanilla::Domain::Object:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a,int(r.tagA));break;
 case vanilla::Domain::MegaMission:
  invoke(vanilla::address(r.domain),self,r.player,r.a,int(r.tagA),r.mission,
   r.b,int(r.tagB),r.c,int(r.tagC),r.d,int(r.nearestTag));break;
 case vanilla::Domain::Production:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a,r.b,r.c);break;
 case vanilla::Domain::SellCell:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,
   const_cast<vanilla::Cell*>(&r.cell));break;
 case vanilla::Domain::NoOptions:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind);break;
 case vanilla::Domain::PlayerBased:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a);break;
 case vanilla::Domain::Waypoints:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a,char(r.tagA),r.b,char(r.tagB));break;
 case vanilla::Domain::Place:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a,r.b,r.c,
   const_cast<vanilla::Cell*>(&r.cell));break;
 case vanilla::Domain::SuperWeaponPlace:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a,
   const_cast<vanilla::Cell*>(&r.cell));break;
 }
 return Verdict::Ready;
}
}} // assist::constructors
