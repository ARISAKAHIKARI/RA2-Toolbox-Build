#pragma once
#include <stddef.h>
#include <stdint.h>

namespace assist { namespace runtimememory {
// The Runtime supplies its own CRT entry points under -fno-builtin. Keep
// small field/structure copies cheap, without aligned uint32_t dereferences
// on arbitrary byte buffers. Constant-size builtins lower to scalar loads
// and stores in the supported Runtime build; they must never call this CRT.
inline void* copy(void*destination,const void*source,size_t count){
 void*const result=destination;
#if defined(__GNUC__) || defined(__clang__)
 auto*d=static_cast<unsigned char*>(destination);auto*s=static_cast<const unsigned char*>(source);
 if(count<=64){
  // The empty register barrier keeps this short loop scalar and prevents
  // loop-idiom rewriting back into the CRT function being implemented.
  while(count>=4){uint32_t word;__builtin_memcpy(&word,s,4);__asm__ volatile("" : : "r"(word) : "memory");__builtin_memcpy(d,&word,4);d+=4;s+=4;count-=4;}
  if(count>=2){uint16_t word;__builtin_memcpy(&word,s,2);__builtin_memcpy(d,&word,2);d+=2;s+=2;count-=2;}
  if(count)*d=*s;return result;
 }
#if defined(__i386__) || defined(__x86_64__)
 // Explicit direction and clobbers; no SIMD, CPU feature dispatch or OS call.
 __asm__ volatile("cld\n\trep movsb" : "+D"(d), "+S"(s), "+c"(count) : : "cc", "memory");
 return result;
#endif
#endif
 // Volatile bytes prevent a portable fallback from being folded into a
 // recursive memcpy call. As with memcpy, overlapping ranges are not valid.
 auto*output=static_cast<volatile unsigned char*>(destination);
 auto*input=static_cast<const volatile unsigned char*>(source);
 while(count--)*output++=*input++;return result;
}

inline void* fill(void*destination,int value,size_t count){
 void*const result=destination;const auto byte=static_cast<unsigned char>(value);
#if defined(__GNUC__) || defined(__clang__)
 auto*d=static_cast<unsigned char*>(destination);
 if(count<=64){
  const uint32_t word=uint32_t(byte)*0x01010101u;
  while(count>=4){__asm__ volatile("" : : "r"(word) : "memory");__builtin_memcpy(d,&word,4);d+=4;count-=4;}
  if(count>=2){const uint16_t half=uint16_t(word);__builtin_memcpy(d,&half,2);d+=2;count-=2;}
  if(count)*d=byte;return result;
 }
#if defined(__i386__) || defined(__x86_64__)
 __asm__ volatile("cld\n\trep stosb" : "+D"(d), "+c"(count) : "a"(byte) : "cc", "memory");
 return result;
#endif
#endif
 auto*output=static_cast<volatile unsigned char*>(destination);
 while(count--)*output++=byte;return result;
}
}}
