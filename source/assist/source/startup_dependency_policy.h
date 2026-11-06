#pragma once
#include <stdint.h>

// Native function entries are mandatory at their actual consumers, not an
// ABI gate for unrelated Runtime work. Instruction patches and DR sites keep
// their independent exact checks. New, unclassified entries fail closed.
namespace startupdependency {
enum class Dependency { Core, ClearBeacons, Reveal, LocalNative, Unused };
constexpr uint32_t BeaconEntry=0x431410,RevealEntry=0x577D90;
constexpr bool eventConstructor(uint32_t address){
 return address==0x4C65E0||address==0x4C6860||address==0x4C6970||address==0x4C6B60;
}
constexpr Dependency dependency(uint32_t address){
 switch(address){
 case BeaconEntry:return Dependency::ClearBeacons;
 case RevealEntry:return Dependency::Reveal;
 case 0x6AA600:return Dependency::Unused; // No direct Runtime caller.
 case 0x6ABC60:case 0x5F7900:case 0x4F7870:case 0x578080:
 case 0x4C65E0:case 0x4C6860:case 0x4C6970:case 0x6A6300:
 case 0x6CC360:case 0x4C6B60:case 0x4A8BF0:case 0x4AC8C0:
 case 0x4AC660:case 0x48DC90:case 0x6FBFA0:case 0x6D2420:
 case 0x6D2140:case 0x473460:return Dependency::LocalNative;
 default:return Dependency::Core;
 }
}
constexpr bool requiredAtStartup(uint32_t address){return dependency(address)==Dependency::Core;}
template<class Range,class Check> bool coreReady(const Range& signatures,Check check){
 for(const auto& signature:signatures)
  if(requiredAtStartup(signature.address)&&!check(signature))return false;
 return true;
}
}
