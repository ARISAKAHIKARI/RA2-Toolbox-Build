#pragma once
#include <stdint.h>
#include <string.h>
#include "direct_patch_policy.h"
// Explicit 1.0.4 judgment opt-ins and 1.0.6 render-only test sites.
// Retired frame/input/CS/sidebar detours remain forbidden.
namespace experimentalpatch {
constexpr uint32_t Placement=0x4A8EB0,AirType=0x7E2908,AirArrival=0x416AB6,ShroudDraw=0x47F038,FogDraw=0x47F274,C4Execution=0x51A4D8,C4Movement=0x51C32B;
constexpr uint32_t MasterBuild=0x44574A,MasterUnit=0x445795,MasterInfantry=0x4457D0,MasterAir=0x445810,MasterPrune=0x6AA788;
inline bool masterTech(uint32_t at){return at==MasterBuild||at==MasterUnit||at==MasterInfantry||at==MasterAir||at==MasterPrune;}
constexpr uint32_t TechDraw=0x6A97D2,TechPrune=0x6AA781;
constexpr uint32_t CameraClamp=0x6D8640,ScrollMap=0x4A9840;
constexpr uint32_t ControlHouse=0x50B6F0,ControlOwner=0x6339BA,ControlPlanning=0x637E40;
inline bool callSite(uint32_t at){return at==TechDraw||at==TechPrune||at==ControlOwner||at==ControlPlanning;}
inline unsigned size(uint32_t at){if(auto s=directpatch::find(at))return s->size;if(masterTech(at))return at==MasterPrune?6:5;if(callSite(at)||at==CameraClamp||at==ControlHouse)return 5;return at==ScrollMap?8:at==Placement?5:at==AirType?4:at==AirArrival?10:at==ShroudDraw?5:at==FogDraw?5:(at==C4Execution||at==C4Movement)?6:0;}
inline uint32_t destination(uint32_t at,const uint8_t* bytes,unsigned n){
 if(!bytes||n!=size(at)||!n)return 0;
 uint32_t value=0;
 if(at==AirType){memcpy(&value,bytes,4);return value;}
 if(bytes[0]!=(callSite(at)?0xE8:0xE9))return 0;
 for(unsigned i=5;i<n;++i)if(bytes[i]!=0x90)return 0;
 memcpy(&value,bytes+1,4);return at+5+value;
}
}
