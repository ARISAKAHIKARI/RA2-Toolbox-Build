#include "../../external_reader/source/cooperative_reader.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
struct Memory {
 std::vector<unsigned char> data=std::vector<unsigned char>(0xc00000);
 template<class T> void put(uint32_t p,T value){memcpy(data.data()+p,&value,sizeof value);}
 void vec(uint32_t p,uint32_t items,const std::vector<uint32_t>& values){put(p+4,items);put<int>(p+8,values.size());put<int>(p+16,values.size());for(auto v:values){put(items,v);items+=4;}}
 bool read(uint32_t a,void*out,size_t n){if(a<0x10000||uint64_t(a)+n>data.size())return false;memcpy(out,data.data()+a,n);return true;}
};
struct Read {Memory*m;bool operator()(uint32_t a,void*out,size_t n)const{return m->read(a,out,n);}};
int main(){
 using namespace assist;Memory m;Read read{&m};external::GameView<Read> game(read);
 const uint32_t house=0x400000,enemy=0x420000,tt=0x300000,bt=0x310000,infType=0x320000;
 const uint32_t tanks[]={0x200000,0x202000,0x204000,0x206000,0x208000,0x20a000};
 const uint32_t bunkers[]={0x220000,0x222000,0x224000,0x226000,0x228000,0x22a000,0x22c000};
 m.put(0xa83d4c,house);m.put<int>(0xa8ed84,100);m.put<uint8_t>(0xa8e9a0,1);m.put<int>(enemy+0x30,1);m.put<uint32_t>(house+0x5788,1);
 m.put<double>(tt+0x380,3.);m.put<uint8_t>(tt+0xd2e,1);m.put<uint8_t>(bt+0x16ab,1);
 auto object=[&](uint32_t p,uint32_t vt,uint32_t type,int id){m.put(p,vt);m.put(p+0x10,id);m.put(p+(vt==0x7e3ebc?0x520:vt==0x7eb058?0x6c0:0x6c4),type);m.put(p+0x21c,house);m.put<int>(p+0x6c,100);m.put<uint8_t>(p+0x74,1);m.put<uint8_t>(p+0x90,1);m.put<int>(p+0x9c,25600+id*32);m.put<int>(p+0xa0,25600);};
 std::vector<uint32_t> all;
 for(int i=0;i<6;++i){object(tanks[i],0x7f5c70,tt,10+i);all.push_back(tanks[i]);}
 for(int i=0;i<7;++i){object(bunkers[i],0x7e3ebc,bt,20+i);all.push_back(bunkers[i]);
  // Native radio storage can have one allocated, EMPTY link. E8 is its
  // length, not occupied count (RadioClass::IsRadioContact: 0x65AE30).
  m.put<int>(bunkers[i]+0xe8,1);m.put<uint32_t>(bunkers[i]+0xe4,0x110400+i*4);
 }
 auto c=game.capacity(bunkers[0]);assert(c.maximum==1&&c.used==0);
 assert(game.entrySlots(tanks[0],bunkers[0])==1);
 assert(game.entryCompatible(tanks[0],bunkers[0],house));
 cooperative::Request r{};r.serial=r.match=1;r.mode=cooperative::Enter;r.target=20;r.type=bt;r.count=4;r.viewWidth=1920;r.viewHeight=1080;r.cameraX=-320;r.cameraY=2700;
 for(int i=0;i<4;++i)r.sources[i]={uint32_t(10+i),0};
 m.vec(0xa8ec78,0x110000,all);
 m.put<int>(tanks[4]+0xac,7);m.put(tanks[4]+0x5a4,bunkers[1]); // already inbound
 m.put(tanks[5]+0x2e4,bunkers[2]);m.put(bunkers[2]+0x2e4,tanks[5]); // occupied
 m.put(bunkers[3]+0x21c,enemy); // enemy excluded
 m.put<int>(bunkers[4]+0x9c,100000); // outside captured viewport
 auto input=external::cooperativeInput(read,r,[](uint32_t){return cooperative::Quota{};});
 auto assigned=cooperative::allocate(input);std::map<uint32_t,int> counts;
 for(auto a:assigned)++counts[a.target];
 assert(assigned.size()==3&&counts[20]==1&&counts[25]==1&&counts[26]==1);
 // The native left click has already linked one SELECTED tank to the clicked
 // bunker. It must keep that reservation and receive Enter, rather than being
 // redistributed while the clicked bunker is excluded as apparently full.
 m.put<uint32_t>(0x110400,tanks[0]);m.put<int>(tanks[0]+0xac,2);m.put(tanks[0]+0x5a4,bunkers[0]);
 input=external::cooperativeInput(read,r,[](uint32_t){return cooperative::Quota{};});assigned=cooperative::allocate(input);counts.clear();
 for(auto a:assigned){++counts[a.target];if(a.id==10)assert(a.target==20);else assert(a.target!=20);}
 assert(assigned.size()==3&&counts[20]==1&&counts[25]==1&&counts[26]==1);
 m.put(bunkers[0]+0x2e4,tanks[5]);assert(!game.entryCompatible(tanks[0],bunkers[0],house));m.put<uint32_t>(bunkers[0]+0x2e4,0);
 m.put<int>(bunkers[0]+0x114,1);assert(!game.entryCompatible(tanks[0],bunkers[0],house));m.put<int>(bunkers[0]+0x114,0);
 m.put<uint32_t>(0x110400,0);m.put<int>(tanks[0]+0xac,5);m.put<uint32_t>(tanks[0]+0x5a4,0);
 // A real radio contact reserves the building; empty storage must not.
 m.put<uint32_t>(0x110400,tanks[4]);assert(game.capacity(bunkers[0]).used==1);
 m.put<uint32_t>(0x110400,0);assert(game.capacity(bunkers[0]).used==0);
 m.put<int>(bunkers[0]+0xe8,100000);assert(game.capacity(bunkers[0]).used>=game.capacity(bunkers[0]).maximum);m.put<int>(bunkers[0]+0xe8,1);
 m.put<uint8_t>(tt+0xd2e,0);assert(!game.entryCompatible(tanks[0],bunkers[0],house));m.put<uint8_t>(tt+0xd2e,1);
 const uint32_t infantry=0x240000;object(infantry,0x7eb058,infType,40);assert(!game.entryCompatible(infantry,bunkers[0],house));
 assert(game.entered(tanks[5],bunkers[2]));m.put<uint32_t>(bunkers[2]+0x2e4,0);assert(!game.entered(tanks[5],bunkers[2]));m.put<uint32_t>(tanks[5]+0x2e4,0);
 // Repair depots and naval repair docks share facility counting. A ship's
 // Size does not consume several docks; mission Repair also reserves a dock.
 m.put<uint8_t>(bt+0x16ab,0);m.put<uint8_t>(bt+0x16a9,1);m.put<int>(bt+0x1780,2);
 m.put<int>(tanks[4]+0xac,9);assert(game.entryDestination(tanks[4])==bunkers[1]);
 m.put<uint8_t>(tt+0xcce,1);m.put<uint8_t>(bt+0xcce,1); // Naval
 m.put<int>(bunkers[0]+0xe8,2);m.put<uint32_t>(bunkers[0]+0xe4,0x110500);
 m.put<uint32_t>(0x110500,tanks[5]);m.put<uint32_t>(0x110504,0);
 m.put<int>(tanks[5]+0xac,9);m.put(tanks[5]+0x5a4,bunkers[0]);
 c=game.capacity(bunkers[0]);assert(c.maximum==2&&c.used==1&&game.entrySlots(tanks[0],bunkers[0])==1);
 input=external::cooperativeInput(read,r,[](uint32_t){return cooperative::Quota{};});
 for(const auto& target:input.targets){if(target.id==20)assert(target.free==1);if(target.id==21)assert(target.free==1);}
 // A radio contact and the same ship's incoming mission reserve only once.
 assigned=cooperative::allocate(input);counts.clear();for(auto a:assigned)++counts[a.target];assert(assigned.size()==4&&counts[20]==1);
 m.put<uint32_t>(0x110504,tanks[4]);assert(game.capacity(bunkers[0]).used==2);
 m.put<uint8_t>(bt+0x16a9,0);assert(!game.entryDestination(tanks[4]));
 // Transport sizes still consume their actual weighted slots.
 m.put<int>(tt+0x5e0,5);m.put<double>(tt+0x388,4.);assert(game.entrySlots(tanks[0],tanks[1])==3);
 // Garrisoned infantry still count one occupant each, even with Size=2.
 m.put<uint8_t>(bt+0x16ab,0);m.put<uint8_t>(bt+0x157b,1);m.put<int>(bt+0x1580,5);m.put<uint8_t>(infType+0xeb4,1);m.put<double>(infType+0x380,2.);
 assert(game.entrySlots(infantry,bunkers[0])==1&&game.entryCompatible(infantry,bunkers[0],house));assert(!game.entryCompatible(tanks[0],bunkers[0],house));
 std::cout<<"PASS tank-bunker/depot/naval-dock allocation: Size=3, empty/active radio links, one tank per bunker, multi-dock counting, Enter/Repair incoming without double count, linked completion, capability and passenger/garrison boundaries\n";
}
