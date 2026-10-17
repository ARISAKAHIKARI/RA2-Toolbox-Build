#include "../source/control_link.h"
#include "../source/command_delivery.h"
#include <cassert>
#include <initializer_list>
using namespace assist;
int main(){
 controllink::View v{true,true,false,1,1,77};delivery::Queue q;
 assert(q.add({AutoBoard,0,0,123,77,100,0}));
 for(uint32_t tick:{100u,1700u,3000u,7000u}){
  auto id=controllink::mergedIdentity(v,500+tick);assert(id==77);
  assert(q.expire(tick,123,id,true)==0&&q.size==1);
  assert(controllink::ready(v)); // Wall time is not session identity.
 }
 v.command=0;assert(!controllink::ready(v)&&controllink::identity(v)==77);
 assert(controllink::transientPause(false,true,true,true));
 assert(!controllink::transientPause(false,false,true,true));
 assert(!controllink::transientPause(false,true,false,true));
 v.command=1;assert(controllink::ready(v));
 v.match=78;assert(q.expire(7001,123,controllink::identity(v),true)==1);
 v.stopping=true;assert(!controllink::ready(v));
 v.mapped=false;assert(controllink::identity(v)==0);
 // Numeric equality after clearing a display bit does not authenticate a bridge.
 v={true,false,false,1,1,10};assert(controllink::identity(v)==0&&!controllink::ready(v));
 assert(controllink::mergedIdentity(v,10)!=(uint32_t)10);
 v.observed=true;v.match=77;assert(controllink::identity(v)==77);v.mapped=false;
 assert(controllink::mergedIdentity(v,10)==(0x80000000u|10));
}
