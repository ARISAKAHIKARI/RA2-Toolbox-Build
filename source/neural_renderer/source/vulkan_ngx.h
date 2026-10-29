#pragma once
// SPDX-License-Identifier: MIT
// A separate SDK consumer: no xBRZ, game runtime, injection or host UI code.
#define VK_NO_PROTOTYPES
#include "../../third_party/vulkan/vulkan_core.h"
#include <nvsdk_ngx_vk.h>
#include <nvsdk_ngx_helpers_dlssnr_vk.h>
#include "../../shared/source/neural_frame_protocol.h"
#include "pixel_codec.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
namespace neuralbackend {
class VulkanNGX {
 struct Image {VkImage image=VK_NULL_HANDLE;VkImageView view=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;VkFormat format=VK_FORMAT_UNDEFINED;};
 struct Buffer {VkBuffer buffer=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;void*mapped=nullptr;VkDeviceSize bytes=0;bool coherent=false;};
 HMODULE loader_=nullptr;VkInstance instance_=VK_NULL_HANDLE;VkPhysicalDevice physical_=VK_NULL_HANDLE;VkDevice device_=VK_NULL_HANDLE;VkQueue queue_=VK_NULL_HANDLE;
 VkPhysicalDeviceMemoryProperties memory_{};VkCommandPool pool_=VK_NULL_HANDLE;VkCommandBuffer command_=VK_NULL_HANDLE;VkFence fence_=VK_NULL_HANDLE;
 NVSDK_NGX_Parameter*parameters_=nullptr;NVSDK_NGX_Handle*feature_=nullptr;
 bool initialized_=false,attempted_=false,ngxStarted_=false,inFlight_=false,broken_=false;uint32_t width_=0,height_=0;uint64_t generation_=0;
 Image color_,output_,motion_,depth_,control_;Buffer upload_,readback_;
 std::wstring base_,runtime_,logs_;std::string reason_;neuralframe::Status status_=neuralframe::Status::BackendFailed;uint32_t code_=0;
 static constexpr char ProjectID[]="356dfc95-3629-45e7-b768-1ff30ead320b";
#define NR_VK_FUNCTIONS(X) \
 X(vkGetInstanceProcAddr) X(vkCreateInstance) X(vkEnumerateInstanceExtensionProperties) \
 X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceFeatures2) X(vkEnumerateDeviceExtensionProperties) X(vkCreateDevice) X(vkGetDeviceProcAddr) \
 X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkAllocateCommandBuffers) X(vkResetCommandPool) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) X(vkCreateFence) X(vkDestroyFence) X(vkResetFences) X(vkQueueSubmit) X(vkWaitForFences) \
 X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCreateImageView) X(vkDestroyImageView) X(vkGetPhysicalDeviceFormatProperties) \
 X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) X(vkAllocateMemory) X(vkFreeMemory) X(vkMapMemory) X(vkUnmapMemory) X(vkFlushMappedMemoryRanges) X(vkInvalidateMappedMemoryRanges) X(vkCmdCopyBufferToImage) X(vkCmdCopyImageToBuffer) X(vkCmdPipelineBarrier)
#define NR_DECLARE(F) PFN_##F F##_=nullptr;
 NR_VK_FUNCTIONS(NR_DECLARE)
