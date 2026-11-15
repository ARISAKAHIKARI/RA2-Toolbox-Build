#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
// Host-only bounded projection of the already sampled local selection.
// No engine address, live reader, command, or renderer dependency.
namespace external { namespace unitpanel {
constexpr unsigned MaxGroups=64,MaxPassengers=48;
struct Group {uint32_t type=0;int kind=-1,count=0;std::array<uint16_t,40> name{};};
struct Unit {
 uint32_t id=0,type=0;int kind=-1,health=0,strength=0,ammo=-1,ammoMax=0,level=0,passengers=0,capacity=0;
 std::array<uint16_t,40> name{};
};
struct Model {
 uint32_t match=0;int frame=-1,selected=0,groupCount=0,passengerCount=0;
 bool groupsTruncated=false,passengersComplete=true;
 std::array<Group,MaxGroups> groups{};Unit single{};std::array<Unit,MaxPassengers> passenger{};
 void reset(uint32_t epoch,int at){*this={};match=epoch;frame=at;}
 Group* selectedType(uint32_t type,int kind){
  ++selected;
  for(int i=0;i<groupCount;++i)if(groups[i].type==type&&groups[i].kind==kind){
   ++groups[i].count;return nullptr;
  }
  if(groupCount>=int(MaxGroups)){groupsTruncated=true;return nullptr;}
  auto&g=groups[groupCount++];g={};g.type=type;g.kind=kind;g.count=1;return &g;
 }
 void appendPassenger(const Unit&unit){
  if(passengerCount>=int(MaxPassengers)){passengersComplete=false;return;}
  passenger[passengerCount++]=unit;
 }
 uint64_t visualSignature()const{
  uint64_t hash=1469598103934665603ull;
  auto mix=[&](uint64_t v){for(int i=0;i<8;++i){hash^=uint8_t(v>>(i*8));hash*=1099511628211ull;}};
  mix(match);mix(selected);mix(groupCount);mix(passengerCount);mix(groupsTruncated);mix(passengersComplete);
  for(int i=0;i<groupCount;++i){const auto&g=groups[i];
   mix(g.type);mix(uint32_t(g.kind));mix(g.count);for(auto c:g.name)mix(c);
  }
  auto unit=[&](const Unit&u){mix(u.id);mix(u.type);mix(uint32_t(u.kind));
   mix(uint32_t(u.health));mix(uint32_t(u.strength));mix(uint32_t(u.ammo));
   mix(uint32_t(u.ammoMax));mix(uint32_t(u.level));mix(uint32_t(u.passengers));mix(uint32_t(u.capacity));
   for(auto c:u.name)mix(c);
  };
  if(selected==1){unit(single);for(int i=0;i<passengerCount;++i)unit(passenger[i]);}
  return hash;
 }
};
inline int pageCount(int entries,int perPage){return std::max(1,(std::max(0,entries)+std::max(1,perPage)-1)/std::max(1,perPage));}
inline int clampPage(int page,int entries,int perPage){return std::clamp(page,0,pageCount(entries,perPage)-1);}
inline bool validCargoCount(int declared,unsigned observed,bool terminated){
 return declared>=0&&declared<=256&&observed<=unsigned(declared)&&terminated&&observed==unsigned(declared);
}
}}
