#pragma once
#include <stdint.h>
namespace assist { namespace sidebarpolicy {
inline int canonicalKind(int k){return k==1?40:k==2?3:k==6?7:k==15?16:k;}
struct CameoSet {
 static constexpr unsigned Size=1024;uint32_t keys[Size]{};
 static uint32_t key(int kind,int index){kind=canonicalKind(kind);return kind>=0&&kind<64&&index>=0&&index<65535?(uint32_t(kind)<<16)|uint32_t(index+1):0;}
 void clear(){for(auto&k:keys)k=0;}
 bool add(int kind,int index){uint32_t k=key(kind,index);if(!k)return false;unsigned s=(k*2654435761u)&(Size-1);
  for(unsigned n=0;n<Size;++n,s=(s+1)&(Size-1))if(!keys[s]||keys[s]==k){keys[s]=k;return true;}return false;
 }
 bool contains(int kind,int index)const{uint32_t k=key(kind,index);if(!k)return false;unsigned s=(k*2654435761u)&(Size-1);
  for(unsigned n=0;n<Size;++n,s=(s+1)&(Size-1)){if(!keys[s])return false;if(keys[s]==k)return true;}return false;
 }
};
}}
