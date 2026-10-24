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
template<class Read> bool identity(const Rule&r,const Record&v,Read read){
 uint32_t items=0,p=0,vt=0;int count=0;char id[24]{};
 return v.pointer>=0x10000&&read(r.array+4,&items,4)&&items==v.items&&read(r.array+16,&count,4)&&
  count>=0&&count<=4096&&v.index>=0&&v.index<count&&read(items+4*v.index,&p,4)&&p==v.pointer&&
  read(p,&vt,4)&&vt==r.vtable&&read(p+0x24,id,sizeof id)&&memchr(id,0,sizeof id)&&!strcmp(id,r.id);
}
inline bool eligible(const Rule&r,int level){return level==-1||(!strcmp(r.id,"LUNR")&&level>1&&level<=100);}
// A full 32-bit 1; never reproduce the capture's FF FF FF FF -> 01 FF FF FF.
template<class Read,class Write> bool apply(const Rule&r,Record&v,Read read,Write write){
 int level=0;if(!identity(r,v,read)||!read(v.pointer+LevelOffset,&level,4))return false;
 if(v.conflict)return false;
 if(v.owned){if(level==1)return true;v.conflict=true;return false;}
 if(!eligible(r,level))return true;
 v.original=level;const int one=1;
 if(!write(v.pointer+LevelOffset,&one,4))return false;v.owned=true;return true;
}
template<class Read,class Write> bool restore(const Rule&r,Record&v,Read read,Write write){
 if(!v.owned)return !v.conflict;
 // A retired object must not be written, even if its address is reused.
 if(!identity(r,v,read)){v.owned=false;return false;}
 int level=0;if(!read(v.pointer+LevelOffset,&level,4))return false;
 if(v.conflict||level!=1){v.conflict=true;return false;}
 if(!write(v.pointer+LevelOffset,&v.original,4))return false;v.owned=false;return true;
}
}
