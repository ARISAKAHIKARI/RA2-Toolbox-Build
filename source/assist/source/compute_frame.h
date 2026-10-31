#pragma once
#include <cstdint>
#include <memory>
#include <array>
#include <type_traits>
namespace assist { namespace compute {
// Bounded provenance for already-running work. This is not a calculation
// queue: the host has one running job and replaces its pending input.
template<class Input,unsigned N=16>class FrameHistory {
 struct Entry {uint32_t serial=0;Input value;std::array<int,3> cursors{};};
 std::array<std::unique_ptr<Entry>,N> slots_;
public:
 void clear(){for(auto&p:slots_)p.reset();}
 void retain(uint32_t serial,const Input&input,std::array<int,3> cursors={}){static_assert(std::is_trivially_copyable<Input>::value,"Frame input must be a value");auto&p=slots_[serial%N];if(!p)p=std::make_unique<Entry>();p->serial=serial;p->value=input;p->cursors=cursors;}
 const std::array<int,3>*cursors(uint32_t serial)const{const auto&p=slots_[serial%N];return serial&&p&&p->serial==serial?&p->cursors:nullptr;}
 const Input*find(uint32_t serial)const{const auto&p=slots_[serial%N];return serial&&p&&p->serial==serial?&p->value:nullptr;}
};
inline uint32_t nextTask(uint32_t serial){return ++serial?serial:1;}
inline bool newer(uint32_t a,uint32_t b){return a&&int32_t(a-b)>0;}
}
}
