#pragma once
#include <stdint.h>
#include "vanilla_constructor_catalog.h"
// Algorithm output. No Windows handle, game pointers, hooks or fixed addresses.
// Producer/Planner writes an intent; the game-thread Entry validates it and
// alone decides which native constructor and queue to use.
namespace assist { namespace intent {
struct EventOrder {
 vanilla::Domain domain=vanilla::Domain::Object;
 int32_t player=0,eventKind=0;
 int32_t a=0,b=0,c=0,d=0,mission=0;
 uint8_t tagA=0,tagB=0,tagC=0,nearestTag=0;
 vanilla::Cell cell{};
 bool appendWaypoint=false; // native MegaMission planning byte: append (1), never execute (2)
};
inline EventOrder object(int player,int type,uint32_t id,uint8_t tag=0x34){
 EventOrder r{};r.domain=vanilla::Domain::Object;r.player=player;r.eventKind=type;r.a=int32_t(id);r.tagA=tag;return r;
}
inline EventOrder produce(int player,int eventType,int abstractType,int index,int naval){
 EventOrder r{};r.domain=vanilla::Domain::Production;r.player=player;r.eventKind=eventType;r.a=abstractType;r.b=index;r.c=naval;return r;
}
inline EventOrder superWeapon(int player,int eventType,int swIndex,vanilla::Cell cell){
 EventOrder r{};r.domain=vanilla::Domain::SuperWeaponPlace;r.player=player;r.eventKind=eventType;r.a=swIndex;r.cell=cell;return r;
}
inline EventOrder placeBuilding(int player,int typeIndex,int naval,vanilla::Cell cell){
 EventOrder r{};r.domain=vanilla::Domain::Place;r.player=player;r.eventKind=vanilla::EventType::Place;
 r.a=7;r.b=typeIndex;r.c=naval;r.cell=cell;return r;
}
inline EventOrder mega(int player,uint32_t actor,int mission,uint32_t target,uint8_t targetTag,
 uint32_t destination,uint8_t destinationTag,uint32_t nearest,uint8_t nearestTag){
 EventOrder r{};r.domain=vanilla::Domain::MegaMission;r.player=player;r.a=int32_t(actor);r.tagA=0x34;
 r.mission=mission;r.b=int32_t(target);r.tagB=targetTag;r.c=int32_t(destination);
 r.tagC=destinationTag;r.d=int32_t(nearest); /* nearestTag must be preserved */
 // nearestTag is a distinct packing byte, not the same field as destTag.
 r.nearestTag=nearestTag;return r;
}
}} // assist::intent
