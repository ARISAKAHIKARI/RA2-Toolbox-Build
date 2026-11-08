#pragma once
#include <cstdint>
#include <cstddef>

namespace external { namespace optionalread {
// Only an explicitly optional feature may use this adapter. A sealed reader
// still requests missing input; it can tolerate a failure only when Runtime
// actually observed that failure in the same publication. There is no live
// fallback and the ordinary object/roster/pose reader remains strict.
template<class Read>auto bytes(Read&read,uint32_t p,void*out,size_t n,int)->decltype(read.optional(p,out,n)){return read.optional(p,out,n);}
template<class Read>bool bytes(Read&read,uint32_t p,void*out,size_t n,long){return read(p,out,n);}
template<class Read>bool bytes(Read read,uint32_t p,void*out,size_t n){return bytes(read,p,out,n,0);}
template<class Read>struct View {
 Read read;
 bool operator()(uint32_t p,void*out,size_t n)const{return bytes(read,p,out,n);}
};
}}
