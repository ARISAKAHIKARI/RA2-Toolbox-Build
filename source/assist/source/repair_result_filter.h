#pragma once
#include <cstdint>

namespace assist { namespace repairfilter {
// The Host has already computed an ordered list of candidate IDs. The game
// dispatcher must check that each ID appeared in the retained input, but may
// NOT sort/copy 8192 values and process 8192 results in one game callback.
//
// This fixed-capacity generational hash is built and queried incrementally.
// No heap, no OS memory reads, no full-table clear, and no unbounded probes.
// An adversarial hash collision is UNKNOWN, never permission to issue an order.
struct Filter {
 static constexpr unsigned MaxItems=8192,Slots=32768,MaxOperationsPerTick=4096,MaxProbe=32;
 uint32_t values[Slots]{},generations[Slots]{},generation=1;
 unsigned built=0,filtered=0;
 bool running=false,failed=false;
 static unsigned hash(uint32_t id){
  id^=id>>16;id*=0x7feb352du;id^=id>>15;
  id*=0x846ca68bu;id^=id>>16;return id&(Slots-1);
 }
 void reset(){running=false;failed=false;built=filtered=0;}
 bool begin(int inputCount,int resultCount){
  reset();
  if(inputCount<0||inputCount>int(MaxItems)||resultCount<0||resultCount>int(MaxItems))return false;
  if(++generation==0){failed=true;return false;} // No 32K-slot reset in the game callback.
  running=true;return true;
 }
 bool add(uint32_t id){
  if(!id)return true;
  unsigned at=hash(id);
  for(unsigned i=0;i<MaxProbe;++i,at=(at+1)&(Slots-1)){
   if(generations[at]!=generation||values[at]==id){
    values[at]=id;generations[at]=generation;return true;
   }
  }
  return false;
 }
 bool contains(uint32_t id)const{
  if(!id)return false;
  unsigned at=hash(id);
  for(unsigned i=0;i<MaxProbe;++i,at=(at+1)&(Slots-1)){
   if(generations[at]!=generation)return false;
   if(values[at]==id)return true;
  }
  return false;
 }
 // Returns true only after the ENTIRE retained input and result have been
 // verified. 'emit' retains Host result order and duplicates. Callers must
 // not publish any partially staged IDs before it returns true.
 template<class InputId,class ResultId,class Emit>
 bool step(unsigned inputCount,unsigned resultCount,InputId inputId,ResultId resultId,Emit emit){
  if(!running||failed||inputCount>MaxItems||resultCount>MaxItems){failed=true;return false;}
  for(unsigned ops=0;ops<MaxOperationsPerTick;++ops){
   if(built<inputCount){
    if(!add(inputId(built++))){failed=true;running=false;return false;}
   }else if(filtered<resultCount){
    const uint32_t id=resultId(filtered++);
    if(contains(id))emit(id);
   }else{running=false;return true;}
  }
  if(built==inputCount&&filtered==resultCount){running=false;return true;}
  return false;
 }
};
}} // namespace assist::repairfilter
