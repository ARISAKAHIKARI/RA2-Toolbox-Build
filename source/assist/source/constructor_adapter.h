#pragma once
#include "command_intent.h"
// Pure serialization adapter. Calls injected ABI invoker only. It cannot
// enqueue, send, affect simulation state or observe objects.
namespace assist { namespace constructors {
enum class Verdict : uint8_t { Ready, ProbeOnly, InvalidPlayer, InvalidFields, UnknownAddress };
inline Verdict admit(const intent::EventOrder&r,bool diagnosticProbe=false){
 if(!vanilla::address(r.domain))return Verdict::UnknownAddress;
 if(r.player<0||r.player>=32)return Verdict::InvalidPlayer;
 const bool planningConnect=r.domain==vanilla::Domain::Waypoints&&r.eventKind==vanilla::EventType::PlanConnect;
 if(vanilla::exposure(r.domain)==vanilla::Exposure::ProbeOnly&&!diagnosticProbe&&!planningConnect&&!(r.domain==vanilla::Domain::NoOptions&&(r.eventKind==vanilla::EventType::AllCheer||r.eventKind==vanilla::EventType::PlanCommit||r.eventKind==vanilla::EventType::Destruct)))return Verdict::ProbeOnly;
 if(r.appendWaypoint&&r.domain!=vanilla::Domain::MegaMission)return Verdict::InvalidFields;
 switch(r.domain){
 case vanilla::Domain::Object:
  if(r.a<=0 || r.tagA!=vanilla::TargetTag::Techno ||
     (r.eventKind!=vanilla::EventType::Idle&&r.eventKind!=vanilla::EventType::Scatter&&r.eventKind!=vanilla::EventType::Repair&&r.eventKind!=vanilla::EventType::Sell))
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
   : (r.eventKind==vanilla::EventType::Produce||r.eventKind==vanilla::EventType::Abandon);
  if(r.a<0||r.b<0||(r.c!=0&&r.c!=1)||!validProdKind)return Verdict::InvalidFields;
  break;
 }
 case vanilla::Domain::SuperWeaponPlace:
  if(r.a<0||r.a>=256||r.eventKind!=vanilla::EventType::SpecialPlace||!vanilla::validCell(r.cell))
   return Verdict::InvalidFields;
  break;
 case vanilla::Domain::Waypoints:
  if(!diagnosticProbe&&(r.eventKind!=vanilla::EventType::PlanConnect||r.a<=0||r.tagA!=vanilla::TargetTag::Techno||r.tagB!=vanilla::TargetTag::Cell||r.b<=0||r.b%1000<=0||r.b%1000>=512||r.b/1000<=0||r.b/1000>=512))return Verdict::InvalidFields;
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
 if(r.appendWaypoint&&r.domain!=vanilla::Domain::MegaMission)return Verdict::InvalidFields;
 switch(r.domain){
 case vanilla::Domain::Object:
  invoke(vanilla::address(r.domain),self,r.player,r.eventKind,r.a,int(r.tagA));break;
 case vanilla::Domain::MegaMission:
  invoke(vanilla::address(r.domain),self,r.player,r.a,int(r.tagA),r.mission,
   r.b,int(r.tagB),r.c,int(r.tagC),r.d,int(r.nearestTag));
  // Same local packet tag set by native Planning::Add (637DD0), after ctor.
  if(r.appendWaypoint)packet.payload[0x1D-7]=1;break;
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

