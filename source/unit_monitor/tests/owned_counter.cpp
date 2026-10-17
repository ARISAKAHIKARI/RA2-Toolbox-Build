#include "../source/owned_counter.h"
#include <cassert>
#include <cstring>
#include <cstdint>
#include <unordered_map>
#include <vector>

struct Memory {
 std::unordered_map<uint32_t,std::vector<uint8_t>> mem;
 void place(uint32_t a, const void* p, size_t n){ auto &v=mem[a];v.resize(n);std::memcpy(v.data(),p,n);}
 bool operator()(uint32_t a, void* p, size_t n) {
  auto it=mem.find(a);if(it==mem.end() || it->second.size()<n)return false;
  std::memcpy(p,it->second.data(),n);return true;
 }
};
void add(Memory& m, uint32_t counter,int capacity,uint32_t ptr,const int* data) {
 uint32_t h[3]={0x12345678,ptr,static_cast<uint32_t>(capacity)};
 m.place(counter,h,sizeof h);
 if(capacity>0&&ptr&&data)m.place(ptr,data,capacity*sizeof(int));
}
int main(){
 constexpr uint32_t counter=0x10000;
 {
  Memory m;int raw[]={2,0,4};add(m,counter,3,0x20000,raw);
  int out[8];int errors=0;
  assert(um::readCounterPrefix(m,counter,8,out,errors));
  assert(errors==0&&out[0]==2&&out[1]==0&&out[2]==4);
  for(int i=3;i<8;i++)assert(out[i]==0); // type index not allocated yet
 }
 {
  Memory m;add(m,counter,0,0,nullptr);int out[6];int errors=0;
  assert(um::readCounterPrefix(m,counter,6,out,errors));
  assert(errors==0);for(int x:out)assert(x==0);
 }
 {
  Memory m;int raw[]={1,2,3,4,5,6};add(m,counter,6,0x22000,raw);
  int out[3];int errors=0;assert(um::readCounterPrefix(m,counter,3,out,errors));
  assert(errors==0&&out[0]==1&&out[2]==3);
 }
 {
  Memory m;add(m,counter,4,0x20000,nullptr);int out[6];int errors=0;
  assert(!um::readCounterPrefix(m,counter,6,out,errors));
  assert(errors==1);for(int x:out)assert(x==0);
 }
 {
  Memory m;add(m,counter,-1,0,nullptr);int out[6];int errors=0;
  assert(!um::readCounterPrefix(m,counter,6,out,errors));
  assert(errors==1);for(int x:out)assert(x==0);
 }
 static_assert(um::OwnedOffsets[0]==0x5528 && um::OwnedOffsets[3]==0x5500,"YRpp layout");
 static_assert(um::OwnedPresentOffsets[0]==0x5578,"YRpp map-present layout");
 return 0;
}
