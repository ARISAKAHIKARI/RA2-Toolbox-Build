#pragma once
#include <cstdint>
// Pure state for explicit read-only cleanup. No game reads or command authority.
namespace readonly {
struct IdleRetirement {
 bool valid=false;
 uint32_t owner=0,epoch=0;
 uint64_t connection=0,generation=0;
 constexpr bool same(uint32_t o,uint32_t e,uint64_t c,uint64_t g)const{
  return valid&&owner==o&&epoch==e&&connection==c&&generation==g;
 }
 constexpr bool needsRetirement(uint32_t o,uint32_t e,uint64_t c,uint64_t g,bool pending)const{
  return pending||!same(o,e,c,g);
 }
};
namespace idle_retirement_tests {
constexpr IdleRetirement empty{};
constexpr IdleRetirement retired{true,1,2,3,4};
static_assert(empty.needsRetirement(1,2,3,4,false),"first idle callback must retire");
static_assert(!retired.needsRetirement(1,2,3,4,false),"stable retired idle callback");
static_assert(retired.needsRetirement(2,2,3,4,false),"owner changed");
static_assert(retired.needsRetirement(1,3,3,4,false),"epoch changed");
static_assert(retired.needsRetirement(1,2,4,4,false),"connection changed");
static_assert(retired.needsRetirement(1,2,3,5,false),"generation changed");
static_assert(retired.needsRetirement(1,2,3,4,true),"pending work requires retirement");
constexpr bool invalidateThenRetire(){
 auto state=retired;state.valid=false;
 return state.needsRetirement(1,2,3,4,false);
}
static_assert(invalidateThenRetire(),"new demand or authority transition invalidates the marker");
}
}
