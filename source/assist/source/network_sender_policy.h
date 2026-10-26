#pragma once
#include <stdint.h>
#include <stddef.h>
namespace assist { namespace networksender {
constexpr unsigned Capacity=128,EventBytes=111;
constexpr uint32_t Sender=0x649CA0,UncompressedHouse=0x649E1E,
 Compressed=0x64B660,CompressedHouse=0x64B9D4,CacheLookup=0x64B4EA;
enum Status:int { Off=0,Active=1,Draining=2,Unsupported=-2,Blocked=-3 };
struct Ring {int count,head,tail;};
inline bool valid(const Ring&r){return r.count>=0&&r.count<=int(Capacity)&&
 r.head>=0&&r.head<int(Capacity)&&r.tail>=0&&r.tail<int(Capacity)&&
 r.tail==((r.head+r.count)&127);}
inline bool same(const Ring&a,const Ring&b){return a.count==b.count&&a.head==b.head&&a.tail==b.tail;}
inline unsigned slot(const Ring&r,unsigned n){return (unsigned(r.head)+n)&127;}
inline bool validHouse(int house){return house>=0&&house<32;}
inline bool protocol(int p){return p>=0&&p<=2;}
inline bool foreign(int house,int current){return house!=current;}
// These are stream framing records, not independently tagged gameplay orders.
inline bool gameplay(uint8_t kind){return kind>0&&kind<0x30&&kind!=0x19&&kind!=0x1C;}
template<class House> unsigned prefix(unsigned available,unsigned limit,House house){
 if(!available||!limit)return 0;
 const int first=house(0);unsigned count=1;
 while(count<available&&count<limit&&house(count)==first)++count;
 return count;
}
struct Site {uint32_t address;uint8_t size,original[10];};
static constexpr Site Sites[]={
 {Sender,5,{0x83,0xEC,0x1C,0x53,0x55}},
 {UncompressedHouse,7,{0x88,0x8C,0x40,0xD6,0x02,0xA8,0x00}},
 {Compressed,6,{0x81,0xEC,0x94,0x00,0x00,0x00}},
 {CompressedHouse,7,{0x88,0x8C,0x40,0xD6,0x02,0xA8,0x00}},
 {CacheLookup,10,{0x8B,0xCA,0xC7,0x44,0x24,0x10,0x00,0x00,0x00,0x00}}
};
}}
