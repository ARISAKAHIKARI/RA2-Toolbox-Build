#include "../source/planner_provenance.h"
#include "../source/execution_proof.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
using namespace assist;
int main(){
 std::unordered_map<uint32_t,planner::Actor> oldWorld;
 planner::Actor actor{},enemy{},transport{};
 actor.id=101;actor.sourcePointer=0x210000;actor.manualVersion=7;
 enemy.id=202;enemy.sourcePointer=0x220000;transport.id=303;transport.sourcePointer=0x230000;
 oldWorld.emplace(actor.id,actor);oldWorld.emplace(enemy.id,enemy);oldWorld.emplace(transport.id,transport);
 auto original=[&](uint32_t id)->const planner::Actor*{
  auto it=oldWorld.find(id);return it==oldWorld.end()?nullptr:&it->second;
 };
 planner::Order attack{101,7,202,0,1,0,planner::CombatOrder,0};
 planner::stampProvenance(attack,original);
 assert(attack.unitPointer==0x210000&&attack.targetPointer==0x220000&&!attack.destinationPointer);
 planner::Order enter{101,7,0,303,7,0x34,planner::OreOrder,0};
 planner::stampProvenance(enter,original);
 assert(enter.unitPointer==0x210000&&enter.destinationPointer==0x230000);
 planner::Order cell{101,7,0,70007,2,11,planner::OreOrder,0};
 planner::stampProvenance(cell,original);
 assert(cell.unitPointer==0x210000&&!cell.destinationPointer);
 planner::Order missing{101,7,404,0,1,0,planner::CombatOrder,0};
 planner::stampProvenance(missing,original);
 assert(!missing.targetPointer); // must fail closed at native admission
 std::unordered_map<uint32_t,uint32_t> live{{101,0x210000},{202,0x220000},{303,0x230000}};
 const uint32_t match=9,house=0x20000;
 auto make=[&](const planner::Order&o,uint32_t tick){
  return executionproof::Ticket{match,house,tick,{o.unit,o.unitPointer,o.token},
    {o.target,o.targetPointer},{o.destinationTag==0x34?o.destination:0,o.destinationPointer}};
 };
 const auto lookup=[&](uint32_t id){auto it=live.find(id);return it==live.end()?0:it->second;};
 auto ver=[&](uint32_t){return 7u;};
 auto own=[&](uint32_t p,uint32_t h){return h==house&&p==0x210000;};
 assert(executionproof::verify(make(attack,100),{match,house,130},lookup,ver,own)==executionproof::Failure::None);
 // A later frame can use the candidate ONLY after live identity validation.
 live[101]=0x310000;
 assert(executionproof::verify(make(attack,100),{match,house,130},lookup,ver,own)==executionproof::Failure::InvalidActor);
 live[101]=0x210000;
 live[202]=0x320000;
 assert(executionproof::verify(make(attack,100),{match,house,130},lookup,ver,own)==executionproof::Failure::TargetChanged);
 live[202]=0x220000;
 assert(executionproof::verify(make(enter,100),{match,house,130},lookup,ver,own)==executionproof::Failure::None);
 live[303]=0x330000;
 assert(executionproof::verify(make(enter,100),{match,house,130},lookup,ver,own)==executionproof::Failure::DestinationChanged);
 assert(executionproof::verify(make(missing,100),{match,house,130},lookup,ver,own)==executionproof::Failure::TargetChanged);
 std::puts("PASS captured planner actor/target/destination provenance, delayed admission and UID/pointer reuse rejection");
}
