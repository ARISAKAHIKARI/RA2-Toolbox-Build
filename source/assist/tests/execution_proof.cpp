#include "../source/execution_proof.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
using namespace assist::executionproof;
struct Unit {uint32_t pointer,manualVersion,house;};
static std::unordered_map<uint32_t,Unit>world;
static unsigned reads=0,versions=0,owners=0;
static uint32_t find(uint32_t uid){++reads;auto it=world.find(uid);return it==world.end()?0:it->second.pointer;}
static uint32_t version(uint32_t uid){++versions;return world.at(uid).manualVersion;}
static bool owns(uint32_t p,uint32_t house){
 ++owners;
 for(const auto&v:world)if(v.second.pointer==p)return v.second.house==house;
 return false;
}
static Failure test(const Ticket&t,Context ctx){
 reads=versions=owners=0;
 return verify(t,ctx,find,version,owns);
}
int main(){
 constexpr uint32_t house=0x20000;
 Ticket t{7,house,1100,{101,0x110000,5},{202,0x220000},{303,0x330000}};
 Context ctx{7,house,1150};
 world[101]={0x110000,5,house};
 world[202]={0x220000,0,0x90000}; // Enemies are allowed as targets.
 world[303]={0x330000,0,house};
 assert(ordered(t,101,202,0x34,303,0x34)==Failure::None);
 assert(test(t,ctx)==Failure::None);
 assert(reads==3&&versions==1&&owners==1);
 assert(ordered(t,999,202,0x34,303,0x34)==Failure::WrongOrder);
 assert(ordered(t,101,202,0,303,0x34)==Failure::WrongOrder);
 assert(ordered(t,101,202,0x34,303,11)==Failure::WrongOrder);
 assert(ordered(t,101,203,0x34,303,0x34)==Failure::WrongOrder);
 assert(test(t,{8,house,1150})==Failure::WrongMatch);
 assert(reads==0); // Never even look up memory across match generations.
 assert(test(t,{7,house+4,1150})==Failure::WrongHouse);
 assert(test(t,{7,house,60000})==Failure::None); // late candidate still validates live actors
 // UID reused with another pointer: the old worker result MUST be rejected.
 world[101].pointer=0x440000;
 assert(test(t,ctx)==Failure::InvalidActor);
 world[101].pointer=0x110000;
 world[101].house=0x40000;
 assert(test(t,ctx)==Failure::InvalidActor);
 world[101].house=house;
 world[101].manualVersion=6; // Player clicked another order during compute.
 assert(test(t,ctx)==Failure::ManualTakeover);
 world[101].manualVersion=5;
 world.erase(202); // Target left the map or was destroyed.
 assert(test(t,ctx)==Failure::TargetChanged);
 world[202]={0x500000,0,0x90000}; // ID reused by another actor.
 assert(test(t,ctx)==Failure::TargetChanged);
 world[202].pointer=0x220000;
 world[303].pointer=0x500000;
 assert(test(t,ctx)==Failure::DestinationChanged);
 world[303].pointer=0x330000;
 // Moving target coordinates alone must not invalidate identity; current
 // mission-specific range/path rules remain the native gateway's authority.
 assert(test(t,ctx)==Failure::None);
 Ticket noObjects{7,house,1100,{101,0x110000,5}};
 assert(ordered(noObjects,101,555,11,42,11)==Failure::None);
 assert(test(noObjects,ctx)==Failure::None&&reads==1);
 // TickCount wraps, without interpreting a wrapped recent plan as expired.
 Ticket wrap{7,house,0xfffffff0u,{101,0x110000,5}};
 assert(test(wrap,{7,house,0x20})==Failure::None);
 assert(test(wrap,{7,house,0x1000})==Failure::None);
 std::puts("PASS immutable native execution proofs: identity, target/dest, owner, original manual revision, match, no time expiry, bounded reads");
}
