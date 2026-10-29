#pragma once
#include <stdint.h>
#include "../../shared/source/key_gesture.h"
namespace typekeys {
inline uint16_t canonical(uint16_t key){return keygesture::normalize(key);}
template<class Bindings> bool firstForKey(const Bindings& bindings,unsigned at){
 const auto key=canonical(bindings[at].key);if(!(key&255))return false;
 for(unsigned i=0;i<at;++i)if(canonical(bindings[i].key)==key)return false;
 return true;
}
template<class Bindings> int groupSize(const Bindings& bindings,uint16_t key){
 int n=0;key=canonical(key);if(!(key&255))return 0;
 for(const auto&b:bindings)if(canonical(b.key)==key)++n;return n;
}
// Legacy unit shortcuts retain screen/global double-tap selection. An explicit
// gesture anywhere in the same merged group selects explicit semantics for
// that whole group, independent of which catalogue row registered it first.
template<class Bindings> bool implicitDouble(const Bindings&bindings,uint16_t key){
 key=canonical(key);bool found=false;
 for(const auto&binding:bindings)if(canonical(binding.key)==key){found=true;if(keygesture::mode(binding.key)!=keygesture::Mode::Legacy)return false;}
 return found;
}
template<class Bindings,class Types> int groupMask(const Bindings& bindings,uint16_t key,const Types& types,int count,uint8_t* mask,int limit){
 for(int i=0;i<limit;++i)mask[i]=0;key=canonical(key);if(!(key&255))return 0;
 int n=0;for(int i=0;i<count&&i<limit;++i){if(types[i].kind==7)continue;
  for(const auto&b:bindings)if(canonical(b.key)==key&&b.id==types[i].id){mask[i]=1;++n;break;}
 }return n;
}
struct TypeSet {
 uint32_t slots[1024]{};
 void clear(){for(auto&v:slots)v=0;}
 void add(uint32_t value){if(!value)return;unsigned i=((value>>4)*2654435761u)&1023;for(unsigned n=0;n<1024;++n,i=(i+1)&1023)if(!slots[i]||slots[i]==value){slots[i]=value;return;}}
 bool contains(uint32_t value)const{if(!value)return false;unsigned i=((value>>4)*2654435761u)&1023;for(unsigned n=0;n<1024;++n,i=(i+1)&1023){if(!slots[i])return false;if(slots[i]==value)return true;}return false;}
};
struct Tap {
 int slot=-1;uint32_t tick=0,match=0;
 void clear(){slot=-1;}
 bool press(int next,uint32_t now,uint32_t scene,uint32_t interval){
  bool twice=next==slot&&scene==match&&uint32_t(now-tick)<=interval;
  slot=twice?-1:next;tick=now;match=scene;return twice;
 }
};
inline bool inView(int x,int y,int w,int h){return w>0&&h>0&&x>=0&&y>=0&&x<w&&y<h;}
inline bool csMission(int mission,int kind,bool active,bool selected,bool armed,bool allowed){
 return mission==2&&(kind==1||kind==2)&&active&&selected&&armed&&allowed;
}
}
