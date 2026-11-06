#pragma once
#include <stdint.h>
#include <stddef.h>

// An ordinary call follows the game's existing entry and platform dispatch.
// This is not permission to patch that entry, skip a handler or place a
// breakpoint inside a replacement function. Those consumers keep their own
// exact instruction/layout checks.
namespace nativeentry {
constexpr uint32_t RecheckCameos=0x6AA600,FindFactory=0x5F7900,CanBuild=0x4F7870;
constexpr uint32_t CacheMs=250;
constexpr int slot(uint32_t entry){return entry==RecheckCameos?0:entry==FindFactory?1:entry==CanBuild?2:-1;}
constexpr bool known(uint32_t entry){return slot(entry)>=0;}
inline bool range(uint32_t address,size_t bytes){return bytes&&address>=0x10000&&uint64_t(address)+bytes<0x80000000ull;}
inline uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
struct TimedResult {
 uint32_t at=0;bool seen=false,value=false;
 bool fresh(uint32_t now)const{return seen&&uint32_t(now-at)<CacheMs;}
 bool set(uint32_t now,bool next){at=now;seen=true;value=next;return next;}
};

// Ares/Syringe owns E9 entries at these three native APIs. Their private
// executable stubs are valid call destinations, not stock instruction sites.
// Keep the original caller address; inspect only bounded dispatch edges here.
// A normal stub body is left to Ares, including its handler and continuation.
template<class Read,class Executable>bool platformCallable(uint32_t entry,bool ares,Read read,Executable executable){
 if(!known(entry)||!ares)return false;
 uint32_t visited[8]{},address=entry;
 for(unsigned hop=0;hop<8;++hop){
  for(unsigned previous=0;previous<hop;++previous)if(visited[previous]==address)return false;
  visited[hop]=address;
  uint8_t bytes[6]{};
  if(!range(address,sizeof bytes)||!executable(address,sizeof bytes)||!read(address,bytes,sizeof bytes))return false;
  if(!hop&&bytes[0]!=0xE9)return false;
  int64_t target=0;
  if(bytes[0]==0xE9)target=int64_t(address)+5+int32_t(word(bytes+1));
  else if(bytes[0]==0xEB)target=int64_t(address)+2+int8_t(bytes[1]);
  else if(bytes[0]==0xFF&&bytes[1]==0x25){
   const uint32_t pointer=word(bytes+2);uint8_t value[4]{};
   if(!range(pointer,sizeof value)||!read(pointer,value,sizeof value))return false;
   target=word(value);
  }else return hop&&bytes[0]!=0&&bytes[0]!=0xCC&&bytes[0]!=0xF4;
  if(target<0x10000||target>=0x80000000ll)return false;
  address=uint32_t(target);
 }
 return false;
}
}
