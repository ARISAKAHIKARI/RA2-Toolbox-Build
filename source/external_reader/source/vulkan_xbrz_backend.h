#pragma once
#include "vulkan_compute.h"
#include "gpu_xbrz.h"
#include "gpu_xbrz_spirv.h"

namespace external { namespace gpuxbrz {
// A native immutable PARGB source and an already reserved reconstructed output
// range in the caller's color atlas. This stage never submits or reads back.
struct RawJob {
 std::shared_ptr<const std::vector<uint32_t>> pixels;
 int width=0,height=0,factor=0;
 uint32_t outputOffset=0;
};
class Stage {
 using Core=vulkancompute::Resolve;
 using Buffer=Core::Buffer;
 struct Resident {
  std::weak_ptr<const std::vector<uint32_t>> owner;
  const void* address=nullptr;
  uint32_t offset=0,words=0,width=0,height=0;
 };
 Core& core_;
 Buffer raw_,flags_,upload_;
 VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;
 VkPipelineLayout pipelineLayout_=VK_NULL_HANDLE;
 VkDescriptorPool descriptorPool_=VK_NULL_HANDLE;
 VkDescriptorSet descriptors_=VK_NULL_HANDLE;
 VkPipeline preprocess_=VK_NULL_HANDLE,scale_=VK_NULL_HANDLE;
 std::vector<Resident> resident_,pending_;
 std::vector<VkBufferCopy> copies_;
 std::vector<Params> params_;
 VkDeviceSize flagBytes_=0,uploadBytes_=0;
 bool tried_=false,ready_=false,prepared_=false;
 static constexpr VkDeviceSize AtlasBytes=96ull*1024*1024;
 static constexpr uint64_t AtlasWords=AtlasBytes/4;
 static constexpr VkDeviceSize ColorAtlasBytes=Core::AtlasBytes;
 static constexpr uint64_t ColorAtlasWords=ColorAtlasBytes/4;
 bool fail(const char* reason){return core_.fail(reason);}
 bool fatal(const char* reason){return core_.fatal(reason);}
 static bool same(const Resident& r,const RawJob& j){
  return r.address==j.pixels.get()&&r.width==uint32_t(j.width)&&r.height==uint32_t(j.height)&&
   !r.owner.owner_before(j.pixels)&&!j.pixels.owner_before(r.owner);
 }
 static bool same(const Resident& a,const Resident& b){
  return a.address==b.address&&a.width==b.width&&a.height==b.height&&
   !a.owner.owner_before(b.owner)&&!b.owner.owner_before(a.owner);
 }
 bool pipeline(const uint32_t* words,size_t bytes,VkPipeline& result){
  VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
  moduleInfo.codeSize=bytes;moduleInfo.pCode=words;VkShaderModule module=VK_NULL_HANDLE;
  if(core_.vkCreateShaderModule_(core_.device_,&moduleInfo,nullptr,&module)!=VK_SUCCESS)return fatal("GPU xBRZ shader module creation failed");
  VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  info.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;info.stage.module=module;info.stage.pName="main";info.layout=pipelineLayout_;
  const auto status=core_.vkCreateComputePipelines_(core_.device_,VK_NULL_HANDLE,1,&info,nullptr,&result);
  core_.vkDestroyShaderModule_(core_.device_,module,nullptr);
  return status==VK_SUCCESS||fatal("GPU xBRZ compute pipeline creation failed");
 }
public:
 explicit Stage(Core& core):core_(core){}
 Stage(const Stage&)=delete;Stage& operator=(const Stage&)=delete;
 ~Stage(){close();}
 bool available()const{return ready_&&!core_.disabled_;}
 uint64_t uploads()const{return copies_.size();}
 uint64_t uploadedBytes()const{return uploadBytes_;}
 uint64_t builds()const{return params_.size();}
 uint64_t dispatches()const{return params_.size()*2;}
 bool initialize(){
  if(ready_)return !core_.disabled_;
  if(tried_)return false;tried_=true;
  if(!core_.initialize())return false;
  if(!core_.features_.shaderFloat64)return fail("GPU xBRZ requires shaderFloat64; CPU fallback");
  if(core_.properties_.limits.maxStorageBufferRange<AtlasBytes||core_.properties_.limits.maxPushConstantsSize<sizeof(Params))return fail("GPU xBRZ limits unavailable; CPU fallback");
  VkDescriptorSetLayoutBinding bindings[3]{};
  for(uint32_t i=0;i<3;++i){bindings[i].binding=i;bindings[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};layout.bindingCount=3;layout.pBindings=bindings;
  if(core_.vkCreateDescriptorSetLayout_(core_.device_,&layout,nullptr,&descriptorLayout_)!=VK_SUCCESS)return fatal("GPU xBRZ descriptor layout creation failed");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params)};
  VkPipelineLayoutCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pipelineInfo.setLayoutCount=1;pipelineInfo.pSetLayouts=&descriptorLayout_;pipelineInfo.pushConstantRangeCount=1;pipelineInfo.pPushConstantRanges=&push;
  if(core_.vkCreatePipelineLayout_(core_.device_,&pipelineInfo,nullptr,&pipelineLayout_)!=VK_SUCCESS)return fatal("GPU xBRZ pipeline layout creation failed");
  if(!pipeline(PreprocessShader,sizeof(PreprocessShader),preprocess_)||!pipeline(ScaleShader,sizeof(ScaleShader),scale_))return false;
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3};VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
  if(core_.vkCreateDescriptorPool_(core_.device_,&pool,nullptr,&descriptorPool_)!=VK_SUCCESS)return fatal("GPU xBRZ descriptor pool creation failed");
  VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};allocation.descriptorPool=descriptorPool_;allocation.descriptorSetCount=1;allocation.pSetLayouts=&descriptorLayout_;
  if(core_.vkAllocateDescriptorSets_(core_.device_,&allocation,&descriptors_)!=VK_SUCCESS)return fatal("GPU xBRZ descriptor set allocation failed");
  if(!core_.buffer(raw_,AtlasBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,true))return fatal("GPU xBRZ raw atlas allocation failed");
  ready_=true;return true;
 }
 bool prepare(const std::vector<RawJob>& jobs){
  prepared_=false;pending_.clear();copies_.clear();params_.clear();flagBytes_=uploadBytes_=0;
  if(jobs.empty()){prepared_=true;return true;}
  if(core_.inFlight_)return fail("GPU xBRZ cannot replace active resources");
  if(jobs.size()>4096)return fail("GPU xBRZ native job budget exceeded");
  uint64_t totalFlags=0;
  for(size_t i=0;i<jobs.size();++i){const auto& j=jobs[i];
   if(!j.pixels||j.width<1||j.height<1||j.width>8192||j.height>8192||j.factor<2||j.factor>4||
    uint64_t(j.width)*j.height!=j.pixels->size()||uint64_t(j.width)*j.factor>8192||uint64_t(j.height)*j.factor>8192||
    uint64_t(j.width)*j.height*j.factor*j.factor>Core::MaxPixels||j.pixels->size()>AtlasWords||
    uint64_t(j.outputOffset)+j.pixels->size()*j.factor*j.factor>ColorAtlasWords)return fail("Invalid GPU xBRZ native job");
   totalFlags+=flagWords(uint32_t(j.width),uint32_t(j.height));if(totalFlags>AtlasWords)return fail("GPU xBRZ corner scratch budget exceeded");
   const uint64_t begin=j.outputOffset,end=begin+j.pixels->size()*j.factor*j.factor;
   for(size_t k=0;k<i;++k){const uint64_t other=jobs[k].outputOffset,otherEnd=other+jobs[k].pixels->size()*jobs[k].factor*jobs[k].factor;
    if(begin<otherEnd&&other<end)return fail("Overlapping GPU xBRZ output ranges");}
  }
  if(!initialize())return false;
  std::vector<Resident> active;std::vector<size_t> indices;std::vector<size_t> representatives;uint64_t activeWords=0;
  active.reserve(jobs.size());indices.reserve(jobs.size());representatives.reserve(jobs.size());
  for(size_t i=0;i<jobs.size();++i){const auto&j=jobs[i];size_t n=0;for(;n<active.size();++n)if(same(active[n],j))break;
   if(n==active.size()){
    Resident entry{j.pixels,j.pixels.get(),0,uint32_t(j.pixels->size()),uint32_t(j.width),uint32_t(j.height)};
    activeWords+=entry.words;if(activeWords>AtlasWords)return fail("GPU xBRZ visible raw atlas budget exceeded");active.push_back(entry);representatives.push_back(i);
   }indices.push_back(n);
  }
  pending_=resident_;
  pending_.erase(std::remove_if(pending_.begin(),pending_.end(),[](const Resident& r){return r.owner.expired();}),pending_.end());
  std::vector<bool> dirty(active.size(),false);bool repack=false;
  for(size_t n=0;n<active.size();++n){
   auto found=std::find_if(pending_.begin(),pending_.end(),[&](const Resident&r){return same(r,active[n]);});
   if(found!=pending_.end()){active[n].offset=found->offset;continue;}
   std::sort(pending_.begin(),pending_.end(),[](const Resident&a,const Resident&b){return a.offset<b.offset;});uint64_t offset=0;
   for(const auto&r:pending_){if(offset+active[n].words<=r.offset)break;offset=uint64_t(r.offset)+r.words;}
   if(offset+active[n].words>AtlasWords){repack=true;break;}
   active[n].offset=uint32_t(offset);pending_.push_back(active[n]);dirty[n]=true;
  }
  if(repack){pending_=active;uint32_t offset=0;
   for(size_t n=0;n<active.size();++n){active[n].offset=pending_[n].offset=offset;offset+=active[n].words;dirty[n]=true;}
  }
  for(size_t n=0;n<active.size();++n)if(dirty[n])uploadBytes_+=VkDeviceSize(active[n].words)*4;
  flagBytes_=totalFlags*4;
  if(!core_.buffer(flags_,flagBytes_,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false,true)||
   (uploadBytes_&&!core_.buffer(upload_,uploadBytes_,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true,true)))return fatal("GPU xBRZ temporary buffer allocation failed");
  VkDeviceSize sourceOffset=0;
  for(size_t n=0;n<active.size();++n)if(dirty[n]){const auto&source=jobs[representatives[n]].pixels;const VkDeviceSize bytes=VkDeviceSize(active[n].words)*4;
   std::memcpy(static_cast<char*>(upload_.mapped)+sourceOffset,source->data(),size_t(bytes));copies_.push_back({sourceOffset,VkDeviceSize(active[n].offset)*4,bytes});sourceOffset+=bytes;
  }
  if(uploadBytes_&&!core_.flush(upload_))return fatal("GPU xBRZ native upload flush failed");
  uint32_t flagsOffset=0;params_.reserve(jobs.size());
  for(size_t i=0;i<jobs.size();++i){const auto&j=jobs[i];
   params_.push_back({active[indices[i]].offset,uint32_t(j.width),uint32_t(j.height),j.outputOffset,uint32_t(j.factor),flagsOffset});flagsOffset+=uint32_t(flagWords(uint32_t(j.width),uint32_t(j.height)));
  }
  prepared_=true;return true;
 }
 bool record(VkCommandBuffer command,VkBuffer target){
  if(!prepared_)return fail("GPU xBRZ stage was not prepared");
  if(params_.empty())return true;
  if(!ready_||!command||!target||core_.inFlight_)return fail("Invalid GPU xBRZ recording state");
  VkDescriptorBufferInfo infos[3]{{raw_.buffer,0,AtlasBytes},{flags_.buffer,0,flagBytes_},{target,0,ColorAtlasBytes}};
  VkWriteDescriptorSet writes[3]{};
  for(uint32_t i=0;i<3;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=descriptors_;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[i].pBufferInfo=&infos[i];}
  core_.vkUpdateDescriptorSets_(core_.device_,3,writes,0,nullptr);
  if(!copies_.empty()){
   VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
   core_.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&host,0,nullptr,0,nullptr);
   core_.vkCmdCopyBuffer_(command,upload_.buffer,raw_.buffer,uint32_t(copies_.size()),copies_.data());
  }
  VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
  core_.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
  core_.vkCmdBindDescriptorSets_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout_,0,1,&descriptors_,0,nullptr);
  core_.vkCmdBindPipeline_(command,VK_PIPELINE_BIND_POINT_COMPUTE,preprocess_);
  for(const auto&p:params_){core_.vkCmdPushConstants_(command,pipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params),&p);core_.vkCmdDispatch_(command,groups(p.width+1),groups(p.height+1),1);}
  VkMemoryBarrier corners{VK_STRUCTURE_TYPE_MEMORY_BARRIER};corners.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;corners.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  core_.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&corners,0,nullptr,0,nullptr);
  core_.vkCmdBindPipeline_(command,VK_PIPELINE_BIND_POINT_COMPUTE,scale_);
  for(const auto&p:params_){core_.vkCmdPushConstants_(command,pipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params),&p);core_.vkCmdDispatch_(command,groups(p.width),groups(p.height),1);}
  VkMemoryBarrier after{VK_STRUCTURE_TYPE_MEMORY_BARRIER};after.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;after.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
  core_.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&after,0,nullptr,0,nullptr);
  return true;
 }
 // Cache contents become authoritative only after the caller's entire queue
 // submission/fence succeeds. A failed attempt never publishes partial owners.
 void commit(){if(prepared_){if(!params_.empty())resident_=std::move(pending_);prepared_=false;}}
 void close(){
  if(core_.abandoned_)return;
  if(core_.inFlight_&&core_.vkGetFenceStatus_&&core_.vkGetFenceStatus_(core_.device_,core_.fence_)!=VK_SUCCESS)return;
  if(core_.device_){
   core_.destroy(raw_);core_.destroy(flags_);core_.destroy(upload_);
   if(preprocess_&&core_.vkDestroyPipeline_)core_.vkDestroyPipeline_(core_.device_,preprocess_,nullptr);
   if(scale_&&core_.vkDestroyPipeline_)core_.vkDestroyPipeline_(core_.device_,scale_,nullptr);
   if(descriptorPool_&&core_.vkDestroyDescriptorPool_)core_.vkDestroyDescriptorPool_(core_.device_,descriptorPool_,nullptr);
   if(pipelineLayout_&&core_.vkDestroyPipelineLayout_)core_.vkDestroyPipelineLayout_(core_.device_,pipelineLayout_,nullptr);
   if(descriptorLayout_&&core_.vkDestroyDescriptorSetLayout_)core_.vkDestroyDescriptorSetLayout_(core_.device_,descriptorLayout_,nullptr);
  }
  raw_={};flags_={};upload_={};preprocess_=scale_=VK_NULL_HANDLE;descriptorPool_=VK_NULL_HANDLE;pipelineLayout_=VK_NULL_HANDLE;descriptorLayout_=VK_NULL_HANDLE;descriptors_=VK_NULL_HANDLE;
  resident_.clear();pending_.clear();copies_.clear();params_.clear();flagBytes_=uploadBytes_=0;ready_=prepared_=false;
 }
};
}}