#undef NR_DECLARE
 bool fail(const char*reason,neuralframe::Status status=neuralframe::Status::BackendFailed,uint32_t code=0){reason_=reason;status_=status;code_=code;return false;}
 bool ngx(NVSDK_NGX_Result result,const char*reason){return NVSDK_NGX_SUCCEED(result)||fail(reason,neuralframe::Status::BackendFailed,uint32_t(result));}
 int memoryType(uint32_t bits,VkMemoryPropertyFlags required,VkMemoryPropertyFlags preferred=0){int best=-1,score=-1;for(uint32_t i=0;i<memory_.memoryTypeCount;++i)if((bits&(1u<<i))&&(memory_.memoryTypes[i].propertyFlags&required)==required){int s=(memory_.memoryTypes[i].propertyFlags&preferred)==preferred?1:0;if(s>score){score=s;best=int(i);}}return best;}
 void destroy(Image&i){if(i.view)vkDestroyImageView_(device_,i.view,nullptr);if(i.image)vkDestroyImage_(device_,i.image,nullptr);if(i.memory)vkFreeMemory_(device_,i.memory,nullptr);i={};}
 void destroy(Buffer&b){if(b.mapped)vkUnmapMemory_(device_,b.memory);if(b.buffer)vkDestroyBuffer_(device_,b.buffer,nullptr);if(b.memory)vkFreeMemory_(device_,b.memory,nullptr);b={};}
 void releaseFrame(){
  if(!device_)return;if(inFlight_)return;
  if(feature_){NVSDK_NGX_VULKAN_ReleaseFeature(feature_);feature_=nullptr;}
  for(auto*p:{&color_,&output_,&motion_,&depth_,&control_})destroy(*p);destroy(upload_);destroy(readback_);width_=height_=0;
 }
 bool buffer(Buffer&b,VkDeviceSize bytes,VkBufferUsageFlags usage){
  VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};info.size=bytes;info.usage=usage;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
  if(vkCreateBuffer_(device_,&info,nullptr,&b.buffer)!=VK_SUCCESS)return fail("DLSS 5 transfer buffer allocation failed");VkMemoryRequirements req{};vkGetBufferMemoryRequirements_(device_,b.buffer,&req);
  int type=memoryType(req.memoryTypeBits,VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);if(type<0)return fail("DLSS 5 host-visible memory is unavailable");
  VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=req.size;allocate.memoryTypeIndex=uint32_t(type);
  if(vkAllocateMemory_(device_,&allocate,nullptr,&b.memory)!=VK_SUCCESS||vkBindBufferMemory_(device_,b.buffer,b.memory,0)!=VK_SUCCESS||vkMapMemory_(device_,b.memory,0,VK_WHOLE_SIZE,0,&b.mapped)!=VK_SUCCESS)return fail("DLSS 5 transfer memory allocation failed");
  b.bytes=bytes;b.coherent=(memory_.memoryTypes[type].propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)!=0;return true;
 }
 bool image(Image&i,VkFormat format,VkImageUsageFlags usage){
  VkFormatProperties properties{};vkGetPhysicalDeviceFormatProperties_(physical_,format,&properties);if(!(properties.optimalTilingFeatures&VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))return fail("DLSS 5 image format is unsupported",neuralframe::Status::Unsupported);
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.imageType=VK_IMAGE_TYPE_2D;info.format=format;info.extent={width_,height_,1};info.mipLevels=1;info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=usage|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
  if(vkCreateImage_(device_,&info,nullptr,&i.image)!=VK_SUCCESS)return fail("DLSS 5 image allocation failed");VkMemoryRequirements req{};vkGetImageMemoryRequirements_(device_,i.image,&req);int type=memoryType(req.memoryTypeBits,0,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);if(type<0)return fail("DLSS 5 image memory type unavailable");
  VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocate.allocationSize=req.size;allocate.memoryTypeIndex=uint32_t(type);if(vkAllocateMemory_(device_,&allocate,nullptr,&i.memory)!=VK_SUCCESS||vkBindImageMemory_(device_,i.image,i.memory,0)!=VK_SUCCESS)return fail("DLSS 5 image memory allocation failed");
  VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};view.image=i.image;view.viewType=VK_IMAGE_VIEW_TYPE_2D;view.format=format;view.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};if(vkCreateImageView_(device_,&view,nullptr,&i.view)!=VK_SUCCESS)return fail("DLSS 5 image view allocation failed");i.format=format;return true;
 }
 bool begin(){if(vkResetCommandPool_(device_,pool_,0)!=VK_SUCCESS)return fail("DLSS 5 command pool reset failed");VkCommandBufferBeginInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};info.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;return vkBeginCommandBuffer_(command_,&info)==VK_SUCCESS||fail("DLSS 5 command recording failed");}
 bool submit(){
  if(vkEndCommandBuffer_(command_)!=VK_SUCCESS||vkResetFences_(device_,1,&fence_)!=VK_SUCCESS)return fail("DLSS 5 command finalization failed");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;
  if(vkQueueSubmit_(queue_,1,&submit,fence_)!=VK_SUCCESS)return fail("DLSS 5 GPU submission failed");inFlight_=true;VkResult result=vkWaitForFences_(device_,1,&fence_,VK_TRUE,4000000000ull);
  if(result!=VK_SUCCESS){broken_=true;return fail("DLSS 5 GPU fence timed out or device failed",neuralframe::Status::BackendFailed,uint32_t(result));}inFlight_=false;return true;
 }
 void barrier(Image&i,VkImageLayout before,VkImageLayout after,VkAccessFlags src,VkAccessFlags dst,VkPipelineStageFlags from,VkPipelineStageFlags to){VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};b.srcAccessMask=src;b.dstAccessMask=dst;b.oldLayout=before;b.newLayout=after;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.image=i.image;b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};vkCmdPipelineBarrier_(command_,from,to,0,0,nullptr,0,nullptr,1,&b);}
 NVSDK_NGX_Resource_VK resource(const Image&i,bool writable){VkImageSubresourceRange sub{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};return NVSDK_NGX_Create_ImageView_Resource_VK(i.view,i.image,sub,i.format,width_,height_,writable);}
