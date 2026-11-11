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
template<class Read> bool capture(Read read,Stamp&out){
 Stamp next;auto mix=[&](const void*p,size_t n){auto*b=(const uint8_t*)p;while(n--)next.hash=(next.hash^*b++)*1099511628211ull;};
 const uint32_t arrays[]={0xA8B218,0xA8E348,0xA83CE0,0xA83C68};
 for(uint32_t a:arrays){uint32_t p=0;int n=-1,cap=-1;
  if(!read(a+4,&p,4)||!read(a+8,&cap,4)||!read(a+16,&n,4)||n<0||n>4096||cap<n||cap>65536||(n&&p<0x10000))return false;
  mix(&p,4);mix(&n,4);next.count+=n;
  for(int i=0;i<n;++i){uint32_t t=0,owners=0;int level=-1;uint8_t insignificant=0,spawned=0;char id[24]{},cameo[24]{};
   if(!read(p+i*4,&t,4)||t<0x10000||!read(t+0x24,id,24)||!id[0]||!read(t+0x634,&level,4)||!read(t+0x6CC,&owners,4)||!read(t+0x232,&insignificant,1)||!read(t+0xD54,&spawned,1)||!read(t+0x6D6,cameo,24))return false;
   mix(&t,4);mix(id,24);mix(cameo,24);mix(&level,4);mix(&owners,4);mix(&insignificant,1);mix(&spawned,1);
  }
  uint32_t after=0;int size=-1;if(!read(a+4,&after,4)||!read(a+16,&size,4)||after!=p||size!=n)return false;
 }
 if(!next.count)return false;out=next;return true;
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
