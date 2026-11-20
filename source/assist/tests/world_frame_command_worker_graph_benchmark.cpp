// Full CaptureGraph A/B: current-process RPM vs in-process copy.
// Synthetic, stable memory only. This is a read-backend benchmark, NOT a
// lifetime-safety proof and NOT a real-game FPS claim.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include "../../shared/source/world_frame_capture.h"
#include "../../shared/source/world_frame_command_index.h"

namespace {
constexpr uint32_t PageSize = 4096;
constexpr uint32_t ActorCount = 700;
constexpr uint32_t SparseSpanPages = 16384;
constexpr uint32_t Scenario = 0x100000;
constexpr uint32_t Cells = 0x140000;
constexpr uint32_t House = 0x700000;
constexpr uint32_t Type = 0x800000;
constexpr uint32_t TechnoItems = 0x600000;
constexpr uint32_t HouseItems = 0x610000;
constexpr uint32_t ActorBase = 0x1200000;
constexpr size_t ImageBytes = 0x5400000;

enum class Mode : unsigned { Memcpy, QueryMemcpy, ReadProcessMemory };
const char* name(Mode mode) {
 switch(mode) {
  case Mode::Memcpy: return "memcpy";
  case Mode::QueryMemcpy: return "VirtualQuery+memcpy";
  default: return "ReadProcessMemory";
 }
}
struct Image {
 std::vector<uint8_t> bytes;
 explicit Image():bytes(ImageBytes) {}
 template<class T> void put(uint32_t at,const T&value) {
  assert(uint64_t(at)+sizeof(T)<=bytes.size());
  std::memcpy(bytes.data()+at,&value,sizeof(T));
 }
 void vector(uint32_t header,uint32_t items,const std::vector<uint32_t>&values) {
  struct NativeVector {uint32_t vtable,items;int32_t capacity;uint32_t flags;int32_t count,step;};
  const NativeVector v{0,items,int32_t(values.size()),0,int32_t(values.size()),1};
  put(header,v);
  if(!values.empty()) {
   assert(uint64_t(items)+values.size()*sizeof(uint32_t)<=bytes.size());
   std::memcpy(bytes.data()+items,values.data(),values.size()*sizeof(uint32_t));
  }
 }
};
struct Counters {uint64_t calls=0,bytes=0;};
struct Reader {
 const Image* image;
 Mode mode;
 Counters* counters;
 bool operator()(uint32_t address,void*out,size_t size)const {
  if(!out||!worldframe::range(address,size)||
     uint64_t(address)+size>image->bytes.size())return false;
  ++counters->calls;counters->bytes+=size;
  const void*source=image->bytes.data()+address;
  if(mode==Mode::ReadProcessMemory) {
   SIZE_T got=0;
   return ReadProcessMemory(GetCurrentProcess(),source,out,size,&got)&&got==size;
  }
  if(mode==Mode::QueryMemcpy) {
   // Stable synthetic pages only. A production version also needs an SEH
   // fault boundary; VirtualQuery alone cannot close a concurrent-free race.
   MEMORY_BASIC_INFORMATION info{};
   if(VirtualQuery(source,&info,sizeof info)!=sizeof info||
      info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
   const DWORD protection=info.Protect&0xFFu;
   const bool readable=protection==PAGE_READONLY||protection==PAGE_READWRITE||
      protection==PAGE_WRITECOPY||protection==PAGE_EXECUTE_READ||
      protection==PAGE_EXECUTE_READWRITE||protection==PAGE_EXECUTE_WRITECOPY;
   const uintptr_t begin=reinterpret_cast<uintptr_t>(source);
   const uintptr_t end=begin+size;
   const uintptr_t regionEnd=reinterpret_cast<uintptr_t>(info.BaseAddress)+info.RegionSize;
   if(!readable||end<begin||end>regionEnd)return false;
  }
  std::memcpy(out,source,size);
  return true;
 }
};
struct RunState {
 worldframe::Snapshot sample;
 worldframe::CaptureScratch scratch;
 worldframe::CommandRoster roster;
};
struct Metrics {
 double ms=0;
 uint64_t calls=0,bytes=0;
 uint32_t pages=0,pageAttempts=0,rangeAttempts=0,failedReads=0,rosterCaptures=0;
};
worldframe::Key makeKey() {
 worldframe::Key key{};
 key.pid=GetCurrentProcessId();key.owner=key.pid;key.match=1;
 key.serial=1;key.tick=100;key.scenario=Scenario;key.cells=Cells;
 key.house=House;key.cellCount=4;key.frame=50;key.session=0;
 return key;
}
Image makeImage(bool scattered) {
 Image image;
 std::vector<uint32_t>actors;actors.reserve(ActorCount);
 image.put(0xA8B230,Scenario);
 image.put(0xA8B238,int32_t(0));
 image.put(0x87F924,Cells);
 image.put(0x87F928,int32_t(4));
 image.put(0xA8ED84,int32_t(50));
 image.put(0xA8E9A0,uint8_t(1));
 image.put(0xA83D4C,House);
 for(uint32_t i=0;i<ActorCount;++i) {
  // Dense layout touches one contiguous run. Sparse layout spreads the same
  // actor count over a 16K-page address window in a deterministic permutation.
  const uint32_t page=scattered?(i*7919u)%SparseSpanPages:i;
  const uint32_t actor=ActorBase+page*PageSize;
  actors.push_back(actor);
  image.put(actor,uint32_t(0x7F5C70)); // infantry/vehicle Techno vtable
  image.put(actor+0x10,i+5u);          // stable nonzero UID
  image.put(actor+0x6C,int32_t(100));  // health
  image.put(actor+0x21C,House);
  image.put(actor+0x6C4,Type);
  image.put(actor+0xE4,uint32_t(0));
  image.put(actor+0xE8,int32_t(0));
 }
 image.vector(0xA8EC78,TechnoItems,actors);
 image.vector(0xA8EB40,0,{});
 image.vector(0x8B4108,0,{});
 image.vector(0xA80228,HouseItems,{House});
 return image;
}
bool samePages(const worldframe::Snapshot&a,const std::vector<worldframe::Page>&b) {
 if(a.pages.size()!=b.size())return false;
 for(size_t i=0;i<b.size();++i) {
  const auto&x=a.pages[i];const auto&y=b[i];
  if(x.address!=y.address||x.full!=y.full||
     std::memcmp(x.valid.data(),y.valid.data(),sizeof x.valid)||
     std::memcmp(x.failed.data(),y.failed.data(),sizeof x.failed)||
     std::memcmp(x.bytes.data(),y.bytes.data(),sizeof x.bytes))return false;
 }
 return true;
}
bool one(RunState&state,const Image&image,Mode mode,Metrics&metrics) {
 Counters counters{};
 const auto start=std::chrono::steady_clock::now();
 state.sample.reset(makeKey());
 Reader reader{&image,mode,&counters};
 worldframe::CaptureGraph<Reader> graph(state.sample,reader,state.scratch);
 if(!graph.capture(worldframe::Base,{}))return false;
 if(!state.sample.seal())return false;
 if(!state.roster.build(state.sample,false,state.scratch.list))return false;
 const auto stop=std::chrono::steady_clock::now();
 metrics.ms=std::chrono::duration<double,std::milli>(stop-start).count();
 metrics.calls=counters.calls;metrics.bytes=counters.bytes;
 metrics.pages=uint32_t(state.sample.pages.size());
 metrics.pageAttempts=state.sample.counters.pageAttempts;
 metrics.rangeAttempts=state.sample.counters.rangeAttempts;
 metrics.failedReads=state.sample.counters.failedReads;
 metrics.rosterCaptures=state.sample.counters.rosterCaptures;
 if(metrics.calls!=state.sample.counters.calls||
    metrics.bytes!=state.sample.counters.bytes||
    metrics.failedReads!=0||!state.roster.objects)return false;
 uint32_t hp=0;
 return state.sample.read(ActorBase+0x6C,&hp,sizeof hp)&&hp==100&&
        state.roster.objects->byId(5)==ActorBase;
}
double percentile(std::vector<double> values,double p) {
 std::sort(values.begin(),values.end());
 const size_t index=std::min(values.size()-1,
   size_t(std::ceil(p*double(values.size())))-1);
 return values[index];
}
bool runLayout(bool scattered) {
 const Image image=makeImage(scattered);
 RunState direct,query,rpm;
 RunState*states[]={&direct,&query,&rpm};
 const Mode modes[]={Mode::Memcpy,Mode::QueryMemcpy,Mode::ReadProcessMemory};
 std::vector<worldframe::Page>reference;
 Metrics last[3]{};
 // Warm each retained Snapshot/CaptureScratch/roster allocation before timing.
 for(unsigned i=0;i<2;++i)for(unsigned m=0;m<3;++m)
  if(!one(*states[m],image,modes[m],last[m]))return false;
 reference.assign(direct.sample.pages.data(),
                  direct.sample.pages.data()+direct.sample.pages.size());
 for(unsigned m=1;m<3;++m)if(!samePages(states[m]->sample,reference))return false;
 constexpr unsigned Samples=21;
 std::vector<double>times[3];
 for(unsigned i=0;i<Samples;++i) {
  // Rotate order to reduce systematic first/last-reader bias.
  for(unsigned offset=0;offset<3;++offset) {
   const unsigned m=(i+offset)%3;
   if(!one(*states[m],image,modes[m],last[m]))return false;
   if(!samePages(states[m]->sample,reference))return false;
   times[m].push_back(last[m].ms);
  }
 }
 const char*layout=scattered?"scattered":"contiguous";
 for(unsigned m=0;m<3;++m) {
  std::printf("GRAPH_READ layout=%s mode=%s actors=%u pages=%u calls=%llu bytes=%llu "
              "p50_ms=%.4f p95_ms=%.4f p99_ms=%.4f pageAttempts=%u rangeAttempts=%u "
              "rosterCaptures=%u real_game=false\n",
   layout,name(modes[m]),ActorCount,last[m].pages,
   static_cast<unsigned long long>(last[m].calls),
   static_cast<unsigned long long>(last[m].bytes),
   percentile(times[m],0.50),percentile(times[m],0.95),
   percentile(times[m],0.99),last[m].pageAttempts,last[m].rangeAttempts,
   last[m].rosterCaptures);
 }
 return true;
}
}
int main() {
 assert(runLayout(false));
 assert(runLayout(true));
 std::puts("PASS full CaptureGraph reader A/B: byte-identical immutable snapshots and UID indexes; stable synthetic memory only, not a production safety or gameplay claim");
}
