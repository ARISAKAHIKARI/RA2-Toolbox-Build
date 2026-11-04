#pragma once
#include "protocol.h"
#include "pointer_intent.h"
#include "../../shared/source/feature_availability.h"
namespace assist {namespace featuregate {
constexpr uint32_t flags(uint32_t requested){return featureavailability::AutoCS?requested:requested&~uint32_t(AutoCS);}
enum class CS {Off,Manual,Automatic};
constexpr CS cs(uint32_t requested,bool held){
 return held&&(requested&CustomCS)?CS::Manual:(flags(requested)&AutoCS)?CS::Automatic:CS::Off;
}
constexpr bool pointer(uint32_t operation){return featureavailability::InfantryForcedEntry||operation!=pointerinput::C4Test;}
}}
