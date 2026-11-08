#include <cstring>
#include <cstdint>
extern "C" void* memset(void*p,int c,size_t n)noexcept{
 volatile unsigned char*d=(volatile unsigned char*)p;uint32_t word=uint32_t((unsigned char)c)*0x01010101u;
 while(n>=4){*(volatile uint32_t*)d=word;d+=4;n-=4;}while(n--)*d++=(unsigned char)c;return p;
}
extern "C" void* memcpy(void*p,const void*q,size_t n)noexcept{
 volatile unsigned char*d=(volatile unsigned char*)p;const volatile unsigned char*s=(const volatile unsigned char*)q;
 while(n>=4){*(volatile uint32_t*)d=*(const volatile uint32_t*)s;d+=4;s+=4;n-=4;}while(n--)*d++=*s++;return p;
}
