#pragma once
#include "protocol.h"
// Pure flag composition: executable unit tests intentionally do not need Win32.
namespace assist { namespace hookpolicy {
struct Switches { bool advanced,smartUnload,eject,autoCS; };
inline uint32_t resolve(Switches v,bool advancedKeyBound,bool ejectRunning,bool csRunning){
 uint32_t r=0;
 if(v.advanced&&advancedKeyBound)r|=AdvancedInput;
 if(v.smartUnload)r|=SmartUnloadOrders;
 if(v.eject&&ejectRunning)r|=EjectAssist;
 if(v.autoCS&&csRunning)r|=AutoCS;
 if(r&(AdvancedInput|SmartUnloadOrders|EjectAssist))r|=NativeOrders;
 return r;
}
inline bool needsCSInputPatch(uint32_t flags){return (flags&(AutoCS|AdvancedInput|MagnetronInput|CustomCS))!=0;}
inline bool needsNativeInputPatch(uint32_t flags){return (flags&(AdvancedInput|MagnetronInput|CustomCS))!=0;}
inline bool needsWorldIndex(bool command,bool activeJobs,uint32_t flags,bool repairDue){return command||activeJobs||repairDue||(flags&(AutoOreReturn|EjectAssist|AntiTheft));}
}}
