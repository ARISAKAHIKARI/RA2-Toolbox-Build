#pragma once
#include <stdint.h>
#include <string.h>
#include "direct_patch_policy.h"
#include "native_sidebar_policy.h"
#include "native_viewport_policy.h"
// Explicit 1.0.4 judgment opt-ins and 1.0.6 render-only test sites.
// Retired scheduling/input/CS/sidebar detours remain forbidden.
// FrameCommitted is a separate read-only completion notification, not a scheduler.
namespace experimentalpatch {
constexpr uint32_t Placement=0x4A8EB0,AirType=0x7E2908,AirArrival=0x416AB6,ShroudDraw=0x47F038,FogDraw=0x47F274,C4Execution=0x51A4D8,C4Movement=0x51C32B;
constexpr uint32_t MasterBuild=0x44574A,MasterUnit=0x445795,MasterInfantry=0x4457D0,MasterAir=0x445810,MasterPrune=0x6AA788;
inline bool masterTech(uint32_t at){return at==MasterBuild||at==MasterUnit||at==MasterInfantry||at==MasterAir||at==MasterPrune;}
constexpr uint32_t TechDraw=0x6A97D2,TechPrune=0x6AA781;
constexpr uint32_t CameraClamp=0x6D8640,ScrollMap=0x4A9840;
constexpr uint32_t ControlHouse=0x50B6F0,ControlOwner=0x6339BA,ControlPlanning=0x637E40;
constexpr uint32_t LowDamage=0x6F7D44;
constexpr uint32_t PauseLoop=0x55D360; // opt-in single-player pause gate
constexpr uint32_t FrameCommitted=0x55DE81; // post-update, notification only
constexpr uint32_t BuildTimeTooltip=0x7F74D8; // CCToolTip vtable+14, local text only
constexpr uint32_t CursorTooltipLayout=0x7F74C8; // CCToolTip vtable+4, native box layout
// Unified outgoing transport, native Sell UI and native MegaMission arbitration.
constexpr uint32_t NetworkSend=0x649CA0;
constexpr uint32_t NetworkPlainHouse=0x649E1E;
constexpr uint32_t NetworkCompressedHouse=0x64B9D4;
constexpr uint32_t NetworkPack=0x64B660;
constexpr uint32_t NetworkDictionary=0x64B4EA;
constexpr uint32_t NetworkFrameGroup=0x649D61,NetworkPlainLimit=0x649DBC;
constexpr uint32_t NativeSellCursor=0x6929E5;
constexpr uint32_t NativeSellOwner=0x4AC179;
constexpr uint32_t NativeMegaSuccess=0x646F4E;
constexpr uint32_t ControlMegaOwner=0x646EF5;
constexpr uint32_t ControlMegaFOwner=0x646FD2;
constexpr uint32_t ControlPlanningOwner=0x6FFC93;
constexpr uint32_t ControlObjectOwner=0x6FFE2B;
inline bool callSite(uint32_t at){return at==TechDraw||at==TechPrune||at==ControlOwner||at==ControlPlanning||at==NativeSellCursor;}
inline unsigned size(uint32_t at){
 if(auto s=nativesidebarpolicy::find(at))return s->size;
 if(nativeviewportpolicy::find(at))return nativeviewportpolicy::size(at);
 if(at==PauseLoop)return 5;
 if(at==0x64DCCA||at==0x4F576B||at==0x577ACA||at==0x577DB0)return 6;
 if(at==0x4FC328||at==0x577BC7||at==0x577CA7||at==0x577F48)return 7;if(at==BuildTimeTooltip||at==CursorTooltipLayout)return 4;if(at==FrameCommitted)return 6;if(at==ControlMegaOwner)return 10;if(at==ControlMegaFOwner)return 10;if(at==ControlPlanningOwner)return 8;if(at==ControlObjectOwner)return 5;if(at==NetworkFrameGroup)return 5;if(at==NetworkPlainLimit)return 8;if(at==NetworkSend)return 5;if(at==NetworkPlainHouse)return 7;if(at==NetworkCompressedHouse)return 7;if(at==NetworkPack)return 6;if(at==NetworkDictionary)return 10;if(at==NativeSellCursor)return 5;if(at==NativeSellOwner)return 9;if(at==NativeMegaSuccess)return 7;if(auto s=directpatch::find(at))return s->size;if(masterTech(at))return at==MasterPrune?6:5;if(callSite(at)||at==CameraClamp||at==ControlHouse)return 5;return at==ScrollMap?8:at==Placement?5:at==AirType?4:at==AirArrival?10:at==ShroudDraw?5:at==FogDraw?5:(at==C4Execution||at==C4Movement||at==LowDamage)?6:0;}
inline uint32_t destination(uint32_t at,const uint8_t* bytes,unsigned n){
 if(!bytes||n!=size(at)||!n)return 0;
 if(nativesidebarpolicy::find(at))return nativesidebarpolicy::destination(at,bytes,n);
 if(nativeviewportpolicy::find(at))return nativeviewportpolicy::destination(at,bytes,n);
 uint32_t value=0;
 if(at==AirType||at==BuildTimeTooltip||at==CursorTooltipLayout){memcpy(&value,bytes,4);return value;}
 if(bytes[0]!=(callSite(at)?0xE8:0xE9))return 0;
 for(unsigned i=5;i<n;++i)if(bytes[i]!=0x90)return 0;
 memcpy(&value,bytes+1,4);return at+5+value;
}
}

