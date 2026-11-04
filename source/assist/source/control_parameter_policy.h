#pragma once
#include <stdint.h>
#include "../../shared/source/master_control_policy.h"

namespace assist { namespace controlparams {
// One execution breakpoint handles both the common permission predicate and
// the two strict owner-index callers. The original CALLs and native leaves run.
inline constexpr uint32_t Entry=0x50B6F0, Allowed=0x50B710, Strict=0x50B716;
inline constexpr uint32_t OwnerReturn=0x6339BF, PlanningReturn=0x637E45;
// Only call this reader inside GuardedTechUiParameter's AV/in-page SEH scope.
// Avoid a ReadProcessMemory syscall for every field on this hot predicate.
inline bool guardedRead(uint32_t address,void*out,size_t size){
 if(address<0x10000||uint64_t(address)+size>=0x80000000ull)return false;
 if(size==4){*static_cast<uint32_t*>(out)=*reinterpret_cast<const volatile uint32_t*>(uintptr_t(address));return true;}
 if(size==1){*static_cast<uint8_t*>(out)=*reinterpret_cast<const volatile uint8_t*>(uintptr_t(address));return true;}
 return false;
}
template<class Read> bool world(uint32_t house,uint32_t id,uint32_t scenario,int session,int frame,Read read){
 uint32_t player=0,uid=0,currentScenario=0,observer=0;int currentSession=-1,currentFrame=-1;uint8_t running=0,dead=0,defeated=0,observerHouse=0;
 return house>=0x10000&&id&&scenario&&read(0xA83D4C,&player,4)&&player==house&&read(house+0x10,&uid,4)&&uid==id&&
  read(0xA8B230,&currentScenario,4)&&currentScenario==scenario&&read(0xA8B238,&currentSession,4)&&currentSession==session&&
  read(0xA8ED84,&currentFrame,4)&&currentFrame>=frame&&read(0xA8E9A0,&running,1)&&running&&read(0xAC1198,&observer,4)&&observer!=house&&
  read(house+0x1F5,&dead,1)&&!dead&&read(house+0x1F6,&defeated,1)&&!defeated&&read(house+0x1F8,&observerHouse,1)&&!observerHouse;
}
inline uint32_t destination(uint32_t caller,uint32_t allowed=Allowed,uint32_t strict=Strict,
                            uint32_t ownerReturn=OwnerReturn,uint32_t planningReturn=PlanningReturn){
 return caller==ownerReturn||caller==planningReturn?strict:allowed;
}
template<class Read> bool compatible(Read read){
 // In this mode even an identical foreign Master patch is not an original.
 for(const auto&s:mastercontrol::Sites){uint8_t bytes[5]{};
  if(!read(s.address,bytes,5)||!mastercontrol::original(s.address,bytes,5))return false;
 }
 return mastercontrol::compatible(read);
}
template<class Read> bool house(uint32_t pointer,Read read){
 if(pointer<0x10000)return false;
 uint32_t items=0,value=0,id=0,table=0;int count=0,index=-1;
 return read(0xA8022C,&items,4)&&items>=0x10000&&read(0xA80238,&count,4)&&count>0&&count<=32&&
  read(pointer+0x30,&index,4)&&index>=0&&index<count&&read(items+uint32_t(index)*4,&value,4)&&value==pointer&&
  read(pointer,&table,4)&&table>=0x10000&&read(pointer+0x10,&id,4)&&id;
}
}}
