#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace assist { namespace techmode {
inline constexpr unsigned Data=3;
inline bool valid(unsigned mode){return mode<=Data;}
inline bool data(unsigned mode){return mode==Data;}
// A false active bit is not restoration evidence. Reject owned, conflicted,
// unreadable or changed sites before advertising a no-code-write mode.
template<class Site,class Registry,class Read> bool retired(const Site&s,const Registry&registry,Read read){
 uint8_t bytes[16]{};
 return s.size&&s.size<=sizeof bytes&&!registry.active(s.address)&&!registry.conflict(s.address)&&
  read(s.address,bytes,s.size)&&!memcmp(bytes,s.before,s.size);
}
}}
