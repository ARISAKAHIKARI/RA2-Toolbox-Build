#pragma once
#include <stdint.h>
namespace assist { namespace singlefps {
inline constexpr uint32_t Tag=32u,Entry=0x55D7BC,Resume=0x55D7C2,TimerDuration=0x887350;
inline bool valid(unsigned rate){return rate==0||(rate>=10&&rate<=240);}
inline bool session(int value){return value==0||value==5;}
// Integer QPC deadlines retain the division remainder. Late frames restart
// from the current time; a pause or slow scene never creates catch-up work.
struct Clock {
 uint64_t next=0,frequency=0;unsigned rate=0,remainder=0;
 void reset(){next=frequency=0;rate=remainder=0;}
 uint64_t deadline(uint64_t now,unsigned requested,uint64_t hz){
  if(!requested||!valid(requested)||hz<requested){reset();return now;}
  if(rate!=requested||frequency!=hz){reset();rate=requested;frequency=hz;next=now;}
  if(now>next+hz/requested) {next=now;remainder=0;}
  const uint64_t due=next<now?now:next;
  next+=hz/requested;remainder+=unsigned(hz%requested);
  if(remainder>=requested){++next;remainder-=requested;}
  return due;
 }
};
}}
