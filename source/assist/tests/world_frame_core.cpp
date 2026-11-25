// Executes the production capture, immutable read-set and (with the driver)
// Runtime/Host transport. Only operating-system calls and game bytes are fake.
#if !defined(WORLD_FRAME_TEST_VISUAL_CACHE) && !defined(WORLD_FRAME_PRODUCT_DISPLAY_FIXTURE)
#define WORLD_FRAME_TEST_VISUAL_CACHE
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../../shared/source/world_frame_capture.h"
#include "../../shared/source/world_frame_command_index.h"
#include "../../shared/source/world_frame_command_policy.h"
#include "../../shared/source/world_frame_visual.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#ifdef REAL_WINDOWS_RPM
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

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
#ifdef REAL_WINDOWS_RPM
  SIZE_T got=0;CHECK(ReadProcessMemory(GetCurrentProcess(),bytes.data()+p,out,n,&got)&&got==n);
#else
  std::memcpy(out,bytes.data()+p,n);
#endif
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
constexpr uint32_t TechnoList=0x600000,BuildingList=0x601000,UnitList=0x602000,HouseList=0x603000,BulletList=0x604000,InfantryList=0x605000,InactiveInfantry=0x211100;
constexpr uint32_t HostPid=900,GamePid=1000;
worldframe::Key key(int frame=50){worldframe::Key k;k.pid=GamePid;k.owner=HostPid;k.match=7;k.serial=1;k.tick=100;k.scenario=Scenario;k.cells=Cells;k.house=House;k.cellCount=4;k.frame=frame;k.session=0;return k;}
void setup(Memory&m){
 m.put(0xA8B230,Scenario);m.put(0xA8B238,int(0));m.put(0x87F924,Cells);m.put(0x87F928,int(4));m.put(0xA8ED84,int(50));m.put(0xA8E9A0,uint8_t(1));m.put(0xA83D4C,House);
 m.vector(0xA8EC78,TechnoList,{ActorA,ActorB,ActorA});m.vector(0xA8EB40,BuildingList,{ActorB});m.vector(0x8B4108,UnitList,{ActorA});m.vector(0xA80228,HouseList,{House});m.vector(0xA8ED40,BulletList,{Bullet});
 for(uint32_t p:{ActorA,ActorB,Passenger}){m.put(p,0x7F5C70u);m.put(p+0x6C4,Type);m.put(p+0x21C,House);m.put(p+0x10,p+5);m.put(p+0x6C,100);}
 m.vector(0xA83DE8,InfantryList,{InactiveInfantry});
 m.put(InactiveInfantry,0x7EB058u);m.put(InactiveInfantry+0x10,43u);
 m.put(InactiveInfantry+0x6C0,Type);m.put(InactiveInfantry+0x11C,ActorA);
 m.put(ActorA+0x30,Passenger);m.put(Passenger+0x118,ActorA);m.put(ActorA+0x218,ActorB);m.put(ActorA+0x274,Temporal);m.put(Temporal+0x44,Temporal);
 m.put(ActorA+0xE4,Radio);m.put(ActorA+0xE8,2);m.put(Radio,ActorB);m.put(Radio+4,Passenger);
 m.put(Type+0x60,0x700100u);m.put(Type+0x898,Weapon);m.put(Type+0xA94,Weapon);m.put(Weapon+0xA0,Warhead);m.put(Weapon+0xAC,Projectile);
 m.put(Bullet+0xAC,BulletType);m.put(Bullet+0xB0,ActorA);m.put(Bullet+0x10C,ActorB);
}

