#include "../source/middle_input_owner.h"
#include "../source/pointer_intent.h"
#include <cassert>
using namespace assist;
int main(){
 // Exhaust every enabled-feature/modifier combination. The reservation is
 // exactly the union of the Runtime owners, independent of preview latency.
 for(unsigned features=0;features<8;++features)for(int formation=0;formation<2;++formation)for(unsigned mods=0;mods<16;++mods){
  uint8_t keys[256]{};keys[16]=mods&1;keys[17]=mods&2;keys[18]=mods&4;keys['Z']=mods&8;
  bool owner=moorders::resolveGesture(moorders::GesturePolicy{},moorders::gestureEnabled(features,formation),1,keys).matched!=0;
  assert(middleinput::ownsPointer(1,features,formation,keys)==owner);
  for(int button:{2,3,4,5,6,7})assert(!middleinput::ownsPointer(button,features,formation,keys));
  // Simulate the real host's old default Ctrl+middle aircraft-spread route.
  uint32_t buttons[pointerinput::MouseCount]{};buttons[pointerinput::MouseAirFirst]=1;
  pointerinput::Bindings bindings{};bindings.airFirst=17;bindings.enabled=pointerinput::AirFirst;
  pointerinput::MouseRoute routes[pointerinput::MouseCount+1]{};
  int sent=middleinput::ownsPointer(1,features,formation,keys)?0:pointerinput::resolveMouseMatches(keys,1,buttons,bindings,0,true,routes);
  if(owner)assert(sent==0);
 }
 uint8_t keys[256]{};keys[17]=1;assert(moorders::claimsMiddle(7,keys)==moorders::Fuzzy);
 keys['Z']=1;assert(moorders::claimsMiddle(7,keys)==moorders::Fuzzy);
 keys[17]=0;keys[16]=1;assert(moorders::claimsMiddle(7,keys)==moorders::Area);
 keys['Z']=0;assert(moorders::claimsMiddle(7,keys)==moorders::Area);
 keys[18]=1;assert(moorders::claimsMiddle(7,keys)==moorders::Area);
 moorders::GesturePolicy policy;policy.keys[0]='G'|(2u<<8);policy.buttons[0]=2;
 uint8_t rebound[256]{};rebound['G']=rebound[17]=1;
 assert(moorders::resolveGesture(policy,moorders::Fuzzy,2,rebound).mode==moorders::Fuzzy);
 assert(!moorders::resolveGesture(policy,moorders::Fuzzy,1,rebound).matched);
 assert(middleinput::ownsPointer(2,policy,moorders::Fuzzy,rebound));
 policy.keys[2]=policy.keys[0];policy.buttons[2]=2;
 assert(moorders::gestureConflicts(policy,5)==5);auto conflict=moorders::resolveGesture(policy,5,2,rebound);
 assert(!conflict.mode&&conflict.matched==5&&middleinput::ownsPointer(2,policy,5,rebound));
 policy.buttons[2]=0;assert(!moorders::gestureConflicts(policy,5));
 policy.keys[0]='G'|0x2000;assert(moorders::gestureInvalid(policy,1)==1);
 policy.keys[0]='G'|0xb000;rebound[17]=0;rebound['G']=1;assert(!moorders::resolveGesture(policy,1,2,rebound).matched);
 rebound['G']=3;assert(moorders::resolveGesture(policy,1,2,rebound).mode==moorders::Fuzzy);
 for(uint32_t button:{1u,2u,3u,4u,6u})assert(moorders::gestureVK(button)&&moorders::gestureUpMessage(button));
 assert(moorders::gestureMouseEdge(0x20B,1u<<16).button==2&&moorders::gestureMouseEdge(0x20C,2u<<16).button==3);
}
