#include "../source/unload_admission.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
using namespace assist;
int main(){
 static unloadadmission::Queue q;
 constexpr int N=unloadadmission::Capacity;
 q.begin(11,0x20000,1000);
 for(int i=0;i<N;++i){
  const uint32_t p=0x10000u+uint32_t(i)*16u;
  assert(q.add({p,uint32_t(i+1),9}));
 }
 assert(q.count==N);
 const auto old=q.carriers[17];
 q.remove(17);
 assert(q.find(old.pointer,old.id)<0);
 assert(q.find(q.carriers[17].pointer,q.carriers[17].id)==17);
 const uint32_t freshP=0x10000u+uint32_t(N+10)*16u,freshId=uint32_t(N+11);
 assert(q.add({freshP,freshId,5}));
 assert(q.find(freshP,freshId)>=0);
 // Every call is at most 128 inserts, regardless of 8192 live carriers.
 // Mutations made halfway through rebuilding must go into both hash owners.
 for(int frame=0;frame<100&&q.rebuilding==false||frame<64;++frame){
  const int before=q.rebuildAt;
  q.reindex();
  if(q.rebuilding)assert(q.rebuildAt-before<=q.ReindexBudget);
  if(frame==9){
   const auto erased=q.carriers[42];q.remove(42);
   assert(q.find(erased.pointer,erased.id)<0);
   assert(q.find(q.carriers[42].pointer,q.carriers[42].id)==42);
   assert(q.add({0x70000u,30000u,3}));
   assert(q.find(0x70000u,30000u)>=0);
  }
  if(frame>=64&&!q.rebuilding)break;
 }
 assert(!q.rebuilding);
 for(int i=0;i<q.count;++i)assert(q.find(q.carriers[i].pointer,q.carriers[i].id)==i);
 assert(q.find(old.pointer,old.id)<0);
 q.begin(12,0x30000,5000);
 assert(q.count==0&&!q.rebuilding&&q.find(freshP,freshId)<0);
 assert(q.add({0x12000u,42u,1})&&q.find(0x12000u,42u)==0);
 std::puts("PASS 8K carrier index incremental 128-work slices, concurrent add/remove, safe swap, epoch retirement");
}
