#pragma once
#include "gpu_voxel.h"
#include <map>
#include <list>
#include <mutex>
#include <tuple>
#include <type_traits>
#include "render_memory_budget.h"
namespace external {namespace nativeimages {
// Immutable native-pixel world materials, BEFORE viewport composition/scaling.
// Image includes its depth, shadows and pure-value GPU triangles. No reader,
// HWND, driver resource or viewport-specific cursor/selection is shared.
struct Signature {
 uint64_t a=1469598103934665603ull,b=0x9e3779b97f4a7c15ull;
 void bytes(const void*data,size_t size){auto p=static_cast<const uint8_t*>(data);for(size_t i=0;i<size;++i){a=(a^p[i])*1099511628211ull;b^=p[i]+0x9e3779b97f4a7c15ull+(b<<6)+(b>>2);}}
 template<class T>void add(const T&value){static_assert(std::is_arithmetic<T>::value,"hash scalar fields, never structure padding");bytes(&value,sizeof value);}
};
struct Key {
 std::array<uint64_t,24> values{};std::array<uint32_t,256> palette{};
 bool operator<(const Key&o)const{return std::tie(values,palette)<std::tie(o.values,o.palette);}
};
inline size_t bytes(const voxel::Image&i){size_t n=sizeof i+i.pixels.capacity()*4+i.depth.capacity()*4+i.indices.capacity()+i.nativeShape.capacity()*sizeof(int32_t)+i.nativeShapeCoverage.capacity();
 // TMP second-shape/coverage and immutable packed-source/palette owners are
 // real storage too. Shared owners are conservatively charged per image.
 if(i.sourcePalette)n+=sizeof(*i.sourcePalette)+i.sourcePalette->capacity()*sizeof(uint32_t);
 if(i.blendSource)n+=sizeof(*i.blendSource)+i.blendSource->capacity()*sizeof(uint32_t);
 n+=(i.attachments.capacity()-i.attachments.size())*sizeof(voxel::Image);
 if(i.gpuRaster)n+=sizeof(gpuvoxel::Prepared)+i.gpuRaster->triangles.capacity()*sizeof(gpuvoxel::Triangle);for(const auto&a:i.attachments)n+=bytes(a);return n;}
template<class KeyType>class BasicBank {
 using Recency=std::list<const KeyType*>;
 struct Entry {std::shared_ptr<const voxel::Image>image;size_t bytes;typename Recency::iterator recent;};
 std::mutex mutex_;std::map<KeyType,Entry>entries_;Recency recent_;size_t bytes_=0,budget_;uint64_t hits_=0,misses_=0;
public:
 explicit BasicBank(size_t budget=rendermemory::SharedImages):budget_(budget){}
 std::shared_ptr<const voxel::Image>get(const KeyType&key){std::lock_guard<std::mutex>lock(mutex_);auto at=entries_.find(key);if(at==entries_.end()){++misses_;return {};}recent_.splice(recent_.end(),recent_,at->second.recent);++hits_;return at->second.image;}
 void put(KeyType key,std::shared_ptr<const voxel::Image>image){if(!image||image->incomplete)return;size_t cost=bytes(*image)+sizeof(Entry)+sizeof(KeyType)+7*sizeof(void*);if(cost>budget_)return;std::lock_guard<std::mutex>lock(mutex_);
  if(entries_.count(key))return;
  // Camera/pose churn must not scan every cached image for each eviction.
  while(!recent_.empty()&&bytes_+cost>budget_){auto old=entries_.find(*recent_.front());bytes_-=old->second.bytes;recent_.pop_front();entries_.erase(old);}
  auto inserted=entries_.emplace(std::move(key),Entry{std::move(image),cost,{}}).first;
  try{recent_.push_back(&inserted->first);}catch(...){entries_.erase(inserted);throw;}
  inserted->second.recent=std::prev(recent_.end());bytes_+=cost;
 }
 struct Stats{size_t resident,entries;uint64_t hits,misses;};Stats stats(){std::lock_guard<std::mutex>lock(mutex_);return {bytes_,entries_.size(),hits_,misses_};}
};
using Bank=BasicBank<Key>;
inline Bank&bank(){static Bank value;return value;}
}}