public:
 VulkanNGX(){wchar_t path[32768]{};DWORD size=GetModuleFileNameW(nullptr,path,32768);if(size&&size<32768){base_.assign(path,size);auto slash=base_.find_last_of(L"\\/");if(slash!=std::wstring::npos)base_.resize(slash);}runtime_=base_+L"\\neural-runtime";logs_=base_+L"\\NeuralLogs";}
 ~VulkanNGX(){
  // A timed-out queue is owned by this isolated process. Do not destroy GPU
  // resources still in use; process teardown, or the parent's job, reclaims it.
  if(inFlight_)return;releaseFrame();if(parameters_)NVSDK_NGX_VULKAN_DestroyParameters(parameters_);if(ngxStarted_)NVSDK_NGX_VULKAN_Shutdown1(device_);
  if(device_){if(fence_&&vkDestroyFence_)vkDestroyFence_(device_,fence_,nullptr);if(pool_&&vkDestroyCommandPool_)vkDestroyCommandPool_(device_,pool_,nullptr);if(vkDestroyDevice_)vkDestroyDevice_(device_,nullptr);}if(instance_&&vkDestroyInstance_)vkDestroyInstance_(instance_,nullptr);if(loader_)FreeLibrary(loader_);
 }
 const std::string&reason()const{return reason_;}neuralframe::Status status()const{return status_;}uint32_t code()const{return code_;}bool broken()const{return broken_;}
 bool initialize(){
  if(attempted_)return initialized_;attempted_=true;
  if(GetFileAttributesW((runtime_+L"\\nvngx_dlssnr.dll").c_str())==INVALID_FILE_ATTRIBUTES)return fail("DLSS 5 runtime is not installed in neural-runtime",neuralframe::Status::NoRuntime);
  wchar_t system[32768]{};UINT size=GetSystemDirectoryW(system,32768);if(!size||size>=32750)return fail("Windows system directory is unavailable");std::wstring loaderPath(system,size);loaderPath+=L"\\vulkan-1.dll";loader_=LoadLibraryW(loaderPath.c_str());if(!loader_)return fail("The system Vulkan loader is unavailable",neuralframe::Status::Unsupported);
  vkGetInstanceProcAddr_=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader_,"vkGetInstanceProcAddr"));if(!vkGetInstanceProcAddr_)return fail("Vulkan entry point unavailable");
  vkCreateInstance_=reinterpret_cast<PFN_vkCreateInstance>(vkGetInstanceProcAddr_(VK_NULL_HANDLE,"vkCreateInstance"));if(!vkCreateInstance_)return fail("Vulkan instance entry point unavailable");
  CreateDirectoryW(logs_.c_str(),nullptr);const wchar_t*featurePath=runtime_.c_str();NVSDK_NGX_FeatureCommonInfo featureInfo{};featureInfo.PathListInfo.Path=&featurePath;featureInfo.PathListInfo.Length=1;
  NVSDK_NGX_FeatureDiscoveryInfo discovery{};discovery.SDKVersion=NVSDK_NGX_Version_API;discovery.FeatureID=NVSDK_NGX_Feature_DLSSNR;discovery.Identifier.IdentifierType=NVSDK_NGX_Application_Identifier_Type_Project_Id;discovery.Identifier.v.ProjectDesc={ProjectID,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"1.0.39"};discovery.ApplicationDataPath=logs_.c_str();discovery.FeatureInfo=&featureInfo;
  uint32_t count=0;VkExtensionProperties*extensions=nullptr;
  if(!ngx(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&discovery,&count,&extensions),"DLSS 5 NGX instance requirements are unavailable")||count>128)return false;
  std::vector<const char*>instanceExtensions;for(uint32_t i=0;i<count;++i)instanceExtensions.push_back(extensions[i].extensionName);
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="RA2 Toolbox Neural Renderer";app.apiVersion=VK_API_VERSION_1_2;
  VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};create.pApplicationInfo=&app;create.enabledExtensionCount=uint32_t(instanceExtensions.size());create.ppEnabledExtensionNames=instanceExtensions.data();if(vkCreateInstance_(&create,nullptr,&instance_)!=VK_SUCCESS)return fail("Vulkan 1.2 with DLSS 5 requirements is unavailable",neuralframe::Status::Unsupported);
