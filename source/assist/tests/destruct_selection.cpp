#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <cstdio>
#ifdef NDEBUG
#error assertions required
#endif
struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
static std::map<uint32_t,uint8_t> mem;static uint32_t fail=0;
template<class T>static void put(uint32_t p,T v){const auto*b=reinterpret_cast<const uint8_t*>(&v);for(size_t n=0;n<sizeof v;++n)mem[p+uint32_t(n)]=b[n];}
static bool read(uint32_t p,void*out,size_t n){auto*b=(uint8_t*)out;for(size_t i=0;i<n;++i){auto a=p+uint32_t(i);if(a==fail||!mem.count(a))return false;b[i]=mem[a];}return true;}
#include "destruct_selection_extract.h"
static constexpr uint32_t Arr=0x20010000,House=0x20020000,Other=0x20021000,A=0x20030000,B=0x20031000;
static void unit(uint32_t p,uint32_t id,uint32_t owner){put<uint32_t>(p,0x7E3EBC);put<uint32_t>(p+0x10,id);put<uint32_t>(p+0x21C,owner);put<uint8_t>(p+0x83,1);put<uint8_t>(p+0x90,1);put<uint8_t>(p+0x74,1);put<uint8_t>(p+0x81,0);put<int>(p+0x6C,100);}
static void setup(){mem.clear();fail=0;Vec v{0,Arr,2,0,2,0};put(0xA8ECB8,v);put<uint32_t>(0xA8B230,0x20040000);put<int>(0xA8ED84,123);put<uint32_t>(Arr,A);put<uint32_t>(Arr+4,B);put<uint32_t>(House+0x10,3);put<int>(House+0x30,2);unit(A,11,House);unit(B,12,House);}
int main(){int checks=0;static DestructSelectedSnapshot first,second;
 setup();assert(captureDestructSelection(first)&&first.house==House&&first.houseID==3&&first.index==2&&first.count==2);assert(captureDestructSelection(second)&&sameDestructSelection(first,second));++checks;
 put<uint32_t>(B+0x21C,Other);assert(!captureDestructSelection(second));++checks;
 setup();put<uint8_t>(B+0x83,0);assert(!captureDestructSelection(second));++checks;
 setup();put<uint8_t>(B+0x81,1);assert(!captureDestructSelection(second));++checks;
 setup();put<int>(B+0x6C,0);assert(!captureDestructSelection(second));++checks;
 setup();put<uint32_t>(B,0x12345678);assert(!captureDestructSelection(second));++checks;
 setup();put<uint32_t>(B+0x10,0);assert(!captureDestructSelection(second));++checks;
 setup();fail=B+0x21C;assert(!captureDestructSelection(second));++checks;
 setup();put<int>(0xA8ECB8+16,0);assert(!captureDestructSelection(second));++checks;
 setup();assert(captureDestructSelection(first));put<uint32_t>(B+0x10,88);assert(captureDestructSelection(second)&&!sameDestructSelection(first,second));++checks;
 setup();assert(captureDestructSelection(first));put<uint32_t>(House+0x10,9);assert(captureDestructSelection(second)&&!sameDestructSelection(first,second));++checks;
 setup();assert(captureDestructSelection(first));put<int>(0xA8ED84,124);assert(captureDestructSelection(second)&&!sameDestructSelection(first,second));++checks;
 setup();assert(captureDestructSelection(first));put<uint32_t>(0xA8B230,0x20050000);assert(captureDestructSelection(second)&&!sameDestructSelection(first,second));++checks;
 setup();put<int>(House+0x30,32);assert(!captureDestructSelection(second));++checks;
 printf("PASS selected-owner production snapshot checks=%d\n",checks);}
