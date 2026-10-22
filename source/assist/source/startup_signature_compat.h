#pragma once
#include <stdint.h>
#include <stddef.h>
namespace startupcompat {
// One observed platform replacement, not a general E9 whitelist. The complete
// function fingerprint includes branch/call offsets and the ret 4 ABI; only
// its five module-absolute operands are rebased before comparison.
// Evidence: research/2026-10-22-platform5-135.md (capture SHA recorded there).
constexpr uint32_t BeaconEntry=0x431410,ModuleBytes=0xBBD000,BeaconRva=0x12FB60;
constexpr size_t BeaconBytes=0x127;
inline uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline bool beaconTarget(const uint8_t*entry,size_t n,uint32_t module,uint32_t&target){
 if(n!=8||entry[0]!=0xE9||entry[5]!=0x60||entry[6]!=0x85||entry[7]!=0xC0||
    module<0x10000||uint64_t(module)+ModuleBytes>=0x80000000ull)return false;
 target=BeaconEntry+5+word(entry+1);
 return target==module+BeaconRva;
}
inline bool beaconBody(const uint8_t*body,size_t n,uint32_t module){
 if(n!=BeaconBytes||module<0x10000||uint64_t(module)+ModuleBytes>=0x80000000ull)return false;
 constexpr size_t relocations[]={4,63,99,105,279};
 uint64_t hash=0xcbf29ce484222325ull;size_t relocation=0;
 for(size_t i=0;i<n;){
  if(relocation<sizeof relocations/sizeof relocations[0]&&i==relocations[relocation]){
   uint32_t value=word(body+i);if(value<module||value-module>=ModuleBytes)return false;
   value-=module;for(unsigned j=0;j<4;++j){hash=(hash^uint8_t(value>>(j*8)))*0x100000001b3ull;}i+=4;++relocation;
  }else hash=(hash^body[i++])*0x100000001b3ull;
 }
 return hash==0x78fb89765759f745ull;
}
}
