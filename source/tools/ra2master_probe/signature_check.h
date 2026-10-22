#pragma once
#include "diff_core.h"
namespace probe {
enum class SignatureState { Match, Changed, Unreadable };
inline SignatureState checkSignature(const Snapshot&s,uintptr_t base,uintptr_t at,const uint8_t*expected,size_t n){
 if(at<base||at-base>s.bytes.size()||n>s.bytes.size()-(at-base))return SignatureState::Unreadable;
 size_t off=at-base;
 if(off>s.valid.size()||n>s.valid.size()-off)return SignatureState::Unreadable;
 bool same=true;for(size_t i=0;i<n;++i){if(!s.valid[off+i])return SignatureState::Unreadable;same=same&&s.bytes[off+i]==expected[i];}
 return same?SignatureState::Match:SignatureState::Changed;
}
inline bool relativeJump(uint32_t at,const uint8_t*p,size_t n,uint32_t&target){
 if(n<5||p[0]!=0xE9)return false;
 uint32_t displacement=uint32_t(p[1])|uint32_t(p[2])<<8|uint32_t(p[3])<<16|uint32_t(p[4])<<24;
 target=at+5+displacement;return true; // x86 rel32 arithmetic wraps at 32 bits.
}
}
