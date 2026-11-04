#include "../source/constructor_adapter.h"
#include <cassert>
#include <vector>
using namespace assist;
int main(){
 // The domain remains diagnostic-only. Only the three explicit production
 // event kinds pass; enabling Destruct must not expose Exit or Options.
 assert(vanilla::exposure(vanilla::Domain::NoOptions)==vanilla::Exposure::ProbeOnly);
 intent::EventOrder order{};order.domain=vanilla::Domain::NoOptions;order.player=1;
 for(int kind=0;kind<=255;++kind){
  order.eventKind=kind;
  const bool allowed=kind==vanilla::EventType::Destruct||kind==vanilla::EventType::AllCheer||kind==vanilla::EventType::PlanCommit;
  assert(constructors::admit(order)==(allowed?constructors::Verdict::Ready:constructors::Verdict::ProbeOnly));
 }
 order.eventKind=vanilla::EventType::Destruct;
 vanilla::Packet packet{};std::vector<uint32_t> called;
 auto invoke=[&](uint32_t address,uint32_t self,auto... args){
  called={address,self};(called.push_back((uint32_t)(uintptr_t)args),...);
 };
 assert(constructors::fill(packet,order,invoke)==constructors::Verdict::Ready);
 assert(called.size()==4&&called[0]==0x4C66C0&&called[2]==1&&called[3]==8);
 for(int player:{-1,32}){order.player=player;assert(constructors::admit(order)==constructors::Verdict::InvalidPlayer);}
 order.player=0;assert(constructors::admit(order)==constructors::Verdict::Ready);
 order.appendWaypoint=true;assert(constructors::admit(order)==constructors::Verdict::InvalidFields);
}
