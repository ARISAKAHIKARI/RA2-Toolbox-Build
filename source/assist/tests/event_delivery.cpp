#include "../source/event_delivery.h"
#include <cassert>
using namespace assist::eventdelivery;
int main(){
 assert(isSingleplayer(0));assert(isSingleplayer(5));assert(!mayQueue(1));assert(!mayQueue(2));assert(!mayQueue(-1));assert(!mayQueue(999));
 assert(isMultiplayer(3));assert(isMultiplayer(4));assert(!isSingleplayer(3));
 assert(singleRoom(0,0));assert(singleRoom(SingleCapacity-1,SingleCapacity-1));
 assert(!singleRoom(SingleCapacity,0));assert(!singleRoom(-1,0));assert(!singleRoom(0,-1));
 unsigned char executed=0xFF;char house=-1;uint32_t frame=0;
 prepareSingle(executed,house,frame,7,200);
 assert(executed==0xFE&&house==7&&frame==201);
 prepareSingle(executed,house,frame,5,202);
 assert(executed==0xFE&&house==5&&frame==203);
 return 0;
}
