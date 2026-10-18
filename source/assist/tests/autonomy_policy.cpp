#include "../source/magnetron_cycle.h"
#include "../source/unload_policy.h"
#include <cassert>
#include <iostream>
using namespace assist;
int main(){
 assert(!magnetcycle::shotObserved(3,2,0,100,100,false));
 assert(!magnetcycle::shotObserved(3,2,9,100,100,false)); // old cooldown is not a new shot
 assert(magnetcycle::shotObserved(3,2,0,3,-1,false));
 assert(magnetcycle::shotObserved(3,2,0,100,100,true));
 assert(!magnetcycle::shotObserved(1,2,9,101,100,true));
 assert(magnetcycle::avoidUntil(100,1)==130);
 assert(magnetcycle::avoidUntil(100,60)==160);
 assert(magnetcycle::avoidUntil(100,300)==220);
 assert(!magnetcycle::shotStalled(40,2));assert(magnetcycle::shotStalled(47,2));
 assert(unloadpolicy::initialMayContinue(0,0,0));
 assert(unloadpolicy::initialMayContinue(5,0,0));
 assert(unloadpolicy::initialMayContinue(11,0,0));
 assert(unloadpolicy::initialMayContinue(2,0,1));
 assert(!unloadpolicy::initialMayContinue(2,0,181));
 assert(unloadpolicy::initialMayContinue(2,2,1));
 assert(!unloadpolicy::initialMayContinue(2,2,10));
 assert(unloadpolicy::initialMayContinue(2,2,20,false,true));
 assert(!unloadpolicy::initialMayContinue(2,2,181,false,true));
 assert(!unloadpolicy::initialMayContinue(13,2,1,false,true));
 assert(!unloadpolicy::initialMayContinue(13,2,1));
 assert(!unloadpolicy::initialMayContinue(7,2,1));
 assert(unloadpolicy::initialMayContinue(7,0,1,true));
 std::cout<<"PASS: magnet shot observation/ROF pacing and eject grace vs user takeover\n";
}
