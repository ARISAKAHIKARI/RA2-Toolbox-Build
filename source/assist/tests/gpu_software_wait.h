#pragma once
// Test-only adapter: large native shader replays on llvmpipe can exceed the
// hardware watchdog even when they complete correctly. Keep production wait
// budgets intact; extend only an explicitly enabled CPU Vulkan device.
#include "../../external_reader/source/vulkan_compute.h"
namespace validationgpu {
template<class Backend>class SlowSoftware : public Backend {
 static PFN_vkWaitForFences& original(){static thread_local PFN_vkWaitForFences value=nullptr;return value;}
 static VkResult VKAPI_CALL wait(VkDevice d,uint32_t n,const VkFence*f,VkBool32 all,uint64_t ns){
  return original()(d,n,f,all,std::max<uint64_t>(ns,240000000000ull));
 }
public:
 SlowSoftware(){
#ifdef RA2_VULKAN_TEST_SOFTWARE
  if(this->initialize()&&this->diagnostics_.softwareDevice){original()=this->vkWaitForFences_;this->vkWaitForFences_=wait;}
#endif
 }
};
}
