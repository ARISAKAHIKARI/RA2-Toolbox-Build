// Reproduce a persistent sealed-frame "missing page": the 4 KiB backing
// page cannot be read as a whole, but a previously missed native field can.
// Exercise the real CaptureGraph and Snapshot, without an RA2 process.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "../../shared/source/world_frame_capture.h"

constexpr uint32_t MissingPage=0x640000,MissingField=MissingPage+0x78;
constexpr uint32_t Expected=0xA5C31D29;
struct Live {
 unsigned pageFailures=0,shortReads=0,calls=0;
 bool operator()(uint32_t at,void*out,size_t n){
  ++calls;
  // A whole-page read of this native allocation is unreliable; narrower
  // fields remain available to the game-thread read boundary.
  if(at==MissingPage&&n==worldframe::PageSize){++pageFailures;return false;}
  auto*bytes=static_cast<unsigned char*>(out);std::memset(bytes,0,n);
  for(size_t i=0;i<n;++i){
   const uint64_t target=uint64_t(at)+i;
   if(target>=MissingField&&target<MissingField+sizeof(Expected))
    bytes[i]=reinterpret_cast<const unsigned char*>(&Expected)[target-MissingField];
  }
  if(at==MissingField&&n==sizeof(Expected))++shortReads;
  return true;
 }
};
static void capture(worldframe::Snapshot&frame,Live&source,
                    const worldframe::RequestedSpan*hint,size_t hints,
                    const std::vector<uint32_t>&pages={}){
 worldframe::Key key{};key.tick=2000;key.frame=10;key.house=0x40000;key.scenario=0x50000;
 frame.reset(key);
 worldframe::CaptureScratch scratch{};
 worldframe::CaptureGraph graph(frame,[&](uint32_t at,void*out,size_t n){return source(at,out,n);},scratch);
 assert(graph.capture(worldframe::Base,pages,hint,hints));
 assert(frame.seal());
}
int main(){
 worldframe::Snapshot first;Live baseline;
 capture(first,baseline,nullptr,0,{MissingPage});
 uint32_t value=0xDADADADA;
 assert(!first.read(MissingField,&value,sizeof value));
 assert(value==0xDADADADA&&baseline.pageFailures>=2);
 // Demand is a byte span, not an old value. It is captured during the next
 // normal graph acquisition, before a failing full-page prefetch poisons it.
 worldframe::Snapshot recovered;Live newer;
 const worldframe::RequestedSpan exact{MissingField,4,1990};
 capture(recovered,newer,&exact,1,{MissingPage});
 assert(recovered.read(MissingField,&value,sizeof value)&&value==Expected);
 assert(newer.shortReads==1);
 const auto attempts=newer.calls;
 assert(recovered.read(MissingField,&value,sizeof value));
 assert(newer.calls==attempts); // no second live read after the seal
 // A stale or malformed hint is not allowed to prolong a past world or to
 // trigger extra acquisition of unrelated memory.
 worldframe::Snapshot stale;Live old;
 const worldframe::RequestedSpan expired{MissingField,4,900};
 capture(stale,old,&expired,1,{MissingPage});
 value=0xDADADADA;
 assert(!stale.read(MissingField,&value,sizeof value)&&value==0xDADADADA);
 assert(old.shortReads==0);
 // The capped exact-range budget does not scan a huge missing roster in
 // response to one large, untrusted span.
 worldframe::Snapshot bounded;Live limited;
 const worldframe::RequestedSpan over{MissingField,257,2000};
 capture(bounded,limited,&over,1,{MissingPage});
 assert(limited.shortReads==0);
 std::puts("PASS sealed world exact-demand prewarm: page miss -> 4-byte recovery, stale/budget skip, no second capture");
}
