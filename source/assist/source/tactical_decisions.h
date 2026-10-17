#pragma once
#include "command_intent.h"
// Pure algorithm: it consumes copies of sampled data, not game pointers.
// No Windows API, engine address, queue, hook, game-state mutation or timing.
namespace assist { namespace decisions {
struct RepairView {
 int hp=0,maxHp=0;
 bool active=false,alreadyRepairing=false,issuedRecently=false;
};
inline bool shouldRepair(const RepairView&v){
 return v.active&&!v.alreadyRepairing&&!v.issuedRecently&&v.hp>0&&v.hp<v.maxHp;
}
constexpr bool canProduceInSession(bool multiplayer,bool unsafeTechAllowed){
 return !multiplayer||unsafeTechAllowed;
}
struct ProductionView {
 int player=-1,abstractType=-1,typeIndex=-1,naval=-1;
 bool allowed=false;
 bool multiplayer=false,unsafeTechAllowed=false;
};
inline bool planProduction(const ProductionView&v,intent::EventOrder&out){
 if(!v.allowed||!canProduceInSession(v.multiplayer,v.unsafeTechAllowed)||
   v.player<0||v.player>=32||v.typeIndex<0||v.abstractType<0||
   (v.naval!=0&&v.naval!=1))return false;
 out=intent::produce(v.player,vanilla::EventType::Produce,v.abstractType,v.typeIndex,v.naval);
 return true;
}
}} // assist::decisions
