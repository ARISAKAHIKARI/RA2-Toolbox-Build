#pragma once
#include <array>
#include <cstdint>

namespace assist::entrycohort {
// The local fast path has a fixed upper bound. Larger games use the normal
// exact worker sample instead of growing a live traversal on the callback.
constexpr int MaxMembers=64;
using Members=std::array<uint32_t,MaxMembers>;
template<class Read,class Frozen,class Lookup>
bool verify(Read read,Frozen frozen,Lookup lookup,uint32_t items,int count,Members&live){
 if(count<=0||count>MaxMembers||items<0x10000)return false;
 struct Vector {uint32_t vt,items;int capacity;uint32_t flags;int count,step;};
 constexpr uint32_t TechnoVector=0xA8EC78;
 Vector before{},after{};
 Members captured{};
 live.fill(0);
 const auto bytes=sizeof(uint32_t)*count;
 if(!read(TechnoVector,&before,sizeof before)||before.items!=items||before.count!=count||
    before.capacity<count||before.capacity>262144||
    !frozen(items,captured.data(),bytes)||!read(items,live.data(),bytes)||live!=captured)return false;
 for(int i=0;i<count;++i){
  const uint32_t pointer=live[i];if(!pointer)continue;
  uint32_t id=0;if(pointer<0x10000||!read(pointer+0x10,&id,sizeof id)||!id||lookup(id)!=pointer)return false;
 }
 return read(TechnoVector,&after,sizeof after)&&after.items==items&&after.count==count&&after.capacity==before.capacity;
}
}
