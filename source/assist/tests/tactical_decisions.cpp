#include "../source/tactical_decisions.h"
#include <cassert>
int main(){
 using namespace assist::decisions;
 assert(shouldRepair({40,100,true,false,false}));
 assert(!shouldRepair({0,100,true,false,false}));
 assert(!shouldRepair({40,100,true,true,false}));
 assert(!shouldRepair({40,100,true,false,true}));
 assert(!shouldRepair({100,100,true,false,false}));
 assert(!shouldRepair({40,100,false,false,false}));
 assist::intent::EventOrder out{};
 assert(planProduction({0,40,5,1,true},out));
 assert(out.a==40&&out.b==5&&out.c==1&&out.eventKind==0xE);
 assert(!planProduction({0,40,5,1,false},out));
 assert(!planProduction({-1,40,5,1,true},out));
 assert(!planProduction({0,40,-1,1,true},out));
 assert(!planProduction({0,40,5,2,true},out));
 // Multiplayer production gating unless UnsafeTech is explicitly enabled
 assert(canProduceInSession(false,false));
 assert(!canProduceInSession(true,false));
 assert(canProduceInSession(true,true));
 assert(!planProduction({0,40,5,1,true,true,false},out));
 assert(planProduction({0,40,5,1,true,true,true},out));
}
