#include "../../external_reader/source/cooperative_reader.h"
#include "../../external_reader/source/tactical_sampler.h"
#include "../../shared/source/world_frame.h"
#include <cassert>
#include <vector>
#include <cstring>
#include <iostream>
struct Memory{std::vector<unsigned char> bytes=std::vector<unsigned char>(0xC00000);uint32_t reads=0,worldScans=0;template<class T>void put(uint32_t p,T x){memcpy(bytes.data()+p,&x,sizeof x);}void vec(uint32_t p,uint32_t a,std::initializer_list<uint32_t> items){put(p+4,a);put<int>(p+8,items.size());put<int>(p+16,items.size());for(auto x:items){put(a,x);a+=4;}}bool read(uint32_t a,void*out,size_t n){++reads;if(a==0xA8EC78+4)++worldScans;if(a<0x10000||uint64_t(a)+n>bytes.size())return false;memcpy(out,bytes.data()+a,n);return true;}};
struct Read{Memory*m;bool operator()(uint32_t a,void*out,size_t n)const{return m->read(a,out,n);}};
struct SealedRead {
 Memory*memory;worldframe::Snapshot*source;bool*complete;
 bool operator()(uint32_t at,void*out,size_t n)const{bool ok=source->read(at,out,n);if(!ok)*complete=false;return ok;}
 bool live(uint32_t at,void*out,size_t n)const{return memory->read(at,out,n);}
};
int main(){using namespace assist;Memory m;Read read{&m};constexpr uint32_t house=0x400000,tactical=0x450000,type=0x300000,unit=0x200000,temporal=0x500000;
 m.put<uint8_t>(0xA8E9A0,1);m.put<uint8_t>(0xA8E378,1);m.put<int>(0xA8ED84,100);m.put(0xA83D4C,house);m.put(0x887324,tactical);m.put<int>(0x8A00A4,1920);m.put<int>(0x8A00A8,1080);m.put<int>(tactical+0xB0,-320);m.put<int>(tactical+0xB4,800);m.put<double>(0xB0CD48,.0866025403784);m.vec(0xA80228,0x110000,{house});m.vec(0xA8EC78,0x110100,{unit});m.vec(0xA8B218,0x110200,{type});m.vec(0xA8ECB8,0x110300,{unit});
 m.put<int>(type+0x634,1);m.put<uint32_t>(type+0x6CC,1);m.put<int>(type+0x5E0,5);m.put(unit,uint32_t(0x7F5C70));m.put<uint32_t>(unit+0x10,1);m.put(unit+0x6C4,type);m.put(unit+0x21C,house);m.put<int>(unit+0x6C,100);for(int off:{0x74,0x90,0x83})m.put<uint8_t>(unit+off,1);m.put<int>(unit+0x9C,10240);m.put<int>(unit+0xA0,10240);m.put<int>(unit+0x18C,50);m.put<int>(unit+0x194,150);m.put<uint8_t>(house+22265,80);m.put<uint8_t>(house+22266,180);m.put<uint8_t>(house+22267,240);
 m.put(unit+0x278,temporal);m.put(temporal+0x28,unit);m.put<int>(temporal+0x48,1000);m.put<int>(temporal+0x4C,20);m.put(temporal+0x44,temporal);

 constexpr uint32_t enemy=0x410000,country=0x420000,spy=0x220000,spyType=0x330000;
 m.put(enemy+0x30,1);m.put(enemy+0x34,country);m.put(house+0x5788,0u);m.vec(0xA80228,0x110000,{house,enemy});
 m.put(unit+0x21C,enemy);m.put(enemy+22265,uint8_t(50));
 external::TacticalSampler<Read> sampler(read);
 auto count=[&](bool only=true,uint32_t serial=0){sampler.sample(100,0,true,0,serial,only);return sampler.enemyMarkers().count;};
 assert(count()==0);assert(count(false)==1);assert(count()==0);
 m.put(unit+0x220,2);assert(count()==1);m.put(unit+0x220,0);assert(count()==0);
 m.put(unit+0x1D8,uint8_t(1));assert(count()==1);m.put(unit+0x1D8,uint8_t(0));
 std::memcpy(m.bytes.data()+type+0x24,"MGTK",5);assert(count()==1); // Moving, undisguised vanilla Mirage.
 std::memcpy(m.bytes.data()+type+0x24,"HTNK",5);assert(count()==0);
 m.put(spy,uint32_t(0x7EB058));m.put(spy+0x6C0,spyType);m.put(spy+0x10,44u);m.put(spy+0x21C,enemy);
 m.put(spy+0x6C,100);m.put(spy+0x74,uint8_t(1));m.put(spy+0x90,uint8_t(1));m.put(spy+0x9C,10240);m.put(spy+0xA0,10240);
 m.put(spyType+0xEBE,uint8_t(1));m.put(spy+0x51C,house);m.vec(0xA8EC78,0x110100,{unit,spy});
 assert(count()==1&&sampler.enemyMarkers().items[0].owner==enemy); // Disguise house never changes allegiance.
 m.put(spy+0x1D8,uint8_t(1));assert(count()==1);
 m.put(house+0x5788,2u);assert(count()==0);m.put(house+0x5788,0u);assert(count()==1);
 m.put(spy+0x21C,house);assert(count()==0);m.put(spy+0x21C,enemy);
 m.put(country+0x1A6,uint8_t(1));assert(count()==0);m.put(country+0x1A6,uint8_t(0));assert(count()==1);
 m.put(spy+0x11C,unit);assert(count()==0);m.put(spy+0x11C,0u);
 m.put(spy+0x6C,0);assert(count()==0);m.put(spy+0x6C,100);
 m.put(spyType+0xC9A,uint8_t(1));assert(count()==0);m.put(spyType+0xC9A,uint8_t(0));
 assert(count()==1);assert(count(false)==2);
 // Different immutable publication, same logic frame: membership must refresh.
 assert(count(true,10)==1);m.put(unit+0x220,2);assert(count(true,11)==2);
 m.put(unit+0x220,0);assert(count(true,12)==1);
 sampler.sample(100,0,false,0,13,true);assert(sampler.enemyMarkers().count==0);
 std::cout<<"PASS special-only markers: cloak/Mirage/spy, real allegiance, allies/neutral/dead/transport/permanent-invisible exclusions, same-frame setting/publication changes\n";
}
