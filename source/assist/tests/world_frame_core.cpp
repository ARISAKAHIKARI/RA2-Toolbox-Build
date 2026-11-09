// Executes the production capture, immutable read-set and (with the driver)
// Runtime/Host transport. Only operating-system calls and game bytes are fake.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../shared/source/world_frame_capture.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace fixture {
size_t checks=0;
void require(bool value,const char*text,int line){++checks;if(!value){std::fprintf(stderr,"FAIL world_frame_core:%d: %s\n",line,text);std::abort();}}
#define CHECK(...) ::fixture::require(bool((__VA_ARGS__)),#__VA_ARGS__,__LINE__)
struct Call {uint32_t address;size_t bytes;bool success;};
struct Memory {
 std::vector<uint8_t> bytes;
 std::vector<Call> calls;
 std::unordered_map<uint32_t,unsigned> successfulPages;
 std::unordered_set<uint32_t> partialPages;
 std::vector<std::pair<uint32_t,size_t>> denied;
 std::function<void(uint32_t,size_t)> beforeRead;
 explicit Memory(size_t n=0x1000000):bytes(n){}
 template<class T>void put(uint32_t p,const T&v){CHECK(uint64_t(p)+sizeof v<=bytes.size());std::memcpy(bytes.data()+p,&v,sizeof v);}
 void vector(uint32_t at,uint32_t list,std::initializer_list<uint32_t> entries){
  struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
  const Vec v{0,list,int(entries.size()),0,int(entries.size()),1};put(at,v);for(uint32_t p:entries){put(list,p);list+=4;}
 }
 bool read(uint32_t p,void*out,size_t n){
  if(beforeRead)beforeRead(p,n);
  bool ok=uint64_t(p)+n<=bytes.size();
  // A page-wide fault also rejects a speculative multi-page read containing
  // that page. Smaller requested ranges may still be readable on fallback.
  for(uint32_t page:partialPages)if(p<=page&&uint64_t(p)+n>=uint64_t(page)+worldframe::PageSize)ok=false;
  for(const auto&span:denied)if(uint64_t(p)<uint64_t(span.first)+span.second&&uint64_t(span.first)<uint64_t(p)+n)ok=false;
  calls.push_back({p,n,ok});
  if(!ok){std::memset(out,0xEF,n);return false;}
  std::memcpy(out,bytes.data()+p,n);
  if(!(p&4095u)&&!(n&4095u))for(size_t i=0;i<n;i+=worldframe::PageSize)++successfulPages[p+uint32_t(i)];
  return true;
 }
 size_t requests(uint32_t address,size_t n)const{size_t count=0;for(const auto&c:calls)if(c.address==address&&c.bytes==n)++count;return count;}
 void uniqueSuccessfulBytes()const{
  std::unordered_map<uint32_t,unsigned> count;
  for(const auto&c:calls)if(c.success)for(size_t i=0;i<c.bytes;++i)CHECK(++count[c.address+uint32_t(i)]==1);
 }
};
struct Read {Memory*memory;bool operator()(uint32_t p,void*out,size_t n)const{return memory->read(p,out,n);}};
constexpr uint32_t Scenario=0x100000,House=0x120000,Cells=0x140000;
constexpr uint32_t ActorA=0x200100,ActorB=0x201100,Passenger=0x210100,Type=0x300100;
constexpr uint32_t Weapon=0x400100,Warhead=0x410100,Projectile=0x420100,Temporal=0x430100,Radio=0x450100;
constexpr uint32_t Bullet=0x500100,BulletType=0x510100;
constexpr uint32_t TechnoList=0x600000,BuildingList=0x601000,UnitList=0x602000,HouseList=0x603000,BulletList=0x604000;
constexpr uint32_t HostPid=900,GamePid=1000;
worldframe::Key key(int frame=50){worldframe::Key k;k.pid=GamePid;k.owner=HostPid;k.match=7;k.serial=1;k.tick=100;k.scenario=Scenario;k.cells=Cells;k.house=House;k.cellCount=4;k.frame=frame;k.session=0;return k;}
void setup(Memory&m){
 m.put(0xA8B230,Scenario);m.put(0xA8B238,int(0));m.put(0x87F924,Cells);m.put(0x87F928,int(4));m.put(0xA8ED84,int(50));m.put(0xA8E9A0,uint8_t(1));m.put(0xA83D4C,House);
 m.vector(0xA8EC78,TechnoList,{ActorA,ActorB,ActorA});m.vector(0xA8EB40,BuildingList,{ActorB});m.vector(0x8B4108,UnitList,{ActorA});m.vector(0xA80228,HouseList,{House});m.vector(0xA8ED40,BulletList,{Bullet});
 for(uint32_t p:{ActorA,ActorB,Passenger}){m.put(p,0x7F5C70u);m.put(p+0x6C4,Type);m.put(p+0x21C,House);m.put(p+0x10,p+5);m.put(p+0x6C,100);}
 m.put(ActorA+0x30,Passenger);m.put(Passenger+0x118,ActorA);m.put(ActorA+0x218,ActorB);m.put(ActorA+0x274,Temporal);m.put(Temporal+0x44,Temporal);
 m.put(ActorA+0xE4,Radio);m.put(ActorA+0xE8,2);m.put(Radio,ActorB);m.put(Radio+4,Passenger);
 m.put(Type+0x60,0x700100u);m.put(Type+0x898,Weapon);m.put(Type+0xA94,Weapon);m.put(Weapon+0xA0,Warhead);m.put(Weapon+0xAC,Projectile);
 m.put(Bullet+0xAC,BulletType);m.put(Bullet+0xB0,ActorA);m.put(Bullet+0x10C,ActorB);
}
void overlap(){
 Memory m;for(uint32_t i=0x10000;i<0x13000;++i)m.bytes[i]=uint8_t(i);
 worldframe::Snapshot s;s.reset(key());Read read{&m};std::array<uint8_t,24> out{};
 CHECK(s.capture(read,0x10013,out.data(),out.size()));CHECK(m.calls.size()==1&&m.calls[0].address==0x10000&&m.calls[0].bytes==4096);
 CHECK(s.capture(read,0x10018,out.data(),16));CHECK(s.prefetch(read,0x10000,4096));CHECK(m.calls.size()==1);
 CHECK(s.capture(read,0x10FF8,out.data(),24));CHECK(m.calls.size()==2&&m.calls[1].address==0x11000);
 CHECK(s.capture(read,0x11000,out.data(),24));CHECK(m.calls.size()==2);m.uniqueSuccessfulBytes();
 CHECK(s.counters.calls==2&&s.counters.pageAttempts==2&&s.counters.rangeAttempts==0&&s.counters.bytes==8192);
 CHECK(!s.read(0x10000,out.data(),4));CHECK(s.seal());
 std::fill(m.bytes.begin()+0x10000,m.bytes.begin()+0x12000,0xFF);
 CHECK(s.read(0x10FF8,out.data(),24));for(unsigned i=0;i<24;++i)CHECK(out[i]==uint8_t(0xF8+i));
 const auto before=m.calls.size();CHECK(!s.capture(read,0x12000,out.data(),4));CHECK(!s.read(0x12000,out.data(),4));CHECK(!s.observed(0x12000,4));CHECK(m.calls.size()==before);
 CHECK(!s.read(0x10000,nullptr,4));CHECK(!s.read(0xFFFF,out.data(),4));CHECK(!s.read(0x7FFFFFFF,out.data(),4));CHECK(!s.read(0x10000,out.data(),0));
 worldframe::Snapshot imported;CHECK(imported.import(s.key,s.counters,s.pages.data(),s.pages.size()));CHECK(imported.read(0x10FF8,out.data(),24));CHECK(imported.key.sameFrame(s.key));
 std::vector<worldframe::Page>pages(s.pages.data(),s.pages.data()+s.pages.size());pages[1].address=pages[0].address;CHECK(!imported.import(s.key,{},pages.data(),pages.size()));CHECK(!imported.sealed());
 pages.assign(s.pages.data(),s.pages.data()+s.pages.size());pages[0].address++;CHECK(!imported.import(s.key,{},pages.data(),pages.size()));pages.assign(s.pages.data(),s.pages.data()+s.pages.size());pages[0].full=2;CHECK(!imported.import(s.key,{},pages.data(),pages.size()));
 CHECK(!imported.import(s.key,{},nullptr,1));CHECK(!imported.import(s.key,{},s.pages.data(),size_t(worldframe::MaxPages)+1));CHECK(imported.import(s.key,{},nullptr,0));CHECK(!imported.read(0x10000,out.data(),4));
}
void partial(){
 Memory m;for(uint32_t i=0x20000;i<0x21000;++i)m.bytes[i]=uint8_t(i);m.partialPages.insert(0x20000);m.denied.push_back({0x20020,8});
 worldframe::Snapshot s;s.reset(key());Read read{&m};std::array<uint8_t,24> out{};
 CHECK(s.capture(read,0x20010,out.data(),8));CHECK(m.calls.size()==2&&m.calls[0].bytes==4096&&!m.calls[0].success&&m.calls[1].bytes==8);
 CHECK(s.capture(read,0x20014,out.data(),12));CHECK(m.calls.size()==3&&m.calls.back().address==0x20018&&m.calls.back().bytes==8);
 CHECK(!s.capture(read,0x20020,out.data(),8));const auto attempted=m.calls.size();CHECK(!s.capture(read,0x20022,out.data(),3));CHECK(m.calls.size()==attempted);
 CHECK(s.counters.pageAttempts==1&&s.counters.rangeAttempts==3&&s.counters.failedReads==2);m.uniqueSuccessfulBytes();CHECK(s.seal());
 CHECK(s.read(0x20010,out.data(),16));for(unsigned i=0;i<16;++i)CHECK(out[i]==uint8_t(0x10+i));CHECK(!s.read(0x20020,out.data(),8));CHECK(s.observed(0x20020,8));CHECK(!s.observed(0x20030,4));CHECK(!s.read(0x20030,out.data(),4));
}
struct GraphStats{uint32_t calls,bytes,rosters;};
GraphStats graph(uint32_t demand){
 Memory m;setup(m);worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read> g(s,Read{&m});CHECK(g.capture(demand,{ActorA&~4095u,ActorA&~4095u}));CHECK(s.seal());
 for(const auto&entry:m.successfulPages)CHECK(entry.second==1);CHECK(s.counters.rangeAttempts==0&&s.counters.calls==m.calls.size());
 uint32_t actors[3]{};CHECK(s.read(TechnoList,actors,sizeof actors));CHECK(actors[0]==ActorA&&actors[1]==ActorB&&actors[2]==ActorA);uint32_t building=0;CHECK(s.read(BuildingList,&building,4)&&building==ActorB);
 CHECK(s.find(ActorA&~4095u)&&s.find(ActorB&~4095u));CHECK(bool(s.find(Weapon&~4095u))==bool(demand&worldframe::Combat));
 CHECK(bool(s.find(Bullet&~4095u))==bool(demand&(worldframe::Bullets|worldframe::Visual)));
 CHECK(bool(s.find(Temporal&~4095u))==bool(demand&(worldframe::Combat|worldframe::Visual)));
 CHECK(bool(s.find(Passenger&~4095u))==bool(demand&(worldframe::Combat|worldframe::Passengers|worldframe::Ore|worldframe::Visual)));
 if(demand==worldframe::Base)CHECK(s.counters.rosterCaptures==3);
 return {s.counters.calls,s.counters.bytes,s.counters.rosterCaptures};
}
void graphs(){
 const auto base=graph(worldframe::Base),combat=graph(worldframe::Combat),passengers=graph(worldframe::Passengers),ore=graph(worldframe::Ore),visual=graph(worldframe::Visual);
 CHECK(base.calls<combat.calls&&base.calls<visual.calls&&passengers.calls<combat.calls&&base.rosters<ore.rosters);
 std::printf("GRAPH Base %u reads/%u bytes/%u rosters; Combat %u/%u/%u; Passengers %u/%u/%u; Ore %u/%u/%u; Visual %u/%u/%u\n",base.calls,base.bytes,base.rosters,combat.calls,combat.bytes,combat.rosters,passengers.calls,passengers.bytes,passengers.rosters,ore.calls,ore.bytes,ore.rosters,visual.calls,visual.bytes,visual.rosters);
 for(int bad:{-1,16385}){Memory m;setup(m);m.put(0xA8EC78+16,bad);worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read>g(s,Read{&m});CHECK(!g.capture(worldframe::Base,{}));}
 {Memory m;setup(m);m.put(0xA8EC78+8,2);worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read>g(s,Read{&m});CHECK(!g.capture(worldframe::Base,{}));}
 {Memory m;setup(m);m.denied.push_back({0xA8EC78,24});worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read>g(s,Read{&m});CHECK(!g.capture(worldframe::Base,{}));CHECK(s.counters.failedReads>0);}
 // More unique layer entries than the work queue can own must be an explicit
 // failure. All referenced actor pages are intentionally absent from memory.
 {Memory m;setup(m);constexpr uint32_t list=0x800000;for(uint32_t i=0;i<65536;++i)m.put(list+i*4,0x10000000u+i*4);
  m.vector(0x8A0360,list,{});m.put(0x8A0360+8,65536);m.put(0x8A0360+16,65536);
  worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read>g(s,Read{&m});CHECK(!g.capture(worldframe::Visual,{}));CHECK(s.counters.overflows==1);CHECK(s.counters.calls<256);
 }
}
void capacity(){
 worldframe::Snapshot s;s.reset(key());size_t calls=0;
 auto live=[&](uint32_t p,void*out,size_t n){++calls;std::memset(out,int(p>>12)&255,n);return true;};
 constexpr uint32_t begin=0x10000000,half=32u*1024u*1024u;
 CHECK(s.prefetch(live,begin,half));CHECK(s.prefetch(live,begin+half,half));CHECK(s.pages.size()==worldframe::MaxPages);CHECK(!s.overflow());
 CHECK(!s.prefetch(live,begin+2*half,4));CHECK(s.overflow()&&s.counters.overflows==1&&calls==worldframe::MaxPages);CHECK(!s.seal());
}
struct AllocationSpy {
 struct Entry {void*pointer=nullptr;size_t bytes=0;};std::array<Entry,32>entries{};
 std::array<size_t,32>requests{};size_t calls=0,frees=0,liveBytes=0,failAt=0;
 static void*allocate(size_t bytes,void*context)noexcept{
  auto&spy=*static_cast<AllocationSpy*>(context);CHECK(spy.calls<spy.requests.size());spy.requests[spy.calls++]=bytes;
  if(spy.failAt==spy.calls)return nullptr;
  void*p=::operator new(bytes,std::nothrow);if(!p)return nullptr;
  for(auto&entry:spy.entries)if(!entry.pointer){entry={p,bytes};spy.liveBytes+=bytes;return p;}
  std::abort();
 }
 static void release(void*p,void*context)noexcept{
  auto&spy=*static_cast<AllocationSpy*>(context);for(auto&entry:spy.entries)if(entry.pointer==p){CHECK(spy.liveBytes>=entry.bytes);spy.liveBytes-=entry.bytes;entry={};++spy.frees;::operator delete(p);return;}std::abort();
 }
 worldframe::PageAllocator allocator(){return {allocate,release,this};}
};
void nothrowPages(){
 AllocationSpy rejected;size_t liveReads=0;auto read=[&](uint32_t p,void*out,size_t n){++liveReads;std::memset(out,int(p>>12)&255,n);return true;};
 {
  worldframe::Snapshot sample(rejected.allocator());CHECK(rejected.calls==0&&!sample.pages.data()&&sample.pages.capacity()==0);sample.reset(key());rejected.failAt=1;
  CHECK(!sample.prefetch(read,0x10000,4));CHECK(sample.overflow()&&sample.counters.overflows==1&&sample.pages.size()==0&&sample.pages.capacity()==0&&liveReads==0);
  CHECK(!sample.prefetch(read,0x11000,4)&&rejected.calls==1&&liveReads==0);CHECK(!sample.seal());
  sample.reset(key(51));rejected.failAt=0;CHECK(sample.prefetch(read,0x10000,4));CHECK(sample.pages.capacity()==1&&rejected.requests[1]==sizeof(worldframe::Page));CHECK(sample.seal());
 }CHECK(rejected.liveBytes==0&&rejected.frees==1);
 AllocationSpy growth;
 {
  worldframe::Snapshot sample(growth.allocator());sample.reset(key());CHECK(sample.prefetch(read,0x10000,4)&&sample.prefetch(read,0x11000,4));
  CHECK(sample.pages.size()==2&&sample.pages.capacity()==2&&growth.calls==2);auto*retained=sample.pages.data();const auto before=liveReads;growth.failAt=3;
  CHECK(!sample.prefetch(read,0x12000,4));CHECK(sample.overflow()&&sample.counters.overflows==1&&sample.pages.data()==retained&&sample.pages.capacity()==2&&sample.pages.size()==2&&liveReads==before);
  CHECK(sample.find(0x10000)&&sample.pages[0].bytes[0]==0x10&&sample.pages[1].bytes[0]==0x11);CHECK(!sample.seal());
  sample.reset(key(51));growth.failAt=0;CHECK(sample.pages.capacity()==2&&sample.pages.data()==retained&&sample.pages.size()==0);CHECK(sample.prefetch(read,0x20000,4)&&sample.prefetch(read,0x21000,4));CHECK(growth.calls==3&&sample.pages.data()==retained);
  CHECK(sample.prefetch(read,0x22000,4));CHECK(sample.pages.capacity()==4&&sample.pages.size()==3&&growth.calls==4&&growth.requests[3]==4*sizeof(worldframe::Page));CHECK(sample.pages[0].bytes[0]==0x20&&sample.pages[1].bytes[0]==0x21&&sample.pages[2].bytes[0]==0x22);CHECK(sample.seal());
 }CHECK(growth.liveBytes==0&&growth.frees==3);
 worldframe::Snapshot source;source.reset(key());CHECK(source.prefetch(read,0x30000,3*4096)&&source.seal());AllocationSpy imports;
 {
  worldframe::Snapshot copy(imports.allocator());CHECK(copy.import(source.key,source.counters,source.pages.data(),2));CHECK(copy.pages.capacity()==2&&imports.calls==1);auto*retained=copy.pages.data();imports.failAt=2;
  CHECK(!copy.import(source.key,source.counters,source.pages.data(),3));CHECK(copy.overflow()&&copy.counters.overflows==1&&!copy.sealed()&&copy.pages.size()==0&&copy.pages.capacity()==2&&copy.pages.data()==retained);
  imports.failAt=0;CHECK(copy.import(source.key,source.counters,source.pages.data(),2));CHECK(imports.calls==2&&copy.pages.data()==retained);CHECK(copy.import(source.key,source.counters,source.pages.data(),3));CHECK(copy.pages.capacity()==4&&imports.calls==3);
  uint32_t value=0;CHECK(copy.read(0x32000,&value,4)&&value==0x32323232u);CHECK(!copy.import(source.key,{},source.pages.data(),size_t(worldframe::MaxPages)+1));CHECK(copy.overflow()&&copy.counters.overflows==1&&imports.calls==3&&copy.pages.capacity()==4);
 }CHECK(imports.liveBytes==0&&imports.frees==2);
 std::puts("PASS nothrow pages: zero initial page allocation, bounded geometric growth, injected initial/growth/import failure, retained bytes/capacity, reset reuse, no repeated allocation after failure and successful recovery");
}
}

