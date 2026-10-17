#pragma once
#include <cstdint>
#include <cstring>
#include <algorithm>

// Yuri's Revenge 1.001 / YRpp: HouseClass::Owned*Types are CounterClass
// instances (vptr, Items, Capacity, flags, Total). A counter's capacity is
// allocated on demand. Unallocated type indexes legitimately have zero count;
// do NOT reject the entire category when Capacity < highest type index + 1.
namespace um {
constexpr uint32_t OwnedOffsets[4] = {0x5528, 0x5514, 0x553C, 0x5500};
constexpr uint32_t OwnedPresentOffsets[4] = {0x5578, 0x5564, 0x558C, 0x5550};

// Reader signature: bool(uint32_t address, void* destination, size_t bytes)
// Output has exactly 'needed' entries, initialized even on failed reads.
template<class Read>
bool readCounterPrefix(Read& read, uint32_t counter, int needed, int* output, int& errors) {
 if(needed<0 || needed>4096 || !output) { ++errors; return false; }
 if(!needed)return true;
 std::memset(output,0,size_t(needed)*sizeof(int));
 uint32_t header[3]{}; // [0] vtable, [1] Items, [2] Capacity
 if(!read(counter,header,sizeof header)) { ++errors; return false; }
 int32_t capacity=static_cast<int32_t>(header[2]);
 if(capacity<0 || capacity>65536) { ++errors; return false; }
 int count=std::min(needed,int(capacity));
 if(!count)return true; // Empty CounterClass is valid: all owned counts are 0.
 if(!header[1] || !read(header[1],output,size_t(count)*sizeof(int))) {
  std::memset(output,0,size_t(needed)*sizeof(int));
  ++errors;return false;
 }
 return true;
}
}
