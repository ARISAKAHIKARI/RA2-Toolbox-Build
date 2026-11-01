#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace assist { namespace techdata {
struct Field {uint32_t offset,value;uint8_t size;};
inline constexpr Field Fields[]={
 {0x634,0,4},{0x648,0,4},{0x6CC,UINT32_MAX,4},{0xDA0,UINT32_MAX,4},{0xDA4,0,4},
 {0xC98,0,1},{0xD9B,0,1},{0xD9C,0,1},{0xD9D,0,1}
};
constexpr unsigned FieldCount=sizeof Fields/sizeof Fields[0];
struct Identity {uint32_t pointer=0,array=0,vtable=0,uid=0;int index=-1;char id[25]{};};
struct Record {Identity identity;uint32_t original[FieldCount]{};uint16_t owned=0,conflict=0;bool captured=false;};
enum class State {Current,Retired,Unavailable};
inline uint32_t array(int kind){return kind==3?0xA8B218:kind==16?0xA8E348:kind==40?0xA83CE0:kind==7?0xA83C68:0;}
inline uint32_t vtable(int kind){return kind==3?0x7E2868:kind==16?0x7EB610:kind==40?0x7F6218:kind==7?0x7E4570:0;}
template<class Read> State inspect(const Identity&i,Read read){
 uint32_t items=0,p=0,vt=0,uid=0;int count=0;char id[25]{};
 if(!i.array||i.pointer<0x10000||i.index<0)return State::Retired;
 if(!read(i.array+4,&items,4)||!read(i.array+16,&count,4))return State::Unavailable;
 if(items<0x10000||count<0||count>4096||uint64_t(items)+uint64_t(count)*4>UINT32_MAX)return State::Unavailable;
 if(i.index>=count)return State::Retired;
 if(!read(items+uint32_t(i.index)*4,&p,4))return State::Unavailable;
 if(p!=i.pointer)return State::Retired;
 if(!read(p,&vt,4)||!read(p+0x10,&uid,4)||!read(p+0x24,id,sizeof id))return State::Unavailable;
 return vt==i.vtable&&uid==i.uid&&!memcmp(id,i.id,sizeof id)?State::Current:State::Retired;
}
template<class Read> bool capture(Record&r,uint32_t pointer,int kind,int index,Read read){
 if(r.captured)return false;
 Record candidate{};auto&i=candidate.identity;i.pointer=pointer;i.array=array(kind);i.vtable=vtable(kind);i.index=index;
 if(!i.array||pointer<0x10000||!read(pointer+0x10,&i.uid,4)||!read(pointer+0x24,i.id,sizeof i.id)||
    !memchr(i.id,0,sizeof i.id)||inspect(i,read)!=State::Current)return false;
 // The count is changed without touching vector storage or capacity. Validate
 // the original vector so a corrupt length is never normalized into validity.
 uint32_t items=0;int count=0,capacity=0;
 if(!read(pointer+0x63C,&items,4)||!read(pointer+0x640,&capacity,4)||!read(pointer+0x648,&count,4)||
    count<0||count>4096||capacity<count||capacity>65536||(count&&(items<0x10000||uint64_t(items)+uint64_t(count)*4>UINT32_MAX)))return false;
 for(unsigned n=0;n<FieldCount;++n)if(!read(pointer+Fields[n].offset,&candidate.original[n],Fields[n].size))return false;
 if(inspect(i,read)!=State::Current)return false;
 candidate.captured=true;r=candidate;return true;
}
template<class Read,class Write> bool apply(Record&r,Read read,Write write){
 if(!r.captured||r.conflict||inspect(r.identity,read)!=State::Current)return false;
 // Preflight all fields before the first write. Each field subsequently owns
 // its attempted write before crossing a possibly failing post-write check.
 for(unsigned n=0;n<FieldCount;++n){uint32_t current=0;const auto&f=Fields[n];
  if(!read(r.identity.pointer+f.offset,&current,f.size))return false;
  if(current!=((r.owned&(1u<<n))?f.value:r.original[n])){r.conflict|=uint16_t(1u<<n);return false;}
 }
 for(unsigned n=0;n<FieldCount;++n){const auto&f=Fields[n];if(r.original[n]==f.value)continue;
  if(inspect(r.identity,read)!=State::Current)return false;
  uint32_t current=0;if(!read(r.identity.pointer+f.offset,&current,f.size))return false;
  if(r.owned&(1u<<n)){if(current!=f.value){r.conflict|=uint16_t(1u<<n);return false;}continue;}
  if(current!=r.original[n]){r.conflict|=uint16_t(1u<<n);return false;}
  r.owned|=uint16_t(1u<<n);
  if(!write(r.identity.pointer+f.offset,&f.value,f.size))return false;
  current=0;if(!read(r.identity.pointer+f.offset,&current,f.size)||current!=f.value)return false;
 }
 return inspect(r.identity,read)==State::Current;
}
template<class Read,class Write> bool restore(Record&r,State world,Read read,Write write){
 if(!r.captured)return true;
 if(!r.owned){r={};return true;}
 if(world==State::Retired){r={};return true;}if(world==State::Unavailable)return !r.owned;
 const auto state=inspect(r.identity,read);if(state==State::Unavailable)return !r.owned;
 if(state==State::Retired){r={};return true;}
 bool ok=true;
 for(unsigned n=FieldCount;n--;)if(r.owned&(1u<<n)){const auto&f=Fields[n];uint32_t current=0;
  if(!read(r.identity.pointer+f.offset,&current,f.size)){ok=false;continue;}
  if(current==r.original[n]){r.owned&=uint16_t(~(1u<<n));r.conflict&=uint16_t(~(1u<<n));continue;}
  if((r.conflict&(1u<<n))||current!=f.value){r.conflict|=uint16_t(1u<<n);ok=false;continue;}
  if(inspect(r.identity,read)!=State::Current||!write(r.identity.pointer+f.offset,&r.original[n],f.size)){ok=false;continue;}
  current=0;if(!read(r.identity.pointer+f.offset,&current,f.size)||current!=r.original[n]){ok=false;continue;}
  r.owned&=uint16_t(~(1u<<n));r.conflict&=uint16_t(~(1u<<n));
 }
 if(!r.owned)r={};return ok&&!r.owned;
}
}}
