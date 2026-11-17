#pragma once
#include "planner_types.h"
// Host-only read-only provenance: no game process reads or pointer derefs.
// A planner order must retain the original capture-time identities, even if
// the game advances several frames before Runtime receives this candidate.
namespace assist { namespace planner {
template<class Lookup>
inline void stampProvenance(Order&order,Lookup lookup){
 const Actor*source=order.unit?lookup(order.unit):nullptr;
 const Actor*target=order.target?lookup(order.target):nullptr;
 const Actor*dest=order.destinationTag==0x34&&order.destination?lookup(order.destination):nullptr;
 order.unitPointer=source?source->sourcePointer:0;
 order.targetPointer=target?target->sourcePointer:0;
 order.destinationPointer=dest?dest->sourcePointer:0;
}
}} // assist::planner
