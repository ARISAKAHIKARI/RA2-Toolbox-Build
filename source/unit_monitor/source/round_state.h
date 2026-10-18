#pragma once
#include <cstdint>
#include <cstring>
#include "../../external_reader/source/layout_probe.h"

namespace um { namespace roundstate {
// Use uncached reads for both bookends. A per-sample page cache must not hide
// a match transition while the (larger) counters/cameos are being collected.
template<class R> auto fresh(R& r,uint32_t p,void* out,size_t n,int)
 -> decltype(r.fresh(p,out,n)){return r.fresh(p,out,n);}
template<class R> bool fresh(R& r,uint32_t p,void* out,size_t n,long){return r(p,out,n);}
inline bool pointer(uint32_t p){return p>=0x10000&&p<0x80000000u;}
struct House {
 uint32_t ptr=0,id=0,type=0;int index=-1;uint16_t name[20]{};
 bool same(const House& b)const{return ptr==b.ptr&&id==b.id&&type==b.type&&index==b.index&&!memcmp(name,b.name,sizeof name);}
};
struct Stamp {
 uint32_t local=0,items=0;int frame=-1,count=0;bool localEnded=false;House houses[32]{};
 bool sameRoster(const Stamp& b)const{
  if(local!=b.local||items!=b.items||count!=b.count)return false;
  for(int i=0;i<count;++i)if(!houses[i].same(b.houses[i]))return false;
  return true;
 }
 bool follows(const Stamp& before)const{
  return sameRoster(before)&&frame>=before.frame&&!(before.localEnded&&!localEnded);
 }
};
template<class R> bool capture(R& r,Stamp& s,external::layout::Result& error){
 using external::layout::Stage;
 auto read=[&](uint32_t p,void*out,size_t n){return fresh(r,p,out,n,0);};
 error=external::layout::inspect(read);if(error.stage!=Stage::Ready)return false;
 if(!read(0xA83D4C,&s.local,4)||!pointer(s.local)){
  error={Stage::InvalidLocalHouse,0xA83D4C,s.local};return false;
 }
 if(!read(0xA8ED84,&s.frame,4)||s.frame<0){error={Stage::UnreadableFrame,0xA8ED84,0};return false;}
 uint32_t vector[5]{},ptrs[32]{};
 if(!read(0xA80228,vector,sizeof vector)||vector[4]==0||vector[4]>32||vector[2]<vector[4]||vector[2]>65536||!pointer(vector[1])||
    !read(vector[1],ptrs,vector[4]*4)){
  error={Stage::UnreadableHouseList,0xA80228,vector[4]};return false;
 }
 s.items=vector[1];s.count=int(vector[4]);bool found=false;
 for(int i=0;i<s.count;++i){
  House& h=s.houses[i];h.ptr=ptrs[i];uint32_t identity[10]{};
  // AbstractClass::UniqueID +0x10; HouseClass::ArrayIndex +0x30,
  // Type +0x34; UIName +90154. IDs alone can repeat after Scenario reset.
  if(!pointer(h.ptr)||!read(h.ptr+0x10,identity,sizeof identity)||!read(h.ptr+90154,h.name,sizeof h.name)){
   error={Stage::UnreadableHouseList,h.ptr,0};return false;
  }
  h.id=identity[0];h.index=int(identity[8]);h.type=identity[9];
  if(!pointer(h.type)){error={Stage::UnreadableHouseList,h.ptr+0x34,h.type};return false;}
  if(h.ptr==s.local){
   uint8_t flags[4]{};if(!read(h.ptr+501,flags,sizeof flags)){
    error={Stage::UnreadableHouseList,h.ptr+501,0};return false;
   }
   s.localEnded=flags[0]||flags[1]||flags[3];found=true;
  }
 }
 if(!found){error={Stage::MissingLocalHouseInList,0xA80228,s.local};return false;}
 return true;
}
} }
