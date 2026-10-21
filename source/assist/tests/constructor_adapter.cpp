#include "../source/constructor_adapter.h"
#include "../source/entry_gate.h"
#include "../source/order_policy.h"
#include <cassert>
#include <vector>
#include <cstdint>
#include <type_traits>
#include <functional>
using namespace assist;
struct Trace {uint32_t address=0;unsigned args=0;std::vector<uintptr_t> values;
 template<typename... T> void operator()(uint32_t at,uint32_t self,T... arg){
  address=at;args=sizeof...(arg);values={uintptr_t(self),uintptr_t(arg)...};
 }
};
int main(){
 using D=vanilla::Domain;using V=constructors::Verdict;
 assert(sizeof(vanilla::Packet)==111);
 assert(vanilla::address(D::Object)==0x4C65E0);
 assert(vanilla::address(D::MegaMission)==0x4C6860);
 assert(vanilla::address(D::Production)==0x4C6970);
 assert(vanilla::address(D::SellCell)==0x4C6650);
 assert(vanilla::address(D::NoOptions)==0x4C66C0);
 assert(vanilla::address(D::PlayerBased)==0x4C6720);
 assert(vanilla::address(D::Waypoints)==0x4C6780);
 assert(vanilla::address(D::Place)==0x4C6AE0);
 assert(vanilla::address(D::SuperWeaponPlace)==0x4C6B60);
 vanilla::Packet packet{};
 Trace a;
 auto repair=intent::object(2,0x15,700);
 assert(constructors::fill(packet,repair,std::ref(a))==V::Ready);
 assert(a.address==0x4C65E0&&a.args==4&&a.values[1]==2&&a.values[2]==0x15&&a.values[3]==700&&a.values[4]==0x34);
 auto scatter=intent::object(2,0x07,700);Trace sc;
 assert(constructors::fill(packet,scatter,std::ref(sc))==V::Ready);
 assert(sc.address==0x4C65E0&&sc.args==4&&sc.values[2]==0x07&&sc.values[3]==700&&sc.values[4]==0x34);
 for(bool mp:{false,true}){
  assert(orderpolicy::objectEvent(7,true,true,true,true,mp)==orderpolicy::Reject::None);
  assert(orderpolicy::objectEvent(7,true,true,false,true,mp)==orderpolicy::Reject::UnauthorizedOwner);
  assert(orderpolicy::objectEvent(7,true,true,true,false,mp)==orderpolicy::Reject::UnauthorizedOwner);
 }
 assert(constructors::admit(intent::object(2,0x08,700))==V::InvalidFields);
 auto mega=intent::mega(2,701,7,702,0x34,0,0,0,0);
 Trace m;assert(constructors::fill(packet,mega,std::ref(m))==V::Ready);
 assert(m.address==0x4C6860&&m.args==10&&m.values[2]==701&&m.values[4]==7&&m.values[5]==702);
 // TargetClass / NetID tag-ID consistency checks on MegaMission
 assert(constructors::admit(intent::mega(2,701,1,702,0,0,0,0,0))==V::InvalidFields);
 assert(constructors::admit(intent::mega(2,701,1,0,0x34,0,0,0,0))==V::InvalidFields);
 assert(constructors::admit(intent::mega(2,701,2,0,0,30020,11,30020,11))==V::Ready);
 assert(constructors::admit(intent::mega(2,701,2,0,0,30020,99,0,0))==V::InvalidFields);
 auto prod=intent::produce(3,0xE,40,12,1);
 Trace p;assert(constructors::fill(packet,prod,std::ref(p))==V::Ready);
 assert(p.address==0x4C6970&&p.args==5&&p.values[2]==0xE&&p.values[3]==40&&p.values[4]==12&&p.values[5]==1);
 // Production accepts Produce/Abandon live, keeps Suspend/AbandonAll probe-only
 assert(constructors::admit(intent::produce(3,0x15,40,12,1))==V::InvalidFields);
 assert(constructors::admit(intent::produce(3,0x10,40,12,1),false)==V::Ready);
 assert(constructors::admit(intent::produce(3,0x10,40,12,1),true)==V::Ready);
 auto sw=intent::superWeapon(0,0x12,8,{4,5});
 Trace w;assert(constructors::fill(packet,sw,std::ref(w))==V::Ready);
 assert(w.address==0x4C6B60&&w.args==4&&w.values[3]==8);
 assert(constructors::admit(intent::superWeapon(0,0x12,256,{4,5}))==V::InvalidFields);
 assert(constructors::admit(intent::superWeapon(0,0x12,8,{-1,5}))==V::InvalidFields);
 assert(constructors::admit(intent::superWeapon(0,0x12,8,{512,5}))==V::InvalidFields);
 // The five newly discovered constructors are available only to ABI probes.
 for(D domain:{D::SellCell,D::NoOptions,D::PlayerBased,D::Waypoints,D::Place}){
  intent::EventOrder r{};r.domain=domain;r.player=1;r.eventKind=0x10;
  Trace z;assert(constructors::fill(packet,r,std::ref(z))==V::ProbeOnly);
  assert(z.address==0); // NO engine call, even with plausible arguments.
  assert(constructors::fill(packet,r,std::ref(z),true)==V::Ready);
  assert(z.address==vanilla::address(domain));
 }
 auto invalid=intent::object(1,0x15,0);
 Trace i;assert(constructors::fill(packet,invalid,std::ref(i))==V::InvalidFields&&i.address==0);
 invalid=intent::object(32,0x15,1);
 assert(constructors::admit(invalid)==V::InvalidPlayer);
 assert(entrygate::admit({42,42,3,true,false,true,true})==entrygate::Outcome::Pass);
 assert(entrygate::admit({42,41,3,true,false,true,true})==entrygate::Outcome::Stale);
 assert(entrygate::admit({42,42,8,true,false,true,true})==entrygate::Outcome::UnsupportedSession);
 assert(entrygate::admit({42,42,3,true,true,true,true})==entrygate::Outcome::NotReady);
 assert(entrygate::admit({42,42,3,true,false,true,false})==entrygate::Outcome::NoRoom);
}
