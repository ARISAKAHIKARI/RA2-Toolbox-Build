#pragma once
#include "vulkan_compute.h"
#include "../../shared/source/gpu_surface.h"
#ifdef _WIN32
#include "../../shared/source/gpu_surface_win32.h"
#endif
namespace external {namespace vulkancompute {
// Vulkan -> shared D3D11 texture -> DirectComposition. No host mapping or
// image readback. Slots remain immutable until every published frame releases.
class DirectOutput {
 Resolve&core_;
#ifdef _WIN32
 struct Slot {std::shared_ptr<gpupresent::SharedImage> source;VkImage image=VK_NULL_HANDLE;VkDeviceMemory memory=VK_NULL_HANDLE;bool initialized=false;};
 std::vector<std::unique_ptr<Slot>> slots_;Slot*pending_=nullptr;std::shared_ptr<gpupresent::Device> device_;
 bool tried_=false,ready_=false;uint64_t sequence_=0;
#define RA2_VK_IMAGE_FUNCTIONS(X) X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) X(vkCmdCopyBufferToImage) X(vkGetMemoryWin32HandlePropertiesKHR)
#define RA2_VK_IMAGE_DECLARE(F) PFN_##F F##_=nullptr;
 RA2_VK_IMAGE_FUNCTIONS(RA2_VK_IMAGE_DECLARE)
#undef RA2_VK_IMAGE_DECLARE
 VkWin32KeyedMutexAcquireReleaseInfoKHR sync_{VK_STRUCTURE_TYPE_WIN32_KEYED_MUTEX_ACQUIRE_RELEASE_INFO_KHR};
 uint64_t acquire_=0,release_=1;uint32_t timeout_=0;
 void destroy(Slot&s){if(s.image&&vkDestroyImage_)vkDestroyImage_(core_.device_,s.image,nullptr);if(s.memory)core_.vkFreeMemory_(core_.device_,s.memory,nullptr);s.image=VK_NULL_HANDLE;s.memory=VK_NULL_HANDLE;}
 bool initialize(){
  if(tried_)return ready_;tried_=true;if(!core_.externalImages_)return false;
#define RA2_VK_IMAGE_LOAD(F) F##_=reinterpret_cast<PFN_##F>(core_.vkGetDeviceProcAddr_(core_.device_,#F));if(!F##_)return false;
  RA2_VK_IMAGE_FUNCTIONS(RA2_VK_IMAGE_LOAD)
#undef RA2_VK_IMAGE_LOAD
  auto query=reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(core_.vkGetInstanceProcAddr_(core_.instance_,"vkGetPhysicalDeviceImageFormatProperties2"));if(!query)return false;
  VkPhysicalDeviceExternalImageFormatInfo external{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO};external.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
  VkPhysicalDeviceImageFormatInfo2 info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};info.pNext=&external;info.format=VK_FORMAT_B8G8R8A8_UNORM;info.type=VK_IMAGE_TYPE_2D;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
  VkExternalImageFormatProperties externalProps{VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES};VkImageFormatProperties2 props{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};props.pNext=&externalProps;
  if(query(core_.physical_,&info,&props)!=VK_SUCCESS||!(externalProps.externalMemoryProperties.externalMemoryFeatures&VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))return false;
  device_=gpupresent::device(core_.adapterLuid_);return ready_=bool(device_);
 }
 std::unique_ptr<Slot> create(int w,int h){
  auto out=std::make_unique<Slot>();out->source=gpupresent::SharedImage::create(device_,w,h);if(!out->source)return {};
  VkExternalMemoryImageCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};external.handleTypes=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
  VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};info.pNext=&external;info.imageType=VK_IMAGE_TYPE_2D;info.format=VK_FORMAT_B8G8R8A8_UNORM;info.extent={uint32_t(w),uint32_t(h),1};info.mipLevels=info.arrayLayers=1;info.samples=VK_SAMPLE_COUNT_1_BIT;info.tiling=VK_IMAGE_TILING_OPTIMAL;info.usage=VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;info.sharingMode=VK_SHARING_MODE_EXCLUSIVE;info.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
  if(vkCreateImage_(core_.device_,&info,nullptr,&out->image)!=VK_SUCCESS)return {};
  VkMemoryRequirements requirements{};core_.memoryDiagnostics();vkGetImageMemoryRequirements_(core_.device_,out->image,&requirements);
  VkMemoryWin32HandlePropertiesKHR properties{VK_STRUCTURE_TYPE_MEMORY_WIN32_HANDLE_PROPERTIES_KHR};
  if(vkGetMemoryWin32HandlePropertiesKHR_(core_.device_,VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT,out->source->handle,&properties)!=VK_SUCCESS){destroy(*out);return {};}
  const uint32_t bits=requirements.memoryTypeBits&properties.memoryTypeBits;uint32_t type=0;while(type<core_.memory_.memoryTypeCount&&!(bits&(1u<<type)))++type;
  if(type==core_.memory_.memoryTypeCount){destroy(*out);return {};}
  VkImportMemoryWin32HandleInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR};import.handleType=VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;import.handle=out->source->handle;
  VkMemoryDedicatedAllocateInfo dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO};dedicated.pNext=&import;dedicated.image=out->image;
  VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};allocation.pNext=&dedicated;allocation.allocationSize=requirements.size;allocation.memoryTypeIndex=type;
  if(core_.vkAllocateMemory_(core_.device_,&allocation,nullptr,&out->memory)!=VK_SUCCESS||vkBindImageMemory_(core_.device_,out->image,out->memory,0)!=VK_SUCCESS){destroy(*out);return {};}
  return out;
 }
