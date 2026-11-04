#pragma once
#include "vulkan_compute.h"
#include "gpu_shroud.h"
#include "gpu_shroud_spirv.h"

namespace external { namespace gpushroud {
// A stage of its owner's ONE command buffer. It never submits, waits or reads
// back. Unchanged Plan identity reuses a persistent packed device mask.
class Stage {
 using Owner=vulkancompute::Resolve;using Buffer=Owner::Buffer;
 static constexpr size_t AlphaWords=256*1024,MaximumMetadataWords=32*1024*1024;
 struct SourceResident {std::weak_ptr<const std::vector<uint8_t>> owner;const void*address=nullptr;uint32_t offset=0,count=0;};
 Owner&owner_;Buffer alpha_,metadata_,mask_;
 VkDescriptorSetLayout descriptorLayout_=VK_NULL_HANDLE;VkPipelineLayout pipelineLayout_=VK_NULL_HANDLE;
 VkDescriptorPool descriptorPool_=VK_NULL_HANDLE;VkDescriptorSet descriptors_=VK_NULL_HANDLE;VkPipeline pipeline_=VK_NULL_HANDLE;
 std::vector<SourceResident> residents_;std::shared_ptr<const Plan> prepared_,pending_;std::weak_ptr<const Plan> recorded_;
 VkBuffer target_=VK_NULL_HANDLE;VkDeviceSize targetOffset_=0;Params params_{};bool ready_=false,needsRaster_=false;
 uint64_t uploads_=0,uploadedBytes_=0,dispatches_=0,copies_=0;
 static bool same(const SourceResident&r,const std::shared_ptr<const std::vector<uint8_t>>&p){return r.address==p.get()&&!r.owner.owner_before(p)&&!p.owner_before(r.owner);}
 bool initialize(){
  if(ready_)return true;auto&o=owner_;if(!o.initialize())return false;
  if(o.properties_.limits.maxPerStageDescriptorStorageBuffers<3||o.properties_.limits.maxDescriptorSetStorageBuffers<3||o.properties_.limits.maxPushConstantsSize<sizeof(Params)||o.properties_.limits.maxComputeWorkGroupInvocations<64||o.properties_.limits.maxComputeWorkGroupSize[0]<64||!o.properties_.limits.maxComputeWorkGroupCount[0]||!o.properties_.limits.maxComputeWorkGroupCount[1])return o.fail("GPU shroud storage bindings unavailable");
  VkDescriptorSetLayoutBinding bindings[3]{};for(uint32_t n=0;n<3;++n){bindings[n].binding=n;bindings[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[n].descriptorCount=1;bindings[n].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ds.bindingCount=3;ds.pBindings=bindings;
  if(o.vkCreateDescriptorSetLayout_(o.device_,&ds,nullptr,&descriptorLayout_)!=VK_SUCCESS)return o.fatal("GPU shroud descriptor layout failed");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(Params)};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&descriptorLayout_;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;
  if(o.vkCreatePipelineLayout_(o.device_,&pl,nullptr,&pipelineLayout_)!=VK_SUCCESS)return o.fatal("GPU shroud pipeline layout failed");
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(ShroudShader);sm.pCode=ShroudShader;VkShaderModule module=VK_NULL_HANDLE;
  if(o.vkCreateShaderModule_(o.device_,&sm,nullptr,&module)!=VK_SUCCESS)return o.fatal("GPU shroud module failed");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=module;cp.stage.pName="main";cp.layout=pipelineLayout_;
  VkResult status=o.vkCreateComputePipelines_(o.device_,VK_NULL_HANDLE,1,&cp,nullptr,&pipeline_);o.vkDestroyShaderModule_(o.device_,module,nullptr);
  if(status!=VK_SUCCESS)return o.fatal("GPU shroud pipeline failed");
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3};VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=1;pool.poolSizeCount=1;pool.pPoolSizes=&size;
  if(o.vkCreateDescriptorPool_(o.device_,&pool,nullptr,&descriptorPool_)!=VK_SUCCESS)return o.fatal("GPU shroud descriptor pool failed");
  VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};alloc.descriptorPool=descriptorPool_;alloc.descriptorSetCount=1;alloc.pSetLayouts=&descriptorLayout_;
  if(o.vkAllocateDescriptorSets_(o.device_,&alloc,&descriptors_)!=VK_SUCCESS)return o.fatal("GPU shroud descriptor allocation failed");
  if(!o.buffer(alpha_,AlphaWords*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true,true))return o.fatal("GPU shroud source allocation failed");ready_=true;return true;
 }
 void close(){
  if(owner_.abandoned_)return;
  auto&o=owner_;if(o.inFlight_&&(!o.vkGetFenceStatus_||o.vkGetFenceStatus_(o.device_,o.fence_)!=VK_SUCCESS))return;
  if(o.device_){
   o.destroy(alpha_);o.destroy(metadata_);o.destroy(mask_);
   if(pipeline_&&o.vkDestroyPipeline_)o.vkDestroyPipeline_(o.device_,pipeline_,nullptr);
   if(descriptorPool_&&o.vkDestroyDescriptorPool_)o.vkDestroyDescriptorPool_(o.device_,descriptorPool_,nullptr);
   if(pipelineLayout_&&o.vkDestroyPipelineLayout_)o.vkDestroyPipelineLayout_(o.device_,pipelineLayout_,nullptr);
   if(descriptorLayout_&&o.vkDestroyDescriptorSetLayout_)o.vkDestroyDescriptorSetLayout_(o.device_,descriptorLayout_,nullptr);
  }
  alpha_={};metadata_={};mask_={};pipeline_=VK_NULL_HANDLE;descriptorPool_=VK_NULL_HANDLE;descriptors_=VK_NULL_HANDLE;pipelineLayout_=VK_NULL_HANDLE;descriptorLayout_=VK_NULL_HANDLE;
  residents_.clear();prepared_.reset();pending_.reset();recorded_.reset();target_=VK_NULL_HANDLE;targetOffset_=0;params_={};ready_=needsRaster_=false;
 }
public:
 explicit Stage(Owner&o):owner_(o){}Stage(const Stage&)=delete;Stage&operator=(const Stage&)=delete;~Stage(){close();}
 void release(){close();} // coordinator releases stages before closing their shared device
 uint64_t uploads()const{return uploads_;}uint64_t uploadedBytes()const{return uploadedBytes_;}uint64_t dispatches()const{return dispatches_;}uint64_t copies()const{return copies_;}
 bool prepare(std::shared_ptr<const Plan>plan,VkBuffer target,uint32_t targetOffsetWords,VkDeviceSize targetBytes){
  auto&o=owner_;uploads_=uploadedBytes_=dispatches_=copies_=0;pending_.reset();
  if(!plan||!target||plan->width<1||plan->height<1||plan->width>8192||plan->height>8192||uint64_t(plan->width)*plan->height>32*1024*1024||
    (uint64_t(targetOffsetWords)+plan->outputWords())*4>targetBytes||!withinGpuBudget(*plan))return o.fail("Invalid GPU native shroud budget");
  if(!initialize())return false;
  target_=target;targetOffset_=VkDeviceSize(targetOffsetWords)*4;
  auto recorded=recorded_.lock();needsRaster_=recorded.get()!=plan.get()||!recorded||recorded.owner_before(plan)||plan.owner_before(recorded);
  if(prepared_==plan&&!needsRaster_)return true;
  Packed packed;std::vector<SourceResident> active;std::vector<uint32_t> sourceIndex,offsets;uint64_t activeWords=0;
  for(const auto&s:plan->sources){if(!s.indices)return o.fail("Invalid GPU native shroud source");size_t n=0;for(;n<active.size();++n)if(same(active[n],s.indices))break;
   if(n==active.size()){active.push_back({s.indices,s.indices.get(),0,uint32_t(s.indices->size())});activeWords+=s.indices->size();}sourceIndex.push_back(uint32_t(n));
  }
  if(activeWords>AlphaWords)return o.fail("GPU native shroud source budget exceeded");
  auto retained=residents_;retained.erase(std::remove_if(retained.begin(),retained.end(),[](const SourceResident&r){return r.owner.expired();}),retained.end());
  std::vector<size_t>dirty;bool repack=false;
  for(size_t n=0;n<active.size();++n){auto owner=active[n].owner.lock();auto found=std::find_if(retained.begin(),retained.end(),[&](const SourceResident&r){return same(r,owner);});
   if(found!=retained.end()){active[n].offset=found->offset;continue;}
   std::sort(retained.begin(),retained.end(),[](const SourceResident&a,const SourceResident&b){return a.offset<b.offset;});uint64_t offset=0;
   for(const auto&r:retained){if(offset+active[n].count<=r.offset)break;offset=uint64_t(r.offset)+r.count;}
   if(offset+active[n].count>AlphaWords){repack=true;break;}active[n].offset=uint32_t(offset);retained.push_back(active[n]);dirty.push_back(n);
  }
  if(repack){retained=active;dirty.clear();uint32_t offset=0;for(size_t n=0;n<active.size();++n){active[n].offset=retained[n].offset=offset;offset+=active[n].count;dirty.push_back(n);}}
  for(uint32_t n:sourceIndex)offsets.push_back(active[n].offset);
  if(!pack(*plan,packed,&offsets)||packed.metadata.size()>MaximumMetadataWords)return o.fail("Invalid GPU native shroud metadata");
  if(plan->outputWords()*4>mask_.size){recorded_.reset();needsRaster_=true;}
  if(!o.buffer(metadata_,std::max<size_t>(4,packed.metadata.size()*4),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true)||
     !o.buffer(mask_,plan->outputWords()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false))return o.fatal("GPU native shroud frame allocation failed");
  auto*alpha=static_cast<uint32_t*>(alpha_.mapped);
  for(size_t n:dirty){auto owner=active[n].owner.lock();if(!owner)return o.fail("GPU native shroud source expired");for(size_t x=0;x<owner->size();++x)alpha[active[n].offset+x]=(*owner)[x];++uploads_;uploadedBytes_+=owner->size()*4;}
  if(!packed.metadata.empty())std::memcpy(metadata_.mapped,packed.metadata.data(),packed.metadata.size()*4);
  if((!dirty.empty()&&!o.flush(alpha_))||!o.flush(metadata_))return o.fatal("GPU native shroud flush failed");
  VkDescriptorBufferInfo infos[3]{{alpha_.buffer,0,alpha_.size},{metadata_.buffer,0,metadata_.size},{mask_.buffer,0,mask_.size}};VkWriteDescriptorSet writes[3]{};
  for(uint32_t n=0;n<3;++n){writes[n].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[n].dstSet=descriptors_;writes[n].dstBinding=n;writes[n].descriptorCount=1;writes[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[n].pBufferInfo=&infos[n];}
  o.vkUpdateDescriptorSets_(o.device_,3,writes,0,nullptr);params_=packed.params;prepared_=std::move(plan);residents_=std::move(retained);return true;
 }
 void record(VkCommandBuffer command){
  auto&o=owner_;if(!prepared_)return;
  if(needsRaster_){
   VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;host.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
   o.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&host,0,nullptr,0,nullptr);
   o.vkCmdBindPipeline_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_);o.vkCmdBindDescriptorSets_(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout_,0,1,&descriptors_,0,nullptr);
   o.vkCmdPushConstants_(command,pipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(params_),&params_);
   const uint32_t groups=uint32_t((prepared_->outputWords()+63)/64),gx=std::min(groups,o.properties_.limits.maxComputeWorkGroupCount[0]);
   o.vkCmdDispatch_(command,gx,(groups+gx-1)/gx,1);++dispatches_;pending_=prepared_;
  }
  VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ready.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
  o.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&ready,0,nullptr,0,nullptr);
  VkBufferCopy copy{0,targetOffset_,prepared_->outputWords()*4};o.vkCmdCopyBuffer_(command,mask_.buffer,target_,1,&copy);++copies_;
  VkMemoryBarrier scene{VK_STRUCTURE_TYPE_MEMORY_BARRIER};scene.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;scene.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
  o.vkCmdPipelineBarrier_(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&scene,0,nullptr,0,nullptr);
 }
 // Recording a command is not GPU completion. A coordinator which abandons
 // a frame before submission must not make uninitialized mask data reusable.
 void commit(){if(pending_){recorded_=pending_;pending_.reset();needsRaster_=false;}}
};
}}
