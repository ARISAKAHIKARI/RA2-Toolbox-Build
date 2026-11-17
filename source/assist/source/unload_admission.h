#pragma once
#include "object_index.h"
namespace assist { namespace unloadadmission {
constexpr int Capacity=8192;
// A transport's manual Move after its passengers all disembarked must not
// invalidate the already-latched passenger handoff. A Move while still loaded
// cancels the old unload so that automation never overrides user movement.
inline bool retainAfterCarrierMove(uint32_t admittedToken,uint32_t currentToken,int loaded){
 return currentToken==admittedToken||loaded<=0;
}
struct Carrier {uint32_t pointer=0,id=0,token=0,attemptedAt=0;bool attempted=false;};
// Runtime-only storage, separate from both passenger planning batches and the
// game's event budget. Swap removal keeps cancellation and admission bounded.
struct Queue {
 static constexpr int ReindexBudget=128,ReindexAfterMutations=512;
 Carrier carriers[Capacity]{};
 // Two fixed hash owners. The active hash always accepts mutations, while
 // the replacement is built in <=128-entry slices and swapped in O(1).
 // Stale removed IDs are never accepted: find checks both UID and pointer
 // against the current carrier slot.
 ObjectIndex indexes[2];
 int count=0,cursor=0,frame=-1;
 uint32_t epoch=0,house=0,lastProgress=0;
 int active=0,rebuildAt=0,changes=0;bool rebuilding=false;
 ObjectIndex&current(){return indexes[active];}
 const ObjectIndex&current()const{return indexes[active];}
 void clear(){
  count=cursor=0;frame=-1;active=0;rebuildAt=changes=0;rebuilding=false;
  indexes[0].clear();indexes[1].clear();
 }
 void begin(uint32_t scene,uint32_t owner,uint32_t now){
  if(epoch!=scene||house!=owner){clear();epoch=scene;house=owner;}
  lastProgress=now;
 }
 int find(uint32_t pointer,uint32_t id)const{
  const uint32_t at=current().byId(id);
  return at&&at<=uint32_t(count)&&carriers[at-1].id==id&&
    carriers[at-1].pointer==pointer?int(at-1):-1;
 }
 void insertIndex(Carrier value,int at){
  current().add(value.pointer,value.id,uint32_t(at+1));
  if(rebuilding)indexes[1-active].add(value.pointer,value.id,uint32_t(at+1));
 }
 bool add(Carrier value){
  int at=find(value.pointer,value.id);
  if(at<0){if(count==Capacity)return false;at=count++;}
  carriers[at]=value;insertIndex(value,at);++changes;return true;
 }
 void remove(int at){
  if(at<0||at>=count)return;
  carriers[at]=carriers[--count];
  if(at<count)insertIndex(carriers[at],at);
  if(at<cursor)cursor=at;
  if(cursor>=count)cursor=0;
  // The removed UID may still hash to an old slot. find() rejects it by
  // checking the currently occupied carrier and the next rebuild reclaims it.
  ++changes;
 }
 void cancel(uint32_t pointer,uint32_t id){remove(find(pointer,id));}
 void reindex(){
  if(!rebuilding){
   if(changes<ReindexAfterMutations)return;
   indexes[1-active].clear();rebuildAt=0;rebuilding=true;
  }
  int work=0;
  while(rebuildAt<count&&work++<ReindexBudget){
   const auto&v=carriers[rebuildAt];
   indexes[1-active].add(v.pointer,v.id,uint32_t(rebuildAt+1));
   ++rebuildAt;
  }
  if(rebuildAt>=count){active=1-active;changes=0;rebuilding=false;}
 }
};
} }
