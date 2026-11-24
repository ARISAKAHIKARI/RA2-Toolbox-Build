#pragma once
#include "protocol.h"
#include "disabled_feature_policy.h"
// Pure feature-flag composition only; this policy never requests executable code patches.
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
inline bool needsWorldIndex(bool command,bool activeJobs,uint32_t flags,bool repairDue){return command||activeJobs||repairDue||(flags&(AutoOreReturn|EjectAssist|AntiTheft|SmartUnloadOrders));}
}}
