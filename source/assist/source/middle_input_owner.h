#pragma once
#include "formation_quick.h"
#include "mo_orders.h"
namespace assist { namespace middleinput {
// Reservation is based on configuration and the captured physical gesture,
// before either command's capability/target tests. A failed target must not
// silently turn one requested action into a different old middle binding.
template<class Keys>inline bool ownsPointer(int physical,const moorders::GesturePolicy&policy,uint32_t enabled,const Keys&keys){return moorders::resolveGesture(policy,enabled,uint32_t(physical),keys).matched!=0;}
// Kept for source compatibility with older portable callers. Runtime and Host
// production paths always pass the same explicit policy.
template<class Keys>inline bool reserved(uint32_t features,bool formationEnabled,const Keys&keys){return ownsPointer(1,moorders::GesturePolicy{},moorders::gestureEnabled(features,formationEnabled),keys);}
template<class Keys>inline bool ownsPointer(int physical,uint32_t features,bool formationEnabled,const Keys&keys){return ownsPointer(physical,moorders::GesturePolicy{},moorders::gestureEnabled(features,formationEnabled),keys);}
}}
