#pragma once
#include <cmath>
#include <cstdint>
namespace external { namespace radar {
inline int rankFrame(float value){return !std::isfinite(value)?-1:value<0?19:value>=2?15:value>=1?14:-1;}
template<class Read>int captureRank(Read read,uint32_t object,uint32_t owner,uint32_t house){
 uint32_t disguise=1;float value=0;
 if(owner!=house&&(!read(object+0x518,&disguise,4)||disguise))return -1;
 return read(object+0x150,&value,4)?rankFrame(value):-1;
}
}}