#ifdef WORLD_FRAME_PLATFORM_FIXTURE
#define WINMOCK_IMAGE_FILES
#include "../../external_reader/source/process_reader.h"
#include "../source/world_lifecycle.h"
#include <atomic>
#include <map>
namespace fixture {
struct ProcessRecord {uint32_t pid;uint64_t creation;bool alive=true;};
size_t mappedBytes=0,peakMappedBytes=0,mappingCreates=0,dataCreates=0,dataOpens=0,dataMaps=0;
unsigned failDataCreate=0,failProducerMap=0,failConsumerMap=0,failDataOpen=0;
struct MappingRecord {
 std::vector<uint8_t> bytes;bool payload;
 explicit MappingRecord(size_t n,bool data=false):bytes(n),payload(data){mappedBytes+=n;peakMappedBytes=std::max(peakMappedBytes,mappedBytes);++mappingCreates;if(data)++dataCreates;}
 ~MappingRecord(){CHECK(mappedBytes>=bytes.size());mappedBytes-=bytes.size();}
};
std::shared_ptr<MappingRecord>forceDataCollision;
std::function<void()>onConsumerMap;
struct EventRecord {bool signaled=false;};
std::map<std::wstring,std::weak_ptr<EventRecord>>events;
unsigned eventSignals=0,eventCreates=0,eventOpens=0,failEventCreate=0;
struct Handle {std::shared_ptr<ProcessRecord> process;std::shared_ptr<MappingRecord> mapping;std::shared_ptr<EventRecord> event;};
uint32_t currentPid=HostPid,clock=100,lastError=0;size_t rpmCalls=0;
Memory*gameMemory=nullptr;
std::shared_ptr<ProcessRecord>process=std::make_shared<ProcessRecord>(ProcessRecord{GamePid,12345,true});
std::map<std::wstring,std::weak_ptr<MappingRecord>>mappings;
bool payloadName(const wchar_t*name){return std::wstring(name).find(L"Local\\RA2WorldData_")==0;}
worldframe::Channel*channel(){wchar_t path[100];worldframe::name(path,GamePid,HostPid);auto entry=mappings.find(path);if(entry==mappings.end())return nullptr;auto mapped=entry->second.lock();return mapped?reinterpret_cast<worldframe::Channel*>(mapped->bytes.data()):nullptr;}
std::shared_ptr<MappingRecord>payload(const worldframe::Slot&slot,unsigned index){wchar_t path[160];worldframe::dataName(path,slot.key,index,slot.data.allocation);auto found=mappings.find(path);return found==mappings.end()?nullptr:found->second.lock();}
}
extern "C" {
HANDLE CreateEventW(void*,BOOL manual,BOOL initial,const wchar_t*name){
 CHECK(!manual);++fixture::eventCreates;if(fixture::failEventCreate){--fixture::failEventCreate;return nullptr;}
 auto&weak=fixture::events[name];auto event=weak.lock();if(!event){event=std::make_shared<fixture::EventRecord>();event->signaled=initial;weak=event;}
 return new fixture::Handle{{},{},std::move(event)};
}
HANDLE OpenEventW(DWORD access,BOOL,const wchar_t*name){
 CHECK(access==SYNCHRONIZE);++fixture::eventOpens;auto event=fixture::events[name].lock();
 return event?new fixture::Handle{{},{},std::move(event)}:nullptr;
}
BOOL SetEvent(HANDLE handle){auto*h=static_cast<fixture::Handle*>(handle);CHECK(h&&h->event);h->event->signaled=true;++fixture::eventSignals;return TRUE;}
HANDLE OpenProcess(DWORD,BOOL,DWORD pid){if(pid!=fixture::GamePid){fixture::lastError=ERROR_INVALID_PARAMETER;return nullptr;}return new fixture::Handle{fixture::process,{}};}
BOOL CloseHandle(HANDLE handle){if(!handle||handle==INVALID_HANDLE_VALUE)return FALSE;delete static_cast<fixture::Handle*>(handle);return TRUE;}
DWORD GetLastError(){return fixture::lastError;}
DWORD GetCurrentProcessId(){return fixture::currentPid;}
DWORD GetTickCount(){return fixture::clock;}
BOOL QueryPerformanceCounter(LARGE_INTEGER*out){out->QuadPart=int64_t(fixture::clock)*1000+77;return TRUE;}
BOOL GetExitCodeProcess(HANDLE handle,DWORD*out){auto*h=static_cast<fixture::Handle*>(handle);if(!h||!h->process)return FALSE;*out=h->process->alive?STILL_ACTIVE:0;return TRUE;}
BOOL GetProcessTimes(HANDLE handle,FILETIME*creation,FILETIME*,FILETIME*,FILETIME*){auto*h=static_cast<fixture::Handle*>(handle);if(!h||!h->process)return FALSE;creation->dwLowDateTime=uint32_t(h->process->creation);creation->dwHighDateTime=uint32_t(h->process->creation>>32);return TRUE;}
BOOL ReadProcessMemory(HANDLE,LPCVOID from,void*out,SIZE_T n,SIZE_T*got){++fixture::rpmCalls;const bool ok=fixture::gameMemory&&fixture::gameMemory->read(uint32_t(reinterpret_cast<uintptr_t>(from)),out,n);*got=ok?n:0;if(!ok)fixture::lastError=ERROR_PARTIAL_COPY;return ok?TRUE:FALSE;}
SIZE_T VirtualQueryEx(HANDLE,LPCVOID,MEMORY_BASIC_INFORMATION*out,SIZE_T n){*out={};return n;}
HANDLE CreateFileMappingW(HANDLE,void*,DWORD,DWORD high,DWORD low,const wchar_t*name){
 CHECK(high==0);const bool data=fixture::payloadName(name);if(data&&fixture::failDataCreate){--fixture::failDataCreate;fixture::lastError=ERROR_NOT_ENOUGH_MEMORY;return nullptr;}
 if(data&&fixture::forceDataCollision){auto collision=std::move(fixture::forceDataCollision);fixture::lastError=ERROR_ALREADY_EXISTS;return new fixture::Handle{{},std::move(collision)};}
 auto&weak=fixture::mappings[name];auto mapped=weak.lock();fixture::lastError=mapped?ERROR_ALREADY_EXISTS:ERROR_SUCCESS;if(!mapped){mapped=std::make_shared<fixture::MappingRecord>(low,data);weak=mapped;}return new fixture::Handle{{},std::move(mapped)};
}
HANDLE OpenFileMappingW(DWORD,BOOL,const wchar_t*name){
 if(fixture::payloadName(name)){++fixture::dataOpens;if(fixture::failDataOpen){--fixture::failDataOpen;fixture::lastError=ERROR_ACCESS_DENIED;return nullptr;}}
 auto found=fixture::mappings.find(name);auto mapped=found==fixture::mappings.end()?nullptr:found->second.lock();if(!mapped)return nullptr;return new fixture::Handle{{},std::move(mapped)};
}
void*MapViewOfFile(HANDLE handle,DWORD,DWORD,DWORD,SIZE_T bytes){
 auto*h=static_cast<fixture::Handle*>(handle);if(!h||!h->mapping||bytes>h->mapping->bytes.size())return nullptr;
 if(h->mapping->payload){++fixture::dataMaps;auto&fail=fixture::currentPid==fixture::GamePid?fixture::failProducerMap:fixture::failConsumerMap;if(fail){--fail;fixture::lastError=ERROR_NOT_ENOUGH_MEMORY;return nullptr;}}
 if(h->mapping->payload&&fixture::currentPid==fixture::HostPid&&fixture::onConsumerMap){auto callback=std::move(fixture::onConsumerMap);fixture::onConsumerMap={};callback();}
 return h->mapping->bytes.data();
}
BOOL UnmapViewOfFile(LPCVOID){return TRUE;}
}
namespace assist {
struct FixtureShared {volatile LONG worldSampleSeq=0;uint32_t owner=fixture::HostPid,worldSampleFailures=0,worldSamplePages=0,worldSampleReads=0,worldSampleBytes=0,worldSampleRosters=0,worldSampleSerial=0;int worldSampleFrame=-1;};
static FixtureShared shared;static FixtureShared*sh=&shared;static uint32_t match=7;
static struct {int frame=50;}snap;
#include "../source/world_frame_runtime.h"
}
namespace fixture {
std::shared_ptr<worldframe::Snapshot>publication(uint32_t serial,int frame,uint32_t value){
 Memory memory;memory.put(0x10000,value);memory.denied.push_back({0x12000,4096});auto s=std::make_shared<worldframe::Snapshot>();auto k=key(frame);k.serial=serial;k.tick=clock;k.generationLow=777;k.connectionLow=1;s->reset(k);Read read{&memory};CHECK(s->prefetch(read,0x10000,4096));CHECK(!s->prefetch(read,0x12000,4096));CHECK(s->seal());return s;
}
void transport(){
 currentPid=HostPid;clock=100;auto client=worldframe::acquireClient(GamePid,1);CHECK(!client->acquire(7));CHECK(channel());
 client->subscribe(1,worldframe::Base);CHECK(channel()->demandMask==worldframe::Base);client->subscribe(2,worldframe::Visual);CHECK(channel()->demandMask==(worldframe::Base|worldframe::Visual));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));auto first=publication(1,50,0x11112222);server.observe(first->key);server.publish(*first);
 currentPid=HostPid;const auto beforeRpm=rpmCalls;auto a=client->acquire(7);CHECK(a&&a->key.sameFrame(first->key));CHECK(client->acquire(7)==a);CHECK(!client->acquire(8));uint32_t value=0;CHECK(a->read(0x10000,&value,4)&&value==0x11112222);
 clock=105;client->request({0x13000,0x14000,0x13000,0xFFFF,0x15001});currentPid=GamePid;const auto&requests=server.requests();CHECK(requests.size()==2);CHECK(std::find(requests.begin(),requests.end(),0x13000)!=requests.end());CHECK(std::find(requests.begin(),requests.end(),0x14000)!=requests.end());
 auto second=publication(2,51,0x33334444);server.observe(second->key);server.publish(*second);currentPid=HostPid;auto b=client->acquire(7);CHECK(b&&b!=a&&client->acquire(7)==b);CHECK(b->read(0x10000,&value,4)&&value==0x33334444);CHECK(a->read(0x10000,&value,4)&&value==0x11112222);CHECK(rpmCalls==beforeRpm);
 auto*c=channel();CHECK(c);auto&slot=c->slots[c->active];++slot.seq;CHECK(!client->acquire(7));++slot.seq;const auto count=slot.count;slot.count=worldframe::MaxPages+1;CHECK(!client->acquire(7));slot.count=count;CHECK(client->acquire(7)==b);
 // An actual independent producer observation can keep a paused frame usable;
 // neither Host polling nor a fresh heartbeat can change its acquisition tick.
 clock=900;client->subscribe(1,worldframe::Base);CHECK(channel()->demandMask==worldframe::Base);CHECK(!client->acquire(7));currentPid=GamePid;server.observe(second->key);currentPid=HostPid;CHECK(client->acquire(7)==b&&b->key.tick==105);
 currentPid=GamePid;auto changed=second->key;++changed.frame;server.observe(changed);currentPid=HostPid;CHECK(!client->acquire(7));currentPid=GamePid;server.observe(second->key);currentPid=HostPid;CHECK(client->acquire(7)==b);
 // A new connection for the same PID invalidates old cached publications and
 // explicit tokens even while the old reader and immutable bytes still live.
 auto replacement=worldframe::acquireClient(GamePid,2);CHECK(!replacement->acquire(7));currentPid=GamePid;CHECK(server.wanted(HostPid,clock));auto next=publication(3,52,0x9999AAAA);next->key.connectionLow=2;server.observe(next->key);server.publish(*next);currentPid=HostPid;
 CHECK(replacement->acquire(7));CHECK(!client->acquire(7));CHECK(!replacement->accepts(a)&&!client->accepts(a));
 const auto heartbeat=c->heartbeat,revision=c->demandRevision,mask=c->demandMask;++clock;client->subscribe(1,worldframe::Visual);client->request({0x777000});CHECK(!client->acquire(7));CHECK(c->heartbeat==heartbeat&&c->demandRevision==revision&&c->demandMask==mask);
 currentPid=GamePid;server.retire();currentPid=HostPid;CHECK(!replacement->acquire(7));CHECK(a->read(0x10000,&value,4)&&value==0x11112222);currentPid=GamePid;server.disconnect();currentPid=HostPid;
}
void fillPages(worldframe::Snapshot&frame,uint32_t count,uint32_t serial,uint32_t connection=10,uint32_t producer=777){
 auto k=key(int(100+serial));k.serial=serial;k.tick=clock;k.generationLow=producer;k.connectionLow=connection;frame.reset(k);
 auto read=[&](uint32_t,void*out,size_t n){std::memset(out,int(serial)&255,n);return true;};
 uint32_t remaining=count,address=0x10000000;while(remaining){const uint32_t batch=std::min<uint32_t>(remaining,8192);CHECK(frame.prefetch(read,address,size_t(batch)*4096));remaining-=batch;address+=batch*4096;}CHECK(frame.seal());
}
void mappingBudgets(){
 CHECK(mappedBytes==0);CHECK(sizeof(worldframe::Channel)<128u*1024u);
 for(uint32_t count:{0u,1u,28u,33u,512u,513u,4096u,16384u}){
  currentPid=HostPid;clock+=10;auto client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));CHECK(mappedBytes==sizeof(worldframe::Channel));const auto initialCreates=dataCreates;
  currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));worldframe::Snapshot source;
  worldframe::Token token;
  for(unsigned serial:{1u,2u}){fillPages(source,count,serial);server.observe(source.key);server.publish(source);CHECK(channel()->publishError==0);currentPid=HostPid;token=client->acquire(7);CHECK(token&&token->pages.size()==count);currentPid=GamePid;}
  const uint32_t capacity=worldframe::grownCapacity(count);const size_t payloadBytes=2u*size_t(worldframe::dataBytes(capacity));
  CHECK(mappedBytes==sizeof(worldframe::Channel)+payloadBytes);CHECK(dataCreates-initialCreates==(count?2:0));
  std::printf("MAPPING pages=%u capacity=%u control=%zu payload=%zu total=%zu\n",count,capacity,sizeof(worldframe::Channel),payloadBytes,mappedBytes);
  const auto creates=dataCreates,opens=dataOpens,mapCalls=dataMaps;
  fillPages(source,count?1:0,3);server.observe(source.key);server.publish(source);currentPid=HostPid;auto smaller=client->acquire(7);CHECK(smaller&&smaller->pages.size()==(count?1:0));CHECK(dataCreates==creates&&dataOpens==opens&&dataMaps==mapCalls);
  CHECK(mappedBytes==sizeof(worldframe::Channel)+payloadBytes); // Reuse the observed peak, never allocate per frame.
  currentPid=GamePid;server.retire();currentPid=HostPid;CHECK(!client->acquire(7));CHECK(mappedBytes==sizeof(worldframe::Channel));
  if(count){uint32_t value=0;CHECK(token->read(0x10000000+(count-1)*4096,&value,4)&&value==0x02020202u);} // Tokens own bytes, not views.
  currentPid=GamePid;server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
 }
}
void mappingFailures(){
 CHECK(mappedBytes==0);currentPid=HostPid;clock+=10;auto client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));worldframe::Snapshot source;
 for(unsigned serial:{1u,2u}){fillPages(source,1,serial);server.observe(source.key);server.publish(source);currentPid=HostPid;CHECK(client->acquire(7));currentPid=GamePid;}
 auto*c=channel();const LONG oldActive=c->active;const uint32_t oldSerial=c->slots[oldActive].key.serial;const auto oldBytes=mappedBytes;currentPid=HostPid;auto pinned=client->acquire(7);currentPid=GamePid;
 fillPages(source,33,3);server.observe(source.key);const auto failures=c->publishFailures;
 failDataCreate=1;server.publish(source);CHECK(c->publishFailures==failures+1&&c->publishError==uint32_t(worldframe::PublishError::CreateMapping)&&c->publishSystemError==ERROR_NOT_ENOUGH_MEMORY);CHECK(c->active==oldActive&&c->slots[oldActive].key.serial==oldSerial&&mappedBytes==oldBytes);
 currentPid=HostPid;CHECK(client->acquire(7)==pinned);currentPid=GamePid;
 failProducerMap=1;server.publish(source);CHECK(c->publishFailures==failures+2&&c->publishError==uint32_t(worldframe::PublishError::MapView));CHECK(c->active==oldActive&&mappedBytes==oldBytes);
 server.publish(source);CHECK(c->publishError==0&&c->active!=oldActive);currentPid=HostPid;failDataOpen=1;CHECK(!client->acquire(7));auto grown=client->acquire(7);CHECK(grown&&grown->pages.size()==33&&grown!=pinned);uint32_t value=0;CHECK(pinned->read(0x10000000,&value,4)&&value==0x02020202u);
 currentPid=GamePid;fillPages(source,129,4);server.observe(source.key);server.publish(source);currentPid=HostPid;failConsumerMap=1;CHECK(!client->acquire(7));CHECK(client->acquire(7));
 // A fresh allocation with a mismatched header must never be copied even
 // when the control descriptor is in bounds and its seqlock is stable.
 currentPid=GamePid;fillPages(source,257,5);server.observe(source.key);server.publish(source);const auto active=c->active;auto data=payload(c->slots[active],unsigned(active));CHECK(data);auto*header=reinterpret_cast<worldframe::DataHeader*>(data->bytes.data());const auto generation=header->generationLow;header->generationLow^=1;
 currentPid=HostPid;CHECK(!client->acquire(7));header->generationLow=generation;CHECK(client->acquire(7));data.reset();
 const auto ref=c->slots[active].data;c->slots[active].data.capacity=1;CHECK(!client->acquire(7));c->slots[active].data=ref;
 // An already-existing OS mapping name is not an allocation we may mutate.
 // Retain the old object and check its header and payload after collision retry.
 data=payload(c->slots[active],unsigned(active));CHECK(data);const auto oldHeader=*reinterpret_cast<worldframe::DataHeader*>(data->bytes.data());const auto oldByte=data->bytes[sizeof(worldframe::DataHeader)];forceDataCollision=data;
 currentPid=GamePid;fillPages(source,1,6,10,778);server.observe(source.key);server.publish(source);CHECK(!forceDataCollision&&c->publishError==0);CHECK(!std::memcmp(&oldHeader,data->bytes.data(),sizeof oldHeader)&&data->bytes[sizeof(worldframe::DataHeader)]==oldByte);currentPid=HostPid;CHECK(client->acquire(7));data.reset();
 currentPid=GamePid;server.retire();currentPid=HostPid;CHECK(!client->acquire(7));CHECK(mappedBytes==sizeof(worldframe::Channel));currentPid=GamePid;server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
 // Reopening the same control namespace cannot resurrect the previous data
 // allocation; a fresh descriptor is required before the reader can bind.
 client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));currentPid=GamePid;CHECK(server.wanted(HostPid,clock));fillPages(source,1,6);server.observe(source.key);server.publish(source);currentPid=HostPid;auto fresh=client->acquire(7);CHECK(fresh&&fresh->read(0x10000000,&value,4)&&value==0x06060606u);CHECK(pinned->read(0x10000000,&value,4)&&value==0x02020202u);
 currentPid=GamePid;server.disconnect();currentPid=HostPid;CHECK(!client->acquire(7));client.reset();CHECK(mappedBytes==0);
 std::puts("PASS dynamic mappings: bounded geometric growth, no per-frame create/open/map, producer create/map failure preserves publication, consumer open/map failure defers, header/descriptor identity, retire/reconnect and pinned tokens");
}
void mappingInterleavings(){
 CHECK(mappedBytes==0);currentPid=HostPid;clock+=10;auto client=worldframe::acquireClient(GamePid,10);CHECK(!client->acquire(7));currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));worldframe::Snapshot first,second,third;
 fillPages(first,1,1);fillPages(second,1,2);fillPages(third,1,3);server.observe(first.key);server.publish(first);
 // The producer publishes twice while the reader is opening slot zero. The
 // same allocation now contains a later frame, so only a new stable sequence
 // may produce a token; mixed old metadata/new bytes must be discarded.
 onConsumerMap=[&]{currentPid=GamePid;server.observe(second.key);server.publish(second);server.observe(third.key);server.publish(third);currentPid=HostPid;};
 currentPid=HostPid;auto token=client->acquire(7);uint32_t value=0;CHECK(token&&token->key.serial==3&&token->read(0x10000000,&value,4)&&value==0x03030303u);CHECK(!onConsumerMap);
 currentPid=GamePid;fillPages(first,2,4);server.observe(first.key);server.publish(first);
 onConsumerMap=[&]{currentPid=GamePid;server.retire();currentPid=HostPid;};currentPid=HostPid;CHECK(!client->acquire(7));CHECK(!onConsumerMap&&mappedBytes==sizeof(worldframe::Channel));CHECK(token->read(0x10000000,&value,4)&&value==0x03030303u);
 currentPid=GamePid;server.disconnect();currentPid=HostPid;client.reset();CHECK(mappedBytes==0);
 std::puts("PASS mapping interleavings: double publication during open rejects mixed bytes; retire during open keeps mapped handle alive and rejects retired token");
}
void hostReads(){
 currentPid=HostPid;clock=1000;Memory memory;gameMemory=&memory;auto first=publication(10,60,0x55556666),second=publication(11,61,0x77778888);external::ProcessReader reader;CHECK(reader.open(GamePid));
 auto client=worldframe::acquireClient(GamePid,reader.generation());CHECK(!client->acquire(7));currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));first->key.connectionLow=second->key.connectionLow=uint32_t(reader.generation());server.observe(second->key);server.publish(*second);currentPid=HostPid;CHECK(reader.beginWorldFrame(7,first));
 const auto before=rpmCalls;uint32_t value=0;CHECK(reader.read(0x10000,&value,4)&&value==0x55556666);CHECK(reader.readFresh(0x10000,&value,4)&&value==0x55556666);CHECK(reader.worldFrameComplete());
 CHECK(!reader.read(0x13000,&value,4));CHECK(!reader.worldFrameComplete());CHECK(rpmCalls==before);
 // Restoring a suspended read domain must retain its earlier failure.
 {external::WorldFrameScope nested(reader,7,second);CHECK(nested&&nested.complete());CHECK(reader.read(0x10000,&value,4)&&value==0x77778888);}
 CHECK(reader.worldFrameToken()==first);CHECK(!reader.worldFrameComplete());
 // Nested consumers of the same immutable token contribute to one aggregate
 // completeness result. Another token has its own independent read domain.
 CHECK(reader.beginWorldFrame(7,first));
 {external::WorldFrameScope nested(reader,7,first);CHECK(nested&&nested.complete());CHECK(!reader.read(0x13000,&value,4)&&!nested.complete());}
 CHECK(reader.worldFrameToken()==first&&!reader.worldFrameComplete());
 {external::WorldFrameScope nested(reader,7,first);CHECK(nested&&nested.complete());CHECK(reader.read(0x10000,&value,4));}
 CHECK(reader.worldFrameToken()==first&&!reader.worldFrameComplete());
 CHECK(reader.beginWorldFrame(7,first));
 {external::WorldFrameScope nested(reader,7,second);CHECK(nested&&nested.complete());CHECK(!reader.read(0x13000,&value,4)&&!nested.complete());}
 CHECK(reader.worldFrameToken()==first&&reader.worldFrameComplete());
 {external::WorldFrameScope nested(reader,7,first);CHECK(nested&&nested.complete());CHECK(!reader.readOptional(0x12000,&value,4)&&nested.complete());}
 CHECK(reader.worldFrameToken()==first&&reader.worldFrameComplete());CHECK(rpmCalls==before);
 CHECK(reader.beginWorldFrame(7,first));CHECK(!reader.read(0x12000,&value,4));CHECK(!reader.worldFrameComplete());
 CHECK(reader.beginWorldFrame(7,first));CHECK(!reader.readOptional(0x12000,&value,4));CHECK(reader.worldFrameComplete());CHECK(!reader.readOptional(0x13000,&value,4));CHECK(!reader.worldFrameComplete());
 CHECK(reader.beginWorldFrame(7,first));CHECK(!reader.read(0xFFFF,&value,4));CHECK(!reader.worldFrameComplete());CHECK(rpmCalls==before);
 reader.endWorldFrame();CHECK(reader.readLive(0x10000,&value,4));CHECK(rpmCalls==before+1);
 const auto oldConnection=reader.connectionIdentity();process->alive=false;process=std::make_shared<ProcessRecord>(ProcessRecord{GamePid,12346,true});external::ProcessReader reused;CHECK(reused.open(GamePid));CHECK(reused.generation()!=oldConnection->generation);CHECK(!reused.beginWorldFrame(7,first));reused.close();reader.close();gameMemory=nullptr;
}
void runtime(){
 using namespace assist;currentPid=GamePid;clock=2000;retireWorldFrame();Memory memory;setup(memory);Read read{&memory};auto before=worldlife::capture(read);CHECK(before.valid());snap.frame=before.frame;
 CHECK(captureWorldFrame(read,before,House,clock,worldframe::Base));CHECK(worldFrameReady&&worldFrame.key.frame==50);const auto firstSerial=worldFrame.key.serial;const auto firstCalls=memory.calls.size();
 memory.put(ActorA+0x6C,25);CHECK(captureWorldFrame(read,before,House,clock,worldframe::Combat));CHECK(memory.calls.size()==firstCalls&&worldFrame.key.serial==firstSerial);
 {SampleReadScope scope;int hp=0;CHECK(scope&&readWorldSample(ActorA+0x6C,&hp,4)&&hp==100&&scope.complete());}
 constexpr uint32_t extra=0x760100;memory.put(extra,0x12345678u);
 {SampleReadScope scope;uint32_t value=0;CHECK(scope&&!readWorldSample(extra,&value,4)&&!scope.complete());}
 CHECK(!runtimeWorldDemands.empty());
 for(int frame:{51,52}){memory.put(0xA8ED84,frame);before=worldlife::capture(read);snap.frame=frame;++clock;CHECK(captureWorldFrame(read,before,House,clock,worldframe::Base));SampleReadScope scope;uint32_t value=0;CHECK(scope&&readWorldSample(extra,&value,4)&&value==0x12345678u&&scope.complete());}
 CHECK(memory.requests(extra&~4095u,4096)==2); // Successful reads keep the page subscribed.
 ++snap.frame;{SampleReadScope wrongFrame;CHECK(!wrongFrame&&!wrongFrame.complete());}snap.frame=52;
 // Mutate an independently checked field after capture has begun. The raw
 // pages cannot certify their own frame/world/House binding.
 auto rejectChange=[&](uint32_t changedAddress,uint32_t value){
  retireWorldFrame();setup(memory);before=worldlife::capture(read);snap.frame=before.frame;bool changed=false;
  memory.beforeRead=[&](uint32_t p,size_t n){if(!changed&&p<=Scenario&&uint64_t(p)+n>=uint64_t(Scenario)+4096){changed=true;memory.put(changedAddress,value);}};
  CHECK(!captureWorldFrame(read,before,House,clock,worldframe::Base));CHECK(changed&&!worldFrameReady);const auto after=memory.calls.size();CHECK(!captureWorldFrame(read,before,House,clock,worldframe::Base));CHECK(memory.calls.size()==after);memory.beforeRead={};
 };
 rejectChange(0xA8ED84,51);rejectChange(0xA8B230,Scenario+0x10000);rejectChange(0x87F924,Cells+0x10000);rejectChange(0xA83D4C,House+0x10000);
 retireWorldFrame();CHECK(!worldFrameReady&&runtimeWorldDemands.empty());{SampleReadScope retired;CHECK(!retired&&!retired.complete());}
}
void platform(){transport();mappingBudgets();mappingFailures();mappingInterleavings();runtime();hostReads();std::puts("PASS production transport/ProcessReader/Runtime: pinned publication, independent pause observation, required/optional failures, no live fallback, sustained demand, same-frame single capture, independent world/frame/House rejection");}
}
#endif

#ifndef WORLD_FRAME_FIXTURE_ONLY
int main(){
 fixture::overlap();fixture::partial();fixture::graphs();fixture::capacity();fixture::nothrowPages();
#ifdef WORLD_FRAME_PLATFORM_FIXTURE
 fixture::platform();
#endif
 std::printf("PASS production world frame core: %zu checks; overlap/immutable/failed/missing/roster/demand/queue/page-capacity\n",fixture::checks);
}
#endif
