#pragma once
#include "planner_types.h"
#include <array>
#include <memory>

namespace assist { namespace planner {
// The UI thread alone acquires writable Worlds. Once submitted, the worker's
// immutable shared_ptr (including its retained Planner indices) pins that slot.
// Reuse only a pool-only slot; saturation defers capture instead of overwriting
// an in-flight observation or allocating another fixed-capacity 6 MiB World.
class WorldPool {
 std::array<std::shared_ptr<World>,3> slots_;
public:
 std::shared_ptr<World> acquire(){
  for(auto&slot:slots_)if(slot&&slot.use_count()==1)return slot;
  for(auto&slot:slots_)if(!slot){slot=std::make_shared<World>();return slot;}
  return {};
 }
};
}}
