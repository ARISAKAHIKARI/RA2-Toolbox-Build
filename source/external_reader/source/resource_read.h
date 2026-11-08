#pragma once
#include <cstdint>
#include <cstddef>
namespace external {namespace resourceread {
template<class Read>auto bytes(Read&read,uint32_t p,void*out,size_t n,int)->decltype(read.resource(p,out,n)){return read.resource(p,out,n);}
template<class Read>bool bytes(Read&read,uint32_t p,void*out,size_t n,long){return read(p,out,n);}
template<class Read>struct View {
 Read read;
 bool operator()(uint32_t p,void*out,size_t n)const{auto copy=read;return bytes(copy,p,out,n,0);}
};
template<class Read>View<Read> view(Read read){return {read};}
}}
