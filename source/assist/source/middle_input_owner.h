#pragma once
#include "formation_quick.h"
#include "mo_orders.h"
namespace assist { namespace middleinput {
// Reservation is based on configuration and the captured physical gesture,
// before either command's capability/target tests. A failed target must not
// silently turn one requested action into a different old middle binding.
template<class Keys>inline bool reserved(uint32_t features,bool formationEnabled,const Keys&keys){
 return moorders::claimsMiddle(features,keys)!=0||formation::quick::claimsMiddle(formationEnabled,keys);
}
template<class Keys>inline bool ownsPointer(int physical,uint32_t features,bool formationEnabled,const Keys&keys){return physical==1&&reserved(features,formationEnabled,keys);}
}}