void hostVisual(){
 // At capacity, one expired lease is replaced by a new page. Total count
 // remains MaxPages, but the previous graph no longer covers the demand.
 {
  worldframe::VisualCache cache;std::vector<uint32_t>pages;
  for(uint32_t i=0;i<worldframe::MaxPages;++i)pages.push_back(0x10000000u+i*worldframe::PageSize);
  CHECK(cache.request(pages,100,false));
  CHECK(!cache.request({pages.front()},101,false));
  std::vector<uint32_t>keep(pages.begin()+1,pages.end());
  CHECK(!cache.request(keep,1101,false));
  const uint32_t replacement=pages.back()+worldframe::PageSize;
  CHECK(!cache.request({replacement},1101,false));
  CHECK(cache.request({replacement},1102,false));
  CHECK(!cache.request({replacement},1103,false));
  CHECK(cache.request({},1103,true));
 }
 Memory m;setup(m);Read live{&m};auto raw=std::make_shared<worldframe::Snapshot>();raw->reset(key());
 worldframe::CaptureGraph<Read>graph(*raw,live);CHECK(graph.capture(worldframe::Base,{}));CHECK(raw->seal());
 const auto rawBytes=raw->counters.bytes,rawCalls=raw->counters.calls;const size_t rawPages=raw->pages.size();
 worldframe::VisualCache cache;std::vector<uint32_t>pages;
 for(unsigned i=0;i<512;++i){const uint32_t p=0xC00000+i*4096;pages.push_back(p);m.put(p,i+123u);}
 cache.request(pages,100,false);m.put(ActorA+0x6C,25);m.put(0xA8ED84,51);
 auto visual=cache.acquire(raw,worldframe::Resources,100,live);CHECK(visual&&visual->controlSource==raw&&visual->visualFrame==51&&visual->visualTick==100&&visual->visualSerial);
 CHECK(visual->key.serial==raw->key.serial&&visual->key.tick==raw->key.tick&&visual->key.frame==50);
 int hp=0;uint32_t value=0;CHECK(visual->read(ActorA+0x6C,&hp,4)&&hp==100);CHECK(!raw->read(0xC00000,&value,4));CHECK(visual->read(0xC00000,&value,4)&&value==123);
 CHECK(raw->counters.bytes==rawBytes&&raw->counters.calls==rawCalls&&raw->pages.size()==rawPages);
 CHECK(visual->counters.bytes>=rawBytes+512*4096&&visual->pages.size()>=rawPages+512);
 const auto calls=m.calls.size();CHECK(cache.acquire(raw,worldframe::Resources,101,live)==visual&&m.calls.size()==calls);
 // A first reader's dependency report for a fully covered graph page must
 // renew the lease, not invalidate the immutable observation.
 const uint32_t actorPage=ActorA&~4095u;
 CHECK(visual->find(actorPage)&&visual->find(actorPage)->full);
 CHECK(!cache.request({actorPage},101,false,visual));
 CHECK(cache.acquire(raw,worldframe::Resources,101,live)==visual&&m.calls.size()==calls);
 CHECK(cache.statistics.captures==1);
 CHECK(worldframe::authoritative(visual)==raw&&!worldframe::sameObservation(raw,visual));
 CHECK(worldframe::visualDemand(worldframe::Resources|worldframe::Visual));
 CHECK(!worldframe::visualDemand(worldframe::Resources|worldframe::Combat));
 CHECK(!worldframe::visualDemand(worldframe::Visual|worldframe::Passengers));
 CHECK(!worldframe::visualDemand(worldframe::Visual|worldframe::HostDisplayOnly));
 CHECK(worldframe::runtimeDemand(worldframe::Resources|worldframe::Combat)==(worldframe::Base|worldframe::Combat));
 CHECK(worldframe::runtimeDemand(worldframe::Visual|worldframe::Passengers)==(worldframe::Base|worldframe::Passengers));
 CHECK(worldframe::runtimeDemand(worldframe::Combat|worldframe::Passengers)==(worldframe::Base|worldframe::Combat|worldframe::Passengers));
 CHECK(worldframe::runtimeDemand(worldframe::Resources|worldframe::Visual)==worldframe::Base);
 // A missing visual page recovers while paused, without renewing control time.
 constexpr uint32_t extra=0xE00100;m.put(extra,456u);CHECK(!visual->read(extra,&value,4));cache.request({extra&~4095u},102,true);
 CHECK(!cache.acquire(raw,worldframe::Resources,102,live));auto extended=cache.acquire(raw,worldframe::Resources,116,live);
 CHECK(extended&&extended!=visual&&extended->visualSerial>visual->visualSerial&&extended->read(extra,&value,4)&&value==456);
 CHECK(!visual->read(extra,&value,4)&&raw->key.tick==100&&extended->key.tick==100);
 // An old consumer reports the same missing page after it was repaired.
 // No new full graph capture is warranted; a current report still retries.
 const auto repairedCalls=m.calls.size();
 CHECK(!cache.request({extra&~4095u},117,true,visual));
 CHECK(cache.acquire(raw,worldframe::Resources,117,live)==extended);
 CHECK(m.calls.size()==repairedCalls);
 CHECK(cache.request({extra&~4095u},117,true,extended));
 CHECK(!cache.acquire(extended,worldframe::Visual,133,live));
 // Independent world/frame guards, including a mutation during collection.
 cache.request({0xE01000},133,true);bool changed=false;
 m.beforeRead=[&](uint32_t p,size_t n){if(!changed&&p<=0xE01000&&uint64_t(p)+n>0xE01000){changed=true;m.put(0xA8ED84,52);}};
 // Host display-only data may span logic ticks when the immutable world
 // identity stays fixed; the control source remains unchanged and cannot
 // authorize commands from the later visual timestamp.
 auto crossed=cache.acquire(raw,worldframe::Resources,133,live);
 CHECK(crossed&&changed&&crossed->visualFrame==52&&worldframe::authoritative(crossed)==raw);
 CHECK(raw->key.frame==50&&crossed->key.frame==50);m.beforeRead={};
 for(auto bad:std::vector<std::pair<uint32_t,uint32_t>>{{0xA8B230,Scenario+4096},{0xA83D4C,House+4096},{0x87F924,Cells+4096},{0xA8ED84,49},{0x87F7E8+0x1240,1},{0xA8B238,3}}){
  const auto old=*reinterpret_cast<uint32_t*>(m.bytes.data()+bad.first);m.put(bad.first,bad.second);cache.request({},150,true);CHECK(!cache.acquire(raw,worldframe::Resources,150,live));m.put(bad.first,old);
  // Retry a different immutable token so the per-source attempt throttle does not hide a guard failure.
  auto replacement=std::make_shared<worldframe::Snapshot>();CHECK(replacement->import(raw->key,raw->counters,raw->pages.data(),raw->pages.size()));raw=replacement;
 }
 auto boundary=[](int frame,uint32_t tick){
  auto source=std::make_shared<worldframe::Snapshot>();auto k=key(frame);k.tick=tick;k.serial=uint32_t(frame);k.demand=worldframe::BoundaryOnlyKeyBit;
  source->reset(k);CHECK(source->seal());return source;
 };
 worldframe::VisualCache fast;
 m.put(0xA8ED84,52);auto first=boundary(52,200);
 auto original=fast.acquire(first,worldframe::Resources,200,live);CHECK(original);
 m.put(0xA8ED84,60);auto next=boundary(60,208);
 CHECK(fast.acquire(next,worldframe::Resources,208,live)==original&&fast.statistics.captures==1);
 const auto repeatedBoundaryReads=m.calls.size();
 CHECK(fast.acquire(next,worldframe::Resources,209,live)==original);
 CHECK(m.calls.size()==repeatedBoundaryReads&&fast.statistics.captures==1);
 CHECK(original->controlSource==first&&original->visualTick==200);
 CHECK(worldframe::visualReuseWindow(256)==10&&worldframe::visualReuseWindow(257)==16);
 CHECK(original->pages.size()<=256);
 auto refreshed=fast.acquire(next,worldframe::Resources,210,live);CHECK(refreshed&&refreshed!=original&&refreshed->controlSource==next);
 CHECK(fast.acquire(next,worldframe::Resources,216,live)==refreshed);
 CHECK(original->visualTick==200&&refreshed->visualTick==210);
 const auto guardsBefore=m.calls.size();
 auto stamp=worldframe::visualStamp(live,*next);
 CHECK(stamp.valid&&m.calls.size()==guardsBefore+8);
 m.denied.push_back({0x87F7E8+0x140,4});
 CHECK(!worldframe::visualStamp(live,*next).valid);m.denied.pop_back();
 m.put(0xA8ED84,61);m.put(0x87F7E8+0x1240,64);
 auto resized=boundary(61,217);auto resizedCapture=fast.acquire(resized,worldframe::Resources,217,live);
 CHECK(resizedCapture&&resizedCapture->controlSource==resized&&fast.statistics.captures==3);
 m.put(0xA8ED84,59);auto rewound=boundary(59,218);
 auto rewindCapture=fast.acquire(rewound,worldframe::Resources,218,live);
 CHECK(rewindCapture&&rewindCapture->controlSource==rewound&&fast.statistics.captures==4);
 // A consumer of the previous match may release its pinned snapshot after
 // the new match has already built a display graph.
 auto changedWorld=std::make_shared<worldframe::Snapshot>();auto changedKey=rewound->key;
 ++changedKey.match;++changedKey.serial;changedWorld->reset(changedKey);CHECK(changedWorld->seal());
 auto freshWorld=fast.acquire(changedWorld,worldframe::Resources,234,live);
 CHECK(freshWorld&&freshWorld->controlSource==changedWorld);
 const auto worldCalls=m.calls.size();const auto worldCaptures=fast.statistics.captures;
 CHECK(!fast.request({0xE80000},235,true,rewindCapture));
 CHECK(fast.acquire(changedWorld,worldframe::Resources,235,live)==freshWorld);
 CHECK(m.calls.size()==worldCalls&&fast.statistics.captures==worldCaptures);
 CHECK(fast.request({0xE80000},235,true,freshWorld));
 std::printf("PASS Host visual isolation: control %u reads/%u bytes/%zu pages; 512 display pages stay external; frozen control, shared reuse, pause recovery, provenance and independent frame/world rejection\n",rawCalls,rawBytes,rawPages);
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
 // A second-page miss must leave the caller's entire result untouched.
 // This catches partial-copy corruption even when callers check false.
 worldframe::Snapshot incomplete;incomplete.reset(key());
 CHECK(incomplete.prefetch(read,0x10FF8,8));CHECK(incomplete.seal());
 std::array<uint8_t,24> untouched{};untouched.fill(0xA5);
 const auto beforeMiss=m.calls.size();
 CHECK(!incomplete.read(0x10FF8,untouched.data(),untouched.size()));
 CHECK(m.calls.size()==beforeMiss);
 for(auto byte:untouched)CHECK(byte==0xA5);
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
 uint32_t infantry=0,uid=0;
 if(demand&worldframe::Passengers){
  CHECK(s.read(InfantryList,&infantry,4)&&infantry==InactiveInfantry);
  CHECK(s.read(infantry+0x10,&uid,4)&&uid==43);
 }else CHECK(!s.read(InfantryList,&infantry,4));
 if(demand==worldframe::Base)CHECK(s.counters.rosterCaptures==3);
 return {s.counters.calls,s.counters.bytes,s.counters.rosterCaptures};
}
void graphs(){
 const auto base=graph(worldframe::Base),combat=graph(worldframe::Combat),passengers=graph(worldframe::Passengers),ore=graph(worldframe::Ore),visual=graph(worldframe::Visual);
 CHECK(base.calls<combat.calls&&base.calls<visual.calls&&passengers.calls<combat.calls&&base.rosters<ore.rosters);
 {Memory m;setup(m);constexpr uint32_t factory=0x606100,product=0x212100,type=0x302100;
  constexpr uint32_t unitFactory=0x608100,unitProduct=0x214100,unitQueue=0x72C100;
  m.put(House+0x53BC,factory);m.put(factory+0x6C,House);m.put(factory+0x58,product);
  m.put(product+0x10,61u);m.put(product+0x520,type);m.put(type+0xDF8,11);
  m.put(House+0x53B0,unitFactory);m.put(unitFactory+0x6C,House);m.put(unitFactory+0x58,unitProduct);
  m.vector(unitFactory+0x40,unitQueue,{type});m.put(unitProduct+0x10,62u);m.put(unitProduct+0x6C0,type);
  worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read>g(s,Read{&m});
  CHECK(g.capture(worldframe::Resources,{}));CHECK(s.seal());
  m.put(unitQueue,0u);m.put(unitProduct+0x6C0,0u);
  uint32_t uid=0,unitType=0;int index=-1;
  CHECK(s.read(product+0x10,&uid,4)&&uid==61);
  CHECK(s.read(type+0xDF8,&index,4)&&index==11);
  CHECK(s.read(unitProduct+0x10,&uid,4)&&uid==62);
  CHECK(s.read(unitProduct+0x6C0,&unitType,4)&&unitType==type);
  CHECK(s.read(unitQueue,&unitType,4)&&unitType==type);
  // The real passive Host acquisition must preserve the Resources profile.
  // Verify the own factory products and queue without demand-page retries.
  m.put(unitQueue,type);m.put(unitProduct+0x6C0,type);
  auto boundary=std::make_shared<worldframe::Snapshot>();auto k=key();
  k.demand=worldframe::BoundaryOnlyKeyBit;boundary->reset(k);CHECK(boundary->seal());
  worldframe::VisualCache cache;Read live{&m};
  auto observed=cache.acquire(boundary,worldframe::Resources,200,live);
  CHECK(observed&&observed->controlSource==boundary);
  CHECK(observed->read(product+0x10,&uid,4)&&uid==61);
  CHECK(observed->read(unitQueue,&unitType,4)&&unitType==type);
  CHECK(observed->read(unitProduct+0x6C0,&unitType,4)&&unitType==type);
  const auto calls=m.calls.size();
  CHECK(cache.acquire(boundary,worldframe::Resources,201,live)==observed);
  CHECK(m.calls.size()==calls&&cache.statistics.captures==1);
 }
 {Memory m;setup(m);
  constexpr uint32_t type=0x721100,counts=0x725100,factory=0x727100,product=0x729100,queue=0x72C100;
  m.vector(0xA8E348,0x720100,{type});
  m.vector(0xA83E30,0x726100,{factory});
  m.vector(factory+64,queue,{type});
  m.put(House+0x5528+4,counts);m.put(House+0x5528+8,1);m.put(counts,7);
  m.put(House+90154,char16_t(u'陈'));m.put(factory+88,product);m.put(product+0x6C0,type);
  m.put(type+0x634,3);
  const auto demand=worldframe::canonicalHostDemand(worldframe::Base|worldframe::MonitorStats);
  CHECK(demand==(worldframe::Base|worldframe::MonitorStats));
  CHECK(worldframe::visualDemand(demand)&&worldframe::runtimeDemand(demand)==worldframe::Base);
  std::vector<uint32_t> noRequests;
  worldframe::Snapshot s;s.reset(key());worldframe::CaptureGraph<Read>g(s,Read{&m});
  CHECK(g.capture(demand,noRequests)&&s.seal());
  m.put(counts,99);m.put(queue,0u);
  int owned=0,level=0;uint32_t queued=0,activeType=0;char16_t houseName=0;
  CHECK(s.read(counts,&owned,4)&&owned==7);
  CHECK(s.read(queue,&queued,4)&&queued==type);
  CHECK(s.read(product+0x6C0,&activeType,4)&&activeType==type);
  CHECK(s.read(type+0x634,&level,4)&&level==3);
  CHECK(s.read(House+90154,&houseName,2)&&houseName==u'陈');
  worldframe::Snapshot baseOnly;baseOnly.reset(key());
  worldframe::CaptureGraph<Read>ordinary(baseOnly,Read{&m});
  CHECK(ordinary.capture(worldframe::Base,noRequests)&&baseOnly.seal());
  CHECK(!baseOnly.read(counts,&owned,4)&&!baseOnly.read(queue,&queued,4));
  // Exercise the actual Host visual path, not just CaptureGraph directly.
  auto boundary=std::make_shared<worldframe::Snapshot>();auto k=key();
  k.demand=worldframe::BoundaryOnlyKeyBit;boundary->reset(k);CHECK(boundary->seal());
  worldframe::VisualCache cache;Read live{&m};
  auto observed=cache.acquire(boundary,worldframe::MonitorStats,200,live);
  CHECK(observed&&observed->controlSource==boundary);
  CHECK(observed->read(counts,&owned,4)&&owned==99);
  CHECK(observed->read(queue,&queued,4)&&queued==0);
  const auto calls=m.calls.size();
  CHECK(cache.acquire(boundary,worldframe::MonitorStats,201,live)==observed);
  CHECK(m.calls.size()==calls&&cache.statistics.captures==1);
 }
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
void speculativeBatchFaultIsolation(){
 // One unreadable page in a 16-page warm batch previously threw away all
 // 16 speculative pages. Bounded split probing salvages 12 good pages in
 // five calls; the remaining pages are still unknown until field reads.
 constexpr uint32_t base=0x760000;
 std::vector<uint32_t>addresses;
 Memory memory;
 for(uint32_t i=0;i<16;++i){
  const uint32_t address=base+i*worldframe::PageSize;
  addresses.push_back(address);memory.put(address+0x28,0xBEEF0000u+i);
 }
 memory.partialPages.insert(base+10*worldframe::PageSize);
 std::array<uint8_t,16*worldframe::PageSize>scratch{};
 Read live{&memory};
 worldframe::Snapshot sample;sample.reset(key());
 CHECK(sample.prefetchPages(live,addresses,scratch.data(),scratch.size(),false));
 CHECK(memory.calls.size()==5&&sample.pages.size()==12);
 CHECK(sample.counters.calls==5&&sample.counters.failedReads==3);
 for(uint32_t i=0;i<16;++i){
  const bool retained=i<8||i>=12;
  CHECK(bool(sample.find(addresses[i]))==retained);
 }
 uint32_t value=0;
 const auto before=memory.calls.size();
 CHECK(sample.capture(live,addresses[2]+0x28,&value,4)&&value==0xBEEF0002u);
 CHECK(memory.calls.size()==before); // already-warmed page does not live-read
 CHECK(sample.capture(live,addresses[9]+0x28,&value,4)&&value==0xBEEF0009u);
 CHECK(sample.capture(live,addresses[10]+0x28,&value,4)&&value==0xBEEF000Au);
 CHECK(memory.calls.size()==before+3); // good full page + failed full page + exact
 memory.uniqueSuccessfulBytes();
 CHECK(sample.seal());
 CHECK(sample.read(addresses[9]+0x28,&value,4)&&value==0xBEEF0009u);
 CHECK(sample.read(addresses[10]+0x28,&value,4)&&value==0xBEEF000Au);
 CHECK(!sample.read(addresses[10]+0x80,&value,4)); // failed page not fabricated
 // If ALL adjacent pages fail, do not recursively scan or invent any
 // successful page. The speculative budget stays at seven attempts.
 Memory allBad;
 for(uint32_t address:addresses)allBad.partialPages.insert(address);
 worldframe::Snapshot absent;absent.reset(key());
 CHECK(absent.prefetchPages(Read{&allBad},addresses,scratch.data(),scratch.size(),false));
 CHECK(allBad.calls.size()==7&&absent.counters.calls==7);
 CHECK(absent.pages.size()==0&&absent.counters.failedReads==7);
 CHECK(absent.seal()&&!absent.read(addresses[0],&value,4));
 // Saturated faults over dozens of units must not allocate a fresh
 // two-level split tree for EVERY group. Base attempts are unchanged (64),
 // but the additional recovery calls are globally capped at 32.
 Memory saturated;
 std::vector<uint32_t>many;
 for(uint32_t i=0;i<64*16;++i)many.push_back(base+i*worldframe::PageSize);
 saturated.denied.push_back({base,64*16*worldframe::PageSize});
 worldframe::Snapshot bounded;bounded.reset(key());
 CHECK(bounded.prefetchPages(Read{&saturated},many,scratch.data(),scratch.size(),false));
 CHECK(saturated.calls.size()==64+32);
 CHECK(bounded.counters.calls==64+32&&bounded.counters.failedReads==64+32);
 CHECK(bounded.pages.size()==0&&bounded.seal());
 // Required page subscribers keep their original fallbacks. A bad page
 // remains missing, while other addresses may still be sealed successfully.
 Memory required;
 for(uint32_t i=0;i<16;++i)required.put(addresses[i]+0x28,0xBEEF0000u+i);
 required.partialPages.insert(addresses[10]);
 worldframe::Snapshot strict;strict.reset(key());
 CHECK(!strict.prefetchPages(Read{&required},addresses,scratch.data(),scratch.size(),true));
 CHECK(strict.pages.size()==16&&strict.seal());
 CHECK(strict.read(addresses[8]+0x28,&value,4)&&value==0xBEEF0008u);
 CHECK(!strict.read(addresses[10]+0x28,&value,4));
 required.uniqueSuccessfulBytes();
 std::puts("PASS bounded speculative split: 12/16 pages salvaged, no duplicate bytes, 7-call worst-case cap");
}
void contiguousWarmBatch32(){
 // Physical 64-page span: the exact same 256KiB of game bytes can be
 // acquired with two 128KiB RPM calls instead of four 64KiB RPM calls.
 // This is a deterministic *call count*, not a wall-time FPS claim.
 constexpr uint32_t base=0x900000;
 std::vector<uint32_t>addresses;addresses.reserve(64);
 Memory small,large;
 for(uint32_t i=0;i<64;++i){
  const uint32_t page=base+i*worldframe::PageSize;
  addresses.push_back(page);
  small.put(page+0x28,0xF00D0000u+i);
  large.put(page+0x28,0xF00D0000u+i);
 }
 std::array<uint8_t,16*worldframe::PageSize>smallScratch{};
 std::array<uint8_t,32*worldframe::PageSize>largeScratch{};
 worldframe::Snapshot traditional;traditional.reset(key());
 worldframe::Snapshot batched;batched.reset(key());
 CHECK(traditional.prefetchPages(Read{&small},addresses,smallScratch.data(),
                                 smallScratch.size(),false));
 CHECK(batched.prefetchPages(Read{&large},addresses,largeScratch.data(),
                             largeScratch.size(),false,32));
 CHECK(small.calls.size()==4&&large.calls.size()==2);
 CHECK(traditional.counters.bytes==64*worldframe::PageSize);
 CHECK(batched.counters.bytes==traditional.counters.bytes);
 CHECK(traditional.pages.size()==64&&batched.pages.size()==64);
 CHECK(traditional.seal()&&batched.seal());
 for(uint32_t i=0;i<64;++i){
  uint32_t a=0,b=0;
  CHECK(traditional.read(addresses[i]+0x28,&a,4));
  CHECK(batched.read(addresses[i]+0x28,&b,4));
  CHECK(a==0xF00D0000u+i&&a==b);
 }
 small.uniqueSuccessfulBytes();large.uniqueSuccessfulBytes();
 // Never speculate across disjoint addresses or invent a readable page.
 Memory sparse;
 std::vector<uint32_t>gapped;
 for(uint32_t i=0;i<32;++i)gapped.push_back(base+i*2*worldframe::PageSize);
 worldframe::Snapshot skipped;skipped.reset(key());
 CHECK(skipped.prefetchPages(Read{&sparse},gapped,largeScratch.data(),
                             largeScratch.size(),false,32));
 CHECK(sparse.calls.empty()&&skipped.pages.size()==0&&skipped.seal());
 // Fault isolation is still bounded even when using the wider buffer.
 Memory fault;
 for(uint32_t i=0;i<32;++i)fault.put(addresses[i]+0x28,0xAB000000u+i);
 fault.partialPages.insert(addresses[17]);
 worldframe::Snapshot candidate;candidate.reset(key());
 CHECK(candidate.prefetchPages(Read{&fault},addresses,largeScratch.data(),
                               largeScratch.size(),false,32));
 // The 32-page affected run salvages the valid left 16 and right 8.
 // The untouched second 32-page run is copied in one OS read.
 CHECK(candidate.pages.size()==56&&candidate.counters.failedReads==3);
 CHECK(candidate.seal());
 uint32_t sample=0;
 CHECK(candidate.read(addresses[1]+0x28,&sample,4)&&sample==0xAB000001u);
 CHECK(!candidate.read(addresses[17]+0x28,&sample,4));
 CHECK(candidate.read(addresses[33]+0x28,&sample,4));
 std::printf("PASS contiguous warmup: 64 pages, 16-page=4 RPM calls, 32-page=2 calls, exact bytes and fault isolation retained\\n");
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
#include <thread>
#include <future>
#include <condition_variable>
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
 client->subscribe(1,worldframe::Base);CHECK(channel()->demandMask==worldframe::Base);client->subscribe(2,worldframe::Visual);CHECK(channel()->demandMask==worldframe::Base);
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));auto first=publication(1,50,0x11112222);server.observe(first->key);server.publish(*first);
 currentPid=HostPid;const auto beforeRpm=rpmCalls;auto a=client->acquire(7);CHECK(a&&a->key.sameFrame(first->key));CHECK(client->acquire(7)==a);CHECK(!client->acquire(8));uint32_t value=0;CHECK(a->read(0x10000,&value,4)&&value==0x11112222);
 clock=105;client->request({0x13000,0x14000,0x13000,0xFFFF,0x15001});currentPid=GamePid;const auto&requests=server.requests();CHECK(requests.size()==2);CHECK(std::find(requests.begin(),requests.end(),0x13000)!=requests.end());CHECK(std::find(requests.begin(),requests.end(),0x14000)!=requests.end());
 auto second=publication(2,51,0x33334444);server.observe(second->key);server.publish(*second);currentPid=HostPid;auto b=client->acquire(7);CHECK(b&&b!=a&&client->acquire(7)==b);CHECK(b->read(0x10000,&value,4)&&value==0x33334444);CHECK(a->read(0x10000,&value,4)&&value==0x11112222);CHECK(rpmCalls==beforeRpm);
 auto*c=channel();CHECK(c);auto&slot=c->slots[c->active];++slot.seq;CHECK(client->acquire(7)==b);++slot.seq;const auto count=slot.count;slot.count=worldframe::MaxPages+1;CHECK(!client->acquire(7));slot.count=count;CHECK(client->acquire(7)==b);
 // An actual independent producer observation can keep a paused frame usable;
 // neither Host polling nor a fresh heartbeat can change its acquisition tick.
 clock=900;client->subscribe(2,worldframe::Visual);
  CHECK(channel()->demandMask==(worldframe::Base|worldframe::HostDisplayOnly));
  client->subscribe(1,worldframe::Base);CHECK(channel()->demandMask==worldframe::Base);CHECK(!client->acquire(7));currentPid=GamePid;server.observe(second->key);currentPid=HostPid;CHECK(client->acquire(7)==b&&b->key.tick==105);
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
 CHECK(reader.worldMissingReads>0&&reader.worldUnreadableReads>0&&reader.worldInvalidReads>0);
 CHECK(reader.worldFirstMissingAddress==0x13000&&reader.worldLastMissingAddress==0x13000);
 reader.endWorldFrame();CHECK(reader.worldMissingReads>0);reader.begin();CHECK(!reader.worldMissingReads&&!reader.worldUnreadableReads&&!reader.worldInvalidReads&&!reader.worldFirstMissingAddress);
 CHECK(reader.readLive(0x10000,&value,4));CHECK(rpmCalls==before+1);
 const auto oldConnection=reader.connectionIdentity();process->alive=false;process=std::make_shared<ProcessRecord>(ProcessRecord{GamePid,12346,true});external::ProcessReader reused;CHECK(reused.open(GamePid));CHECK(reused.generation()!=oldConnection->generation);CHECK(!reused.beginWorldFrame(7,first));reused.close();reader.close();gameMemory=nullptr;
}

