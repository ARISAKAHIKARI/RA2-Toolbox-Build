#pragma once
#include <stdint.h>
#include <stddef.h>
// Shared by the Assist dispatcher, sole code writer and optional frame notifier.
// All access is under the existing game-thread entry lease. Restore operations
// deliberately bypass this gate so a lost/changed world can still detach.
namespace multiplayerpatch {
struct Approval {uint32_t scenario=0,items=0,house=0,id=0;int mode=-1,frame=-1;bool valid=false;};
inline Approval approval{};
inline void clear(){approval={};}
inline void approve(uint32_t scenario,uint32_t items,uint32_t house,uint32_t id,int mode,int frame){approval={scenario,items,house,id,mode,frame,true};}
template<class Read> bool mayInstall(Read read){
 int mode=-1;if(!read(0xA8B238,&mode,4))return false;
 if(mode==0||mode==5)return true;
 if((mode!=3&&mode!=4)||!approval.valid||mode!=approval.mode)return false;
 uint32_t scenario=0,items=0,house=0,id=0;int frame=-1;uint8_t running=0,oos=0;
 return read(0xA8B230,&scenario,4)&&scenario==approval.scenario&&
  read(0xA8022C,&items,4)&&items==approval.items&&read(0xA83D4C,&house,4)&&house==approval.house&&
  house>=0x10000&&read(house+0x10,&id,4)&&id==approval.id&&read(0xA8ED84,&frame,4)&&frame>=approval.frame&&
  read(0xA8E9A0,&running,1)&&running&&read(0xA8B8C2,&oos,1)&&!oos;
}
}
