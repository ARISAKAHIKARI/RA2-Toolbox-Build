#pragma once
#include <stdint.h>

// Optional native commands do not define the ABI of unrelated Runtime work.
// Their signatures remain mandatory at the actual command/feature consumer.
// Core readiness uses the supplied check: ordinary Ares-owned API entries
// may be callable through platform dispatch without matching stock code.
// This classification does not grant instruction patches or DR site access.
namespace startupdependency {
enum class Dependency { Core, ClearBeacons, Reveal };
constexpr uint32_t BeaconEntry=0x431410,RevealEntry=0x577D90;
constexpr Dependency dependency(uint32_t address){
 return address==BeaconEntry?Dependency::ClearBeacons:
  address==RevealEntry?Dependency::Reveal:Dependency::Core;
}
constexpr bool requiredAtStartup(uint32_t address){return dependency(address)==Dependency::Core;}
template<class Range,class Check> bool coreReady(const Range& signatures,Check check){
 for(const auto& signature:signatures)
  if(requiredAtStartup(signature.address)&&!check(signature))return false;
 return true;
}
}
