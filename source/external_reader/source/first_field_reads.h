#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace external { namespace fieldreads {
// One capture, not a frame/page cache. Nearby first observations can share a
// fresh RPM block; repeated or overlapping fields always go back to the source.
// In particular, capture's trailing identity/position checks must not observe
// the same bytes as its initial sample. A failed block is only a missed
// optimization: the original field reads retain their own failure semantics.
template<class Read>class First {
 static constexpr size_t Limit=1024,Capacity=4;
 struct Block {uint32_t address=0;size_t size=0;bool valid=false;
  std::array<uint8_t,Limit>bytes;std::array<uint8_t,Limit>used{};
 };
 Read source_;std::array<Block,Capacity>blocks_;size_t count_=0;
 bool get(uint32_t p,void*out,size_t n){
  if(!out||!n||n>0x80000000ull)return source_(p,out,n);
  const Block*chosen=nullptr;bool repeated=false;
  for(size_t i=0;i<count_;++i){auto&b=blocks_[i];
   if(!b.valid)continue;
   const uint64_t begin=std::max<uint64_t>(p,b.address),end=std::min<uint64_t>(uint64_t(p)+n,uint64_t(b.address)+b.size);
   if(begin>=end)continue;
   auto first=b.used.begin()+size_t(begin-b.address),last=b.used.begin()+size_t(end-b.address);
   repeated|=std::any_of(first,last,[](uint8_t used){return used!=0;});std::fill(first,last,1);
   if(!chosen&&p>=b.address&&uint64_t(p)+n<=uint64_t(b.address)+b.size)chosen=&b;
  }
  if(chosen&&!repeated){std::memcpy(out,chosen->bytes.data()+(p-chosen->address),n);return true;}
  return source_(p,out,n);
 }
public:
 explicit First(Read source):source_(source){}
 First(const First&)=delete;First&operator=(const First&)=delete;
 void block(uint32_t p,size_t n){
  if(count_==Capacity||p<0x10000||!n||n>Limit||uint64_t(p)+n>0x80000000ull)return;
  auto&b=blocks_[count_++];b.address=p;b.size=n;b.valid=source_(p,b.bytes.data(),n);
 }
 auto view(){return [this](uint32_t p,void*out,size_t n){return get(p,out,n);};}
};
}}
