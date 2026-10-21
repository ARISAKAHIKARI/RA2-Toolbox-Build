#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "experimental_patch_policy.h"

// Fixed ABI dependencies of the existing adapters, verified in both supplied
// EXEs. Do not fingerprint the whole function: resource loaders and unrelated
// platform fixes may legitimately differ. Never include our own patch bytes.
namespace patchcontext {
struct Probe {uint32_t site,address;uint8_t size,bytes[16];};
static constexpr Probe Probes[]={
 {experimentalpatch::Placement,0x4A8EB5,7,{0x8B,0x4C,0x24,0x08,0x83,0xEC,0x20}},
 {experimentalpatch::Placement,0x4A9060,6,{0x83,0xC4,0x20,0xC2,0x10,0}},
 {experimentalpatch::AirArrival,0x416AC0,13,{0x5F,0x5E,0x5D,0xB8,1,0,0,0,0x5B,0x83,0xC4,0x44,0xC3}},
 {experimentalpatch::ShroudDraw,0x47EFE0,6,{0x55,0x8B,0xEC,0x83,0xEC,0x34}},
 {experimentalpatch::ShroudDraw,0x47F03D,6,{0x57,0x8D,0x4D,0xCC,0x50,0x51}},
 {experimentalpatch::ShroudDraw,0x47F247,6,{0x8B,0xE5,0x5D,0xC2,0x0C,0}},
 {experimentalpatch::FogDraw,0x47F255,3,{0x83,0xEC,0x2C}},
 {experimentalpatch::FogDraw,0x47F279,9,{0x55,0x56,0x57,0x8D,0x4C,0x24,0x2C,0x50,0x51}},
 {experimentalpatch::FogDraw,0x47F4FB,6,{0x83,0xC4,0x2C,0xC2,0x0C,0}},
 {experimentalpatch::TechDraw,0x6A97C3,15,{0x8B,0x0D,0x4C,0x3D,0xA8,0,0x6A,0,0x6A,0,0x50,0x89,0x44,0x24,0x38}},
 {experimentalpatch::TechPrune,0x6AA777,10,{0x8B,0x88,0x1C,0x02,0,0,0x6A,1,0x57,0x53}},
 {experimentalpatch::TechDraw,0x4F7870,8,{0x83,0xEC,0x3C,0x8A,0x44,0x24,0x44,0x53}},
 {experimentalpatch::TechPrune,0x4F7870,8,{0x83,0xEC,0x3C,0x8A,0x44,0x24,0x44,0x53}}
};
template<class Read> bool compatible(uint32_t site,Read read){
 if(!experimentalpatch::size(site))return false;
 if(site==experimentalpatch::ScrollMap){
  uint8_t body[0x46]{};if(!read(site+8,body,sizeof body))return false;
  uint32_t hash=2166136261u;for(uint8_t b:body)hash=(hash^b)*16777619u;
  if(hash!=0x4C52FE69u)return false;
 }
 if(site==experimentalpatch::CameraClamp){
  // This adapter replaces the whole clamp method, so require its entire
  // remaining body (excluding our five entry bytes) to be the verified one.
  uint8_t body[0xB9]{};if(!read(site+5,body,sizeof body))return false;
  uint32_t hash=2166136261u;for(uint8_t b:body)hash=(hash^b)*16777619u;
  if(hash!=0xA41008F5u)return false;
 }
 for(const auto&p:Probes)if(p.site==site){
  uint8_t actual[16]{};
  if(!read(p.address,actual,p.size)||memcmp(actual,p.bytes,p.size))return false;
 }
 return true;
}
}
