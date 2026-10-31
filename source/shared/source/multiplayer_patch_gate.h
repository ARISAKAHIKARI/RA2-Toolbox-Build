#pragma once
#include <stdint.h>
#include <stddef.h>
// Local playable-state check only. Peer FrameInfo/CRC observation is NOT an
// installation prerequisite. Restore operations always bypass this check.
namespace multiplayerpatch {
template<class Read> bool mayInstall(Read read){
 int mode=-1;if(!read(0xA8B238,&mode,4))return false;
 if(mode!=0&&mode!=5&&mode!=3&&mode!=4)return false;
 uint32_t house=0;int frame=-1;uint8_t running=0,oos=0;
 return read(0xA83D4C,&house,4)&&house>=0x10000&&
  read(0xA8ED84,&frame,4)&&frame>=0&&frame<0x7fffffff&&
  read(0xA8E9A0,&running,1)&&running&&read(0xA8B8C2,&oos,1)&&!oos;
}
}
