#ifndef CAPTURE_HEADER
#define CAPTURE_HEADER "/workspace/scratch/40121bc881b8/ra2-audit-followup/source/shared/source/world_frame_capture.h"
#endif
#include CAPTURE_HEADER
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>
#ifdef CRT_HEADER
#include CRT_HEADER
#endif

static bool trackAllocations=false;
static size_t allocationCalls=0,allocationBytes=0,freeCalls=0;
void* operator new(std::size_t n){if(trackAllocations){++allocationCalls;allocationBytes+=n;}if(void*p=std::malloc(n?n:1))return p;std::abort();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void*p) noexcept {if(p&&trackAllocations)++freeCalls;std::free(p);}
void operator delete[](void*p) noexcept {::operator delete(p);}
void operator delete(void*p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void*p,std::size_t) noexcept {::operator delete(p);}
void* countedPageAllocate(size_t n,void*)noexcept{if(trackAllocations){++allocationCalls;allocationBytes+=n;}return std::malloc(n);}
void countedPageFree(void*p,void*)noexcept{if(p&&trackAllocations)++freeCalls;std::free(p);}
uint64_t payloadHash(const worldframe::Snapshot&sample){std::vector<uint32_t>addresses;for(size_t i=0;i<sample.pages.size();++i)addresses.push_back(sample.pages[i].address);std::sort(addresses.begin(),addresses.end());uint64_t value=1469598103934665603ull;for(uint32_t p:addresses){const auto*page=sample.find(p);const auto*bytes=reinterpret_cast<const uint8_t*>(page);for(size_t i=0;i<sizeof(*page);++i){value^=bytes[i];value*=1099511628211ull;}}return value;}
struct Memory {
 std::vector<uint8_t>bytes=std::vector<uint8_t>(0x05000000);
 size_t calls=0,readBytes=0;
 template<class T>void put(uint32_t p,const T&v){std::memcpy(bytes.data()+p,&v,sizeof v);}
 bool read(uint32_t p,void*out,size_t n){++calls;readBytes+=n;if(uint64_t(p)+n>bytes.size())return false;std::memcpy(out,bytes.data()+p,n);return true;}
 void vector(uint32_t at,uint32_t p,const std::vector<uint32_t>&entries){
  struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;}v{0,p,int(entries.size()),0,int(entries.size()),1};put(at,v);
  for(uint32_t e:entries){put(p,e);p+=4;}
 }
 void setup(unsigned count){
  std::fill(bytes.begin(),bytes.end(),0);
  constexpr uint32_t House=0x120000,Type=0x300100,Weapon=0x400100,Warhead=0x580100,Projectile=0x590100;
  put(0xA8B230,0x100000u);put(0xA8B238,int(0));put(0x87F924,0x180000u);put(0x87F928,int(65536));put(0xA8ED84,int(50));put(0xA8E9A0,uint8_t(1));put(0xA83D4C,House);
  std::vector<uint32_t>actors,buildings,units,houses;
  for(unsigned i=0;i<8;++i)houses.push_back(House+i*0x6000);
  for(unsigned i=0;i<count;++i){
   const uint32_t actor=0x1000100+i*0x800;actors.push_back(actor);const bool building=i%8==0;
   if(building)buildings.push_back(actor);else units.push_back(actor);
   put(actor,building?0x7E3EBCu:0x7F5C70u);put(actor+(building?0x520:0x6C4),Type+(i%64)*0x2000);
   put(actor+0x21C,houses[i%houses.size()]);put(actor+0x10,uint32_t(1000+i));put(actor+0x6C,100);
   put(actor+0x218,0x1000100+((i+1)%count)*0x800);
  }
  vector(0xA8EC78,0x600000,actors);vector(0xA8EB40,0x610000,buildings);vector(0x8B4108,0x620000,units);vector(0xA80228,0x630000,houses);vector(0xA8ED40,0x640000,{});
  for(unsigned t=0;t<64;++t)for(unsigned slot=0;slot<4;++slot){const uint32_t weapon=Weapon+(t*4+slot)*0x800;
   for(uint32_t base:{0x898u,0xA94u})put(Type+t*0x2000+base+slot*28,weapon);
   put(weapon+0xA0,Warhead+(t%16)*0x100);put(weapon+0xAC,Projectile+(t%16)*0x100);
  }
 }
};
struct Reader{Memory*m;bool operator()(uint32_t p,void*out,size_t n)const{return m->read(p,out,n);}};
void runCase(const char*name,unsigned actors,uint32_t demand,unsigned extraPages){
 Memory memory;memory.setup(actors);worldframe::PageAllocator allocator{countedPageAllocate,countedPageFree,nullptr};worldframe::Snapshot snapshot(allocator);worldframe::Key key;
#ifdef REUSE_CAPTURE_SCRATCH
 worldframe::CaptureScratch scratch(allocator);
#endif
 key.pid=1000;key.owner=900;key.match=7;key.scenario=0x100000;key.cells=0x180000;key.house=0x120000;key.cellCount=65536;key.frame=50;key.session=0;
 std::vector<uint32_t>requests;for(unsigned i=0;i<extraPages;++i)requests.push_back(0x3000000+i*4096);
 constexpr unsigned Iterations=25;std::vector<double>microseconds;microseconds.reserve(Iterations-1);
 size_t allocations=0,allocatedBytes=0,deallocations=0;worldframe::Counters counters;size_t pages=0;
 for(unsigned i=0;i<Iterations;++i){
  ++key.frame;key.serial=i+1;memory.put(0xA8ED84,key.frame);memory.calls=memory.readBytes=0;allocationCalls=allocationBytes=freeCalls=0;
  const auto start=std::chrono::steady_clock::now();trackAllocations=i>0;
  snapshot.reset(key);{
#ifdef REUSE_CAPTURE_SCRATCH
   worldframe::CaptureGraph<Reader> capture(snapshot,Reader{&memory},scratch);
#else
   worldframe::CaptureGraph<Reader> capture(snapshot,Reader{&memory});
#endif
   if(!capture.capture(demand,requests)||!snapshot.seal())std::abort();}
  trackAllocations=false;const auto end=std::chrono::steady_clock::now();
  if(i){microseconds.push_back(std::chrono::duration<double,std::micro>(end-start).count());allocations+=allocationCalls;allocatedBytes+=allocationBytes;deallocations+=freeCalls;}
  counters=snapshot.counters;pages=snapshot.pages.size();
  if(memory.calls!=counters.calls||memory.readBytes!=counters.bytes)std::abort();
 }
 std::sort(microseconds.begin(),microseconds.end());double sum=0;for(auto n:microseconds)sum+=n;
 const auto payload=payloadHash(snapshot);
 std::printf("{\"case\":\"%s\",\"actors\":%u,\"demand\":%u,\"requested_extra_pages\":%u,\"pages\":%zu,\"read_calls_per_frame\":%u,\"read_bytes_per_frame\":%u,\"rosters\":%u,\"heap_allocations_per_frame\":%zu,\"heap_allocation_bytes_per_frame\":%zu,\"heap_frees_per_frame\":%zu,\"mean_us\":%.3f,\"median_us\":%.3f,\"p95_us\":%.3f,\"iterations\":%u,\"payload_fnv64\":\"%016llx\"}\n",name,actors,demand,extraPages,pages,counters.calls,counters.bytes,counters.rosterCaptures,allocations/(Iterations-1),allocatedBytes/(Iterations-1),deallocations/(Iterations-1),sum/microseconds.size(),microseconds[microseconds.size()/2],microseconds[(microseconds.size()*95)/100],Iterations-1,static_cast<unsigned long long>(payload));
}
int main(){runCase("base-128",128,worldframe::Base,0);runCase("base-2048",2048,worldframe::Base,0);runCase("combat-2048",2048,worldframe::Combat,0);runCase("all-2048-map4096",2048,63,4096);runCase("combat-8192",8192,worldframe::Combat,0);}
