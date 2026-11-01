#pragma once
#include <windows.h>
#include "protocol.h"
namespace assist { namespace revealpermission {
// Only update this permission bit. Other feature flags belong to their owners.
inline void publish(volatile uint32_t&flags,bool enabled){
 static_assert(sizeof(LONG)==sizeof(uint32_t),"shared flag atomic width");
 auto*word=reinterpret_cast<volatile LONG*>(&flags);
 if(enabled)InterlockedOr(word,LONG(ExperimentalReveal));
 else InterlockedAnd(word,LONG(~uint32_t(ExperimentalReveal)));
}
template<class PublishRequest>inline void beforeRequest(volatile uint32_t&flags,bool enabled,PublishRequest publishRequest){
 publish(flags,enabled);MemoryBarrier();publishRequest();
}
}}
