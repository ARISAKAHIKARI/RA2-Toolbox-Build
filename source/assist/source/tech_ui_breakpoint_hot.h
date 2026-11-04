#pragma once
#include <stddef.h>
#include <stdint.h>

namespace assist { namespace techbp {
// One owning game thread writes these diagnostic counters. Other threads only
// sample them; they do not publish permissions or synchronize a transaction.
// Use unsigned arithmetic so a long session wraps without signed overflow.
template<class Counter> inline void countOwner(volatile Counter&counter){static_assert(sizeof(Counter)==sizeof(uint32_t),"owner counters require 32-bit storage");counter=Counter(uint32_t(counter)+1u);}

// Admission still uses the bounded safe reader and the established digest.
// The guarded callback compares EVERY current byte with that verified image:
// no process-memory syscall or repeated digest calculation on each hit.
template<size_t N> struct SignatureSnapshot {
 uint8_t bytes[N]{};bool valid=false;
 template<class Read> bool refresh(uint32_t address,uint32_t expected,Read read){
  valid=false;uint8_t next[N]{};if(!read(address,next,N))return false;
  uint32_t hash=2166136261u;for(size_t i=0;i<N;++i)hash=(hash^next[i])*16777619u;
  if(hash!=expected)return false;
  for(size_t i=0;i<N;++i)bytes[i]=next[i];valid=true;return true;
 }
 // Call only inside the callback's AV/in-page SEH boundary.
 bool matches(const volatile uint8_t*code)const{
  if(!valid)return false;for(size_t i=0;i<N;++i)if(code[i]!=bytes[i])return false;return true;
 }
};

struct Hit {uint32_t slots=0,tag=0;};
// Classify before modifying CONTEXT or statistics. A mixed cause must reach
// the foreign handler with its complete original register/flag state intact.
template<class Lease,class Registers> inline Hit ownedHit(const Lease&lease,const Registers&r,uint32_t eip,unsigned siteCount){
 const uint32_t pending=r.status&15u;
 if(!pending||(r.status&0xE000u)||(pending&~lease.mask))return {};
 uint32_t candidates=pending,tag=0;
 while(candidates){const unsigned n=unsigned(__builtin_ctz(candidates));candidates&=candidates-1;
  const unsigned actual=lease.actualTag(r,n);
  if(actual>=siteCount||eip!=lease.sites[actual])return {};tag=actual;
 }
 return {pending,tag};
}
}}
