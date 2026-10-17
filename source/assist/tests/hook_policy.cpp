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
 assert(!needsCSInputPatch(f)&&!needsNativeInputPatch(f));
 s.smartUnload=false;s.eject=true;
 f=resolve(s,true,true,false);
 assert(f==(NativeOrders|EjectAssist));
 assert(!needsNativeInputPatch(f));
 assert(resolve(s,true,false,false)==0);
 s.eject=false;s.autoCS=true;
 f=resolve(s,true,false,true);
 assert(f==AutoCS && needsCSInputPatch(f)&&!needsNativeInputPatch(f));
 s.autoCS=false;s.advanced=true;
 assert(resolve(s,false,false,false)==0);
 f=resolve(s,true,false,false);
 assert(f==(AdvancedInput|NativeOrders) && needsCSInputPatch(f)&&needsNativeInputPatch(f));
 s.smartUnload=true;s.eject=true;s.autoCS=true;
 f=resolve(s,true,true,true);
 assert((f&(AutoCS|NativeOrders|AdvancedInput|SmartUnloadOrders|EjectAssist)) ==
  (AutoCS|NativeOrders|AdvancedInput|SmartUnloadOrders|EjectAssist));
 // MO-style cooperative mouse capture must not patch the outgoing mission
 // constructor nor the local input event dispatch as an unrelated dependency.
 for(uint32_t f:{uint32_t(AdvancedEnter),uint32_t(AircraftSpread),uint32_t(DiskPrimary),uint32_t(SellDrained),uint32_t(SellVehicles),uint32_t(SellEnemy)}){
  assert(!needsCSInputPatch(f));
  assert(!needsNativeInputPatch(f));
 }
 assert(!needsCSInputPatch(AutoOreReturn) && !needsNativeInputPatch(AutoOreReturn));
 assert(!needsCSInputPatch(AutoOreReturn|SmartUnloadOrders|EjectAssist));
 assert(!needsNativeInputPatch(AutoOreReturn|SmartUnloadOrders|EjectAssist));
 // Enabling an input feature alone does not scan the world while idle.
 for(uint32_t flag:{uint32_t(AdvancedInput),uint32_t(MagnetronInput),uint32_t(CustomCS),uint32_t(SmartUnloadOrders),uint32_t(AutoCS)}){
  assert(!needsWorldIndex(false,false,flag,false));
  assert(needsWorldIndex(true,false,flag,false));
  assert(needsWorldIndex(false,true,flag,false));
 }
 assert(needsWorldIndex(false,false,AutoOreReturn,false));
 assert(needsWorldIndex(false,false,EjectAssist,false));
 assert(needsWorldIndex(false,false,0,true));
}