void hostIsolation(){
 CHECK(mappedBytes==0);currentPid=HostPid;clock=10000;Memory m;setup(m);gameMemory=&m;
 external::ProcessReader reader;CHECK(reader.open(GamePid));reader.setWorldDemand(worldframe::Resources);CHECK(!reader.beginWorldFrame(7));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));auto raw=std::make_shared<worldframe::Snapshot>();auto k=key();k.tick=clock;k.generationLow=777;k.connectionLow=uint32_t(reader.generation());k.connectionHigh=uint32_t(reader.generation()>>32);raw->reset(k);
 Read live{&m};worldframe::CaptureGraph<Read>graph(*raw,live);CHECK(graph.capture(worldframe::Base,{}));CHECK(raw->seal());server.observe(raw->key);server.publish(*raw);
 currentPid=HostPid;CHECK(reader.beginWorldFrame(7));auto derived=reader.worldFrameToken();CHECK(derived&&derived->controlSource);
  CHECK(channel()->demandMask==(worldframe::Base|worldframe::HostDisplayOnly));
 constexpr uint32_t extra=0x760100;m.put(extra,0x12345678u);uint32_t value=0;const auto before=rpmCalls;CHECK(!reader.read(extra,&value,4)&&rpmCalls==before&&!reader.worldFrameComplete());reader.endWorldFrame();
 CHECK(server.requests().empty());clock+=16;CHECK(reader.beginWorldFrame(7));derived=reader.worldFrameToken();CHECK(reader.read(extra,&value,4)&&value==0x12345678u);reader.endWorldFrame();CHECK(server.requests().empty());
 // A command scope unwraps the frozen authoritative source. It cannot read
 // an additional display page, nor can it fall back to live memory.
 reader.setWorldDemand(worldframe::Base);CHECK(reader.beginWorldFrame(7,derived));
  CHECK(channel()->demandMask==worldframe::Base);
  CHECK(!reader.worldFrameToken()->controlSource);const auto noFallback=rpmCalls;CHECK(!reader.read(extra,&value,4)&&rpmCalls==noFallback);reader.endWorldFrame();CHECK(server.requests().size()==1);
 currentPid=GamePid;const auto failures=channel()->publishFailures;server.publish(*derived);CHECK(channel()->publishFailures==failures+1&&channel()->publishError==uint32_t(worldframe::PublishError::Identity));
 // Block only a Host visual page. A separate transport mutex must permit
 // the producer to publish and an authoritative consumer to acquire it.
 currentPid=HostPid;auto client=worldframe::acquireClient(GamePid,reader.generation());client->requestVisual({0x770000},false);clock+=16;
 std::mutex mutex;std::condition_variable cv;bool blocked=false,release=false;
 auto readBlocked=[&](uint32_t at,void*out,size_t n){if(at<=0x770000&&uint64_t(at)+n>0x770000){std::unique_lock<std::mutex>lock(mutex);blocked=true;cv.notify_all();cv.wait(lock,[&]{return release;});}return m.read(at,out,n);};
 auto work=std::async(std::launch::async,[&]{return client->visual(worldframe::authoritative(derived),worldframe::Resources,readBlocked);});
 {std::unique_lock<std::mutex>lock(mutex);CHECK(cv.wait_for(lock,std::chrono::seconds(2),[&]{return blocked;}));}
 auto next=std::make_shared<worldframe::Snapshot>();auto nextKey=raw->key;++nextKey.serial;++nextKey.frame;CHECK(next->import(nextKey,raw->counters,raw->pages.data(),raw->pages.size()));
 currentPid=GamePid;server.observe(nextKey);server.publish(*next);currentPid=HostPid;
 auto consumer=std::async(std::launch::async,[&]{return client->acquire(7);});const bool independent=consumer.wait_for(std::chrono::seconds(2))==std::future_status::ready;
 {std::lock_guard<std::mutex>lock(mutex);release=true;}cv.notify_all();auto observed=consumer.get();auto display=work.get();
 CHECK(independent&&observed&&observed->key.serial==nextKey.serial&&display);
 reader.close();client.reset();currentPid=GamePid;server.disconnect();currentPid=HostPid;gameMemory=nullptr;CHECK(mappedBytes==0);
 std::puts("PASS Host transport isolation: visual requests remain local, command scopes unwrap authoritative bytes, derived wire publication rejected; a blocked display reader does not block producer publication or control acquisition");
}

