#pragma once
#include <stdint.h>
#include "protocol.h"
namespace assist { namespace delivery {
// UI-side, bounded request staging; never injects or rewrites game state.
// UnknownMatch is used only if a user clicked while a new round was loading.
constexpr uint32_t UnknownMatch=0;
constexpr uint32_t MaxAgeMs=12000;
constexpr uint32_t SelectAgeMs=2500; // A selection is transient; never select units seconds later.
constexpr uint32_t RetryAgeMs=5000; // In-flight commands have a separate clock from queued intents.
constexpr int Capacity=512; // queued requests, independent of native per-frame delivery
struct Item {int command=0,arg=0,arg2=0;uint32_t pid=0,match=0,created=0;int retries=0;uint32_t notBefore=0,token=0;uint64_t typeHash=0;};
struct Queue {
 Item slots[Capacity]{};int size=0;
 void clear(){size=0;}
 bool add(Item it){
  if(!it.pid||(it.match==UnknownMatch&&it.command!=RevealMap))return false;
  if(it.command==SelectType){
   // One latest selection intent per command queue. On a double tap, replace
   // any queued screen-only selection with a global one, in-place and with
   // a new timestamp. This also prevents rapid A -> B presses selecting A late.
   for(int i=0;i<size;)if(slots[i].command==SelectType)discard(i);else ++i;
   if(size>=Capacity)return false;
   for(int j=size;j>0;--j)slots[j]=slots[j-1];
   slots[0]=it;++size;return true;
  }
  for(int i=0;it.command!=Produce&&i<size;++i)
   if(slots[i].command==it.command&&slots[i].arg==it.arg&&slots[i].arg2==it.arg2&&slots[i].pid==it.pid&&slots[i].match==it.match&&slots[i].token==it.token&&slots[i].typeHash==it.typeHash){
    // Keep original timestamp, to avoid repeated clicks extending stale commands forever.
    return true;
   }
  if(size>=Capacity)return false;
  slots[size++]=it;return true;
 }
 bool has(int command)const{for(int i=0;i<size;++i)if(slots[i].command==command)return true;return false;}
 void discard(int i){for(int j=i+1;j<size;++j)slots[j-1]=slots[j];--size;}
 // Drop old-session commands instead of leaking them into another match.
 int expire(uint32_t now,uint32_t pid,uint32_t activeMatch,bool active){
  int dropped=0;
  for(int i=0;i<size;){auto const& it=slots[i];
   bool bad=it.pid!=pid||uint32_t(now-it.created)>(it.command==SelectType?SelectAgeMs:MaxAgeMs)||
    (active&&it.match!=UnknownMatch&&it.match!=activeMatch);
   if(bad){discard(i);++dropped;}else ++i;
  }return dropped;
 }
 bool peek(uint32_t now,uint32_t pid,uint32_t activeMatch,bool active,Item&out){
  expire(now,pid,activeMatch,active);if(!active||!size)return false;
  if(slots[0].notBefore&&int32_t(now-slots[0].notBefore)<0)return false;
  out=slots[0];if(out.match==UnknownMatch)out.match=activeMatch;return true;
 }
 void consume(){if(size)discard(0);}
};
inline bool retrySelection(int result,int retries,uint32_t now,uint32_t sentAt,uint32_t sentMatch,uint32_t currentMatch,bool gameFocused,bool chat){
 return result==-7&&retries<1&&uint32_t(now-sentAt)<RetryAgeMs&&
        sentMatch==currentMatch&&gameFocused&&!chat;
}
inline bool focusIsRelevant(bool gameFocused,bool toolFocused){return gameFocused||toolFocused;}
inline bool bridgeReady(int status,bool fresh,bool chat,bool acked){return status==1&&fresh&&!chat&&acked;}
} }
