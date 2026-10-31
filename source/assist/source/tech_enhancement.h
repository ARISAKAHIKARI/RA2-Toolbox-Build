#pragma once
#include <stdint.h>
#include <string.h>
namespace techenhancement {
struct Rule {uint32_t array,vtable;const char*id;};
inline constexpr Rule Rules[]={
 {0xA8E348,0x7EB610,"WEEDGUY"},{0xA8E348,0x7EB610,"COW"},{0xA8E348,0x7EB610,"ALL"},{0xA8E348,0x7EB610,"PRES"},
 {0xA8E348,0x7EB610,"JOSH"},{0xA8E348,0x7EB610,"POLARB"},{0xA8E348,0x7EB610,"CLNT"},{0xA8E348,0x7EB610,"ARND"},
 {0xA8E348,0x7EB610,"STLN"},{0xA8E348,0x7EB610,"LUNR"},
 {0xA83CE0,0x7F6218,"BUS"},{0xA83CE0,0x7F6218,"HOWI"},{0xA83CE0,0x7F6218,"CMON"},{0xA83CE0,0x7F6218,"HIND"},
 {0xA83CE0,0x7F6218,"XCOMET"},{0xA83CE0,0x7F6218,"VLAD"},{0xA83CE0,0x7F6218,"CDEST"},{0xA83CE0,0x7F6218,"SCHD"},
 {0xA8B218,0x7E2868,"V3ROCKET"},{0xA8B218,0x7E2868,"CMISL"},{0xA8B218,0x7E2868,"DMISL"}
};
constexpr unsigned Count=sizeof Rules/sizeof Rules[0];
constexpr uint32_t LevelOffset=0x634;
struct Record {uint32_t pointer=0,items=0;int index=0,original=0;bool owned=false,conflict=false;};
enum class IdentityState { Current, Retired, Unavailable };
enum Status { Off=0, Applied=1, SingleplayerOnly=2, Incompatible=-2 };
constexpr bool mayApply(int mode){return mode==0||mode==5;}
constexpr bool sameID(const char*a,const char*b){
 for(unsigned i=0;i<24;++i){if(a[i]!=b[i])return false;if(!a[i])return true;}return false;
}
template<class Read> constexpr IdentityState inspect(const Rule&r,const Record&v,Read read){
 uint32_t items=0,p=0,vt=0;int count=0;char id[24]{};
 if(v.pointer<0x10000||v.items<0x10000||v.index<0)return IdentityState::Retired;
 if(!read(r.array+4,&items,4)||!read(r.array+16,&count,4))return IdentityState::Unavailable;
 if(items<0x10000||count<0||count>4096)return IdentityState::Unavailable;
 if(items!=v.items||v.index>=count)return IdentityState::Retired;
 if(!read(items+4*v.index,&p,4))return IdentityState::Unavailable;
 if(p!=v.pointer)return IdentityState::Retired;
 if(!read(p,&vt,4)||!read(p+0x24,id,sizeof id))return IdentityState::Unavailable;
 return vt==r.vtable&&sameID(id,r.id)?IdentityState::Current:IdentityState::Retired;
}
template<class Read> constexpr bool identity(const Rule&r,const Record&v,Read read){return inspect(r,v,read)==IdentityState::Current;}
constexpr bool eligible(const Rule&r,int level){return level==-1||(sameID(r.id,"LUNR")&&level>1&&level<=100);}
// A full 32-bit 1; never reproduce the capture's FF FF FF FF -> 01 FF FF FF.
template<class Read,class Write> constexpr bool apply(const Rule&r,Record&v,Read read,Write write){
 int level=0;if(!identity(r,v,read)||!read(v.pointer+LevelOffset,&level,4))return false;
 if(v.conflict)return false;
 if(v.owned){if(level==1)return true;v.conflict=true;return false;}
 if(!eligible(r,level))return true;
 v.original=level;const int one=1;
 // A writer can modify memory and then fail its read-back verification.
 // Own the attempted write before crossing that boundary, including failure.
 v.owned=true;return write(v.pointer+LevelOffset,&one,4);
}
template<class Read,class Write> constexpr bool restore(const Rule&r,Record&v,Read read,Write write){
 if(!v.owned)return !v.conflict;
 // A retired object must not be written, even if its address is reused.
 const auto state=inspect(r,v,read);
 if(state==IdentityState::Unavailable)return false;
 if(state==IdentityState::Retired){v={};return true;}
 int level=0;if(!read(v.pointer+LevelOffset,&level,4))return false;
 // Idempotent after a writer restored the field but could not verify it.
 if(level==v.original){v.owned=false;v.conflict=false;return true;}
 if(v.conflict||level!=1){v.conflict=true;return false;}
 if(!write(v.pointer+LevelOffset,&v.original,4))return false;v.owned=false;return true;
}
struct Restoration {bool complete=true,ok=true,changed=false;unsigned retained=0;};
template<class Read,class Write> constexpr Restoration restoreRecords(Record(&records)[Count],IdentityState world,Read read,Write write){
 Restoration result{};
 for(unsigned i=0;i<Count;++i){auto&v=records[i];
  if(world==IdentityState::Retired){v={};continue;}
  if(v.owned){
   if(world==IdentityState::Unavailable)result.ok=false;
   else {result.changed=true;if(!restore(Rules[i],v,read,write))result.ok=false;}
   if(v.owned){result.complete=false;++result.retained;continue;}
  }
  v={};
 }
 return result;
}
}
