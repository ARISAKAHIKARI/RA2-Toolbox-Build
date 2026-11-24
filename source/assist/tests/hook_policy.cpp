#include "../source/hook_policy.h"
#include <cassert>
#include <initializer_list>
using namespace assist;
int main(){
 using namespace hookpolicy;
 auto s=Switches{false,false,false,false};
 assert(resolve(s,true,true,true)==0);
 s.smartUnload=true;
 uint32_t f=resolve(s,true,true,true);
 assert(f==(NativeOrders|SmartUnloadOrders));
 s.smartUnload=false;s.eject=true;
 f=resolve(s,true,true,false);
 assert(f==(NativeOrders|EjectAssist));
 assert(resolve(s,true,false,false)==0);
 s.eject=false;s.autoCS=true;
 f=resolve(s,true,false,true);
 const uint32_t autoCS=featureavailability::AutoCS?uint32_t(AutoCS):0u;
 assert(f==autoCS);
 s.autoCS=false;s.advanced=true;
 assert(resolve(s,false,false,false)==0);
 f=resolve(s,true,false,false);
 assert(f==(AdvancedInput|NativeOrders));
 s.smartUnload=true;s.eject=true;s.autoCS=true;
 f=resolve(s,true,true,true);
 assert((f&(AutoCS|NativeOrders|AdvancedInput|SmartUnloadOrders|EjectAssist)) ==
  (autoCS|NativeOrders|AdvancedInput|SmartUnloadOrders|EjectAssist));
 // Input flags are ordinary feature policy, not code-patch demand.
 // The source contract separately proves that their production paths install no detours.
 for(uint32_t flag:{uint32_t(AdvancedInput),uint32_t(MagnetronInput),uint32_t(CustomCS),uint32_t(AutoCS)}){
  assert(!needsWorldIndex(false,false,flag,false));
  assert(needsWorldIndex(true,false,flag,false));
  assert(needsWorldIndex(false,true,flag,false));
 }
 // Background automation is different: it needs a world index while idle.
 assert(needsWorldIndex(false,false,SmartUnloadOrders,false));
 assert(needsWorldIndex(false,false,AutoOreReturn,false));
 assert(needsWorldIndex(false,false,EjectAssist,false));
 assert(needsWorldIndex(false,false,AntiTheft,false));
 assert(needsWorldIndex(false,false,0,true));
}
