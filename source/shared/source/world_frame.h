#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>
#include <vector>

// Raw, immutable world observations. No Win32, engine call, pointer ownership or
// live-reader fallback belongs in this type. The producer alone may fill pages.
namespace worldframe {
constexpr uint32_t Version=1,PageSize=4096,MaxPages=16384,IndexSize=32768;
enum Demand:uint32_t {
 Base=1,Combat=2,Passengers=4,Ore=8,Bullets=16,Visual=32,Resources=64,HostDisplayOnly=128,
 HostVisualPassengers=256,HostVisualCombat=512,HostVisualOre=1024,MonitorStats=2048,TypeCatalogue=4096
};
// Explicit Host-only visual requirements never grant command authority.
// Unknown or mixed native demand remains on the full authoritative path.
inline bool visualDemand(uint32_t demand){
 constexpr uint32_t displayFlags=uint32_t(Base|Visual|Resources|HostVisualPassengers|HostVisualCombat|HostVisualOre|MonitorStats);
 return (demand&uint32_t(Visual|Resources|MonitorStats))!=0&&(demand&~displayFlags)==0;
}
inline uint32_t runtimeDemand(uint32_t demand){
 return visualDemand(demand)?uint32_t(Base):
 ((demand&~uint32_t(Visual|Resources|HostDisplayOnly|HostVisualPassengers|HostVisualCombat|HostVisualOre|MonitorStats))|uint32_t(Base));
}
inline uint32_t canonicalHostDemand(uint32_t demand){
 uint32_t capture=runtimeDemand(demand);
 capture|=demand&MonitorStats;
 if(visualDemand(demand)){
  capture|=demand&uint32_t(Visual|Resources);
  if(demand&HostVisualPassengers)capture|=Passengers;
  if(demand&HostVisualCombat)capture|=Combat;
  if(demand&HostVisualOre)capture|=Ore;
 }
 return capture;
}
struct Key {
 uint32_t pid=0,owner=0,match=0,serial=0,tick=0,demand=0;
 uint32_t scenario=0,cells=0,house=0,cellCount=0;
 int32_t frame=-1,session=-1;
 uint32_t generationLow=0,generationHigh=0;
 uint32_t connectionLow=0,connectionHigh=0;
 bool sameWorld(const Key&o)const{return pid==o.pid&&owner==o.owner&&match==o.match&&scenario==o.scenario&&cells==o.cells&&house==o.house&&cellCount==o.cellCount&&session==o.session&&generationLow==o.generationLow&&generationHigh==o.generationHigh&&connectionLow==o.connectionLow&&connectionHigh==o.connectionHigh;}
 bool sameFrame(const Key&o)const{return sameWorld(o)&&frame==o.frame;}
};
// Producer-local field request; not part of the serialized Key/Page layout.
struct RequestedSpan {uint32_t address=0,bytes=0,tick=0;};
// High bit of Key::demand marks a scene boundary that intentionally omitted
// actor/type graph pages. Source-revision values occupy the lower 31 bits.
// A Host command reader may not treat this as a full command sample.
constexpr uint32_t BoundaryOnlyKeyBit=0x80000000u;
// World bytes copied by the GameRuntime background worker may have crossed
// simulation frames. This marker authorizes Host *observation* only, never a
// native EventClass. Runtime has a separate exact-frame admission check.
constexpr uint32_t ObservationOnlyKeyBit=0x40000000u;
constexpr uint32_t SceneKeyBit=0x20000000u;
constexpr uint32_t PublicationKeyFlags=BoundaryOnlyKeyBit|ObservationOnlyKeyBit|SceneKeyBit;
inline bool boundaryOnlySource(const Key&key){return (key.demand&BoundaryOnlyKeyBit)!=0;}
inline bool observationOnlySource(const Key&key){return (key.demand&ObservationOnlyKeyBit)!=0;}
inline bool canonicalDisplaySource(const Key&key,uint32_t demand){
 return observationOnlySource(key)&&!boundaryOnlySource(key)&&(!(demand&Visual)||(key.demand&SceneKeyBit));
}
struct Counters {uint32_t calls=0,bytes=0,pageHits=0,pageAttempts=0,rangeAttempts=0,failedReads=0,rosterCaptures=0,overflows=0;};
struct Page {
 uint32_t address=0,full=0;
 std::array<uint64_t,64> valid{},failed{};
 std::array<uint8_t,PageSize> bytes{};
};
static_assert(sizeof(Key)==64&&sizeof(Page)==5128,"world sample wire layout");
static_assert(std::is_trivially_copyable<Page>::value&&std::is_trivially_destructible<Page>::value&&std::is_nothrow_copy_constructible<Page>::value,"raw page storage lifetime");
static_assert(alignof(Page)<=alignof(std::max_align_t),"C heap supports page alignment");
// Allocation is deliberately independent of exceptions: the injected Runtime
// is built with -fno-exceptions. Hooks provide Page-aligned raw storage with a
// matching release and may reject growth without discarding prior storage.
struct PageAllocator {
 using Allocate=void*(*)(size_t,void*)noexcept;
 using Release=void(*)(void*,void*)noexcept;
 static void*allocateDefault(size_t bytes,void*)noexcept{return std::malloc(bytes);}
 static void releaseDefault(void*p,void*)noexcept{std::free(p);}
 Allocate allocate=allocateDefault;Release release=releaseDefault;void*context=nullptr;
};
class PageBuffer {
 Page*data_=nullptr;size_t size_=0,capacity_=0;PageAllocator allocator_;
public:
 explicit PageBuffer(PageAllocator allocator={})noexcept:allocator_(allocator.allocate&&allocator.release?allocator:PageAllocator{}){}
 ~PageBuffer(){if(data_)allocator_.release(data_,allocator_.context);}
 PageBuffer(const PageBuffer&)=delete;PageBuffer&operator=(const PageBuffer&)=delete;
 size_t size()const noexcept{return size_;}size_t capacity()const noexcept{return capacity_;}
 Page*data()noexcept{return data_;}const Page*data()const noexcept{return data_;}
 Page&back()noexcept{return data_[size_-1];}const Page&back()const noexcept{return data_[size_-1];}
 Page&operator[](size_t i)noexcept{return data_[i];}const Page&operator[](size_t i)const noexcept{return data_[i];}
 void clear()noexcept{size_=0;}
 void swap(PageBuffer&other)noexcept{std::swap(data_,other.data_);std::swap(size_,other.size_);std::swap(capacity_,other.capacity_);std::swap(allocator_,other.allocator_);}
 bool reserve(size_t needed)noexcept{
  if(needed<=capacity_)return true;if(needed>MaxPages)return false;
  size_t next=capacity_?capacity_:1;while(next<needed)next*=2;
  auto*replacement=static_cast<Page*>(allocator_.allocate(next*sizeof(Page),allocator_.context));if(!replacement)return false;
  // Construct only the live prefix. Unused capacity is raw storage, so growth
  // does not first zero tens of MiB that will be overwritten by copied pages.
  for(size_t i=0;i<size_;++i)::new(static_cast<void*>(replacement+i))Page(data_[i]);
  if(data_)allocator_.release(data_,allocator_.context);data_=replacement;capacity_=next;return true;
 }
 bool append()noexcept{
  if(size_>=MaxPages||!reserve(size_+1))return false;
  ::new(static_cast<void*>(data_+size_))Page{};++size_;return true;
 }
 // Import already owns a complete initialized wire page. Construct its copy
 // directly instead of zeroing the payload only to overwrite every byte.
 // Capture still uses append() so its unread/failed bytes start initialized.
 bool append(const Page&source)noexcept{
  if(size_>=MaxPages||!reserve(size_+1))return false;
  ::new(static_cast<void*>(data_+size_))Page(source);++size_;return true;
 }
};
inline bool range(uint32_t address,size_t n){return address>=0x10000&&n&&n<=32u*1024u*1024u&&uint64_t(address)+n<=0x80000000ull;}
inline uint32_t bucket(uint32_t page){return ((page>>12)*2654435761u)&(IndexSize-1);}
// Keep dependency addresses, never bytes, across short derived-cache gaps.
// Renewing a request cannot prolong any sampled data's lifetime. Fixed storage
// bounds membership and avoids allocating or reading a clock on every field.
class DemandLeases {
 struct Entry {uint32_t page=0,tick=0;};
 std::array<Entry,MaxPages> entries_{};
 std::array<uint16_t,IndexSize> index_{};
 uint32_t count_=0,checkedAt_=0,checkAfter_=0;
#ifdef WORLD_FRAME_LEASE_DIAGNOSTICS
 uint64_t pruneVisits_=0; // Instrument only the regression fixture, never production.
#endif
 void prune(uint32_t now){
  if(uint32_t(now-checkedAt_)<checkAfter_)return;
  uint32_t kept=0,next=Lifetime+1;
#ifdef WORLD_FRAME_LEASE_DIAGNOSTICS
  pruneVisits_+=count_;
#endif
  for(uint32_t i=0;i<count_;++i){const auto entry=entries_[i];const uint32_t age=uint32_t(now-entry.tick);
   if(age<=Lifetime){entries_[kept++]=entry;next=std::min(next,Lifetime+1-age);}}
  if(kept!=count_){count_=kept;index_.fill(0);
   for(uint32_t i=0;i<count_;++i){uint32_t slot=bucket(entries_[i].page);while(index_[slot])slot=(slot+1)&(IndexSize-1);index_[slot]=uint16_t(i+1);}}
  checkedAt_=now;checkAfter_=next;
 }
public:
 static constexpr uint32_t Lifetime=1000;
#ifdef WORLD_FRAME_LEASE_DIAGNOSTICS
 uint64_t pruneVisits()const{return pruneVisits_;}
#endif
 void clear(){count_=0;index_.fill(0);checkAfter_=0;}
 size_t size()const{return count_;}
 enum class RetainResult:uint8_t { Invalid,Existing,Added,Full };
 RetainResult retainStatus(uint32_t page,uint32_t now){
  if((page&(PageSize-1))||!range(page,PageSize))return RetainResult::Invalid;
  uint32_t slot=bucket(page);
  while(index_[slot]){auto&entry=entries_[index_[slot]-1];if(entry.page==page){entry.tick=now;return RetainResult::Existing;}slot=(slot+1)&(IndexSize-1);}
  if(count_==MaxPages){
   // The earliest existing lease cannot have expired yet. Do not scan all
   // 16K entries simply because an unknown page appeared at capacity.
   if(uint32_t(now-checkedAt_)<checkAfter_)return RetainResult::Full;
   prune(now);if(count_==MaxPages)return RetainResult::Full;
   slot=bucket(page);while(index_[slot])slot=(slot+1)&(IndexSize-1);
  }
  const bool wasEmpty=count_==0;
  entries_[count_]={page,now};index_[slot]=uint16_t(++count_);
  if(wasEmpty){checkedAt_=now;checkAfter_=Lifetime+1;}
  return RetainResult::Added;
 }
 bool retain(uint32_t page,uint32_t now){
  const auto result=retainStatus(page,now);
  return result==RetainResult::Existing||result==RetainResult::Added;
 }
 void append(uint32_t now,std::vector<uint32_t>&out){prune(now);for(uint32_t i=0;i<count_;++i)out.push_back(entries_[i].page);}
 template<class F> bool visit(uint32_t now,F visitor){
  prune(now);for(uint32_t i=0;i<count_;++i)if(!visitor(entries_[i].page))return false;
  return true;
 }
};
inline bool bit(const std::array<uint64_t,64>&bits,size_t n){return (bits[n>>6]>>(n&63))&1u;}
inline void mark(std::array<uint64_t,64>&bits,size_t at,size_t n){
 while(n){const unsigned shift=unsigned(at&63);const size_t count=std::min<size_t>(n,64-shift);
  const uint64_t mask=count==64?~uint64_t(0):((uint64_t(1)<<count)-1)<<shift;
  bits[at>>6]|=mask;at+=count;n-=count;}
}
class Snapshot {
 std::array<uint32_t,IndexSize> index_{};
 bool sealed_=false,overflow_=false,indexFresh_=true;
 bool capacityFailure(){overflow_=true;++counters.overflows;return false;}
 Page* add(uint32_t address){
  // A zero-page Host boundary does not touch the 128-KiB lookup array.
  // The first later full page reinitializes it before reading old indices.
  if(!indexFresh_){index_.fill(0);indexFresh_=true;}
  uint32_t slot=bucket(address);
  while(index_[slot]){auto&p=pages[index_[slot]-1];if(p.address==address)return &p;slot=(slot+1)&(IndexSize-1);}
  if(overflow_)return nullptr;
  if(!pages.append()){capacityFailure();return nullptr;}
  auto&p=pages.back();p.address=address;index_[slot]=uint32_t(pages.size());return &p;
 }
 static bool covered(const Page&p,size_t offset,size_t n){
  if(p.full)return true;
  while(n){const unsigned shift=unsigned(offset&63);const size_t count=std::min<size_t>(n,64-shift);
   const uint64_t mask=count==64?~uint64_t(0):((uint64_t(1)<<count)-1)<<shift;
   if((p.valid[offset>>6]&mask)!=mask)return false;offset+=count;n-=count;}
  return true;
 }
public:
 Key key{};Counters counters{};PageBuffer pages;
 struct CatalogueObservation { uint64_t hash=0;bool attempted=false,complete=false; } catalogue;
 // Local provenance only; not part of Key/Page/Channel wire layouts. All
 // command consumers unwrap this owner and read only its authoritative bytes.
 std::shared_ptr<const Snapshot> controlSource;
 int32_t visualFrame=-1;uint32_t visualTick=0;uint64_t visualSerial=0;
 explicit Snapshot(PageAllocator allocator={})noexcept:pages(allocator){}
 Snapshot(const Snapshot&)=delete;Snapshot&operator=(const Snapshot&)=delete;
 // Transfer a completed worker-owned capture without allocating or copying any
 // page payloads. Both Snapshot owners must be exclusive and quiescent here.
 // The fixed 128 KiB lookup table is swapped, never rebuilt from game memory.
 void swap(Snapshot&other)noexcept{
  using std::swap;
  swap(index_,other.index_);swap(sealed_,other.sealed_);swap(overflow_,other.overflow_);
  swap(indexFresh_,other.indexFresh_);
  swap(key,other.key);swap(counters,other.counters);pages.swap(other.pages);
  swap(catalogue,other.catalogue);
  controlSource.swap(other.controlSource);
  swap(visualFrame,other.visualFrame);swap(visualTick,other.visualTick);
  swap(visualSerial,other.visualSerial);
 }
 void reset(const Key&next){controlSource.reset();visualFrame=-1;visualTick=0;visualSerial=0;key=next;counters={};catalogue={};pages.clear();index_.fill(0);indexFresh_=true;sealed_=overflow_=false;}
 // No page buffer or key-index contents can become command authority on a
 // HostDisplayOnly Key. Defer clearing the large lookup until the next page add.
 void resetBoundary(const Key&next){
  controlSource.reset();visualFrame=-1;visualTick=0;visualSerial=0;
  key=next;counters={};catalogue={};pages.clear();indexFresh_=false;sealed_=overflow_=false;
 }
 bool sealed()const{return sealed_;}bool overflow()const{return overflow_;}
 bool seal(){sealed_=true;return !overflow_;}
 const Page*find(uint32_t address)const{
  if(!indexFresh_)return nullptr; // Never examine stale page indices after zero reset.
  uint32_t slot=bucket(address);
  for(uint32_t count=0;count<IndexSize;++count,slot=(slot+1)&(IndexSize-1)){
   const uint32_t i=index_[slot];if(!i)return nullptr;if(pages[i-1].address==address)return &pages[i-1];}
  return nullptr;
 }
 bool read(uint32_t address,void*out,size_t n)const{
  if(!sealed_||!out||!range(address,n))return false;
  // A missing later page must not partly modify the caller's output.
  // Check the sealed CPU index first; no extra game read or IPC is performed.
  if(n<=PageSize-(address&(PageSize-1))){
   const size_t offset=address&(PageSize-1);const auto*p=find(address&~(PageSize-1));
   if(!p||!covered(*p,offset,n))return false;
   std::memcpy(out,p->bytes.data()+offset,n);return true;
  }
  uint32_t verify=address;size_t remaining=n;
  while(remaining){const size_t offset=verify&(PageSize-1),count=std::min<size_t>(remaining,PageSize-offset);
   const auto*p=find(verify&~(PageSize-1));if(!p||!covered(*p,offset,count))return false;
   verify+=uint32_t(count);remaining-=count;
  }
  auto*dest=static_cast<uint8_t*>(out);
  while(n){const size_t offset=address&(PageSize-1),count=std::min<size_t>(n,PageSize-offset);
   const auto*p=find(address&~(PageSize-1));
   std::memcpy(dest,p->bytes.data()+offset,count);dest+=count;address+=uint32_t(count);n-=count;}
  return true;
 }
 bool observed(uint32_t address,size_t n)const{
  if(!sealed_||!range(address,n))return false;
  while(n){const size_t offset=address&(PageSize-1),count=std::min<size_t>(n,PageSize-offset);const auto*p=find(address&~(PageSize-1));
   if(!p)return false;if(!p->full)for(size_t i=offset;i<offset+count;++i)if(!bit(p->valid,i)&&!bit(p->failed,i))return false;
   address+=uint32_t(count);n-=count;}
  return true;
 }
 template<class Read>bool capture(Read&&live,uint32_t address,void*out,size_t n){
  if(sealed_||overflow_||!range(address,n))return false;
  auto*dest=static_cast<uint8_t*>(out);bool ok=true;
  while(n){const uint32_t base=address&~(PageSize-1);const size_t offset=address-base,count=std::min<size_t>(n,PageSize-offset);
   Page*p=const_cast<Page*>(find(base));const bool existing=p!=nullptr;if(!p)p=add(base);if(!p)return false;
   if(!existing){++counters.calls;++counters.pageAttempts;counters.bytes+=PageSize;
    if(live(base,p->bytes.data(),PageSize))p->full=1;else ++counters.failedReads;
   }
   if(!p->full&&!covered(*p,offset,count)){
    // A partly readable OS page is not an all-zero world object. Preserve
    // successful bytes and failed ranges separately; never reacquire overlap.
    size_t at=offset,end=offset+count;
    while(at<end){if(bit(p->valid,at)||bit(p->failed,at)){++at;continue;}
     size_t stop=at+1;while(stop<end&&!bit(p->valid,stop)&&!bit(p->failed,stop))++stop;
     ++counters.calls;++counters.rangeAttempts;counters.bytes+=uint32_t(stop-at);
     if(live(base+uint32_t(at),p->bytes.data()+at,stop-at))mark(p->valid,at,stop-at);
     else{mark(p->failed,at,stop-at);++counters.failedReads;}at=stop;
    }
   }else if(existing)++counters.pageHits;
   if(!covered(*p,offset,count))ok=false;
   else if(dest)std::memcpy(dest,p->bytes.data()+offset,count);
   if(dest)dest+=count;address+=uint32_t(count);n-=count;
  }return ok;
 }
 template<class Read>bool prefetch(Read&&live,uint32_t address,size_t n){return capture(live,address,nullptr,n);}
 // The caller supplies retained scratch and a bounded page list. Coalesce
 // only adjacent, completely unobserved pages; existing/partial pages keep
 // their exact read-set semantics. A failed speculative batch publishes no
 // invalid data. Up to two split levels salvage readable subranges without
 // allowing a bad page to discard a whole 16-page unit warmup.
 // Each 16-page batch uses at most 7 speculative calls, with no more than
 // 32 extra split calls across this invocation. Required fields still use
 // the original per-page and partial-range fallback.
 template<class Read>bool prefetchPages(Read&&live,const std::vector<uint32_t>&addresses,uint8_t*scratch,size_t scratchBytes,bool individualFallback=true,size_t maxBatchPages=16){
  if(sealed_||overflow_)return false;
  // Mandatory/unknown pages retain the 16-page default. An explicitly
  // prefiltered actor warmup can batch 32 adjacent pages in one live read.
  // Clamp to the caller's retained scratch and never read outside a run.
  const size_t limit=std::min({size_t(32),maxBatchPages,scratchBytes/PageSize});bool ok=true;
  // Bound the TOTAL number of extra speculative RPM calls per invocation.
  // Thousands of inaccessible actors must not multiply the split cost.
  unsigned splitBudget=32;
  for(size_t first=0;first<addresses.size();){
   const uint32_t at=addresses[first];size_t end=first+1;
   if(scratch&&limit>1&&!(at&(PageSize-1))&&range(at,PageSize)&&!find(at)){
    while(end<addresses.size()&&end-first<limit&&
      uint64_t(at)+(end-first)*PageSize==addresses[end]&&range(addresses[end],PageSize)&&!find(addresses[end]))++end;
   }
   const size_t count=end-first;
   if(!individualFallback&&count==1){first=end;continue;}
   if(count>1&&pages.reserve(pages.size()+count)){
    // A failed live read may alter scratch, but never publishes its bytes.
    // Successful subranges are copied before the next reuse of scratch.
    const auto salvage=[&](auto&& self,size_t begin,size_t span,unsigned depth)->bool{
     ++counters.calls;counters.pageAttempts+=uint32_t(span);counters.bytes+=uint32_t(span*PageSize);
     if(live(addresses[begin],scratch,span*PageSize)){
      for(size_t i=0;i<span;++i){
       auto*p=add(addresses[begin+i]);if(!p)return false;
       p->full=1;std::memcpy(p->bytes.data(),scratch+i*PageSize,PageSize);
      }
      return true;
     }
     ++counters.failedReads;
     if(depth>=2||span<4||splitBudget<2)return false;
     splitBudget-=2; // Reserve BOTH child calls before any recursive split.
     const size_t left=span/2;
     const bool leftOk=self(self,begin,left,depth+1);
     if(overflow_)return false;
     const bool rightOk=self(self,begin+left,span-left,depth+1);
     return leftOk&&rightOk;
    };
    if(salvage(salvage,first,count,0)){first=end;continue;}
    if(overflow_)return false;
   }
   if(!individualFallback){first=end;continue;}
   for(;first<end;++first){if(!prefetch(live,addresses[first],PageSize))ok=false;if(overflow_)return false;}
  }return ok;
 }
 // The transport copies one producer publication. Reject malformed, duplicate
 // or unaligned page keys before exposing any bytes to another process.
 bool import(const Key&source,const Counters&stats,const Page*input,size_t count){
  reset(source);if(count>MaxPages)return capacityFailure();if(!input&&count)return false;
  if(!pages.reserve(count))return capacityFailure();
  for(size_t i=0;i<count;++i){const auto&p=input[i];
   if((p.address&(PageSize-1))||!range(p.address,PageSize)||p.full>1||find(p.address)){reset(source);return false;}
   if(!pages.append(p))return capacityFailure();
   uint32_t slot=bucket(p.address);while(index_[slot])slot=(slot+1)&(IndexSize-1);
   index_[slot]=uint32_t(pages.size());
  }counters=stats;return seal();
 }
 bool copyForPublication(const Snapshot&source){
  // Publication-only copy: retain immutable wire pages without rebuilding the
  // 128-KiB command lookup index. This snapshot is never used for Runtime reads.
  if(&source==this||!source.sealed_||source.overflow_||source.controlSource)return false;
  controlSource.reset();visualFrame=-1;visualTick=0;visualSerial=0;
  key=source.key;counters=source.counters;catalogue=source.catalogue;
  pages.clear();indexFresh_=false;sealed_=overflow_=false;
  if(source.pages.size()>MaxPages||!pages.reserve(source.pages.size()))return capacityFailure();
  for(size_t i=0;i<source.pages.size();++i)if(!pages.append(source.pages[i]))return capacityFailure();
  sealed_=true;return true;
 }
 bool extendVisual(const std::shared_ptr<const Snapshot>&source){
  if(!source||source.get()==this||source->controlSource||!source->sealed()||source->overflow())return false;
  if(!import(source->key,source->counters,source->pages.data(),source->pages.size()))return false;
  controlSource=source;sealed_=false;return true;
 }
};
using Token=std::shared_ptr<const Snapshot>;
inline Token authoritative(const Token&token){return token&&token->controlSource?token->controlSource:token;}
inline bool sameObservation(const Token&a,const Token&b){return a&&b&&(a->controlSource||b->controlSource?a==b:a->key.sameFrame(b->key)&&a->key.serial==b->key.serial);}
}
