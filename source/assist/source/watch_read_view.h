#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
namespace watchscene {
// Preserve the reader's bounded batching capability through the renderer.
// std::function erased prefetch(), making watch terrain take one remote page
// read at a time even though the common Sampler already batches those pages.
template<class Reader>struct ReadView {
 static constexpr bool SealedWorldReads=true;
 Reader*reader;
 bool operator()(uint32_t p,void*out,size_t n)const{return reader->read(p,out,n);}
 template<class R>static auto nullable(R&r,uint32_t p,void*out,size_t n,int)->decltype(r.readOptional(p,out,n)){return r.readOptional(p,out,n);}
 template<class R>static bool nullable(R&r,uint32_t p,void*out,size_t n,long){return r.read(p,out,n);}
 bool optional(uint32_t p,void*out,size_t n)const{return nullable(*reader,p,out,n,0);}
 template<class R>static auto batch(R&r,const std::vector<uint32_t>&p,size_t n,int)->decltype(r.prefetch(p,n),void()){r.prefetch(p,n);}
 template<class R>static void batch(R&,const std::vector<uint32_t>&,size_t,long){}
 void prefetch(const std::vector<uint32_t>&p,size_t n)const{batch(*reader,p,n,0);}
};
}
