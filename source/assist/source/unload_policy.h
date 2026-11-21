#pragma once
#include <cstdint>
namespace assist { namespace unloadpolicy {
// Per-observation pointer lookup. Clear only occupied slots so a small selected
// roster does not zero a large table every admitted tick. The history itself
// retains its order and expiry/manual-token rules.
template<unsigned Capacity=8192> struct RosterLookup {
 static_assert(Capacity&&!(Capacity&(Capacity-1))&&Capacity<=16384,"bounded power-of-two roster required");
 static constexpr unsigned Slots=Capacity*2;
 struct Entry {uint32_t pointer=0;int index=0;};
 Entry entries[Slots]{};uint16_t occupied[Capacity]{};unsigned count=0;
 void clear(){for(unsigned n=0;n<count;++n)entries[occupied[n]]={};count=0;}
 unsigned slot(uint32_t pointer)const{
  uint32_t key=pointer;key^=key>>16;key*=0x7FEB352Du;key^=key>>15;key*=0x846CA68Bu;key^=key>>16;
  unsigned at=key&(Slots-1);
  for(unsigned n=0;n<Slots;++n,at=(at+1)&(Slots-1))if(!entries[at].pointer||entries[at].pointer==pointer)return at;
  return Slots;
 }
 int find(uint32_t pointer)const{if(!pointer)return -1;unsigned at=slot(pointer);return at<Slots&&entries[at].pointer?entries[at].index:-1;}
 bool add(uint32_t pointer,int index){
  if(!pointer)return false;unsigned at=slot(pointer);if(at==Slots)return false;
  if(!entries[at].pointer){if(count==Capacity)return false;occupied[count++]=uint16_t(at);}
  entries[at]={pointer,index};return true;
 }
};
// Stable fixed-storage LSD radix ordering for the native Ctrl+D candidate
// list. Equivalent key order to the former std::sort(carrier,pointer,history),
// with bounded 12 * N copies, no heap allocations and no comparisons that
// grow as N log N. This only orders candidate records; final native eligibility
// and manual-token checks remain in the game dispatcher.
template<class Entry,unsigned Capacity>bool radixSortRoster(Entry (&entries)[Capacity],
 Entry (&scratch)[Capacity],int count){
 if(count<0||unsigned(count)>Capacity)return false;
 if(count<2)return true;
 // Ordinary Ctrl+D has only a handful of passengers. Avoid 12 radix
 // histograms for 2..32 candidates; stable insertion costs <=496 compares.
 if(count<=32){
  for(int i=1;i<count;++i){
   const Entry entry=entries[i];int j=i;
   while(j>0&&(entry.carrier<entries[j-1].carrier||
         (entry.carrier==entries[j-1].carrier&&
           (entry.pointer<entries[j-1].pointer||
            (entry.pointer==entries[j-1].pointer&&entry.history<entries[j-1].history))))){
    entries[j]=entries[j-1];--j;
   }
   entries[j]=entry;
  }
  return true;
 }
 Entry*src=entries;Entry*dst=scratch;
 for(unsigned pass=0;pass<12;++pass){
  unsigned positions[256]{};
  for(int i=0;i<count;++i){
   const uint32_t key=pass<4?(uint32_t(src[i].history)^0x80000000u):
                      pass<8?src[i].pointer:src[i].carrier;
   ++positions[(key>>((pass%4)*8))&255u];
  }
  unsigned position=0;
  for(unsigned b=0;b<256;++b){const unsigned n=positions[b];positions[b]=position;position+=n;}
  for(int i=0;i<count;++i){
   const uint32_t key=pass<4?(uint32_t(src[i].history)^0x80000000u):
                      pass<8?src[i].pointer:src[i].carrier;
   dst[positions[(key>>((pass%4)*8))&255u]++]=src[i];
  }
  Entry*previous=src;src=dst;dst=previous;
 }
 // Twelve passes are even, so src is the original fixed array.
 return src==entries;
}
// Locate a passenger by pointer inside ONE carrier's already-sorted roster.
// Only those entries whose pointer equals the native cargo link need their
// saved UID/manual-token checked. O(log N + duplicates), with no cache,
// allocation, new game read or change to UNKNOWN/invalid admission semantics.
template<class Entry>Entry* firstRosterPointer(Entry*first,Entry*last,uint32_t pointer){
 while(first<last){
  Entry*mid=first+(last-first)/2;
  if(mid->pointer<pointer)first=mid+1;
  else last=mid;
 }
 return first;
}
// Carrier-keyed adjacency for candidate-only admission. Replaces sorting up
// to 32K passengers on the game thread with O(N) fixed-storage insertions.
// The mapping does not authorize a command; every passenger is still checked
// live before its native EventClass is queued.
template<unsigned Capacity=32768,unsigned Buckets=65536> struct CarrierGroups {
 static_assert(Capacity&&Buckets>=Capacity&&!(Buckets&(Buckets-1)),"power-of-two buckets required");
 static constexpr unsigned MaxProbe=64;
 struct Bucket {uint32_t carrier=0,epoch=0;int first=-1,last=-1;};
 Bucket buckets[Buckets]{};
 uint32_t epoch=1;
 bool valid=true;
 static unsigned hash(uint32_t p){p^=p>>16;p*=0x7feb352du;p^=p>>15;return p&(Buckets-1);}
 void clear(){
  if(++epoch==0){for(auto&v:buckets)v.epoch=0;epoch=1;}
  valid=true;
 }
 int first(uint32_t carrier)const{
  if(!carrier||!valid)return -1;
  unsigned at=hash(carrier);
  for(unsigned probe=0;probe<MaxProbe;++probe,at=(at+1)&(Buckets-1)){
   const auto&b=buckets[at];
   if(b.epoch!=epoch)return -1;
   if(b.carrier==carrier)return b.first;
  }
  return -1;
 }
 template<class Entry>bool add(uint32_t carrier,int index,Entry*entries){
  if(!carrier||!valid||index<0||index>=int(Capacity)||!entries){valid=false;return false;}
  unsigned at=hash(carrier);
  for(unsigned probe=0;probe<MaxProbe;++probe,at=(at+1)&(Buckets-1)){
   auto&b=buckets[at];
   if(b.epoch!=epoch){b={carrier,epoch,index,index};entries[index].next=-1;return true;}
   if(b.carrier==carrier){
    entries[index].next=-1;
    if(b.last>=0)entries[b.last].next=index;
    else b.first=index;
    b.last=index;return true;
   }
  }
  valid=false;return false; // UNKNOWN: do not publish partial enrolment.
 }
};
// An ejected passenger may briefly inherit an engine Move/Attack mission.
// For automatic ejection only, allow a short grace after landing, but never
// override explicit user Stop (13) or Enter (7).
constexpr int ScatterFrames=180;
inline bool completeIdentityIndex(int indexedFrame,int frame){return indexedFrame==frame;}
// YRpp Sequence::Deploy=27; Deployed/DeployedFire/DeployedIdle=28..30.
inline bool completedDeploy(int sequence){return sequence>=28&&sequence<=30;}
inline bool deployObserved(int sequence){return sequence>=27&&sequence<=30;}
inline bool initialMayContinue(int mission,int mode,int landedAge,bool enteringOldCarrier=false,bool ejectDeployer=false){
 if(mission==2&&(mode==0||ejectDeployer&&mode!=0))return landedAge>=0&&landedAge<=(mode==0?900:ScatterFrames);
 if(mission==0||mission==5||mission==6||mission==11)return true;
 if(mission==7&&enteringOldCarrier)return true;
 if(mission==13||mission==7||mission==16)return false;
 return mode!=0&&landedAge>=0&&landedAge<=8;
}
inline bool shouldKeepWatching(bool feature,bool carrierAlive,bool alreadyTracked){
 return feature&&(carrierAlive||alreadyTracked);
}
} }