#endif
public:
 explicit DirectOutput(Resolve&core):core_(core){}
 ~DirectOutput(){close();}
 void close(){
#ifdef _WIN32
  if(core_.inFlight_&&core_.vkGetFenceStatus_&&core_.vkGetFenceStatus_(core_.device_,core_.fence_)!=VK_SUCCESS)return;
  for(auto&s:slots_)destroy(*s);slots_.clear();pending_=nullptr;device_.reset();
#endif
 }
 bool prepare(int w,int h){
#ifdef _WIN32
  pending_=nullptr;if(!gpupresent::available()||!initialize())return false;
  for(auto i=slots_.begin();i!=slots_.end();){auto&s=**i;if(s.source.use_count()==1&&(s.source->width!=w||s.source->height!=h)){destroy(s);i=slots_.erase(i);}else ++i;}
  for(auto&s:slots_)if(s->source.use_count()==1&&s->source->width==w&&s->source->height==h&&s->source->writable()){pending_=s.get();return true;}
  if(slots_.size()>=6)return false;auto next=create(w,h);if(!next)return false;pending_=next.get();slots_.push_back(std::move(next));return true;
#else
  (void)w;(void)h;return false;
#endif
 }
 void record(VkCommandBuffer command,VkBuffer source){
#ifdef _WIN32
  auto&s=*pending_;VkImageMemoryBarrier acquire{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};acquire.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;acquire.oldLayout=s.initialized?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_UNDEFINED;acquire.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;acquire.srcQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;acquire.dstQueueFamilyIndex=core_.queueFamily_;acquire.image=s.image;acquire.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
  core_.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&acquire);
  VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={uint32_t(s.source->width),uint32_t(s.source->height),1};vkCmdCopyBufferToImage_(command,source,s.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
  VkImageMemoryBarrier release=acquire;release.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;release.dstAccessMask=0;release.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;release.newLayout=VK_IMAGE_LAYOUT_GENERAL;release.srcQueueFamilyIndex=core_.queueFamily_;release.dstQueueFamilyIndex=VK_QUEUE_FAMILY_EXTERNAL;
  core_.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&release);
#else
  (void)command;(void)source;
#endif
 }
 void submission(VkSubmitInfo&submit){
#ifdef _WIN32
  sync_.acquireCount=sync_.releaseCount=1;sync_.pAcquireSyncs=sync_.pReleaseSyncs=&pending_->memory;sync_.pAcquireKeys=&acquire_;sync_.pAcquireTimeouts=&timeout_;sync_.pReleaseKeys=&release_;submit.pNext=&sync_;
#else
  (void)submit;
#endif
 }
 std::shared_ptr<const gpupresent::Frame> completed(){
#ifdef _WIN32
  auto&s=*pending_;s.initialized=true;s.source->produced=true;s.source->sequence=++sequence_;auto out=std::make_shared<gpupresent::Frame>();out->resource=s.source;pending_=nullptr;return out;
#else
  return {};
#endif
 }
};
}}
