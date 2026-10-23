#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include "blowfish_pi.h"

// Read-only Westwood MIX directory decoding. These public format keys protect
// the directory, not the resource payload. No game calls or external DLLs.
namespace external { namespace assets { namespace mixcrypto {
using Number=std::array<uint32_t,11>; // one extra limb for modular addition
static constexpr Number Modulus={{0xaa437f15,0xd1fb4f3d,0xf8b0c1e6,0xabdd0e6a,0x6682b04a,0xe8aa54fa,0x51713fa2,0x565160d6,0x6d39fce4,0x51bcda08,0}};
inline bool atLeast(const Number&a,const Number&b){for(int i=10;i>=0;--i)if(a[i]!=b[i])return a[i]>b[i];return true;}
inline void subtract(Number&a,const Number&b){uint64_t borrow=0;for(unsigned i=0;i<11;++i){uint64_t v=uint64_t(b[i])+borrow;borrow=uint64_t(a[i])<v;a[i]=uint32_t(uint64_t(a[i])-v);}}
inline Number add(Number a,const Number&b){uint64_t carry=0;for(unsigned i=0;i<11;++i){uint64_t n=uint64_t(a[i])+b[i]+carry;a[i]=uint32_t(n);carry=n>>32;}if(atLeast(a,Modulus))subtract(a,Modulus);return a;}
inline Number multiply(Number a,const Number&b){Number out{};for(unsigned i=0;i<320;++i){if((b[i/32]>>(i%32))&1)out=add(out,a);a=add(a,a);}return out;}
inline void key(const uint8_t source[80],uint8_t out[56]){
 unsigned dest=0;
 for(unsigned block=0;block<2;++block){
  Number a{};for(unsigned i=0;i<40;++i)a[i/4]|=uint32_t(source[block*40+i])<<((i%4)*8);
  while(atLeast(a,Modulus))subtract(a,Modulus);
  Number n=a;for(int bit=0;bit<16;++bit)n=multiply(n,n);n=multiply(n,a); // exponent 65537
  for(unsigned i=0;i<39&&dest<56;++i)out[dest++]=uint8_t(n[i/4]>>((i%4)*8));
 }
}
class Blowfish {
 uint32_t p[18],s[1024];
 uint32_t f(uint32_t n)const{return ((s[n>>24]+s[256+((n>>16)&255)])^s[512+((n>>8)&255)])+s[768+(n&255)];}
 void enc(uint32_t&l,uint32_t&r)const{for(int i=0;i<16;++i){l^=p[i];r^=f(l);std::swap(l,r);}std::swap(l,r);r^=p[16];l^=p[17];}
 static uint32_t big(const uint8_t*b){return uint32_t(b[0])<<24|uint32_t(b[1])<<16|uint32_t(b[2])<<8|b[3];}
 static void put(uint8_t*b,uint32_t n){for(int i=0;i<4;++i)b[i]=uint8_t(n>>(24-i*8));}
public:
 explicit Blowfish(const uint8_t*key,unsigned size=56){
  std::copy(Pi,Pi+18,p);std::copy(Pi+18,Pi+1042,s);
  unsigned at=0;for(auto&v:p){uint32_t n=0;for(int i=0;i<4;++i)n=(n<<8)|key[at++%size];v^=n;}
  uint32_t l=0,r=0;for(int i=0;i<18;i+=2){enc(l,r);p[i]=l;p[i+1]=r;}for(int i=0;i<1024;i+=2){enc(l,r);s[i]=l;s[i+1]=r;}
 }
 void decrypt(uint8_t*bytes,unsigned size)const{
  for(unsigned at=0;at+8<=size;at+=8){uint32_t l=big(bytes+at),r=big(bytes+at+4);for(int i=17;i>=2;--i){l^=p[i];r^=f(l);std::swap(l,r);}std::swap(l,r);r^=p[1];l^=p[0];put(bytes+at,l);put(bytes+at+4,r);}
 }
};
}}}
