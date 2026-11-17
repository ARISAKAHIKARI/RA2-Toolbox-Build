#pragma once
#include <cstdint>

// A planner's result is never a permission to issue an EventClass. Only the
// native game dispatcher can verify these immutable capture-time identities.
// No roster traversal, allocations, OS reads or engine calls belong here.
namespace assist { namespace executionproof {
struct Actor {
 uint32_t id=0,pointer=0,manualVersion=0;
};
struct Identity {
 uint32_t id=0,pointer=0;
};
struct Ticket {
 uint32_t match=0,house=0,createdTick=0;
 Actor actor{};
 Identity target{},destination{};
};
struct Context {uint32_t match=0,house=0,now=0;};
enum class Failure:uint8_t {
 None, WrongMatch, WrongHouse, Expired, InvalidActor, ManualTakeover,
 TargetChanged, DestinationChanged, WrongOrder
};
constexpr uint32_t MaxAgeMs=3500;
inline Failure ordered(const Ticket&t,uint32_t actor,uint32_t target,uint8_t targetTag,
                       uint32_t destination,uint8_t destinationTag){
 if(t.actor.id!=actor)return Failure::WrongOrder;
 if(t.target.id&&(targetTag!=0x34||t.target.id!=target))return Failure::WrongOrder;
 if(t.destination.id&&(destinationTag!=0x34||t.destination.id!=destination))return Failure::WrongOrder;
 return Failure::None;
}
// Live lookup is supplied by Runtime's worldByID (including guarded UID
// re-read); owner and manual token come from the current Game thread. The
// target is NOT presumed friendly: individual missions apply target policy.
template<class Lookup,class Version,class Own>
inline Failure verify(const Ticket&t,Context now,Lookup lookup,Version version,Own own){
 if(!t.match||t.match!=now.match)return Failure::WrongMatch;
 if(!t.house||t.house!=now.house)return Failure::WrongHouse;
 if(t.createdTick&&uint32_t(now.now-t.createdTick)>MaxAgeMs)return Failure::Expired;
 if(!t.actor.id||t.actor.pointer<0x10000||lookup(t.actor.id)!=t.actor.pointer||
    !own(t.actor.pointer,t.house))return Failure::InvalidActor;
 if(version(t.actor.id)!=t.actor.manualVersion)return Failure::ManualTakeover;
 if(t.target.id&&
    (t.target.pointer<0x10000||lookup(t.target.id)!=t.target.pointer))
  return Failure::TargetChanged;
 if(t.destination.id&&
    (t.destination.pointer<0x10000||lookup(t.destination.id)!=t.destination.pointer))
  return Failure::DestinationChanged;
 return Failure::None;
}
}} // namespace assist::executionproof
