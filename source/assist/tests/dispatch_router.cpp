#include <cassert>
#include <cstdint>
#include "../source/dispatch_router.h"
int main(){
 using namespace scheduler;using namespace dispatchrouter;
 State state{false,Message,Pending,false,false,false,1000,0};
 assert(owner(state)==Trigger::Message);
 assert(accepts(Trigger::Message,state));
 assert(!accepts(Trigger::GameFrame,state));
 state={true,GameFrame,FrameInstalled,true,false,true,1000,950};
 assert(owner(state)==Trigger::Message); // installed is not validated
 state.status=FrameActive;
 assert(owner(state)==Trigger::Message); // status is not a callback
 state.frameValidated=true;state.matchingGameThread=false;
 assert(owner(state)==Trigger::Message); // unknown thread cannot own commands
 state.matchingGameThread=true;
 assert(owner(state)==Trigger::GameFrame);
 assert(!accepts(Trigger::Message,state));
 state.now=1201; // more than 250ms since last callback
 assert(owner(state)==Trigger::Message);
 state.frameCallbackAt=1199;
 assert(owner(state)==Trigger::GameFrame);
 state.status=FrameStalled;
 assert(owner(state)==Trigger::Message);
 state.status=FrameConflict;
 assert(owner(state)==Trigger::Message);
 state.status=FrameActive;state.frameInstalled=false;
 assert(owner(state)==Trigger::Message);
 state.frameInstalled=true;state.requested=Message;
 assert(owner(state)==Trigger::Message);
 state.requested=GameFrame;state.now=0x00000015u;
 state.frameCallbackAt=0xFFFFFFFEu;
 assert(owner(state)==Trigger::GameFrame); // wrap-safe clock delta
}
