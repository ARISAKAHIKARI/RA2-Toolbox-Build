#pragma once
#include "protocol.h"
#include "disabled_feature_policy.h"
// Pure flag composition: executable unit tests intentionally do not need Win32.
namespace assist { namespace hookpolicy {
struct Switches { bool advanced,smartUnload,eject,autoCS; };
inline uint32_t resolve(Switches v,bool advancedKeyBound,bool ejectRunning,bool csRunning){
 uint32_t r=0;
 if(v.advanced&&advancedKeyBound)r|=AdvancedInput;
 if(v.smartUnload)r|=SmartUnloadOrders;
 if(v.eject&&ejectRunning)r|=EjectAssist;
 if(featureavailability::AutoCS&&v.autoCS&&csRunning)r|=AutoCS;
 if(r&(AdvancedInput|SmartUnloadOrders|EjectAssist))r|=NativeOrders;
 return r;
}
// Ordinary command execution must not request feature-specific executable patches.
// Runtime message scheduling and rule-changing options are separate policies.
inline constexpr bool needsCSInputPatch(uint32_t){return false;}
inline constexpr bool needsNativeInputPatch(uint32_t){return false;}
inline bool needsWorldIndex(bool command,bool activeJobs,uint32_t flags,bool repairDue){return command||activeJobs||repairDue||(flags&(AutoOreReturn|EjectAssist|AntiTheft|SmartUnloadOrders));}
}}
