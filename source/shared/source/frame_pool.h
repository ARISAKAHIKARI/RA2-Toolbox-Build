#pragma once
#include <array>
#include <memory>
// Published frames (including aliasing pixel owners) remain immutable. Only
// a slot with no reader can be reused; a saturated pool allocates temporarily
// instead of blocking or overwriting the compositor's pixels.
namespace asyncwork {
template<class T,size_t Slots=3>class FramePool {
 std::array<std::shared_ptr<T>,Slots> slots_{};
public:
 std::shared_ptr<T> acquire(){
  for(auto&slot:slots_)if(slot&&slot.use_count()==1)return slot;
  auto next=std::make_shared<T>();for(auto&slot:slots_)if(!slot){slot=next;break;}return next;
 }
 void clear(){for(auto&slot:slots_)slot.reset();}
};
}
