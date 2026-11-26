#include "../source/pointer_intent.h"
#include <cassert>
#include <iostream>
using namespace assist::pointerinput;
int main(){
 constexpr uint32_t started=1000;
 // At 240 FPS, native/Host capture can cross 40+ frames in <150 ms.
 assert(clickFresh(Enhanced,Enter,false,false,started,1040,100,155));
 assert(clickFresh(Enhanced,Enter,true,false,started,1115,100,210));
 assert(clickFresh(Enhanced,AirFirst,false,false,started,1149,100,300));
 assert(!clickFresh(Enhanced,Enter,false,false,started,1151,100,300));
 assert(!clickFresh(Enhanced,Enter,false,false,started,1001,100,99));
 assert(!clickFresh(Enhanced,Enter,false,false,started,999,100,101));
 assert(!clickFresh(Enhanced,Enter,false,false,started,1010,-1,101));
 // A continuation with a still-owned batch keeps its original timestamp,
 // rather than being artificially limited to 90 high-speed frames.
 assert(clickFresh(Enhanced,BatchNext,false,true,started,2100,100,2900));
 assert(!clickFresh(Enhanced,BatchNext,false,true,started,2501,100,2900));
 assert(!clickFresh(Enhanced,BatchNext,false,true,started,1100,100,99));
 std::cout<<"PASS high-FPS enhanced-enter: 150ms native capture survives hundreds of frames, stale/rewind rejected, 1.5s batch capped\n";
}
