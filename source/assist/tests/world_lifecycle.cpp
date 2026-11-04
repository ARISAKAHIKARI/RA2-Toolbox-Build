#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../source/world_lifecycle.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace assist::worldlife;
namespace {
std::vector<uint8_t> memory(0x1000000);uint32_t fail=0;
template<class T>void put(uint32_t at,T value){std::memcpy(memory.data()+at,&value,sizeof value);}
bool read(uint32_t at,void*out,size_t n){if(uint64_t(at)+n>memory.size()||(fail>=at&&fail<uint64_t(at)+n))return false;std::memcpy(out,memory.data()+at,n);return true;}
}
int main(){
 put(0xA8B230,uint32_t(0x100000));put(0xA8B238,3);put(0x87F924,uint32_t(0x200000));put(0x87F928,4096);put(0xA8ED84,100);put(0xA8E9A0,uint8_t(1));
 put(0xA83D4C,uint32_t(0x300000));put(0xA8022C,uint32_t(0x400000));put(0xA80230,2);put(0xA80238,2);put(0x400000,uint32_t(0x300000));put(0x400004,uint32_t(0x310000));
 Tracker tracker;assert(tracker.observe(capture(read))==Change::Start);assert(tracker.observe(capture(read))==Change::Same);
 uint32_t house=0;assert(authority(read,house)==Authority::Command&&house==0x300000);
 put(0xAC1198,house);assert(authority(read,house)==Authority::ViewOnly&&tracker.observe(capture(read))==Change::Same);put(0xAC1198,uint32_t(0));
 for(unsigned offset:{0x1F5u,0x1F6u,0x1F8u}){put(house+offset,uint8_t(1));assert(authority(read,house)==Authority::ViewOnly&&tracker.observe(capture(read))==Change::Same);put(house+offset,uint8_t(0));}
 put(0xA8E9A0,uint8_t(0));assert(tracker.observe(capture(read))==Change::Same&&tracker.bound);put(0xA8E9A0,uint8_t(1));
 for(uint32_t at:{0xA8B230u,0xA8B238u,0x87F924u,0x87F928u,0xA8ED84u,0xA8E9A0u}){fail=at;assert(tracker.observe(capture(read))==Change::Unknown&&tracker.bound&&tracker.world.frame==100);fail=0;assert(tracker.observe(capture(read))==Change::Same);}
 for(uint32_t at:{0xA83D4Cu,0xAC1198u,0x3001F5u,0xA8022Cu,0xA80230u,0xA80238u,0x400000u}){fail=at;assert(authority(read,house)==Authority::Unknown);fail=0;assert(authority(read,house)==Authority::Command);}
 put(0xA83D4C,uint32_t(0x310000));assert(authority(read,house)==Authority::Command&&tracker.observe(capture(read))==Change::Same);
 put(0xA8ED84,105);assert(tracker.observe(capture(read))==Change::Same);put(0xA8ED84,103);assert(tracker.observe(capture(read))==Change::Rewind);
 put(0xA8B230,uint32_t(0x110000));assert(tracker.observe(capture(read))==Change::Replaced);
 put(0x87F924,uint32_t(0x210000));assert(tracker.observe(capture(read))==Change::Replaced);
 put(0x87F928,8192);assert(tracker.observe(capture(read))==Change::Replaced);
 put(0xA8B238,5);assert(tracker.observe(capture(read))==Change::Replaced);
 put(0xA8ED84,-1);assert(tracker.observe(capture(read))==Change::Unknown&&tracker.bound);put(0xA8ED84,104);assert(tracker.observe(capture(read))==Change::Same);
 put(0xA8E9A0,uint8_t(0));put(0xA8B230,uint32_t(0));put(0x87F924,uint32_t(0));put(0x87F928,0);assert(tracker.observe(capture(read))==Change::Absent&&!tracker.bound);
 std::puts("PASS: world identity survives observer, defeat, pause, House change and failed reads; true Scenario/map/session changes and frame rewind retire; authority remains separate");
}
