#include "../source/middle_input_owner.h"
#include "../source/pointer_intent.h"
#include <cassert>
using namespace assist;
int main(){
 // Exhaust every enabled-feature/modifier combination. The reservation is
 // exactly the union of the Runtime owners, independent of preview latency.
 for(unsigned features=0;features<8;++features)for(int formation=0;formation<2;++formation)for(unsigned mods=0;mods<16;++mods){
  uint8_t keys[256]{};keys[16]=mods&1;keys[17]=mods&2;keys[18]=mods&4;keys['Z']=mods&8;
  bool owner=moorders::claimsMiddle(features,keys)||formation::quick::claimsMiddle(formation,keys);
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
 keys['Z']=1;assert(!moorders::claimsMiddle(7,keys));
 keys[17]=0;keys[16]=1;assert(moorders::claimsMiddle(7,keys)==moorders::Patrol);
 keys['Z']=0;assert(moorders::claimsMiddle(7,keys)==moorders::Area);
 keys[18]=1;assert(!moorders::claimsMiddle(7,keys));
}
