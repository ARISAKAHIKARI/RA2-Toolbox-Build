#include "../source/protocol.h"
#include <cassert>
#include <climits>
using namespace assist;
int main(){
 assert(remaining(100,150,145)==105);
 assert(remaining(-1,150,999)==150); // paused engine timer
 assert(remaining(100,150,251)==0);
 assert(remaining(100,150,90)==150); // frame rewind must not inflate duration
 assert(remaining(INT_MIN,INT_MAX,INT_MAX)==0); // no signed overflow
 assert(idleMission(5,-1,false,false));
 assert(!idleMission(5,2,false,false)); // queued attack is not idle
 assert(!idleMission(5,-1,true,false));
 assert(!idleMission(5,-1,false,true));
 assert(!idleMission(2,-1,false,false));
 assert(canRepair(50,100,true,false));
 assert(!canRepair(0,100,true,false));
 assert(!canRepair(50,100,true,true)); // no toggle-off of active repair
 assert(!canRepair(100,100,true,false));
 assert(queueAvailable(111,127));
 assert(!queueAvailable(112,0)); // reserved capacity for normal game input
 assert(!queueAvailable(-1,0));assert(!queueAvailable(0,128));
 assert(validType(10,false,false,1));
 assert(!validType(-1,false,false,1)); // disabled/civilian type
 assert(!validType(1,true,false,1));assert(!validType(1,false,true,1));
 assert(!validType(1,false,false,0));
}
