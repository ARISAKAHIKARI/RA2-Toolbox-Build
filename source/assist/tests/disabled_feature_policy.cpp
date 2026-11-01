#include <cassert>
#include <cstdio>
#include "../source/disabled_feature_policy.h"
#include "../source/hook_policy.h"
using namespace assist;
int main(){
 // All old toggle combinations are denied for automatic conversion; ordinary
 // movement automation and native-event backends keep their original flags.
 for(unsigned bits=0;bits<16;++bits){
  auto resolved=hookpolicy::resolve({bool(bits&1),bool(bits&2),bool(bits&4),bool(bits&8)},true,true,true);
  assert(!(resolved&AutoCS));assert(bool(resolved&AdvancedInput)==bool(bits&1));
  assert(bool(resolved&SmartUnloadOrders)==bool(bits&2));assert(bool(resolved&EjectAssist)==bool(bits&4));
 }
 for(uint32_t requested:{0u,uint32_t(AutoCS),uint32_t(CustomCS),uint32_t(AutoCS|CustomCS)}){
  assert(featuregate::cs(requested,false)==featuregate::CS::Off);
  assert(featuregate::cs(requested,true)==((requested&CustomCS)?featuregate::CS::Manual:featuregate::CS::Off));
 }
 assert(featuregate::flags(AutoCS|CustomCS|AdvancedEnter|AdvancedInput)==uint32_t(CustomCS|AdvancedEnter|AdvancedInput));
 assert(!hookpolicy::needsCSInputPatch(AutoCS));assert(hookpolicy::needsCSInputPatch(CustomCS));
 for(unsigned operation=1;operation<=9;++operation)assert(featuregate::pointer(operation)==(operation!=pointerinput::C4Test));
 assert(featuregate::pointer(pointerinput::Enhanced)&&featuregate::pointer(pointerinput::AttackMove));
 assert(!featureavailability::InfantryForcedEntry);
 puts("PASS disabled-feature admission: old AutoCS flags cannot convert orders; held manual CustomCS and AdvancedEnter preserved; forced-entry mailbox denied");
}
