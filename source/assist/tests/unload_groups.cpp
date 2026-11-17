#include "../source/unload_policy.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
using assist::unloadpolicy::CarrierGroups;
struct Entry{int next=-1;uint32_t id=0;};
int main(){
 static CarrierGroups<>groups;
 static Entry entries[32768];
 const uint32_t carriers[]={0x10000u,0x20000u,0x30000u,0x40000u};
 groups.clear();
 for(int i=0;i<32768;++i){
  const uint32_t carrier=carriers[unsigned(i)%4];
  entries[i].id=uint32_t(i+1);
  assert(groups.add(carrier,i,entries));
 }
 int counted=0;for(unsigned k=0;k<4;++k){
  int next=groups.first(carriers[k]);int previous=-4;int inGroup=0;
  while(next>=0){
   assert(next>previous&&next<32768);
   assert(unsigned(next)%4==k);
   ++counted;++inGroup;previous=next;next=entries[next].next;
  }
  assert(inGroup==8192);
 }
 assert(counted==32768&&groups.first(0xA0000u)==-1);
 groups.clear();assert(groups.first(carriers[0])==-1);
 assert(groups.add(carriers[1],0,entries));
 assert(groups.first(carriers[1])==0&&entries[0].next==-1);
 assert(!groups.add(0,1,entries)&&!groups.valid);
 assert(groups.first(carriers[1])==-1); // fail-closed incomplete grouping
 groups.clear();assert(groups.valid&&groups.first(carriers[1])==-1);
 // Do not authorize a partially built roster when its 64-probe hash budget
 // is exhausted by an adversarial set of colliding carrier addresses.
 const unsigned bucket=CarrierGroups<>::hash(0x10000u);
 std::vector<uint32_t>collisions;
 for(uint32_t p=0x10000u;p<25000000u&&collisions.size()<65;++p)
  if(CarrierGroups<>::hash(p)==bucket)collisions.push_back(p);
 assert(collisions.size()==65);
 for(int i=0;i<64;++i)assert(groups.add(collisions[size_t(i)],i,entries));
 assert(!groups.add(collisions[64],64,entries));
 assert(!groups.valid&&groups.first(collisions[0])==-1);
 std::puts("PASS unload carrier groups: 32K stable grouping, O(1) reset and collision fail-closed");
}
