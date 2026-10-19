#pragma once
#include <stdint.h>
namespace assist {
// Fixed storage: no allocator or OS reads in input hooks.
struct ObjectIndex {
 static constexpr unsigned Size=32768;
 uint32_t pointers[Size]{},ids[Size]{},objects[Size]{},pointerEpoch[Size]{},idEpoch[Size]{},epoch=1;
 static unsigned hash(uint32_t value){return (value*2654435761u)&(Size-1);}
 void clear(){if(++epoch==0){for(unsigned i=0;i<Size;++i)pointerEpoch[i]=idEpoch[i]=0;epoch=1;}}
 void add(uint32_t pointer,uint32_t id,uint32_t value=0){
  if(!pointer)return;
  unsigned slot=hash(pointer>>4);
  for(unsigned n=0;n<Size;++n,slot=(slot+1)&(Size-1))if(pointerEpoch[slot]!=epoch||pointers[slot]==pointer){pointers[slot]=pointer;pointerEpoch[slot]=epoch;break;}
  if(!id)return;slot=hash(id);
  for(unsigned n=0;n<Size;++n,slot=(slot+1)&(Size-1))if(idEpoch[slot]!=epoch||ids[slot]==id){ids[slot]=id;objects[slot]=value?value:pointer;idEpoch[slot]=epoch;break;}
 }
 bool contains(uint32_t pointer)const{
  if(!pointer)return false;unsigned slot=hash(pointer>>4);
  for(unsigned n=0;n<Size;++n,slot=(slot+1)&(Size-1)){if(pointerEpoch[slot]!=epoch)return false;if(pointers[slot]==pointer)return true;}return false;
 }
 uint32_t byId(uint32_t id)const{
  if(!id)return 0;unsigned slot=hash(id);
  for(unsigned n=0;n<Size;++n,slot=(slot+1)&(Size-1)){if(idEpoch[slot]!=epoch)return 0;if(ids[slot]==id)return objects[slot];}return 0;
 }
};
struct ManualVersions {
 static constexpr unsigned Size=8192;
 uint32_t ids[Size]{},versions[Size]{},orePointers[Size]{};
 uint8_t oreState[Size]{},oreExpected[Size]{};
 uint16_t pausedSlots[Size]{},pausedPosition[Size]{};unsigned used=0,pausedCount=0;
 unsigned slot(uint32_t id)const{unsigned s=ObjectIndex::hash(id)&(Size-1);for(unsigned n=0;n<Size;++n,s=(s+1)&(Size-1))if(!ids[s]||ids[s]==id)return s;return Size;}
 uint32_t get(uint32_t id)const{unsigned s=slot(id);return s<Size&&ids[s]==id?versions[s]:0;}
 bool trackable(uint32_t id)const{return slot(id)<Size;}
 void removePaused(unsigned s){if(!pausedPosition[s])return;unsigned at=pausedPosition[s]-1,last=pausedSlots[--pausedCount];pausedSlots[at]=uint16_t(last);if(at<pausedCount)pausedPosition[last]=uint16_t(at+1);pausedPosition[s]=0;}
 void addPaused(unsigned s){if(!orePointers[s]||!(oreState[s]&1)||pausedPosition[s])return;pausedSlots[pausedCount]=uint16_t(s);pausedPosition[s]=uint16_t(++pausedCount);}
 void change(uint32_t id,uint32_t pointer=0,bool miner=true){unsigned s=slot(id);if(s<Size){if(!ids[s])++used;removePaused(s);ids[s]=id;if(++versions[s]==0)++versions[s];oreState[s]=miner?1:0;oreExpected[s]=0;orePointers[s]=miner?pointer:0;addPaused(s);}}
 void expectOreMission(uint32_t id,int mission){unsigned s=slot(id);if(s<Size&&ids[s]==id)oreExpected[s]=mission>=0&&mission<32?uint8_t(mission+1):0;}
 void resumeOre(uint32_t id){unsigned s=slot(id);if(s<Size&&ids[s]==id){oreState[s]=0;removePaused(s);}}
 bool watchingOre(uint32_t id)const{unsigned s=slot(id);return s<Size&&ids[s]==id&&orePointers[s]&&(oreState[s]&1);}
 bool orePaused(uint32_t id)const{unsigned s=slot(id);return s==Size||(ids[s]==id&&(oreState[s]&1));}
 bool observeOre(uint32_t id,int mission,int queued){
  unsigned s=slot(id);if(s>=Size)return true;if(ids[s]!=id||!(oreState[s]&1))return false;
  // A click can precede its synchronized execution. An old Harvest snapshot
  // cannot undo that click: require an observed exit and then a fresh Harvest.
  // Cancellation-only input has no inferred mission (e.g. S or right DOWN).
  // Observe an actual non-mining mission; mining Return/Guard cycles do not
  // acknowledge a delayed manual order. A resolved click narrows this further.
  if(mission>=0&&mission<32&&mission!=10&&
     (oreExpected[s]?mission+1==oreExpected[s]:(mission!=5&&mission!=12&&mission!=16)))oreState[s]|=2;
  else if(mission==10&&(oreState[s]&2)&&(queued==-1||queued==10)){oreState[s]=0;removePaused(s);}
  return (oreState[s]&1)!=0;
 }
 template<class Keep> void copyLive(const ManualVersions& source,Keep keep){
  clear();for(unsigned i=0;i<Size;++i)if(source.ids[i]&&keep(source.ids[i])){unsigned s=slot(source.ids[i]);if(s>=Size)break;
   ids[s]=source.ids[i];versions[s]=source.versions[i];oreState[s]=source.oreState[i];oreExpected[s]=source.oreExpected[i];orePointers[s]=source.orePointers[i];++used;addPaused(s);
  }
 }
 void clear(){used=pausedCount=0;for(unsigned i=0;i<Size;++i){ids[i]=versions[i]=orePointers[i]=0;oreState[i]=oreExpected[i]=0;pausedSlots[i]=pausedPosition[i]=0;}}
};
}
