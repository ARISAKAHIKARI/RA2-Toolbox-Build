#pragma once
#include "gpu_memory_policy.h"
#include "../../shared/source/gpu_surface.h"
// Optional Vulkan 1.0 compute resolve. No game handles, hooks or memory writes.
// Input tiles are immutable premultiplied ARGB8; only changed tiles are uploaded.
#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include "../../third_party/vulkan/vulkan_core.h"
#include "vulkan_bilinear_spirv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../../third_party/vulkan/vulkan_win32.h"
#else
#include <dlfcn.h>
#endif
namespace external { namespace gpuxbrz {class Stage;} namespace gpuvoxel {class Stage;} namespace gpushroud {class Stage;} namespace gpuarea {class ObjectStage;} namespace vulkancompute {
struct TileJob {
 std::shared_ptr<const std::vector<uint32_t>> pixels;
 int sourceWidth=0,sourceHeight=0,left=0,top=0,width=0,height=0;
 double sourceX=0,sourceY=0,stepX=1,stepY=1;
 int reconstructionFactor=0; // 0: prepared pixels;2..4: raw pixels handled by FrameResolve
 bool nearest=false; // floor source centers; no filtering or xBRZ implied
 // Optional shared full-output axes preserve the CPU center calculation at
 // rational scales. Repeated step addition can land one ULP below a cell.
 std::shared_ptr<const std::vector<int32_t>> nearestXs,nearestYs;
};
struct Diagnostics {
 uint64_t frames=0,uploadedBytes=0,uploads=0,dispatches=0,readbacks=0,xbrzBuilds=0,voxelRasters=0;
 bool xbrzAvailable=false;
 uint64_t allocatedBufferBytes=0,mappedBufferBytes=0,peakMappedBufferBytes=0;
 double milliseconds=0,prepareMilliseconds=0,waitMilliseconds=0,readbackMilliseconds=0;
 bool available=false,softwareDevice=false,readbackCached=false;
 std::string device,reason;
};
class Resolve {
 friend class DirectOutput;
 friend class external::gpuxbrz::Stage;
 friend class external::gpuvoxel::Stage;
 friend class external::gpushroud::Stage;
 friend class external::gpuarea::ObjectStage;
protected:
#ifdef RA2_VULKAN_TEST_SOFTWARE
 static constexpr bool AllowSoftware=true;
#else
 static constexpr bool AllowSoftware=false;
#endif
 static constexpr VkDeviceSize AtlasBytes=128ull*1024*1024;
 static constexpr uint64_t MaxPixels=32ull*1024*1024,MaxCoordinates=4ull*1024*1024;
 struct Buffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;VkDeviceSize size=0;void* mapped=nullptr;bool coherent=false,cached=false;VkDeviceSize allocationBytes=0;};
 struct Tile {std::weak_ptr<const std::vector<uint32_t>> owner;const void* address=nullptr;uint32_t offset=0,count=0;uint64_t used=0;};
 struct Coordinate {uint32_t a,b,weight,pad;};
 struct Params {uint32_t offset,sourceWidth,outputWidth,left,top,width,height,xOffset,yOffset;};
 struct Dispatch {Params params;};
#ifdef _WIN32
 HMODULE library_=nullptr;
#else
 void* library_=nullptr;
#endif
 VkInstance instance_=VK_NULL_HANDLE;VkPhysicalDevice physical_=VK_NULL_HANDLE;VkDevice device_=VK_NULL_HANDLE;
 VkQueue queue_=VK_NULL_HANDLE;uint32_t queueFamily_=0;
 bool externalImages_=false;uint64_t adapterLuid_=0;
 VkPhysicalDeviceMemoryProperties memory_{};VkPhysicalDeviceProperties properties_{};VkPhysicalDeviceFeatures features_{};
 VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;VkPipelineLayout pipelineLayout_=VK_NULL_HANDLE;
 VkPipeline pipeline_=VK_NULL_HANDLE;VkDescriptorPool descriptorPool_=VK_NULL_HANDLE;VkDescriptorSet descriptors_=VK_NULL_HANDLE;
 VkCommandPool commandPool_=VK_NULL_HANDLE;VkCommandBuffer command_=VK_NULL_HANDLE;VkFence fence_=VK_NULL_HANDLE;
 Buffer atlas_,coordinates_,output_,upload_,readback_;
 std::vector<Tile> tiles_;uint64_t serial_=0;bool tried_=false,disabled_=false,inFlight_=false,abandoned_=false;
 Diagnostics diagnostics_;
 uint64_t allocatedBufferBytes_=0,mappedBufferBytes_=0,peakMappedBufferBytes_=0;
#ifdef RA2_VULKAN_MAPPED_BUDGET_BYTES
 static constexpr uint64_t MappedBudget=RA2_VULKAN_MAPPED_BUDGET_BYTES;
#else
 static constexpr uint64_t MappedBudget=sizeof(void*)>=8?1536ull*1024*1024:512ull*1024*1024;
#endif
 void memoryDiagnostics(){diagnostics_.allocatedBufferBytes=allocatedBufferBytes_;diagnostics_.mappedBufferBytes=mappedBufferBytes_;diagnostics_.peakMappedBufferBytes=peakMappedBufferBytes_;diagnostics_.readbackCached=readback_.cached;}
 static std::atomic<bool>& quarantined(){static std::atomic<bool> value{false};return value;}
#define RA2_VK_GLOBAL(F) PFN_##F F##_=nullptr;
 RA2_VK_GLOBAL(vkGetInstanceProcAddr) RA2_VK_GLOBAL(vkCreateInstance)
#define RA2_VK_INSTANCE_FUNCTIONS(X) \
 X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
 X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceFeatures) X(vkCreateDevice) X(vkGetDeviceProcAddr)
 RA2_VK_INSTANCE_FUNCTIONS(RA2_VK_GLOBAL)
#define RA2_VK_DEVICE_FUNCTIONS(X) \
 X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) \
 X(vkAllocateMemory) X(vkFreeMemory) X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory) \
 X(vkFlushMappedMemoryRanges) X(vkInvalidateMappedMemoryRanges) X(vkCreateShaderModule) X(vkDestroyShaderModule) \
 X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) \
 X(vkCreateComputePipelines) X(vkDestroyPipeline) X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) \
 X(vkAllocateDescriptorSets) X(vkUpdateDescriptorSets) X(vkCreateCommandPool) X(vkDestroyCommandPool) \
 X(vkAllocateCommandBuffers) X(vkResetCommandPool) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
 X(vkCmdCopyBuffer) X(vkCmdFillBuffer) X(vkCmdPipelineBarrier) X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) \
 X(vkCmdPushConstants) X(vkCmdDispatch) X(vkCreateFence) X(vkDestroyFence) X(vkResetFences) \
 X(vkQueueSubmit) X(vkWaitForFences) X(vkGetFenceStatus)
 RA2_VK_DEVICE_FUNCTIONS(RA2_VK_GLOBAL)