#define NR_LOAD_INSTANCE(F) F##_=reinterpret_cast<PFN_##F>(vkGetInstanceProcAddr_(instance_,#F));if(!F##_)return fail("Required Vulkan instance function missing: " #F);
  NR_LOAD_INSTANCE(vkDestroyInstance) NR_LOAD_INSTANCE(vkEnumeratePhysicalDevices) NR_LOAD_INSTANCE(vkGetPhysicalDeviceProperties) NR_LOAD_INSTANCE(vkGetPhysicalDeviceMemoryProperties) NR_LOAD_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties) NR_LOAD_INSTANCE(vkGetPhysicalDeviceFeatures2) NR_LOAD_INSTANCE(vkEnumerateDeviceExtensionProperties) NR_LOAD_INSTANCE(vkCreateDevice) NR_LOAD_INSTANCE(vkGetDeviceProcAddr) NR_LOAD_INSTANCE(vkGetPhysicalDeviceFormatProperties)
#undef NR_LOAD_INSTANCE
  count=0;if(vkEnumeratePhysicalDevices_(instance_,&count,nullptr)!=VK_SUCCESS||!count||count>128)return fail("No Vulkan device is available",neuralframe::Status::Unsupported);std::vector<VkPhysicalDevice>physical(count);if(vkEnumeratePhysicalDevices_(instance_,&count,physical.data())!=VK_SUCCESS)return fail("Vulkan device enumeration failed");
  physical.resize(count);uint32_t queueFamily=0;
  for(auto candidate:physical){VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties_(candidate,&properties);if(properties.vendorID!=0x10de||properties.apiVersion<VK_API_VERSION_1_2)continue;NVSDK_NGX_FeatureRequirement requirement{};auto result=NVSDK_NGX_VULKAN_GetFeatureRequirements(instance_,candidate,&discovery,&requirement);if(NVSDK_NGX_FAILED(result)||requirement.FeatureSupported!=NVSDK_NGX_FeatureSupportResult_Supported)continue;
   uint32_t queues=0;vkGetPhysicalDeviceQueueFamilyProperties_(candidate,&queues,nullptr);if(!queues||queues>1024)continue;std::vector<VkQueueFamilyProperties>propertiesQueue(queues);vkGetPhysicalDeviceQueueFamilyProperties_(candidate,&queues,propertiesQueue.data());for(uint32_t i=0;i<queues;++i)if(propertiesQueue[i].queueCount&&(propertiesQueue[i].queueFlags&(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT))==(VK_QUEUE_GRAPHICS_BIT|VK_QUEUE_COMPUTE_BIT)){physical_=candidate;queueFamily=i;break;}if(physical_)break;
  }
  if(!physical_)return fail("No driver-supported DLSS 5 Neural Rendering device",neuralframe::Status::Unsupported);
  count=0;extensions=nullptr;if(!ngx(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance_,physical_,&discovery,&count,&extensions),"DLSS 5 device requirements are unavailable")||count>256)return false;std::vector<const char*>deviceExtensions;for(uint32_t i=0;i<count;++i)deviceExtensions.push_back(extensions[i].extensionName);
  VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};features11.pNext=&features12;VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};features.pNext=&features11;vkGetPhysicalDeviceFeatures2_(physical_,&features);
  float priority=1;VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};queue.queueFamilyIndex=queueFamily;queue.queueCount=1;queue.pQueuePriorities=&priority;VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};device.pNext=&features11;device.pEnabledFeatures=&features.features;device.queueCreateInfoCount=1;device.pQueueCreateInfos=&queue;device.enabledExtensionCount=uint32_t(deviceExtensions.size());device.ppEnabledExtensionNames=deviceExtensions.data();if(vkCreateDevice_(physical_,&device,nullptr,&device_)!=VK_SUCCESS)return fail("DLSS 5 Vulkan device initialization failed",neuralframe::Status::Unsupported);
