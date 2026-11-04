#pragma once
#include <cstdint>
namespace external { namespace nativepips {
// Bounded positive-input specialization of Vera20K's MaskedX87Chop53 and
// rounded_pip_quantity (GPL-3.0, 3d42426). Each arithmetic step truncates to
// 53 significand bits before the next step. No host rounding-mode changes.
// Caller admits whole nonnegative storage slots <= 1,000,000 each, a positive
// Type.Storage <= 1,000,000, and three/five displayed slots. Other raw storage
// representations stay outside this reader's supported state.
inline int quantity(uint32_t amount,uint32_t capacity,uint32_t maximum){
 if(!amount||!capacity||!maximum)return 0;
 using Wide=uint64_t;
 auto bits=[](Wide n){int count=0;while(n){++count;n>>=1;}return count;};
 int exponent=bits(amount)-bits(capacity);
 if(exponent>=0){if(Wide(amount)<(Wide(capacity)<<exponent))--exponent;}
 else if((Wide(amount)<<(-exponent))<capacity)--exponent;
 int power=exponent-52;
 // Binary long division keeps the remainder below capacity and the quotient
 // below 2^53; it also works with MSVC and the legacy 32-bit host.
 Wide mantissa=amount/capacity,remainder=amount%capacity;
 for(int n=0;n<52-exponent;++n){remainder*=2;mantissa=mantissa*2+remainder/capacity;remainder%=capacity;}
 auto chop=[&](){int drop=bits(mantissa)-53;if(drop>0){mantissa>>=drop;power+=drop;}};
 mantissa*=maximum;chop();
 // A value below one half stays below one after adding 0.5 and chopping.
 if(power<-53)return 0;
 // Otherwise the alignment and sum fit in 54 bits.
 mantissa+=Wide(1)<<(-1-power);chop();
 Wide result=power<0?mantissa>>(-power):mantissa<<power;
 return result>maximum?int(maximum):int(result);
}
}}
