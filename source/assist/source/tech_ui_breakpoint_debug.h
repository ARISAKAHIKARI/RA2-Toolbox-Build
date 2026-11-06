#pragma once
#include "tech_ui_breakpoint_hot.h"
#include "../../shared/source/debug_entry_policy.h"

namespace assist { namespace techbp {
// Cold admission keeps the observed image separate from the reference match.
// Even an explicit reference bypass needs a complete read; a hot callback
// receives a snapshot only after the entire logical entry was admitted.
template<size_t N> struct ObservedPattern {
 uint8_t bytes[N]{};bool readable=false,matched=false;
 template<class Read> bool load(uint32_t address,uint32_t expected,Read read){
  readable=read(address,bytes,N);matched=false;if(!readable)return false;
  uint32_t hash=2166136261u;for(size_t i=0;i<N;++i)hash=(hash^bytes[i])*16777619u;
  matched=hash==expected;return matched;
 }
 void publish(SignatureSnapshot<N>&snapshot,bool admitted)const{
  snapshot.valid=false;if(!admitted||!readable)return;
  for(size_t i=0;i<N;++i)snapshot.bytes[i]=bytes[i];snapshot.valid=true;
 }
};
template<size_t N> inline uint32_t patternDigest(const uint8_t(&bytes)[N]){
 uint32_t hash=2166136261u;for(uint8_t byte:bytes)hash=(hash^byte)*16777619u;return hash;
}
template<size_t N,class Read> bool debugSignatureSnapshot(SignatureSnapshot<N>&snapshot,
 uint32_t logicalAddress,uint32_t codeAddress,uint32_t expected,Read read,bool publish=true){
 ObservedPattern<N> observed;observed.load(codeAddress,expected,read);
 const bool admitted=debugentries::allow(debugentries::Kind::Breakpoint,logicalAddress,observed.readable,observed.matched);
 if(publish)observed.publish(snapshot,admitted);return admitted;
}
}}
