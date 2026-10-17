#pragma once
#include <cmath>
#include <stdint.h>
// Read-only business outcomes; no native events or game writes live here.
namespace assist { namespace effects {
enum class Cargo : uint8_t { Unknown, Pending, Decreasing, Delivered };
inline Cargo cargo(double original,double current,bool observedDocking){
 if(!std::isfinite(original)||!std::isfinite(current)||original<=0||current<0||
    original>4000000||current>4000000)return Cargo::Unknown;
 if(!observedDocking)return Cargo::Pending;
 // Observing an empty hold only counts after a real docking/unloading signal.
 if(current<=0.01)return Cargo::Delivered;
 if(current<original-0.01)return Cargo::Decreasing;
 return Cargo::Pending;
}
enum class Aircraft : uint8_t { Pending, TargetAcquired, ShotObserved };
inline Aircraft aircraft(bool valid,bool targetAcquired,int baselineRearm,int currentRearm,int sentFrame,int frame){
 if(!valid||frame<=sentFrame)return Aircraft::Pending;
 if(targetAcquired&&currentRearm!=baselineRearm&&currentRearm>=sentFrame&&currentRearm<=frame)return Aircraft::ShotObserved;
 return targetAcquired?Aircraft::TargetAcquired:Aircraft::Pending;
}
} }
