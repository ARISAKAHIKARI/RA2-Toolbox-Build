#pragma once
#include "debug_entry_policy.h"
namespace debugentries {
// Bound the reference read; a disabled comparison never substitutes for it.
template<class Read>bool pattern(Kind kind,uint32_t address,const uint8_t*expected,size_t n,Read read){
 uint8_t bytes[256]{};const bool readable=expected&&n&&n<=sizeof bytes&&read(address,bytes,n);
 return allow(kind,address,readable,readable&&!memcmp(bytes,expected,n));
}
}
