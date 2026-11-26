#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>
// A process may expose House/IsActive before all type vectors and metadata are
// loaded. Recheck identity and filter metadata; a nonempty partial list is not
// a permanently authenticated catalogue. Never persist these addresses to disk.
namespace external { namespace catalogue {
struct Stamp {uint64_t hash=1469598103934665603ull;int count=0;};
// Bound incomplete metadata probes independently of successful refresh time.
class ProbeRetryGate {
 bool attempted_=false;uint32_t at_=0;
public:
 void reset(){attempted_=false;at_=0;}
 bool begin(uint32_t now){
  if(attempted_&&uint32_t(now-at_)<100)return false;
  attempted_=true;at_=now;return true;
 }
};
// Optional probe visitor reuses the exact bytes that contributed to Stamp.
// No cached observation survives the caller's current acquisition.
struct TypeProbe {
 uint32_t pointer=0,items=0,owners=0;int level=-1;
 uint8_t insignificant=0,spawned=0;char id[24]{};
};
template<class Read,class Visit> bool captureVisit(Read read,Stamp&out,Visit visit){
 Stamp next;auto mix=[&](const void*p,size_t n){auto*b=(const uint8_t*)p;while(n--)next.hash=(next.hash^*b++)*1099511628211ull;};
 const uint32_t arrays[]={0xA8B218,0xA8E348,0xA83CE0,0xA83C68};
 struct Header {uint32_t items;int capacity;uint32_t flags;int count;};
 static_assert(sizeof(Header)==16,"native vector header layout");
 constexpr int Chunk=64;
 for(uint32_t a:arrays){
  Header head{};if(!read(a+4,&head,sizeof head))return false;
  const uint32_t p=head.items;const int n=head.count,cap=head.capacity;
  if(n<0||n>4096||cap<n||cap>65536||(n&&(p<0x10000||uint64_t(p)+uint64_t(n)*4>=0x80000000ull)))return false;
  mix(&p,4);mix(&n,4);next.count+=n;
  uint32_t pointers[Chunk];
  for(int first=0;first<n;first+=Chunk){
   const int count=std::min(Chunk,n-first);
   if(!read(p+uint32_t(first)*4,pointers,size_t(count)*4))return false;
   for(int i=0;i<count;++i){uint32_t t=pointers[i],owners=0;int level=-1;uint8_t insignificant=0,spawned=0;char id[24]{},cameo[24]{};
    if(t<0x10000||!read(t+0x24,id,24)||!id[0]||!read(t+0x634,&level,4)||!read(t+0x6CC,&owners,4)||!read(t+0x232,&insignificant,1)||!read(t+0xD54,&spawned,1)||!read(t+0x6D6,cameo,24))return false;
    mix(&t,4);mix(id,24);mix(cameo,24);mix(&level,4);mix(&owners,4);mix(&insignificant,1);mix(&spawned,1);
    TypeProbe probe{};probe.pointer=t;probe.items=p;probe.owners=owners;probe.level=level;probe.insignificant=insignificant;probe.spawned=spawned;
    std::copy_n(id,24,probe.id);visit(a,first+i,probe);
   }
  }
  Header after{};if(!read(a+4,&after,sizeof after)||after.items!=p||after.count!=n)return false;
 }
 if(!next.count)return false;out=next;return true;
}
template<class Read> bool capture(Read read,Stamp&out){
 return captureVisit(read,out,[](uint32_t,int,const TypeProbe&){});
}
// A 500 ms metadata cache can outlive the transport's 250 ms page lease.
// Retain its dependencies while the consumer is active, not its old bytes.
// Incomplete discovery keeps the old dependencies until a full replacement
// succeeds; otherwise successive misses can evict each other's warmup pages.
class Dependencies {
 std::vector<uint32_t> pages_,candidate_;
 template<class R>static auto retain(R read,const std::vector<uint32_t>&pages,int)->decltype(read.retainWorldPages(pages),void()){read.retainWorldPages(pages);}
 template<class R>static void retain(R,const std::vector<uint32_t>&,long){}
public:
 void clear(){pages_.clear();candidate_.clear();}
 template<class R>void maintain(R read)const{retain(read,pages_,0);}
 template<class R>bool capture(R read,Stamp&out){
  candidate_.clear();auto tracked=[&](uint32_t at,void*to,size_t n){
   if(at>=0x10000&&n&&uint64_t(at)+n<=0x80000000ull)
    for(uint32_t page=at&~4095u;uint64_t(page)<uint64_t(at)+n;page+=4096)candidate_.push_back(page);
   return read(at,to,n);
  };
  const bool complete=catalogue::capture(tracked,out);
  if(!complete)candidate_.insert(candidate_.end(),pages_.begin(),pages_.end());
  std::sort(candidate_.begin(),candidate_.end());candidate_.erase(std::unique(candidate_.begin(),candidate_.end()),candidate_.end());
  pages_.swap(candidate_);return complete;
 }
};
} }
