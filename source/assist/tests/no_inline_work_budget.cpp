#include "../source/repair_delivery.h"
#include "../source/outlist_observer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>
using namespace assist::repairdelivery;
static unsigned checks=0;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static void repairBudgets(){
 Pending<> p;std::vector<unsigned> visits(4097);
 for(unsigned id=1;id<=4096;++id)CHECK(p.queued(id,0x10000+id*16));
 for(unsigned pass=0;pass<128;++pass){
  auto w=p.observe([&](uint32_t id,uint32_t ptr){CHECK(ptr==0x10000+id*16);++visits[id];return Observation{true,false,false};});
  CHECK(w.sampled==ObservationReadBudget&&w.scanned<=ObservationSlotBudget&&!w.retired);
  CHECK(p.count==4096);
 }
 for(unsigned id=1;id<=4096;++id)CHECK(visits[id]==1&&p.contains(id,0x10000+id*16));
 // No clock/timeout permits retries, even after thousands of sweeps.
 for(int n=0;n<10000;++n){auto w=p.observe([](uint32_t,uint32_t){return Observation{true,false,false};});CHECK(!w.retired&&w.sampled<=32&&w.scanned<=256);}
 CHECK(p.count==4096);
 unsigned retired=0;
 for(unsigned pass=0;pass<128;++pass){
  auto w=p.observe([](uint32_t,uint32_t){return Observation{true,true,false};});
  CHECK(w.sampled<=32&&w.scanned<=256);retired+=w.retired;
 }
 CHECK(retired==4096&&p.count==0&&p.cursor==0);
 // Sparse end-of-table entry: slot budget also bounds an otherwise empty scan.
 unsigned sparse=1;while((sparse*2654435761u&4095)!=4095)++sparse;
 CHECK(p.queued(sparse,1234));unsigned calls=0;
 for(unsigned pass=0;pass<16;++pass){
  auto w=p.observe([&](uint32_t id,uint32_t ptr){++calls;CHECK(id==sparse&&ptr==1234);return Observation{true,false,false};});
  CHECK(w.scanned<=256&&w.sampled<=1&&p.contains(sparse,1234));
 }
 CHECK(calls==1);
 auto before=p;auto zero=p.observe([](uint32_t,uint32_t){std::abort();return Observation{};},0,256);
 CHECK(!zero.scanned&&!zero.sampled&&!std::memcmp(&p,&before,sizeof p));
 zero=p.observe([](uint32_t,uint32_t){std::abort();return Observation{};},32,0);
 CHECK(!zero.scanned&&!zero.sampled&&!std::memcmp(&p,&before,sizeof p));
 // Latest identity is passed to the observer; no delayed sample is cached.
 CHECK(p.queued(sparse,5678)&&!p.contains(sparse,1234)&&p.count==1);
 unsigned observed=0;
 for(unsigned pass=0;pass<16;++pass)p.observe([&](uint32_t,uint32_t ptr){CHECK(ptr==5678);++observed;return Observation{};});
 CHECK(observed==1&&!p.count);
 Pending<8> collision;CHECK(collision.queued(1,100)&&collision.queued(9,900)&&collision.queued(17,1700));
 for(unsigned n=0;n<16;++n){auto w=collision.observe([](uint32_t id,uint32_t){return Observation{id!=9,false,false};},1,2);CHECK(w.scanned<=2&&w.sampled<=1);}
 CHECK(!collision.contains(9,900)&&collision.contains(1,100)&&collision.contains(17,1700));
 auto w=collision.observe([](uint32_t,uint32_t){return Observation{true,false,true};},~0u,~0u);
 CHECK(w.scanned<=8&&w.sampled==2&&w.retired==2&&!collision.count);
 collision.clear();CHECK(collision.cursor==0&&collision.room(25));
}
static uint32_t seed=0x12345678;
static uint32_t randomValue(){seed=seed*1664525u+1013904223u;return seed;}
static void randomizedRepair(){
 Pending<64> p;std::map<uint32_t,uint32_t> expected;
 for(unsigned round=0;round<10000;++round){
  const uint32_t id=1+(randomValue()%192),ptr=0x10000+(randomValue()%16384)*4;
  if(randomValue()%3){
   bool can=expected.count(id)||expected.size()<64;
   CHECK(p.room(id)==can);CHECK(p.queued(id,ptr)==can);if(can)expected[id]=ptr;
  }else{
   const unsigned readBudget=1+randomValue()%8,slotBudget=1+randomValue()%16;
   auto w=p.observe([&](uint32_t i,uint32_t v){
    CHECK(expected.count(i)&&expected[i]==v);
    bool terminal=(randomValue()%5)==0;if(terminal)expected.erase(i);
    return Observation{!terminal,false,false};
   },readBudget,slotBudget);
   CHECK(w.sampled<=readBudget&&w.scanned<=slotBudget&&w.retired<=w.sampled);
  }
  CHECK(p.count==expected.size());
  for(const auto&kv:expected)CHECK(p.contains(kv.first,kv.second));
 }
 for(unsigned n=0;n<64&&p.count;++n)p.observe([&](uint32_t id,uint32_t){expected.erase(id);return Observation{};},4,8);
 CHECK(!p.count&&expected.empty());
}
static void cadence(){
 using namespace assist::outlistobserve;
 SampleCadence c;Identity i{1,0x10000,0x20000,7,3,0};
 CHECK(c.admit(i,100));for(unsigned n=0;n<10000;++n)CHECK(!c.admit(i,100));
 CHECK(c.admit(i,101));CHECK(!c.admit(i,101));
 ++i.match;CHECK(c.admit(i,101));++i.scenario;CHECK(c.admit(i,101));
 ++i.house;CHECK(c.admit(i,101));++i.houseID;CHECK(c.admit(i,101));
 i.session=4;CHECK(c.admit(i,101));++i.houseIndex;CHECK(c.admit(i,101));
 CHECK(c.admit(i,99));CHECK(!c.admit(i,99));
 CHECK(!c.admit(i,-1));CHECK(c.admit(i,99));
 auto invalid=i;invalid.houseIndex=-1;CHECK(!c.admit(invalid,99));CHECK(c.admit(i,99));
 c.clear();CHECK(c.admit(i,99));CHECK(!c.admit(i,99));
}
int main(){repairBudgets();randomizedRepair();cadence();std::printf("PASS: %u work-budget checks; bounded reads/slots, fairness, collisions, identity, no retry timeout, frame cadence\n",checks);}
