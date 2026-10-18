#pragma once
#include <stdint.h>
#include <string.h>
// Only the game-judgment sites authorized by the 1.0.4 opt-ins.
// Retired frame/input/CS/sidebar detours remain forbidden.
namespace experimentalpatch {
constexpr uint32_t Placement=0x4A8EB0,AirType=0x7E2908,AirArrival=0x416AB6;
inline unsigned size(uint32_t at){return at==Placement?5:at==AirType?4:at==AirArrival?10:0;}
inline uint32_t destination(uint32_t at,const uint8_t* bytes,unsigned n){
 if(!bytes||n!=size(at)||!n)return 0;
 uint32_t value=0;
 if(at==AirType){memcpy(&value,bytes,4);return value;}
 if(bytes[0]!=0xE9)return 0;
 for(unsigned i=5;i<n;++i)if(bytes[i]!=0x90)return 0;
 memcpy(&value,bytes+1,4);return at+5+value;
}
}