#undef RA2_VK_GLOBAL
 bool fail(const char* reason){diagnostics_.reason=reason;return false;}
 bool fatal(const char* reason){disabled_=true;diagnostics_.available=false;return fail(reason);}
 bool abandon(const char*reason){abandoned_=inFlight_=disabled_=true;quarantined().store(true,std::memory_order_relaxed);diagnostics_.available=false;return fail(reason);}
 void destroy(Buffer& b){if(!device_)return;if(b.mapped&&vkUnmapMemory_){vkUnmapMemory_(device_,b.memory);mappedBufferBytes_-=b.allocationBytes;}if(b.buffer&&vkDestroyBuffer_)vkDestroyBuffer_(device_,b.buffer,nullptr);if(b.memory&&vkFreeMemory_){vkFreeMemory_(device_,b.memory,nullptr);allocatedBufferBytes_-=b.allocationBytes;}b={};memoryDiagnostics();}
 void close(){
  if(abandoned_)return;
  // A timed-out queue may still reference every allocation. Never destroy or
  // unload that live device. Quarantine this single bounded backend until exit; a process-wide sticky flag blocks
  // subsequent backend initialization after such a timeout, including worker
  // restart or UI toggles.
  // CPU fallback remains usable without waiting indefinitely on a broken driver.
  if(inFlight_&&vkGetFenceStatus_&&vkGetFenceStatus_(device_,fence_)!=VK_SUCCESS)return;
  inFlight_=false;
  if(device_){
   destroy(atlas_);destroy(coordinates_);destroy(output_);destroy(upload_);destroy(readback_);
   if(fence_&&vkDestroyFence_)vkDestroyFence_(device_,fence_,nullptr);
   if(commandPool_&&vkDestroyCommandPool_)vkDestroyCommandPool_(device_,commandPool_,nullptr);
   if(descriptorPool_&&vkDestroyDescriptorPool_)vkDestroyDescriptorPool_(device_,descriptorPool_,nullptr);
   if(pipeline_&&vkDestroyPipeline_)vkDestroyPipeline_(device_,pipeline_,nullptr);
   if(pipelineLayout_&&vkDestroyPipelineLayout_)vkDestroyPipelineLayout_(device_,pipelineLayout_,nullptr);
   if(descriptorLayout_&&vkDestroyDescriptorSetLayout_)vkDestroyDescriptorSetLayout_(device_,descriptorLayout_,nullptr);
   if(vkDestroyDevice_)vkDestroyDevice_(device_,nullptr);device_=VK_NULL_HANDLE;
  }
  if(instance_&&vkDestroyInstance_)vkDestroyInstance_(instance_,nullptr);instance_=VK_NULL_HANDLE;
#ifdef _WIN32
  if(library_)FreeLibrary(library_);
#else
  if(library_)dlclose(library_);
#endif
  library_=nullptr;
 }
 bool buffer(Buffer& b,VkDeviceSize bytes,VkBufferUsageFlags usage,bool host,bool exact=false,VkDeviceSize maximum=0){
  if(maximum&&bytes>maximum)return fail("Vulkan buffer budget exceeded");
  if((usage&VK_BUFFER_USAGE_STORAGE_BUFFER_BIT)&&bytes>properties_.limits.maxStorageBufferRange)return fail("Vulkan device storage range exceeded");
  if(bytes<=b.size&&b.buffer)return true;
  destroy(b);VkDeviceSize capacity=exact?bytes:4096;while(capacity<bytes)capacity*=2;if(maximum)capacity=std::min(capacity,maximum);
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=capacity;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  if(vkCreateBuffer_(device_,&info,nullptr,&b.buffer)!=VK_SUCCESS)return fail("Vulkan buffer creation failed");
  VkMemoryRequirements req{};vkGetBufferMemoryRequirements_(device_,b.buffer,&req);
  if(host&&(req.size>MappedBudget||mappedBufferBytes_>MappedBudget-req.size)){destroy(b);return fail("Vulkan mapped memory budget exceeded");}
  int best=-1,score=-1;
  for(uint32_t i=0;i<memory_.memoryTypeCount;++i)if(req.memoryTypeBits&(1u<<i)){
   const auto flags=memory_.memoryTypes[i].propertyFlags;
   const bool readback=host&&(usage&VK_BUFFER_USAGE_TRANSFER_DST_BIT)&&!(usage&VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
   const int s=gpumemory::score(host,readback,flags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,flags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,flags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT,flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   if(s>score){best=int(i);score=s;}
  }
  if(best<0){destroy(b);return fail("No suitable Vulkan memory type");}
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.allocationSize=req.size;allocation.memoryTypeIndex=uint32_t(best);
  if(vkAllocateMemory_(device_,&allocation,nullptr,&b.memory)!=VK_SUCCESS){destroy(b);return fail("Vulkan buffer allocation failed");}
  b.allocationBytes=req.size;allocatedBufferBytes_+=req.size;memoryDiagnostics();
  if(vkBindBufferMemory_(device_,b.buffer,b.memory,0)!=VK_SUCCESS){destroy(b);return fail("Vulkan buffer binding failed");}
  b.size=capacity;b.coherent=(memory_.memoryTypes[best].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;b.cached=(memory_.memoryTypes[best].propertyFlags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)!=0;
  if(host){void*mapped=nullptr;if(vkMapMemory_(device_,b.memory,0,VK_WHOLE_SIZE,0,&mapped)!=VK_SUCCESS){destroy(b);return fail("Vulkan mapping failed");}b.mapped=mapped;}
  if(host){mappedBufferBytes_+=b.allocationBytes;peakMappedBufferBytes_=std::max(peakMappedBufferBytes_,mappedBufferBytes_);memoryDiagnostics();}
  return true;
 }
 bool flush(Buffer& b){if(b.coherent)return true;VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=b.memory;range.size=VK_WHOLE_SIZE;return vkFlushMappedMemoryRanges_(device_,1,&range)==VK_SUCCESS;}
 bool initialize(){
  if(quarantined().load(std::memory_order_relaxed))return fatal("Vulkan disabled after driver timeout; restart required");
  if(tried_)return !disabled_&&device_;tried_=true;
#ifdef _WIN32
  // Search only the Windows system directory: never load a game-local DLL.
  wchar_t system[MAX_PATH]{};UINT length=GetSystemDirectoryW(system,MAX_PATH);
  if(!length||length>=MAX_PATH-15)return fatal("Windows Vulkan loader unavailable");
  std::wstring path(system,length);path+=L"\\vulkan-1.dll";library_=LoadLibraryW(path.c_str());
  if(library_)vkGetInstanceProcAddr_=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(library_,"vkGetInstanceProcAddr"));
#else
  library_=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
  if(library_)vkGetInstanceProcAddr_=reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(library_,"vkGetInstanceProcAddr"));
#endif
  if(!vkGetInstanceProcAddr_)return fatal("Vulkan loader unavailable");
  vkCreateInstance_=reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr_(VK_NULL_HANDLE,"vkCreateInstance"));
  if(!vkCreateInstance_)return fatal("Vulkan 1.0 entry point unavailable");
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="RA2 Toolbox fractional resolve";app.apiVersion=VK_API_VERSION_1_0;
#ifdef _WIN32
  // DirectComposition sharing uses the Vulkan 1.1 external-memory queries.
  // Keep the established compute/readback path on older loaders/devices.
  auto instanceVersion=reinterpret_cast<PFN_vkEnumerateInstanceVersion>(vkGetInstanceProcAddr_(VK_NULL_HANDLE,"vkEnumerateInstanceVersion"));uint32_t version=VK_API_VERSION_1_0;
  if(instanceVersion&&instanceVersion(&version)==VK_SUCCESS&&version>=VK_API_VERSION_1_1)app.apiVersion=VK_API_VERSION_1_1;
#endif
  VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pApplicationInfo=&app;
  if(vkCreateInstance_(&create,nullptr,&instance_)!=VK_SUCCESS)return fatal("Vulkan instance unavailable");
#define RA2_VK_LOAD_INSTANCE(F) F##_=reinterpret_cast<PFN_##F>(vkGetInstanceProcAddr_(instance_,#F));if(!F##_)return fatal("Missing Vulkan instance function " #F);
  RA2_VK_INSTANCE_FUNCTIONS(RA2_VK_LOAD_INSTANCE)
#undef RA2_VK_LOAD_INSTANCE
  uint32_t count=0;if(vkEnumeratePhysicalDevices_(instance_,&count,nullptr)!=VK_SUCCESS||!count||count>128)return fatal("No Vulkan compute device");
  std::vector<VkPhysicalDevice> devices(count);if(vkEnumeratePhysicalDevices_(instance_,&count,devices.data())!=VK_SUCCESS)return fatal("Vulkan device enumeration failed");
  int best=-1;
  for(auto candidate:devices){
   VkPhysicalDeviceProperties props{};vkGetPhysicalDeviceProperties_(candidate,&props);
   if(props.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU&&!AllowSoftware)continue;
   if(props.limits.maxStorageBufferRange<AtlasBytes||props.limits.maxPushConstantsSize<sizeof(Params)||props.limits.maxComputeWorkGroupInvocations<64||props.limits.maxComputeWorkGroupSize[0]<8||props.limits.maxComputeWorkGroupSize[1]<8)continue;
   uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties_(candidate,&n,nullptr);if(!n||n>1024)continue;
   std::vector<VkQueueFamilyProperties> queues(n);vkGetPhysicalDeviceQueueFamilyProperties_(candidate,&n,queues.data());
   for(uint32_t i=0;i<n;++i)if(queues[i].queueCount&&(queues[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){
    int s=props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU?30:props.deviceType==VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU?20:props.deviceType==VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU?10:0;
    if(s>best){best=s;physical_=candidate;properties_=props;queueFamily_=i;}break;
   }
  }
  if(!physical_)return fatal("No compatible Vulkan compute device");
  diagnostics_.device=properties_.deviceName;diagnostics_.softwareDevice=properties_.deviceType==VK_PHYSICAL_DEVICE_TYPE_CPU;
  vkGetPhysicalDeviceMemoryProperties_(physical_,&memory_);
  VkPhysicalDeviceFeatures supported{};vkGetPhysicalDeviceFeatures_(physical_,&supported);features_.shaderFloat64=supported.shaderFloat64;
  float priority=1;VkDeviceQueueCreateInfo q{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};q.queueFamilyIndex=queueFamily_;q.queueCount=1;q.pQueuePriorities=&priority;
  VkDeviceCreateInfo d{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;d.pEnabledFeatures=&features_;
#ifdef _WIN32
  const char* sharingExtensions[]={VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME};
  if(gpupresent::DirectPresentationEnabled&&app.apiVersion>=VK_API_VERSION_1_1&&properties_.apiVersion>=VK_API_VERSION_1_1){
   auto enumerate=reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(vkGetInstanceProcAddr_(instance_,"vkEnumerateDeviceExtensionProperties"));
   auto properties2=reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(vkGetInstanceProcAddr_(instance_,"vkGetPhysicalDeviceProperties2"));uint32_t n=0;
   if(enumerate&&properties2&&enumerate(physical_,nullptr,&n,nullptr)==VK_SUCCESS&&n<=4096){std::vector<VkExtensionProperties> extensions(n);
    if(enumerate(physical_,nullptr,&n,extensions.data())==VK_SUCCESS){bool supported=true;for(auto name:sharingExtensions)supported&=std::any_of(extensions.begin(),extensions.end(),[&](const auto&e){return std::strcmp(e.extensionName,name)==0;});
     VkPhysicalDeviceIDProperties id{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};VkPhysicalDeviceProperties2 props{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};props.pNext=&id;properties2(physical_,&props);
     if(supported&&id.deviceLUIDValid){std::memcpy(&adapterLuid_,id.deviceLUID,8);externalImages_=true;d.enabledExtensionCount=2;d.ppEnabledExtensionNames=sharingExtensions;}
    }
   }
  }
#endif
  if(vkCreateDevice_(physical_,&d,nullptr,&device_)!=VK_SUCCESS)return fatal("Vulkan logical device creation failed");
#define RA2_VK_LOAD_DEVICE(F) F##_=reinterpret_cast<PFN_##F>(vkGetDeviceProcAddr_(device_,#F));if(!F##_)return fatal("Missing Vulkan device function " #F);
  RA2_VK_DEVICE_FUNCTIONS(RA2_VK_LOAD_DEVICE)
#undef RA2_VK_LOAD_DEVICE
  vkGetDeviceQueue_(device_,queueFamily_,0,&queue_);
  VkDescriptorSetLayoutBinding bindings[3]{};for(uint32_t i=0;i<3;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout.bindingCount=3;layout.pBindings=bindings;
  if(vkCreateDescriptorSetLayout_(device_,&layout,nullptr,&descriptorLayout_)!=VK_SUCCESS)return fatal("Vulkan descriptor layout failed");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params)};
  VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&descriptorLayout_;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;
  if(vkCreatePipelineLayout_(device_,&pl,nullptr,&pipelineLayout_)!=VK_SUCCESS)return fatal("Vulkan pipeline layout failed");
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(BilinearShader);sm.pCode=BilinearShader;VkShaderModule module=VK_NULL_HANDLE;
  if(vkCreateShaderModule_(device_,&sm,nullptr,&module)!=VK_SUCCESS)return fatal("Vulkan shader module failed");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=module;cp.stage.pName="main";cp.layout=pipelineLayout_;
  VkResult result=vkCreateComputePipelines_(device_,VK_NULL_HANDLE,1,&cp,nullptr,&pipeline_);vkDestroyShaderModule_(device_,module,nullptr);
  if(result!=VK_SUCCESS)return fatal("Vulkan compute pipeline failed");
  VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=1;dp.pPoolSizes=&poolSize;
  if(vkCreateDescriptorPool_(device_,&dp,nullptr,&descriptorPool_)!=VK_SUCCESS)return fatal("Vulkan descriptor pool failed");
  VkDescriptorSetAllocateInfo da{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};da.descriptorPool=descriptorPool_;da.descriptorSetCount=1;da.pSetLayouts=&descriptorLayout_;
  if(vkAllocateDescriptorSets_(device_,&da,&descriptors_)!=VK_SUCCESS)return fatal("Vulkan descriptor allocation failed");
  VkCommandPoolCreateInfo pc{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pc.queueFamilyIndex=queueFamily_;
  if(vkCreateCommandPool_(device_,&pc,nullptr,&commandPool_)!=VK_SUCCESS)return fatal("Vulkan command pool failed");
  VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commandPool_;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
  if(vkAllocateCommandBuffers_(device_,&ca,&command_)!=VK_SUCCESS)return fatal("Vulkan command allocation failed");
  VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};if(vkCreateFence_(device_,&fi,nullptr,&fence_)!=VK_SUCCESS)return fatal("Vulkan fence failed");
  if(!buffer(atlas_,AtlasBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false,true))return fatal("Vulkan atlas allocation failed");
  diagnostics_.available=true;diagnostics_.reason.clear();return true;
 }
 static bool same(const Tile& tile,const std::shared_ptr<const std::vector<uint32_t>>& owner){return tile.address==owner.get()&&!tile.owner.owner_before(owner)&&!owner.owner_before(tile.owner);}
 static Coordinate coordinate(double p,int size){p=std::clamp(p,0.,double(size-1));uint32_t a=uint32_t(p);return {a,std::min(a+1,uint32_t(size-1)),uint32_t((p-a)*256+.5),0};}
public:
 static bool driverQuarantined(){return quarantined().load(std::memory_order_relaxed);}
 Resolve()=default;Resolve(const Resolve&)=delete;Resolve& operator=(const Resolve&)=delete;
 ~Resolve(){close();}
 const Diagnostics& diagnostics()const{return diagnostics_;}
 bool render(int width,int height,const std::vector<TileJob>& jobs,std::vector<uint32_t>& out){
  const auto started=std::chrono::steady_clock::now();
  diagnostics_.uploads=diagnostics_.uploadedBytes=diagnostics_.dispatches=diagnostics_.readbacks=diagnostics_.xbrzBuilds=diagnostics_.voxelRasters=0;diagnostics_.prepareMilliseconds=diagnostics_.waitMilliseconds=diagnostics_.readbackMilliseconds=0;
  if(disabled_)return false;
  if(width<1||height<1||width>8192||height>8192||uint64_t(width)*height>MaxPixels||jobs.empty()||jobs.size()>4096)return fail("Invalid Vulkan output dimensions");
  uint64_t coordinateCount=0;
  for(const auto& j:jobs){
   if(bool(j.nearestXs)!=bool(j.nearestYs)||(j.nearestXs&&(!j.nearest||j.nearestXs->size()!=size_t(width)||j.nearestYs->size()!=size_t(height))))return fail("Invalid nearest GPU tile axes");
   if(j.reconstructionFactor)return fail("Raw xBRZ work requires FrameResolve");
   if(!j.pixels||j.sourceWidth<1||j.sourceHeight<1||j.sourceWidth>8192||j.sourceHeight>8192||uint64_t(j.sourceWidth)*j.sourceHeight!=j.pixels->size()||j.pixels->size()>AtlasBytes/4||j.width<1||j.height<1||j.left<0||j.top<0||int64_t(j.left)+j.width>width||int64_t(j.top)+j.height>height||!std::isfinite(j.sourceX)||!std::isfinite(j.sourceY)||!std::isfinite(j.stepX)||!std::isfinite(j.stepY)||std::abs(j.sourceX)>1e10||std::abs(j.sourceY)>1e10||std::abs(j.stepX)>1e6||std::abs(j.stepY)>1e6)return fail("Invalid Vulkan tile");
   coordinateCount+=uint64_t(j.width)+j.height;
  }
  if(coordinateCount>MaxCoordinates)return fail("Vulkan coordinate budget exceeded");
  // Disjoint destination rectangles avoid undefined simultaneous shader writes.
  for(size_t i=0;i<jobs.size();++i)for(size_t k=0;k<i;++k)if(jobs[i].left<jobs[k].left+jobs[k].width&&jobs[k].left<jobs[i].left+jobs[i].width&&jobs[i].top<jobs[k].top+jobs[k].height&&jobs[k].top<jobs[i].top+jobs[i].height)return fail("Overlapping Vulkan tile destinations");
  if(!initialize())return false;
  std::vector<Tile> active;std::vector<size_t> indices;std::vector<size_t> dirty;
  active.reserve(jobs.size());indices.reserve(jobs.size());uint64_t activeWords=0;
  for(const auto& j:jobs){size_t index=0;for(;index<active.size();++index)if(same(active[index],j.pixels))break;
   if(index==active.size()){Tile tile;tile.owner=j.pixels;tile.address=j.pixels.get();tile.count=uint32_t(j.pixels->size());tile.used=++serial_;activeWords+=tile.count;active.push_back(tile);}indices.push_back(index);
  }
  if(activeWords>AtlasBytes/4)return fail("Vulkan visible tile budget exceeded");
  tiles_.erase(std::remove_if(tiles_.begin(),tiles_.end(),[](const Tile& t){return t.owner.expired();}),tiles_.end());
  std::vector<Tile> retained=tiles_;bool repack=false;
  for(size_t a=0;a<active.size();++a){auto found=std::find_if(retained.begin(),retained.end(),[&](const Tile&t){return same(t,active[a].owner.lock());});
   if(found!=retained.end()){active[a].offset=found->offset;found->used=active[a].used;continue;}
   std::sort(retained.begin(),retained.end(),[](const Tile&a,const Tile&b){return a.offset<b.offset;});uint64_t offset=0;
   for(const auto& t:retained){if(offset+active[a].count<=t.offset)break;offset=uint64_t(t.offset)+t.count;}
   if(offset+active[a].count>AtlasBytes/4){repack=true;break;}
   active[a].offset=uint32_t(offset);retained.push_back(active[a]);dirty.push_back(a);
  }
  if(repack){retained=active;dirty.clear();uint32_t offset=0;for(size_t a=0;a<active.size();++a){active[a].offset=offset;retained[a].offset=offset;offset+=active[a].count;dirty.push_back(a);}}
  const VkDeviceSize outputBytes=VkDeviceSize(width)*height*4,coordinateBytes=coordinateCount*sizeof(Coordinate);
  VkDeviceSize uploadBytes=0;for(size_t a:dirty)uploadBytes+=VkDeviceSize(active[a].count)*4;
  if(!buffer(coordinates_,coordinateBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true)||!buffer(output_,outputBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false)||!buffer(readback_,outputBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true)||(uploadBytes&&!buffer(upload_,uploadBytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true)))return fatal("Vulkan frame buffer allocation failed; CPU fallback");
  std::vector<Dispatch> dispatches;dispatches.reserve(jobs.size());auto* coords=static_cast<Coordinate*>(coordinates_.mapped);uint32_t next=0;
  for(size_t i=0;i<jobs.size();++i){const auto&j=jobs[i];Params p{active[indices[i]].offset,uint32_t(j.sourceWidth),uint32_t(width),uint32_t(j.left),uint32_t(j.top),uint32_t(j.width),uint32_t(j.height),next,0};
   for(int x=0;x<j.width;++x){double at=j.sourceX+(j.nearestXs?double((*j.nearestXs)[j.left+x]):x*j.stepX);coords[next++]=coordinate(j.nearest?std::floor(at):at,j.sourceWidth);}p.yOffset=next;
   for(int y=0;y<j.height;++y){double at=j.sourceY+(j.nearestYs?double((*j.nearestYs)[j.top+y]):y*j.stepY);coords[next++]=coordinate(j.nearest?std::floor(at):at,j.sourceHeight);}dispatches.push_back({p});
  }
  VkDeviceSize uploadOffset=0;std::vector<VkBufferCopy> copies;copies.reserve(dirty.size());
  for(size_t a:dirty){auto owner=active[a].owner.lock();if(!owner)return fail("Vulkan tile expired");VkDeviceSize bytes=VkDeviceSize(active[a].count)*4;std::memcpy(static_cast<char*>(upload_.mapped)+uploadOffset,owner->data(),size_t(bytes));copies.push_back({uploadOffset,VkDeviceSize(active[a].offset)*4,bytes});uploadOffset+=bytes;}
  if(!flush(coordinates_)||(uploadBytes&&!flush(upload_)))return fatal("Vulkan host upload flush failed");
  VkDescriptorBufferInfo infos[3]{{atlas_.buffer,0,AtlasBytes},{coordinates_.buffer,0,coordinateBytes},{output_.buffer,0,outputBytes}};VkWriteDescriptorSet writes[3]{};
  for(uint32_t i=0;i<3;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=descriptors_;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];}
  vkUpdateDescriptorSets_(device_,3,writes,0,nullptr);
  if(vkResetCommandPool_(device_,commandPool_,0)!=VK_SUCCESS||vkResetFences_(device_,1,&fence_)!=VK_SUCCESS)return fatal("Vulkan command reset failed");
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  if(vkBeginCommandBuffer_(command_,&begin)!=VK_SUCCESS)return fatal("Vulkan command recording failed");
  if(!copies.empty())vkCmdCopyBuffer_(command_,upload_.buffer,atlas_.buffer,uint32_t(copies.size()),copies.data());
  vkCmdFillBuffer_(command_,output_.buffer,0,outputBytes,0);
  VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
  vkCmdBindPipeline_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);vkCmdBindDescriptorSets_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout_,0,1,&descriptors_,0,nullptr);
  for(const auto& dispatch:dispatches){vkCmdPushConstants_(command_,pipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params),&dispatch.params);vkCmdDispatch_(command_,(dispatch.params.width+7)/8,(dispatch.params.height+7)/8,1);}
  VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&after,0,nullptr,0,nullptr);
  VkBufferCopy resultCopy{0,0,outputBytes};vkCmdCopyBuffer_(command_,output_.buffer,readback_.buffer,1,&resultCopy);
  VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);
  if(vkEndCommandBuffer_(command_)!=VK_SUCCESS)return fatal("Vulkan command completion failed");
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;
  if(vkQueueSubmit_(queue_,1,&submit,fence_)!=VK_SUCCESS)return fatal("Vulkan queue submission failed");inFlight_=true;
  // A bounded wait prevents a driver stall from blocking scene publication.
  // This is a safety ceiling, not a promised frame budget or measured latency.
  // Software Vulkan exists only in the explicit CI build; shader JIT is not
  // hardware performance evidence and gets a separate test-only wait budget.
  const uint64_t waitBudget=AllowSoftware&&diagnostics_.softwareDevice?10000000000ull:100000000ull;
  const auto waitStarted=std::chrono::steady_clock::now();
  diagnostics_.prepareMilliseconds=std::chrono::duration<double,std::milli>(waitStarted-started).count();
  VkResult waited=vkWaitForFences_(device_,1,&fence_,VK_TRUE,waitBudget);
  diagnostics_.waitMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-waitStarted).count();
  if(waited!=VK_SUCCESS){quarantined().store(true,std::memory_order_relaxed);return fatal(waited==VK_TIMEOUT?"Vulkan compute timed out; CPU fallback":"Vulkan device lost; CPU fallback");}inFlight_=false;
  const auto copyStarted=std::chrono::steady_clock::now();
  if(!readback_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=readback_.memory;range.size=VK_WHOLE_SIZE;if(vkInvalidateMappedMemoryRanges_(device_,1,&range)!=VK_SUCCESS)return fatal("Vulkan readback invalidation failed");}
  out.resize(size_t(width)*height);std::memcpy(out.data(),readback_.mapped,size_t(outputBytes));tiles_=std::move(retained);
  diagnostics_.readbackMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-copyStarted).count();
  ++diagnostics_.frames;diagnostics_.uploads=copies.size();diagnostics_.uploadedBytes=uploadBytes;diagnostics_.dispatches=dispatches.size();diagnostics_.readbacks=1;diagnostics_.reason.clear();diagnostics_.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return true;
 }
};
#undef RA2_VK_INSTANCE_FUNCTIONS
#undef RA2_VK_DEVICE_FUNCTIONS
}}