#define NR_LOAD_DEVICE(F) F##_=reinterpret_cast<PFN_##F>(vkGetDeviceProcAddr_(device_,#F));if(!F##_)return fail("Required Vulkan device function missing: " #F);
  NR_LOAD_DEVICE(vkDestroyDevice) NR_LOAD_DEVICE(vkGetDeviceQueue) NR_LOAD_DEVICE(vkDeviceWaitIdle) NR_LOAD_DEVICE(vkCreateCommandPool) NR_LOAD_DEVICE(vkDestroyCommandPool) NR_LOAD_DEVICE(vkAllocateCommandBuffers) NR_LOAD_DEVICE(vkResetCommandPool) NR_LOAD_DEVICE(vkBeginCommandBuffer) NR_LOAD_DEVICE(vkEndCommandBuffer) NR_LOAD_DEVICE(vkCreateFence) NR_LOAD_DEVICE(vkDestroyFence) NR_LOAD_DEVICE(vkResetFences) NR_LOAD_DEVICE(vkQueueSubmit) NR_LOAD_DEVICE(vkWaitForFences) NR_LOAD_DEVICE(vkCreateImage) NR_LOAD_DEVICE(vkDestroyImage) NR_LOAD_DEVICE(vkGetImageMemoryRequirements) NR_LOAD_DEVICE(vkBindImageMemory) NR_LOAD_DEVICE(vkCreateImageView) NR_LOAD_DEVICE(vkDestroyImageView) NR_LOAD_DEVICE(vkCreateBuffer) NR_LOAD_DEVICE(vkDestroyBuffer) NR_LOAD_DEVICE(vkGetBufferMemoryRequirements) NR_LOAD_DEVICE(vkBindBufferMemory) NR_LOAD_DEVICE(vkAllocateMemory) NR_LOAD_DEVICE(vkFreeMemory) NR_LOAD_DEVICE(vkMapMemory) NR_LOAD_DEVICE(vkUnmapMemory) NR_LOAD_DEVICE(vkFlushMappedMemoryRanges) NR_LOAD_DEVICE(vkInvalidateMappedMemoryRanges) NR_LOAD_DEVICE(vkCmdCopyBufferToImage) NR_LOAD_DEVICE(vkCmdCopyImageToBuffer) NR_LOAD_DEVICE(vkCmdPipelineBarrier)
