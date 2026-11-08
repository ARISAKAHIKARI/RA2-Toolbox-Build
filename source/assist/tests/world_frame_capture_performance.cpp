// Baseline/current production graphs receive identical synthetic memory.
// The driver supplies the frozen old headers in a separate namespace.
#define main inherited_world_frame_core_main
#include "world_frame_core.cpp"
#undef main
#include BASELINE_CAPTURE_HEADER

namespace captureperformance {
using namespace fixture;
static size_t comparisons=0;
void roster(Memory&m,uint32_t table,uint32_t list,const std::vector<uint32_t>&items){
 struct Vec{uint32_t vt,items;int capacity;uint32_t flags;int count,step;}v{0,list,int(items.size()),0,int(items.size()),1};m.put(table,v);
 for(uint32_t p:items){m.put(list,p);list+=4;}
}
void many(Memory&m){
 setup(m);std::vector<uint32_t>actors;
 for(unsigned i=0;i<64;++i){uint32_t p=ActorA+i*0x1000;actors.push_back(p);m.put(p,0x7F5C70u);m.put(p+0x6C4,Type);m.put(p+0x21C,House);m.put(p+0x10,p+5);m.put(p+0x6C,100);}
 std::reverse(actors.begin(),actors.end());actors.insert(actors.begin()+1,actors.front());roster(m,0xA8EC78,TechnoList,actors);
 constexpr uint32_t independent=0x2E0100;m.put(independent,0x7E3EBCu);m.put(independent+0x520,Type);m.put(independent+0x21C,House);m.put(independent+0x10,independent+5);
 roster(m,0xA8EB40,BuildingList,{independent,ActorB});
}
void noSuccessfulOverlap(const Memory&m){
 std::vector<uint8_t>seen(m.bytes.size());bool unique=true;
 for(const auto&call:m.calls)if(call.success)for(size_t i=0;i<call.bytes;++i)if(seen[call.address+i]++)unique=false;
 CHECK(unique);
}
void equalPages(const baseline_worldframe::Snapshot&before,const worldframe::Snapshot&after){
 static_assert(sizeof(baseline_worldframe::Page)==sizeof(worldframe::Page),"unchanged page ABI");
 CHECK(before.pages.size()==after.pages.size());
 for(size_t i=0;i<before.pages.size();++i){const auto&old=before.pages[i];const auto*now=after.find(old.address);CHECK(now);
  CHECK(old.full==now->full&&old.valid==now->valid&&old.failed==now->failed);
  CHECK(!std::memcmp(old.bytes.data(),now->bytes.data(),old.bytes.size()));
 }
}
void compare(const char*name,Memory input,uint32_t demand,const std::vector<uint32_t>&requests={},bool preobserve=false){
 Memory before=input,after=input;before.calls.clear();after.calls.clear();worldframe::Key source=key();
 baseline_worldframe::Key oldKey;static_assert(sizeof(oldKey)==sizeof(source),"unchanged key ABI");std::memcpy(&oldKey,&source,sizeof source);
 baseline_worldframe::Snapshot old;worldframe::Snapshot current;old.reset(oldKey);current.reset(source);Read oldRead{&before},newRead{&after};
 if(preobserve){uint32_t a=0,b=0;CHECK(old.capture(oldRead,ActorA+0x100,&a,4)==current.capture(newRead,ActorA+0x100,&b,4));CHECK(a==b);}
 baseline_worldframe::CaptureGraph<Read>oldGraph(old,oldRead);worldframe::CaptureScratch scratch;worldframe::CaptureGraph<Read>newGraph(current,newRead,scratch);
 const bool oldOk=oldGraph.capture(demand,requests),newOk=newGraph.capture(demand,requests);CHECK(oldOk==newOk);
 CHECK(old.overflow()==current.overflow());if(oldOk){CHECK(old.seal()&&current.seal());equalPages(old,current);}
 noSuccessfulOverlap(before);noSuccessfulOverlap(after);++comparisons;
 std::printf("COMPARE %s: result=%d pages=%zu old_reads=%u new_reads=%u old_bytes=%u new_bytes=%u\n",name,int(newOk),current.pages.size(),old.counters.calls,current.counters.calls,old.counters.bytes,current.counters.bytes);
}
void spans(){
 Memory input;many(input);
 for(uint32_t demand:{1u,2u,4u,8u,16u,32u,63u})compare(("demand-"+std::to_string(demand)).c_str(),input,demand);
 std::vector<uint32_t>requests;for(uint32_t i=0;i<48;++i)requests.push_back(0x740000+i*4096);
 compare("contiguous-request-pages",input,63,requests);
 requests.insert(requests.begin()+3,requests[1]);std::reverse(requests.begin()+10,requests.begin()+20);compare("duplicate-unsorted-requests",input,63,requests);
 {auto m=input;m.partialPages.insert(0x202000);m.denied.push_back({0x202F00,16});compare("actor-page-unreadable-outside-prefix",m,63);}
 {auto m=input;m.partialPages.insert(Type&~4095u);m.denied.push_back({Type+0x1E00,16});compare("sparse-type-page",m,63);}
 {auto m=input;m.partialPages.insert(ActorA&~4095u);m.denied.push_back({ActorA+0x700,16});compare("preobserved-partial-page",m,63,{},true);}
 {auto m=input;m.denied.push_back({0x750400,128});compare("unreadable-request-page",m,63,{0x74F000,0x750000,0x751000,0x752000});}
 {auto m=input;m.put(0xA8EC78+16,-1);compare("invalid-required-roster",m,63);}
 {auto m=input;m.put(ActorA+0x274,Temporal);for(unsigned i=0;i<60;++i)m.put(Temporal+i*0x100+0x44,Temporal+((i+1)%60)*0x100);compare("temporal-chain-52-bound",m,63);}
 {auto m=input;m.put(ActorA+0x274,Temporal);for(unsigned i=0;i<12;++i)m.put(Temporal+i*0x100+0x44,Temporal+((i+1)%12)*0x100);compare("temporal-cycle",m,63);}
}
void reusedScratch(){
 Memory memory;many(memory);worldframe::Snapshot sample;worldframe::CaptureScratch scratch;
 for(int frame:{50,51,1}){auto source=key(frame);source.match=frame==1?8:7;sample.reset(source);memory.put(ActorA+0x6C,frame);memory.put(0xA8ED84,frame);
  worldframe::CaptureGraph<Read>graph(sample,Read{&memory},scratch);CHECK(graph.capture(63,{})&&sample.seal());int hp=0;CHECK(sample.read(ActorA+0x6C,&hp,4)&&hp==frame);
 }
 AllocationSpy allocations;worldframe::CaptureScratch retained(allocations.allocator());
 for(int frame:{1,2}){sample.reset(key(frame));worldframe::CaptureGraph<Read>graph(sample,Read{&memory},retained);CHECK(graph.capture(63,{})&&sample.seal());const auto calls=allocations.calls;if(frame==1)allocations.failAt=calls+1;else CHECK(allocations.calls+1==allocations.failAt);}
 AllocationSpy rejected;rejected.failAt=1;worldframe::CaptureScratch denied(rejected.allocator());sample.reset(key());worldframe::CaptureGraph<Read>graph(sample,Read{&memory},denied);CHECK(!graph.capture(63,{}));CHECK(sample.counters.overflows==1);
}
void pageCapacity(){
 AllocationSpy rejected;rejected.failAt=1;worldframe::Snapshot sample(rejected.allocator());sample.reset(key());size_t reads=0;
 auto read=[&](uint32_t p,void*out,size_t n){++reads;size_t offset=0;while(offset<n){const uint32_t address=p+uint32_t(offset);const size_t chunk=std::min(n-offset,size_t(worldframe::PageSize-(address&(worldframe::PageSize-1))));std::memset(static_cast<uint8_t*>(out)+offset,int(address>>12)&255,chunk);offset+=chunk;}return true;};
 CHECK(!sample.prefetch(read,0x10000,3*4096));CHECK(sample.overflow()&&sample.pages.size()==0&&reads==0);
 sample.reset(key(51));rejected.failAt=0;CHECK(sample.prefetch(read,0x10000,2*4096));const auto before=reads;auto*prior=sample.pages.data();rejected.failAt=rejected.calls+1;
 CHECK(!sample.prefetch(read,0x20000,3*4096));CHECK(sample.overflow()&&sample.pages.size()==2&&sample.pages.data()==prior&&reads==before);CHECK(sample.pages[0].bytes[0]==0x10&&sample.pages[1].bytes[0]==0x11);
 worldframe::Snapshot full;full.reset(key());constexpr size_t half=32u*1024u*1024u;
 CHECK(full.prefetch(read,0x10000000,half));CHECK(full.prefetch(read,0x12000000,half));CHECK(full.pages.size()==worldframe::MaxPages);
 const auto atLimit=reads;CHECK(!full.prefetch(read,0x14000000,3*4096));CHECK(full.overflow()&&full.pages.size()==worldframe::MaxPages&&reads==atLimit&&!full.seal());
}
}
int main(){captureperformance::spans();captureperformance::reusedScratch();captureperformance::pageCapacity();
 std::printf("PASS capture performance correctness: %zu baseline/current scenarios, %zu checks; identical page-key bytes/valid/failed maps, no successful overlap, capacity, freshness, allocation recovery\n",captureperformance::comparisons,fixture::checks);
}
