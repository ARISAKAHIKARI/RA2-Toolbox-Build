#include "../source/repair_result_filter.h"
#include <cassert>
#include <cstdio>
#include <vector>

int main(){
 using assist::repairfilter::Filter;
 Filter proof;
 std::vector<uint32_t> source,results,accepted;
 for(uint32_t i=1;i<=Filter::MaxItems;++i)source.push_back(100000u+i);
 // Reverse Host order and add duplicates: output order must remain unchanged.
 for(uint32_t i=Filter::MaxItems;i>0;--i)results.push_back(100000u+i);
 results[111]=results[110];
 results[222]=42; // An ID absent from retained Host input is not an order.
 assert(proof.begin(int(source.size()),int(results.size())));
 unsigned calls=0;
 while(proof.running){
  const size_t before=accepted.size();
  const bool complete=proof.step(unsigned(source.size()),unsigned(results.size()),
   [&](unsigned i){return source[i];},
   [&](unsigned i){return results[i];},
   [&](uint32_t id){accepted.push_back(id);});
  ++calls;assert(calls<=5&&!proof.failed);
  if(calls<=2)assert(accepted.empty()); // build entire input before result membership
  if(complete)assert(!proof.running);
  assert(accepted.size()-before<=Filter::MaxOperationsPerTick);
 }
 assert(accepted.size()==results.size()-1);
 unsigned resultCursor=0;
 for(auto id:accepted){
  while(results[resultCursor]==42)++resultCursor;
  assert(id==results[resultCursor++]);
 }
 // Reset while incomplete must not publish any remaining source IDs.
 assert(proof.begin(int(source.size()),int(results.size())));
 assert(!proof.step(unsigned(source.size()),unsigned(results.size()),
  [&](unsigned i){return source[i];},[&](unsigned i){return results[i];},
  [&](uint32_t){assert(false);}));
 proof.reset();assert(!proof.running);
 assert(!proof.begin(8193,1));
 assert(!proof.begin(1,-1));
 // The same memory and hash table can be reused without a 32K-slot clear.
 assert(proof.begin(1,2));
 int matched=0;
 assert(proof.step(1,2,[&](unsigned){return 11u;},
   [&](unsigned i){return i?12u:11u;},
   [&](uint32_t id){assert(id==11);++matched;}));
 assert(matched==1);
 // Degenerate IDs: 33 colliding live records cannot make a worker result
 // authoritative if hash lookup would exceed its 32-probe budget.
 assert(proof.begin(33,1));
 std::vector<uint32_t> collisions;
 const unsigned target=Filter::hash(7);
 for(uint32_t id=1;id<4000000&&collisions.size()<33;++id)
  if(Filter::hash(id)==target)collisions.push_back(id);
 assert(collisions.size()==33);
 assert(!proof.step(33,1,[&](unsigned i){return collisions[i];},
  [&](unsigned){return collisions[0];},[&](uint32_t){assert(false);}));
 assert(proof.failed&&!proof.running);
 std::puts("PASS bounded incremental repair validation, Host order/duplicates, no partial result, collision failure and safe reuse");
}
