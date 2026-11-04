#include "../source/neural_history.h"
#include <cassert>
#include <cstdio>
#include <limits>
int main(){
 neuralhistory::Gate gate;
 assert(gate.reset(7,1,false));gate.accepted(7,1);
 assert(!gate.reset(7,2,false));
 // A scene can complete without being fed to NR (annotations, missing guides,
 // failed inference). Its next vectors describe the skipped scene, not NR's.
 assert(gate.reset(7,3,false));gate.accepted(7,3);
 assert(!gate.reset(7,4,false));gate.invalidate();assert(gate.reset(7,4,false));
 gate.accepted(7,4);assert(gate.reset(7,5,true));
 assert(gate.reset(8,5,false));assert(gate.reset(7,4,false));assert(gate.reset(7,3,false));
 gate.accepted(8,9);assert(!gate.reset(8,10,false));
 gate.accepted(8,std::numeric_limits<uint64_t>::max());assert(gate.reset(8,0,false));
 gate.accepted(8,0);assert(gate.reset(8,1,false));
 std::puts("PASS: neural history resets across skipped/failed/duplicate/backward frames, contexts, explicit resets and sequence wrap");
}
