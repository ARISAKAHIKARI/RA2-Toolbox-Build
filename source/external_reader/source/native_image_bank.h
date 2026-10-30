#pragma once
#include "gpu_voxel.h"
#include <map>
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
inline size_t bytes(const voxel::Image&i){size_t n=sizeof i+i.pixels.capacity()*4+i.depth.capacity()*4+i.indices.capacity();if(i.gpuRaster)n+=sizeof(gpuvoxel::Prepared)+i.gpuRaster->triangles.capacity()*sizeof(gpuvoxel::Triangle);for(const auto&a:i.attachments)n+=bytes(a);return n;}
class Bank {
 struct Entry {std::shared_ptr<const voxel::Image>image;uint64_t used;size_t bytes;};
 std::mutex mutex_;std::map<Key,Entry>entries_;size_t bytes_=0,budget_;uint64_t serial_=0,hits_=0,misses_=0;
public:
 explicit Bank(size_t budget=rendermemory::SharedImages):budget_(budget){}
 std::shared_ptr<const voxel::Image>get(const Key&key){std::lock_guard<std::mutex>lock(mutex_);auto at=entries_.find(key);if(at==entries_.end()){++misses_;return {};}at->second.used=++serial_;++hits_;return at->second.image;}
 void put(Key key,std::shared_ptr<const voxel::Image>image){if(!image||image->incomplete)return;size_t cost=bytes(*image)+sizeof(Entry)+sizeof(Key);if(cost>budget_)return;std::lock_guard<std::mutex>lock(mutex_);
  if(entries_.count(key))return;
  while(!entries_.empty()&&bytes_+cost>budget_){auto old=std::min_element(entries_.begin(),entries_.end(),[](const auto&a,const auto&b){return a.second.used<b.second.used;});bytes_-=old->second.bytes;entries_.erase(old);}
  bytes_+=cost;entries_.emplace(std::move(key),Entry{std::move(image),++serial_,cost});
 }
 struct Stats{size_t resident,entries;uint64_t hits,misses;};Stats stats(){std::lock_guard<std::mutex>lock(mutex_);return {bytes_,entries_.size(),hits_,misses_};}
};
inline Bank&bank(){static Bank value;return value;}
}}
