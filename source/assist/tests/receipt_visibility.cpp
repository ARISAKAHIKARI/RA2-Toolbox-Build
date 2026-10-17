#include "../source/protocol.h"
#include <cassert>
int main(){
 assist::TaskReceiptSummary r{};
 static_assert(sizeof(r)==28,"protocol receipt layout changed");
 r.serial=15;r.match=3;r.unit=20;r.target=21;
 r.phase=uint8_t(assist::gateway::Phase::Queued);
 assert(r.serial&&r.unit==20&&r.target==21);
 assert(r.phase!=uint8_t(assist::gateway::Phase::Completed));
}
