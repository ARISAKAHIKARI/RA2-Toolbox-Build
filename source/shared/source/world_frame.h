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
enum Demand:uint32_t {Base=1,Combat=2,Passengers=4,Ore=8,Bullets=16,Visual=32};
struct Key {
 uint32_t pid=0,owner=0,match=0,serial=0,tick=0,demand=0;
 uint32_t scenario=0,cells=0,house=0,cellCount=0;
 int32_t frame=-1,session=-1;
 uint32_t generationLow=0,generationHigh=0;
 uint32_t connectionLow=0,connectionHigh=0;
 bool sameWorld(const Key&o)const{return pid==o.pid&&owner==o.owner&&match==o.match&&scenario==o.scenario&&cells==o.cells&&house==o.house&&cellCount==o.cellCount&&session==o.session&&generationLow==o.generationLow&&generationHigh==o.generationHigh&&connectionLow==o.connectionLow&&connectionHigh==o.connectionHigh;}
 bool sameFrame(const Key&o)const{return sameWorld(o)&&frame==o.frame;}
};
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
inline bool bit(const std::array<uint64_t,64>&bits,size_t n){return (bits[n>>6]>>(n&63))&1u;}
inline void mark(std::array<uint64_t,64>&bits,size_t at,size_t n){
 while(n){const unsigned shift=unsigned(at&63);const size_t count=std::min<size_t>(n,64-shift);
  const uint64_t mask=count==64?~uint64_t(0):((uint64_t(1)<<count)-1)<<shift;
  bits[at>>6]|=mask;at+=count;n-=count;}
}
class Snapshot {
 std::array<uint32_t,IndexSize> index_{};
 bool sealed_=false,overflow_=false;
 bool capacityFailure(){overflow_=true;++counters.overflows;return false;}
 Page* add(uint32_t address){
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
 explicit Snapshot(PageAllocator allocator={})noexcept:pages(allocator){}
 Snapshot(const Snapshot&)=delete;Snapshot&operator=(const Snapshot&)=delete;
 void reset(const Key&next){key=next;counters={};pages.clear();index_.fill(0);sealed_=overflow_=false;}
 bool sealed()const{return sealed_;}bool overflow()const{return overflow_;}
 bool seal(){sealed_=true;return !overflow_;}
 const Page*find(uint32_t address)const{
  uint32_t slot=bucket(address);
  for(uint32_t count=0;count<IndexSize;++count,slot=(slot+1)&(IndexSize-1)){
   const uint32_t i=index_[slot];if(!i)return nullptr;if(pages[i-1].address==address)return &pages[i-1];}
  return nullptr;
 }
 bool read(uint32_t address,void*out,size_t n)const{
  if(!sealed_||!out||!range(address,n))return false;
  auto*dest=static_cast<uint8_t*>(out);
  while(n){const size_t offset=address&(PageSize-1),count=std::min<size_t>(n,PageSize-offset);
   const auto*p=find(address&~(PageSize-1));if(!p||!covered(*p,offset,count))return false;
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
 // their exact read-set semantics. A failed speculative span publishes no
 // bytes and falls back to the original per-page/partial-range path.
 // Speculative roster warming can suppress individual fallback: a failed
 // span then observes no bytes, leaving each original field/range read to
 // establish its own partial-page validity and failure masks.
 template<class Read>bool prefetchPages(Read&&live,const std::vector<uint32_t>&addresses,uint8_t*scratch,size_t scratchBytes,bool individualFallback=true){
  if(sealed_||overflow_)return false;
  const size_t limit=std::min<size_t>(16,scratchBytes/PageSize);bool ok=true;
  for(size_t first=0;first<addresses.size();){
   const uint32_t at=addresses[first];size_t end=first+1;
   if(scratch&&limit>1&&!(at&(PageSize-1))&&range(at,PageSize)&&!find(at)){
    while(end<addresses.size()&&end-first<limit&&
      uint64_t(at)+(end-first)*PageSize==addresses[end]&&range(addresses[end],PageSize)&&!find(addresses[end]))++end;
   }
   const size_t count=end-first;
   if(!individualFallback&&count==1){first=end;continue;}
   if(count>1&&pages.reserve(pages.size()+count)){
    ++counters.calls;counters.pageAttempts+=uint32_t(count);counters.bytes+=uint32_t(count*PageSize);
    if(live(at,scratch,count*PageSize)){
     for(size_t n=0;n<count;++n){auto*p=add(addresses[first+n]);if(!p)return false;p->full=1;std::memcpy(p->bytes.data(),scratch+n*PageSize,PageSize);}
     first=end;continue;
    }
    ++counters.failedReads;
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
};
using Token=std::shared_ptr<const Snapshot>;
}
