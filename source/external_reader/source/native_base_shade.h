#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#if !defined(RA2_NATIVE_BASE_SCALAR_ONLY) && (defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP>=2))
#include <emmintrin.h>
#define RA2_NATIVE_BASE_SSE2 1
#else
#define RA2_NATIVE_BASE_SSE2 0
#endif
namespace external {namespace nativesurface {
inline void shadeBase(uint32_t*pixels,const uint8_t*alpha,size_t count,bool allowSIMD){
 size_t n=0;
#if RA2_NATIVE_BASE_SSE2
 if(allowSIMD){const auto zero=_mm_setzero_si128(),one=_mm_set1_epi16(1),limit=_mm_set1_epi16(32385),mask=_mm_set1_epi32(int(0xff000000u));
  // Saturate the product before division: min(255, x/127) is unchanged by
  // min(x, 255*127). The exact reciprocal sum then fits every u16 lane.
  auto scale=[&](__m128i color,__m128i a){auto product=_mm_mullo_epi16(color,a);product=_mm_sub_epi16(product,_mm_subs_epu16(product,limit));
   auto y=_mm_add_epi16(product,one);return _mm_srli_epi16(_mm_add_epi16(y,_mm_add_epi16(_mm_srli_epi16(y,7),_mm_srli_epi16(y,14))),7);};
  for(;n+4<=count;n+=4){uint32_t a;std::memcpy(&a,alpha+n,4);if(a==0x7f7f7f7fu)continue;
   auto av=_mm_cvtsi32_si128(int(a));av=_mm_unpacklo_epi8(av,av);av=_mm_unpacklo_epi16(av,av);
   auto source=_mm_loadu_si128(reinterpret_cast<const __m128i*>(pixels+n));
   auto low=scale(_mm_unpacklo_epi8(source,zero),_mm_unpacklo_epi8(av,zero));
   auto high=scale(_mm_unpackhi_epi8(source,zero),_mm_unpackhi_epi8(av,zero));
   auto result=_mm_packus_epi16(low,high);result=_mm_or_si128(_mm_and_si128(source,mask),_mm_andnot_si128(mask,result));
   _mm_storeu_si128(reinterpret_cast<__m128i*>(pixels+n),result);
  }
 }
#else
 (void)allowSIMD;
#endif
 for(;n<count;++n){unsigned a=alpha[n];if(a==127)continue;uint32_t c=pixels[n],out=c&0xff000000u;
  for(int shift:{0,8,16})out|=std::min(255u,((c>>shift)&255u)*a/127u)<<shift;pixels[n]=out;
 }
}
}}
