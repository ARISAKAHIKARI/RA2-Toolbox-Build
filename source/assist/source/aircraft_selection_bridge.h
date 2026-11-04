#pragma once
#include <stdint.h>

// Original Object::Deselect, shared by the supported Aircraft/Unit vtables.
// It removes only this object from CurrentObjects and preserves its neighbours.
// Keep the entire native call chain guarded; do not fall back to unselect-all.
static bool aircraftSelectionCode(uint32_t address,unsigned length,uint32_t expected){
 uint8_t code[128]{};if(length>sizeof code||!read(address,code,length))return false;
 uint32_t hash=2166136261u;for(unsigned i=0;i<length;++i)hash=(hash^code[i])*16777619u;
 return hash==expected;
}
static bool dispatchedAircraftSelectionReady(uint32_t p){
 if(!gameentry::active(gameentry::Client::Assist)||!p||!live(p)||get<uint32_t>(p+0x21C)!=lastHouse||!get<uint8_t>(p+0x83))return false;
 int k=kind(p);if(k!=0&&k!=2)return false;
 uint32_t table=get<uint32_t>(p),vectorTable=get<uint32_t>(0xA8ECB8),items=get<uint32_t>(0xA8ECBC);
 int count=0,capacity=0;if(!read(0xA8ECC8,&count,sizeof count)||!read(0xA8ECC0,&capacity,sizeof capacity))return false;
 if(get<uint32_t>(table+0x150)!=0x5F44A0||get<uint32_t>(vectorTable+0x10)!=0x40CF00||
    count<=0||count>4096||capacity<count||capacity>65536||items<0x10000||uint64_t(items)+uint64_t(count)*4>=0x80000000ull)return false;
 int matches=0;for(int i=0;i<count;++i){uint32_t member=0;if(!read(items+uint32_t(i)*4,&member,sizeof member))return false;if(member==p)++matches;}
 if(matches!=1)return false;
 return aircraftSelectionCode(0x5F44A0,123,0x1064603Bu)&&
        aircraftSelectionCode(0x40CF00,38,0xE88F130Eu)&&
        aircraftSelectionCode(0x4AEB10,23,0x0EF2E454u)&&
        aircraftSelectionCode(0x4AEB30,24,0xE51B873Cu);
}
static bool deselectDispatchedAircraft(uint32_t p){
 if(!dispatchedAircraftSelectionReady(p))return false;
 int count=get<int>(0xA8ECC8);call<void>(0x5F44A0,p);
 return !get<uint8_t>(p+0x83)&&get<int>(0xA8ECC8)==count-1;
}
