#include "../source/unload_policy.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
struct Entry {uint32_t carrier=0,pointer=0;int history=-1;uint32_t stamp=0;};
static constexpr unsigned Capacity=16384;
static Entry actual[Capacity],scratch[Capacity],reference[Capacity];
static uint32_t rnd=0xCDEF6312u;
static uint32_t next(){rnd^=rnd<<13;rnd^=rnd>>17;rnd^=rnd<<5;return rnd;}
static bool less(const Entry&a,const Entry&b){
 if(a.carrier!=b.carrier)return a.carrier<b.carrier;
 if(a.pointer!=b.pointer)return a.pointer<b.pointer;
 return a.history<b.history;
}
static void verify(int n,bool collide=false){
 for(int i=0;i<n;++i){
  const uint32_t carrier=collide?0x20000u:((next()%19u)+1u)*0x10000u;
  const uint32_t pointer=(next()%127u)*16u+0x10000u;
  const int history=(i%3==0)?-1:int(next()%8192u);
  actual[i]={carrier,pointer,history,uint32_t(i)};
  reference[i]=actual[i];
 }
 // Base implementation is std::sort by (carrier,pointer,history). Stable
 // radix may reorder equal-key records; compare sorted keys and the multiset.
 std::sort(reference,reference+n,less);
 assert(assist::unloadpolicy::radixSortRoster(actual,scratch,n));
 for(int i=0;i<n;++i){
  assert(actual[i].carrier==reference[i].carrier);
  assert(actual[i].pointer==reference[i].pointer);
  assert(actual[i].history==reference[i].history);
  if(i)assert(!less(actual[i],actual[i-1]));
 }
 std::vector<uint32_t> before,after;
 for(int i=0;i<n;++i){before.push_back(reference[i].stamp);after.push_back(actual[i].stamp);}
 std::sort(before.begin(),before.end());std::sort(after.begin(),after.end());
 assert(before==after);
}
static void verifyNativeCargoLookup(){
 // Synthetic cargo links include many passengers with repeated pointer and
 // different manual tokens/history. Every conflicting entry must be checked.
 for(unsigned i=0;i<Capacity;++i)
  actual[i]={0x10000u+uint32_t(i%17u)*0x1000u,
             0x80000u+uint32_t((i*37u)%127u)*16u,
             i%3?int(i):-1, i%11?7u:8u};
 assert(assist::unloadpolicy::radixSortRoster(actual,scratch,int(Capacity)));
 unsigned queries=0;
 for(uint32_t c=0x10000u;c<=0x20000u;c+=0x1000u){
  const auto*begin=std::lower_bound(actual,actual+Capacity,c,
    [](const Entry&e,uint32_t carrier){return e.carrier<carrier;});
  const auto*end=std::upper_bound(begin,actual+Capacity,c,
    [](uint32_t carrier,const Entry&e){return carrier<e.carrier;});
  for(unsigned p=0;p<140;++p){
   const uint32_t pointer=0x80000u+p*16u;
   const auto*at=assist::unloadpolicy::firstRosterPointer(begin,end,pointer);
   const auto*expected=std::lower_bound(begin,end,pointer,
    [](const Entry&e,uint32_t v){return e.pointer<v;});
   assert(at==expected);
   for(uint32_t token:{7u,8u,9u}){
    bool oldReject=false,newReject=false;
    for(const auto*v=begin;v<end;++v)if(v->pointer==pointer&&v->stamp!=token)oldReject=true;
    for(const auto*v=at;v<end&&v->pointer==pointer;++v)
     if(v->stamp!=token)newReject=true;
    assert(oldReject==newReject);
    ++queries;
   }
  }
 }
 // The old linear scan found every mismatching duplicate, not just the first
 // entry. A one-person roster must not shortcut this identity validation.
 Entry duplicated[4]={{10,20,-1,7},{10,20,0,8},{10,30,-1,7},{11,20,-1,7}};
 const Entry*first=assist::unloadpolicy::firstRosterPointer(duplicated,duplicated+3,20);
 assert(first==duplicated&&first[1].pointer==20&&first[1].stamp!=7);
 assert(assist::unloadpolicy::firstRosterPointer(duplicated+2,duplicated+3,20)==duplicated+2);
 assert(assist::unloadpolicy::firstRosterPointer(duplicated,duplicated+3,99)==duplicated+3);
 std::printf("PASS native cargo binary search: %u old/new equivalent token and duplicate checks across 16K candidate roster\n",queries);
}
int main(){
 assert(!assist::unloadpolicy::radixSortRoster(actual,scratch,-1));
 assert(!assist::unloadpolicy::radixSortRoster(actual,scratch,int(Capacity)+1));
 for(int n:{0,1,2,3,9,32,255,256,2048,8192,16384}){
  for(int trial=0;trial<(n>=8192?3:16);++trial)verify(n,trial%2==0);
 }
 // Explicit signed history ordering: original -1 must precede history 0.
 actual[0]={10,20,0,1};actual[1]={10,20,-1,2};
 actual[2]={10,20,2,3};actual[3]={10,20,1,4};
 assert(assist::unloadpolicy::radixSortRoster(actual,scratch,4));
 for(int i=0;i<4;++i)assert(actual[i].history==i-1);
 verifyNativeCargoLookup();
 using Clock=std::chrono::steady_clock;
 for(unsigned i=0;i<Capacity;++i)reference[i]=actual[i]={uint32_t(0x10000+((i*13)%4096)*16),uint32_t(0x20000+((i*59)%8192)*16),i%2?-1:int(i),i};
 auto started=Clock::now();
 for(int k=0;k<10;++k){std::copy(reference,reference+Capacity,actual);std::sort(actual,actual+Capacity,less);}
 const double baseline=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
 started=Clock::now();
 for(int k=0;k<10;++k){std::copy(reference,reference+Capacity,actual);assert(assist::unloadpolicy::radixSortRoster(actual,scratch,Capacity));}
 const double radix=std::chrono::duration<double,std::milli>(Clock::now()-started).count();
 std::printf("PASS Ctrl+D exact-key radix sort: 16K candidates, 12 linear passes, unsigned pointer and signed history, collision / hash-independent equivalence\n");
 std::printf("SYNTHETIC_MS comparator=%.3f radix=%.3f (not game FPS)\n",baseline,radix);
}