void runtime(){
 using namespace assist;currentPid=GamePid;clock=2000;retireWorldFrame();Memory memory;setup(memory);Read read{&memory};auto before=worldlife::capture(read);CHECK(before.valid());snap.frame=before.frame;
 CHECK(captureWorldFrame(read,before,House,clock,worldframe::Base));CHECK(worldFrameReady&&worldFrame.key.frame==50);const auto firstSerial=worldFrame.key.serial;const auto firstCalls=memory.calls.size();
 // The native UID index may borrow ONLY the Techno pointer roster sealed by
 // this exact acquisition. This reuse does not perform another page read.
 auto*roster=worldFrameCaptureScratch.capturedTechnoRoster(worldFrame.key,TechnoList,3);
 CHECK(roster&&roster[0]==ActorA&&roster[1]==ActorB&&roster[2]==ActorA);
 CHECK(worldFrameCaptureScratch.technoCount==3&&worldFrameCaptureScratch.technoItems==TechnoList);
 CHECK(memory.calls.size()==firstCalls);
 auto stale=worldFrame.key;++stale.serial;
 CHECK(!worldFrameCaptureScratch.capturedTechnoRoster(stale,TechnoList,3));
 stale=worldFrame.key;++stale.frame;
 CHECK(!worldFrameCaptureScratch.capturedTechnoRoster(stale,TechnoList,3));
 stale=worldFrame.key;++stale.match;
 CHECK(!worldFrameCaptureScratch.capturedTechnoRoster(stale,TechnoList,3));
 stale=worldFrame.key;++stale.house;
 CHECK(!worldFrameCaptureScratch.capturedTechnoRoster(stale,TechnoList,3));
 CHECK(!worldFrameCaptureScratch.capturedTechnoRoster(worldFrame.key,TechnoList+4,3));
 CHECK(!worldFrameCaptureScratch.capturedTechnoRoster(worldFrame.key,TechnoList,2));
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
  // Display-only captured a zero-page Key boundary (no CaptureGraph). The
  // first Ctrl+D or
  // native read arriving in this SAME frame must upgrade it atomically to a
  // new full graph; merely re-exporting the sealed boundary would lose input.
  setup(memory);before=worldlife::capture(read);snap.frame=before.frame;
  // A command dependency requested before the display pulse must survive the
  // zero-page marker and be included in the later full capture.
  requestWorldRange(ActorA+0x6C,4);
  CHECK(runtimeWorldDemands.size()==1);
  CHECK(captureWorldFrame(read,before,House,clock,worldframe::Base|worldframe::HostDisplayOnly,true));
  CHECK(runtimeWorldDemands.size()==1);
  CHECK(worldFrameBoundaryOnly&&worldFrameReady&&worldframe::boundaryOnlySource(worldFrame.key));
  CHECK(worldFrame.pages.size()==0&&worldFrame.counters.calls==0&&worldFrame.counters.bytes==0);
  // The previous complete frame may have left tens of thousands of hash
  // indices; resetBoundary never exposes them as new observations.
  CHECK(!worldFrame.find(ActorA&~4095u));
  // Display-only boundaries cannot admit a native command, even if a
  // particular required scalar happens to be present.
  {SampleReadScope blocked;CHECK(!blocked&&!blocked.complete());}
  int hp=0;CHECK(!worldFrame.read(ActorA+0x6C,&hp,sizeof hp));
  const auto shallowSerial=worldFrame.key.serial;
  // Direct Snapshot misuse/reuse is also fail-closed: a prior complete page
  // remains invisible after O(1) reset, and a subsequent real page lazily
  // clears the stale index before admission.
  {
   worldframe::Snapshot reused;
   reused.reset(worldFrame.key);
   uint32_t firstID=0,newID=0;
   CHECK(reused.capture(read,ActorA+0x10,&firstID,4)&&firstID==ActorA+5);
   reused.resetBoundary(worldFrame.key);
   CHECK(reused.pages.size()==0&&!reused.find(ActorA&~4095u));
   CHECK(reused.capture(read,ActorA+0x10,&newID,4)&&newID==firstID);
   CHECK(reused.seal()&&reused.read(ActorA+0x10,&newID,4)&&newID==firstID);
  }
  CHECK(captureWorldFrame(read,before,House,clock,worldframe::Base));
  CHECK(worldFrameReady&&!worldFrameBoundaryOnly&&!worldframe::boundaryOnlySource(worldFrame.key)&&worldFrame.key.serial>shallowSerial);
  {SampleReadScope upgraded;CHECK(upgraded&&upgraded.complete());}
  CHECK(worldFrame.read(ActorA+0x6C,&hp,sizeof hp)&&hp==100);
  CHECK(runtimeWorldDemands.empty()); // Full command source consumed it.
  // A second command read in the same logic frame is still exactly one
  // complete acquisition. No stale previous visual bytes are borrowed.
  const auto upgradeReads=memory.calls.size();
  CHECK(captureWorldFrame(read,before,House,clock,worldframe::Base));
  CHECK(memory.calls.size()==upgradeReads);
  // Even a zero-page display Key cannot be published if the world/House
  // changes during its TWO independent live checks.
  retireWorldFrame();setup(memory);before=worldlife::capture(read);
  snap.frame=before.frame;bool changed=false;
  memory.beforeRead=[&](uint32_t at,size_t){
   if(!changed&&at==0xA8ED84){changed=true;memory.put(0xA83D4C,House+0x10000);}
  };
  CHECK(!captureWorldFrame(read,before,House,clock,worldframe::Base|worldframe::HostDisplayOnly,true));
  CHECK(changed&&!worldFrameReady&&!worldFrameBoundaryOnly);
  memory.beforeRead={};
  retireWorldFrame();
}
void displayLeaseTransition(){
 // An existing Host reader can change purpose without changing reader ID.
 // Command subscriptions must revoke the display-only marker immediately,
 // even after their prior command lease aged out behind a display lease.
 currentPid=HostPid;clock=1400;
 auto subscriber=worldframe::acquireClient(GamePid,71234);
 CHECK(!subscriber->acquire(7));
 subscriber->subscribe(79,worldframe::Resources);
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::HostDisplayOnly));
 subscriber->subscribe(79,worldframe::Base);
 CHECK(channel()->demandMask==worldframe::Base);
 clock+=252;
 subscriber->subscribe(79,worldframe::Resources);
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::HostDisplayOnly));
 subscriber->subscribe(79,worldframe::Base);
 CHECK(channel()->demandMask==worldframe::Base);
 // Mixed visual+native subscriptions must NEVER look like display-only.
 // Exercise both cold and hot transitions on the same reader lease.
 clock+=252;
 subscriber->subscribe(79,worldframe::Visual|worldframe::Combat);
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::Combat));
 clock+=252;
 subscriber->subscribe(79,worldframe::Resources|worldframe::Passengers);
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::Passengers));
 clock+=252;
 subscriber->subscribe(79,worldframe::Visual);
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::HostDisplayOnly));
 subscriber->subscribe(79,worldframe::Resources|worldframe::Ore);
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::Ore));
 subscriber.reset();clock=2400;
}
void hostBoundaryCommandGuard(){
 currentPid=HostPid;clock=3000;Memory memory;setup(memory);gameMemory=&memory;
 external::ProcessReader reader;
 CHECK(reader.open(GamePid));reader.setWorldDemand(worldframe::Resources);
 CHECK(!reader.beginWorldFrame(7)); // establish display-only subscription
 currentPid=GamePid;worldframe::Server server;
 CHECK(server.wanted(HostPid,clock));
 auto shallow=std::make_shared<worldframe::Snapshot>();
 auto k=key();k.tick=clock;k.serial=80;k.generationLow=777;
 k.connectionLow=uint32_t(reader.generation());k.connectionHigh=uint32_t(reader.generation()>>32);
 k.demand=worldframe::BoundaryOnlyKeyBit;
 shallow->reset(k);Read read{&memory};
 // The real Runtime publishes ZERO source pages, not a second miniature
 // CaptureGraph. Host VisualCache must still decode its independent view.
 CHECK(shallow->seal()&&shallow->pages.size()==0&&shallow->counters.calls==0);
 server.observe(shallow->key);CHECK(server.publish(*shallow));
 // Repeated passive wakeups neither rewrite the observation sequence nor
 // retag its actual timestamp. A periodic heartbeat and changed frame do.
 auto* observation=channel();CHECK(observation);
 const auto firstSeq=observation->observationSeq;
 const auto firstTick=observation->observedTick;
 ++clock;server.observe(shallow->key,true);
 CHECK(observation->observationSeq==firstSeq&&observation->observedTick==firstTick);
 clock+=15;server.observe(shallow->key,true);
 CHECK(observation->observationSeq==firstSeq+2&&observation->observedTick==clock);
 auto changedBoundary=shallow->key;++changedBoundary.frame;
 server.observe(changedBoundary,true);
 CHECK(observation->observationSeq==firstSeq+4&&observation->observed.frame==changedBoundary.frame);
 server.observe(shallow->key,true);
 CHECK(observation->observationSeq==firstSeq+6&&observation->observed.frame==shallow->key.frame);
 server.observe(shallow->key); // non-display observation is never throttled
 CHECK(observation->observationSeq==firstSeq+8);
 currentPid=HostPid;CHECK(reader.beginWorldFrame(7));
 CHECK(reader.worldFrameToken()->controlSource);
 reader.endWorldFrame();reader.setWorldDemand(worldframe::Base);
 // This request revoked the display-only lease immediately. Even though
 // the old sealed bytes are still valid for drawing, Host native admission
 // must reject them and must never fall back to live RPM.
 const auto oldReads=rpmCalls;
 CHECK(!reader.beginWorldFrame(7));
 CHECK(channel()->demandMask==worldframe::Base);
 CHECK(rpmCalls==oldReads);
 // Mixed demand cannot route through Host VisualCache to bypass the command
 // guard, even if Visual is one of the flags requested by the reader.
 reader.setWorldDemand(worldframe::Visual|worldframe::Combat);
 CHECK(!reader.beginWorldFrame(7));
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::Combat));
 CHECK(rpmCalls==oldReads);
 currentPid=GamePid;
 auto full=std::make_shared<worldframe::Snapshot>();
 ++k.serial;++k.frame;k.tick+=16;k.demand=0;
 memory.put(0xA8ED84,k.frame);
 full->reset(k);worldframe::CaptureGraph<Read> graph(*full,read);
 CHECK(graph.capture(worldframe::Base,{}));CHECK(full->seal());
 server.observe(full->key);CHECK(server.publish(*full));
 currentPid=HostPid;clock+=16;
 CHECK(reader.beginWorldFrame(7));
 CHECK(!reader.worldFrameToken()->controlSource);
 int hp=0;CHECK(reader.read(ActorA+0x6C,&hp,4)&&hp==100);
 reader.close();currentPid=GamePid;server.disconnect();
 currentPid=HostPid;gameMemory=nullptr;
 std::puts("PASS Host first-command guard: display-only source rejected, live fallbacks forbidden, full next publication accepted");
}
void platform(){transport();mappingBudgets();mappingFailures();mappingInterleavings();runtime();hostReads();hostIsolation();displayLeaseTransition();hostBoundaryCommandGuard();std::puts("PASS production transport/ProcessReader/Runtime: pinned publication, independent pause observation, required/optional failures, no live fallback, sustained demand, same-frame single capture, independent world/frame/House rejection");}
#ifdef WORLD_FRAME_PRODUCT_DISPLAY_FIXTURE
void canonicalDisplayTransport(){
 currentPid=HostPid;clock=3000;Memory memory;setup(memory);gameMemory=&memory;
 external::ProcessReader reader;CHECK(reader.open(GamePid));
 const uint32_t sceneDemand=worldframe::Visual|worldframe::HostVisualPassengers;
 reader.setWorldDemand(sceneDemand);CHECK(!reader.beginWorldFrame(7));
 CHECK(channel()->demandMask==(worldframe::Base|worldframe::Visual|worldframe::Passengers));
 currentPid=GamePid;worldframe::Server server;CHECK(server.wanted(HostPid,clock));
 auto k=key();k.tick=clock;k.generationLow=777;
 k.connectionLow=uint32_t(reader.generation());k.connectionHigh=uint32_t(reader.generation()>>32);
 Read producer{&memory};
 auto publish=[&](uint32_t flags){
  currentPid=GamePid;k.demand=flags;++k.serial;
  worldframe::Snapshot sample;sample.reset(k);
  CHECK(sample.prefetch(producer,ActorA+0x6C,4));CHECK(sample.seal());
  server.observe(k);CHECK(server.publish(sample));currentPid=HostPid;
 };
 const auto oldReads=rpmCalls;
 publish(worldframe::BoundaryOnlyKeyBit);CHECK(!reader.beginWorldFrame(7));
 publish(worldframe::ObservationOnlyKeyBit);CHECK(!reader.beginWorldFrame(7));
 reader.setWorldDemand(worldframe::MonitorStats);CHECK(reader.beginWorldFrame(7));
 CHECK((channel()->demandMask&worldframe::MonitorStats)!=0&&rpmCalls==oldReads);
 reader.setWorldDemand(worldframe::Resources);CHECK(reader.beginWorldFrame(7));
 publish(worldframe::ObservationOnlyKeyBit|worldframe::SceneKeyBit);
 reader.setWorldDemand(sceneDemand);CHECK(reader.beginWorldFrame(7));
 int hp=0;CHECK(reader.read(ActorA+0x6C,&hp,4)&&hp==100);
 constexpr uint32_t missing=0xED0000;
 CHECK(!reader.read(missing,&hp,4)&&!reader.worldFrameComplete()&&rpmCalls==oldReads);
 reader.endWorldFrame();currentPid=GamePid;
 CHECK(server.hasPublication()&&server.requestedRevision()!=server.demandRevision());
 const auto&requests=server.requests();
 CHECK(std::find(requests.begin(),requests.end(),missing)!=requests.end());
 CHECK(server.requestedRevision()==server.demandRevision());
 server.retire();CHECK(!server.hasPublication());
 server.disconnect();currentPid=HostPid;reader.close();gameMemory=nullptr;
 std::puts("PASS canonical display: one Runtime source, explicit scene coverage, missing pages routed to Runtime, zero Host RPM");
}
#endif
}
#endif

#ifndef WORLD_FRAME_FIXTURE_ONLY
int main(){
 fixture::hostVisual();fixture::overlap();fixture::partial();fixture::graphs();fixture::speculativeBatchFaultIsolation();fixture::contiguousWarmBatch32();fixture::capacity();fixture::nothrowPages();
#ifdef WORLD_FRAME_PLATFORM_FIXTURE
#ifdef WORLD_FRAME_PRODUCT_DISPLAY_FIXTURE
 fixture::canonicalDisplayTransport();
#else
 fixture::platform();
#endif
#endif
 std::printf("PASS production world frame core: %zu checks; overlap/immutable/failed/missing/roster/demand/queue/page-capacity\n",fixture::checks);
}
#endif
