#include "../source/network_sender_policy.h"
#include <cassert>
#include <cstdio>
#include <vector>
using namespace assist::networksender;
int main(){
 unsigned rings=0,partitions=0;
 // Every legal circular queue visits exactly count distinct slots, including
 // empty/full queues where head==tail. Advancing head must preserve order.
 for(int head=0;head<128;++head)for(int count=0;count<=128;++count){
  Ring ring{count,head,(head+count)&127};assert(valid(ring));bool visited[128]{};
  for(unsigned i=0;i<unsigned(count);++i){unsigned at=slot(ring,i);assert(at<128&&!visited[at]);visited[at]=true;}
  if(count){Ring popped{count-1,(head+1)&127,ring.tail};assert(valid(popped));
   for(unsigned i=0;i<unsigned(popped.count);++i)assert(slot(popped,i)==slot(ring,i+1));}
  ++rings;
 }
 for(int bad:{-1,129})assert(!valid(Ring{bad,0,0}));
 assert(!valid(Ring{1,0,0})&&!valid(Ring{0,-1,127})&&!valid(Ring{0,128,0}));
 assert(!validHouse(-1)&&!validHouse(32)&&validHouse(0)&&validHouse(31));
 assert(!gameplay(0)&&!gameplay(0x19)&&!gameplay(0x1C)&&!gameplay(0x30));
 assert(gameplay(4)&&gameplay(8)&&gameplay(0x16));
 // Exhaust all binary House streams up to eight events and independent packet
 // limits. Partitioning may neither reorder/drop an event nor cross House.
 for(unsigned length=0;length<=8;++length)for(unsigned bits=0;bits<(1u<<length);++bits)
  for(unsigned limit=1;limit<=8;++limit){
   std::vector<int> input,output;for(unsigned i=0;i<length;++i)input.push_back((bits>>i)&1);
   unsigned consumed=0;
   while(consumed<length){
    const auto house=[&](unsigned i){return input[consumed+i];};
    unsigned n=prefix(length-consumed,limit,house);assert(n&&n<=limit&&n<=length-consumed);
    for(unsigned i=0;i<n;++i){assert(house(i)==house(0));output.push_back(house(i));}
    if(n<limit&&consumed+n<length)assert(house(n)!=house(0));
    consumed+=n;
   }
   assert(output==input);++partitions;
  }
 assert(prefix(3,0,[](unsigned){return 0;})==0);
 assert(prefix(0,3,[](unsigned){return 0;})==0);
 std::printf("PASS %u circular queues and %u ordered House partitions\n",rings,partitions);
}
