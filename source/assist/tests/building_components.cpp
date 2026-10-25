// Public fixtures for the native independent-building-part state machine.
// The private native replay establishes the offsets/formula; these fixtures
// guard production selection without redistributing game code or art.
#include "../../external_reader/source/building_components.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
using namespace external;
int main(){
 std::vector<uint8_t> memory(0xB00000);
 auto put=[&](uint32_t at,auto value){assert(uint64_t(at)+sizeof value<=memory.size());memcpy(memory.data()+at,&value,sizeof value);};
 auto read=[&](uint32_t at,void*out,size_t n){if(uint64_t(at)+n>memory.size())return false;memcpy(out,memory.data()+at,n);return true;};
 constexpr uint32_t b=0x10000,type=0x20000,links=0x30000,unit=0x40000,ut=0x50000;
 put(b+0x520,type);put(type+0xA0,100);put(b+0x6C,100);put(type+0x150C,0x60000u);put(type+0xF00,8);
 put(b+0xAC,18);put(b+0x534,0);put(b+0x358,90);put(b+0x360,20);put(b+0x364,20);put(b+0x368,uint8_t(1));put(b+0x369,uint8_t(1));put(0xA8ED84,100);
 auto sample=[&](){return buildings::state(read,b,type,.5);};
 GameView g(read);assert(buildings::buildup(g,b));
 auto s=sample();assert(s.flags==0&&s.doorFrame==-1); // constructing cannot show a Bib or independently advance a door
 put(b+0x534,1);put(b+0xAC,0);s=sample();assert(s.flags==buildings::Constructed&&s.doorFrame==-1);
 assert(buildings::buildup(g,b));put(b+0x6E4,uint8_t(1));assert(!buildings::buildup(g,b));
 put(b+0xAC,-1);put(b+0xB4,16);s=sample();assert(s.flags==(buildings::Constructed|buildings::Unloading)&&s.doorFrame==4);
 put(0xA8ED84,105);assert(sample().doorFrame==6); // actual simulation clock, no wall-clock animation
 put(b+0x358,-1);put(b+0x360,10);assert(sample().doorFrame==4);put(0xA8ED84,500);assert(sample().doorFrame==4); // stopped transition
 put(b+0x368,uint8_t(0));assert(sample().doorFrame==7); // open endpoint is last frame, never the shadow half
 put(b+0x369,uint8_t(0));assert(sample().doorFrame==0); // closed and inactive
 put(b+0x368,uint8_t(1));put(b+0x360,5);assert(sample().doorFrame==2); // closing uses the complementary progress
 put(b+0x6C,50);s=sample();assert((s.flags&buildings::Damaged)&&s.doorFrame==2);
 put(type+0x1700,uint8_t(1));assert(sample().doorFrame==10); // DamagedDoor is separate from the base damage frame
 put(b+0x6C,51);assert(!(sample().flags&buildings::Damaged)&&sample().doorFrame==2);
 put(b+0xE4,links);put(b+0xE8,1);put(links,unit);put(unit,0x7F5C70u);put(unit+0x6C4,ut);put(ut+0xD94,uint8_t(1));
 assert(sample().flags&buildings::Roof);put(ut+0xD94,uint8_t(0));put(ut+0xD6A,uint8_t(1));assert(sample().flags&buildings::Roof);
 put(ut+0xD6A,uint8_t(0));assert(!(sample().flags&buildings::Roof));
 put(ut+0xD94,uint8_t(1));put(b+0xE8,0);assert(!(sample().flags&buildings::Roof)); // allocated pointer is not a live radio contact
 put(b+0xAC,0);s=sample();assert(s.flags==buildings::Constructed&&s.doorFrame==-1); // production completion returns to the original body
 using buildings::doorFrame;
 assert(doorFrame(8,90,20,20,true,true,0,false)==0);
 assert(doorFrame(8,90,20,20,true,true,1000000,false)==7);
 assert(doorFrame(8,-1,100,20,true,false,0,false)==7);
 assert(doorFrame(8,-1,-100,20,true,false,0,false)==0);
 assert(doorFrame(1,-1,0,0,false,true,0,true)==1);
 assert(doorFrame(4096,-1,0,0,false,true,0,true)==8191);
 assert(doorFrame(0,-1,0,0,false,true,0,false)==-1&&doorFrame(4097,-1,0,0,false,true,0,false)==-1);
 std::cout<<"PASS Bib construction gate, actual unload/roof selection, opening/closing/paused door timer, damage equality and frame bounds\n";
}