#undef NR_LOAD_DEVICE
  vkGetDeviceQueue_(device_,queueFamily,0,&queue_);vkGetPhysicalDeviceMemoryProperties_(physical_,&memory_);
  if(!ngx(NVSDK_NGX_VULKAN_Init_with_ProjectID(ProjectID,NVSDK_NGX_ENGINE_TYPE_CUSTOM,"1.0.39",logs_.c_str(),instance_,physical_,device_,vkGetInstanceProcAddr_,vkGetDeviceProcAddr_,&featureInfo),"DLSS 5 NGX initialization failed"))return false;ngxStarted_=true;
  if(!ngx(NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters_),"DLSS 5 capabilities could not be queried"))return false;int available=0,updated=0;NVSDK_NGX_Parameter_GetI(parameters_,NVSDK_NGX_Parameter_DLSSNR_NeedsUpdatedDriver,&updated);auto result=NVSDK_NGX_Parameter_GetI(parameters_,NVSDK_NGX_Parameter_DLSSNR_Available,&available);if(NVSDK_NGX_FAILED(result)||!available||updated)return fail(updated?"DLSS 5 requires a newer NVIDIA driver":"DLSS 5 runtime is unsupported on this device",neuralframe::Status::Unsupported,uint32_t(result));
  VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pool.queueFamilyIndex=queueFamily;pool.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;if(vkCreateCommandPool_(device_,&pool,nullptr,&pool_)!=VK_SUCCESS)return fail("DLSS 5 command pool allocation failed");VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};allocate.commandPool=pool_;allocate.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;allocate.commandBufferCount=1;if(vkAllocateCommandBuffers_(device_,&allocate,&command_)!=VK_SUCCESS)return fail("DLSS 5 command buffer allocation failed");VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};if(vkCreateFence_(device_,&fence,nullptr,&fence_)!=VK_SUCCESS)return fail("DLSS 5 fence allocation failed");
  initialized_=true;status_=neuralframe::Status::Idle;reason_="DLSS 5 driver/runtime ready; no frame has been processed";return true;
 }
 bool process(const neuralframe::Header&h,const uint32_t*color,const float*motion,const float*depth,const float*control,uint32_t*out){
  if(broken_)return fail("DLSS 5 backend was stopped after a GPU failure");if(!initialize())return false;const uint64_t pixels=uint64_t(h.width)*h.height;
  if(!feature_||width_!=h.width||height_!=h.height){releaseFrame();width_=h.width;height_=h.height;
   if(!image(color_,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT)||!image(output_,VK_FORMAT_R16G16B16A16_SFLOAT,VK_IMAGE_USAGE_TRANSFER_SRC_BIT)||!image(motion_,VK_FORMAT_R32G32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT)||!image(depth_,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT)||!image(control_,VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_USAGE_TRANSFER_DST_BIT)||!buffer(upload_,pixels*24,VK_BUFFER_USAGE_TRANSFER_SRC_BIT)||!buffer(readback_,pixels*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT)||!begin())return false;
   for(auto*i:{&color_,&output_,&motion_,&depth_,&control_})barrier(*i,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,0,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   NVSDK_NGX_Parameter_SetI(parameters_,NVSDK_NGX_Parameter_DLSSNR_Hint_Render_Preset,NVSDK_NGX_DLSSNR_Hint_Render_Preset_B);NVSDK_NGX_DLSSNR_Create_Params create{width_,height_};if(!ngx(NGX_VULKAN_CREATE_DLSSNR_EXT1(device_,command_,1,1,&feature_,parameters_,&create),"DLSS 5 feature creation failed")||!submit())return false;generation_=0;
  }
  auto*upload=static_cast<uint8_t*>(upload_.mapped);auto*rgba=reinterpret_cast<uint16_t*>(upload);auto*mv=reinterpret_cast<float*>(upload+pixels*8);auto*z=reinterpret_cast<float*>(upload+pixels*16);auto*mask=upload+pixels*20;
  for(size_t i=0;i<pixels;++i){neuralpixels::unpack(color[i],rgba+i*4);mv[i*2]=motion[i*2];mv[i*2+1]=motion[i*2+1];z[i]=depth[i];uint8_t value=uint8_t(std::clamp(std::lround(control[i]*255),0l,255l));mask[i*4]=mask[i*4+1]=mask[i*4+2]=value;mask[i*4+3]=0;}
  if(!upload_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=upload_.memory;range.size=VK_WHOLE_SIZE;if(vkFlushMappedMemoryRanges_(device_,1,&range)!=VK_SUCCESS)return fail("DLSS 5 input flush failed");}
  if(!begin())return false;Image*images[]={&color_,&motion_,&depth_,&control_};uint64_t offsets[]={0,pixels*8,pixels*16,pixels*20};
  for(size_t i=0;i<4;++i){barrier(*images[i],VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);VkBufferImageCopy copy{};copy.bufferOffset=offsets[i];copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={width_,height_,1};vkCmdCopyBufferToImage_(command_,upload_.buffer,images[i]->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);barrier(*images[i],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);}
  auto inColor=resource(color_,false),outColor=resource(output_,true),inMotion=resource(motion_,false),inDepth=resource(depth_,false),inControl=resource(control_,false);
  NVSDK_NGX_VK_DLSSNR_Eval_Params eval{};eval.pInColor=&inColor;eval.pInOutput=&outColor;eval.pInMVec=&inMotion;eval.pInDepth=&inDepth;eval.pInControlMask=&inControl;eval.InLocalToneStrength=h.toneStrength;eval.InLocalStructureStrength=h.structureStrength;eval.InStyle=h.style;eval.InUseAutoMask=0;eval.InMaskBlendMode=NVSDK_NGX_DLSSNR_Mask_Blend_Mode_None;eval.InSkinStructureStrength=0;eval.InReset=(h.flags&neuralframe::ResetHistory)||generation_!=h.generation;eval.InDepthInverted=(h.flags&neuralframe::DepthInverted)?1:0;eval.InEnabled=1;eval.InIntensity=h.intensity;eval.InMVecScaleX=eval.InMVecScaleY=1;eval.InColorSubrectSize=eval.InOutputSubrectSize=eval.InMVecSubrectSize=eval.InDepthSubrectSize=eval.InControlMaskSubrectSize={width_,height_};
  NVSDK_NGX_Parameter_SetF(parameters_,NVSDK_NGX_Parameter_Jitter_Offset_X,0);NVSDK_NGX_Parameter_SetF(parameters_,NVSDK_NGX_Parameter_Jitter_Offset_Y,0);
  if(!ngx(NGX_VULKAN_EVALUATE_DLSSNR_EXT(command_,feature_,parameters_,&eval),"DLSS 5 Neural Rendering evaluation failed"))return false;
  barrier(output_,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT);VkBufferImageCopy read{};read.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};read.imageExtent={width_,height_,1};vkCmdCopyImageToBuffer_(command_,output_.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback_.buffer,1,&read);barrier(output_,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
  VkBufferMemoryBarrier host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;host.srcQueueFamilyIndex=host.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;host.buffer=readback_.buffer;host.size=VK_WHOLE_SIZE;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&host,0,nullptr);
  if(!submit())return false;if(!readback_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=readback_.memory;range.size=VK_WHOLE_SIZE;if(vkInvalidateMappedMemoryRanges_(device_,1,&range)!=VK_SUCCESS)return fail("DLSS 5 output invalidate failed");}
  auto*result=static_cast<const uint16_t*>(readback_.mapped);for(size_t i=0;i<pixels;++i)if(!neuralpixels::pack(result+i*4,color[i],control[i],out[i]))return fail("DLSS 5 output contained non-finite color values");generation_=h.generation;status_=neuralframe::Status::Completed;code_=uint32_t(NVSDK_NGX_Result_Success);reason_="DLSS 5 Neural Rendering produced this frame";return true;
 }
};
}
