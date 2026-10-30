#pragma once
#include "vulkan_compute.h"
#include "gpu_area.h"
#include "vulkan_area_spirv.h"
#include "vulkan_shroud_backend.h"
#include "vulkan_present.h"
#include "gpu_effect_plan.h"
#include "gpu_effects_spirv.h"
#include <unordered_map>
namespace external {namespace vulkancompute {
// Whole-map resolve preserves native painter/Z composition. Sparse immutable
// pages stay resident; only changed pages rebuild exact integer4x4 integrals.
class AreaResolve : public Resolve {
 DirectOutput direct_{*this};
 VkDescriptorSet effectsSet_=VK_NULL_HANDLE;VkPipeline effectsPipeline_=VK_NULL_HANDLE;
 struct Resident {std::weak_ptr<const std::vector<uint32_t>>owner;const void*address=nullptr;uint32_t slot=0;uint64_t version=0;};
 struct AreaParams {uint32_t width,height,nativeWidth,nativeHeight,columns,tableOffset,xOffset,yOffset,background,fogOffset,buildCount,buildOffset,phase;};
 Buffer prefix_,metadata_,rawBanks_[3];gpushroud::Stage mask_{*this};bool banked_=false;uint32_t storageBindings_=4;
 VkDescriptorSetLayout areaLayout_=VK_NULL_HANDLE;VkPipelineLayout areaPipelineLayout_=VK_NULL_HANDLE;VkDescriptorPool areaPool_=VK_NULL_HANDLE;VkDescriptorSet areaSet_=VK_NULL_HANDLE;VkPipeline areaPipeline_=VK_NULL_HANDLE;
 std::vector<Resident>residents_;uint64_t prefixBuilds_=0;bool areaReady_=false;VkDeviceSize areaAtlasBytes_=AtlasBytes;
 static constexpr uint64_t PrefixBytes=uint64_t(gpuarea::MaximumPages)*gpuarea::PrefixWords*4;
 static bool samePage(const Resident&r,const std::shared_ptr<const std::vector<uint32_t>>&p,uint64_t version){return r.version==version&&r.address==p.get()&&!r.owner.owner_before(p)&&!p.owner_before(r.owner);}
 void releaseArea(){
  if(inFlight_&&(!vkGetFenceStatus_||vkGetFenceStatus_(device_,fence_)!=VK_SUCCESS))return;
  direct_.close();mask_.release();if(device_){if(effectsPipeline_&&vkDestroyPipeline_)vkDestroyPipeline_(device_,effectsPipeline_,nullptr);effectsPipeline_=VK_NULL_HANDLE;effectsSet_=VK_NULL_HANDLE;for(auto&bank:rawBanks_)destroy(bank);destroy(prefix_);destroy(metadata_);if(areaPipeline_&&vkDestroyPipeline_)vkDestroyPipeline_(device_,areaPipeline_,nullptr);if(areaPool_&&vkDestroyDescriptorPool_)vkDestroyDescriptorPool_(device_,areaPool_,nullptr);if(areaPipelineLayout_&&vkDestroyPipelineLayout_)vkDestroyPipelineLayout_(device_,areaPipelineLayout_,nullptr);if(areaLayout_&&vkDestroyDescriptorSetLayout_)vkDestroyDescriptorSetLayout_(device_,areaLayout_,nullptr);}
  prefix_={};metadata_={};areaPipeline_=VK_NULL_HANDLE;areaPool_=VK_NULL_HANDLE;areaPipelineLayout_=VK_NULL_HANDLE;areaLayout_=VK_NULL_HANDLE;areaSet_=VK_NULL_HANDLE;areaReady_=false;residents_.clear();
 }
 bool initializeArea(){
  if(areaReady_)return true;if(!initialize())return false;
  if(!features_.shaderFloat64)return fatal("GPU area filter needs shaderFloat64");
  if(properties_.limits.maxPerStageDescriptorStorageBuffers<4||properties_.limits.maxDescriptorSetStorageBuffers<4||properties_.limits.maxPushConstantsSize<sizeof(AreaParams))return fatal("GPU area storage bindings unavailable");
  banked_=sizeof(void*)>=8&&properties_.limits.maxStorageBufferRange<uint64_t(gpuarea::MaximumPages)*gpuarea::PagePixels*4&&properties_.limits.maxPerStageDescriptorStorageBuffers>=7&&properties_.limits.maxDescriptorSetStorageBuffers>=7;storageBindings_=banked_?7:4;
  VkDescriptorSetLayoutBinding bindings[7]{};for(uint32_t n=0;n<storageBindings_;++n){bindings[n].binding=n;bindings[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;bindings[n].descriptorCount=1;bindings[n].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;}
  VkDescriptorSetLayoutCreateInfo ds{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};ds.bindingCount=storageBindings_;ds.pBindings=bindings;if(vkCreateDescriptorSetLayout_(device_,&ds,nullptr,&areaLayout_)!=VK_SUCCESS)return fatal("GPU area layout failed");
  VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(AreaParams)};VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&areaLayout_;pl.pushConstantRangeCount=1;pl.pPushConstantRanges=&push;if(vkCreatePipelineLayout_(device_,&pl,nullptr,&areaPipelineLayout_)!=VK_SUCCESS)return fatal("GPU area pipeline layout failed");
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=banked_?sizeof(AreaBankedShader):sizeof(AreaShader);sm.pCode=banked_?AreaBankedShader:AreaShader;VkShaderModule shader=VK_NULL_HANDLE;if(vkCreateShaderModule_(device_,&sm,nullptr,&shader)!=VK_SUCCESS)return fatal("GPU area shader module failed");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=shader;cp.stage.pName="main";cp.layout=areaPipelineLayout_;VkResult status=vkCreateComputePipelines_(device_,VK_NULL_HANDLE,1,&cp,nullptr,&areaPipeline_);vkDestroyShaderModule_(device_,shader,nullptr);if(status!=VK_SUCCESS)return fatal("GPU area pipeline failed");
  VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,storageBindings_*2};VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pool.maxSets=2;pool.poolSizeCount=1;pool.pPoolSizes=&poolSize;if(vkCreateDescriptorPool_(device_,&pool,nullptr,&areaPool_)!=VK_SUCCESS)return fatal("GPU area descriptor pool failed");
  VkDescriptorSetAllocateInfo set{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};set.descriptorPool=areaPool_;set.descriptorSetCount=1;set.pSetLayouts=&areaLayout_;if(vkAllocateDescriptorSets_(device_,&set,&areaSet_)!=VK_SUCCESS||vkAllocateDescriptorSets_(device_,&set,&effectsSet_)!=VK_SUCCESS)return fatal("GPU area descriptor set failed");if(!buffer(prefix_,PrefixBytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,false,true))return fatal("GPU area prefix allocation failed");areaReady_=true;return true;
 }
 bool initializeEffects(){
  if(effectsPipeline_)return true;VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(EffectsShader);sm.pCode=EffectsShader;VkShaderModule shader=VK_NULL_HANDLE;
  if(vkCreateShaderModule_(device_,&sm,nullptr,&shader)!=VK_SUCCESS)return fatal("GPU area effects shader unavailable");
  VkComputePipelineCreateInfo cp{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cp.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;cp.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cp.stage.module=shader;cp.stage.pName="main";cp.layout=areaPipelineLayout_;
  auto status=vkCreateComputePipelines_(device_,VK_NULL_HANDLE,1,&cp,nullptr,&effectsPipeline_);vkDestroyShaderModule_(device_,shader,nullptr);return status==VK_SUCCESS||fatal("GPU area effects pipeline failed");
 }
 bool renderArea(const gpuarea::Plan&p,std::vector<uint32_t>&out,std::shared_ptr<const gpupresent::Frame>*gpuOut){
  const auto started=std::chrono::steady_clock::now();diagnostics_.uploads=diagnostics_.uploadedBytes=diagnostics_.dispatches=diagnostics_.readbacks=0;prefixBuilds_=0;
  if(disabled_)return false;if(!gpuarea::valid(p))return fail("Invalid sparse GPU area plan");
  if(p.mask&&(p.mask->width!=p.width||p.mask->height!=p.height))return fail("GPU area mask dimensions disagree");if(!initializeArea())return false;
  if(p.effects&&(!p.effects->valid||p.effects->width!=p.width||p.effects->height!=p.height))return fail("GPU area effects dimensions disagree");
  const auto requiredAtlas=gpuarea::atlasBytes(p.pages.size(),banked_?uint64_t(gpuarea::MaximumPages)*gpuarea::PagePixels*4:properties_.limits.maxStorageBufferRange);
  if(!requiredAtlas)return fail("GPU area native pages exceed device storage-buffer range; CPU fallback");
  if(banked_){const uint64_t rawBytes=uint64_t(p.pages.size())*gpuarea::PagePixels*4;
   for(unsigned bank=1;bank<4;++bank){const auto need=rawBytes>uint64_t(bank)*AtlasBytes?std::min<uint64_t>(AtlasBytes,rawBytes-uint64_t(bank)*AtlasBytes):0;
    if(need>rawBanks_[bank-1].size){residents_.clear();if(!buffer(rawBanks_[bank-1],need,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false,false,AtlasBytes))return fatal("GPU area bank allocation failed; CPU fallback");}}
  }else if(atlas_.size<requiredAtlas){residents_.clear();if(!buffer(atlas_,requiredAtlas,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false,true))return fatal("GPU area atlas growth failed; CPU fallback");}
  areaAtlasBytes_=atlas_.size;for(const auto&bank:rawBanks_)areaAtlasBytes_+=bank.size;
  std::vector<Resident>active;std::vector<uint32_t>pageSlots;std::vector<size_t>dirty;std::vector<uint8_t>used(gpuarea::MaximumPages);
  std::unordered_multimap<const void*,size_t>activeIndex,residentIndex;activeIndex.reserve(p.pages.size());residentIndex.reserve(residents_.size());
  for(size_t n=0;n<residents_.size();++n)residentIndex.emplace(residents_[n].address,n);
  for(const auto&page:p.pages){size_t n=active.size();auto range=activeIndex.equal_range(page.pixels.get());for(auto i=range.first;i!=range.second;++i)if(samePage(active[i->second],page.pixels,page.version)){n=i->second;break;}
   if(n==active.size()){active.push_back({page.pixels,page.pixels.get(),gpuarea::Missing,page.version});activeIndex.emplace(page.pixels.get(),n);}pageSlots.push_back(uint32_t(n));}
  // Current Plan aliases protect source pages during this submission only.
  // Indexing avoids O(N²) weak locks for a warm whole-map presentation.
  for(auto&a:active){auto source=a.owner.lock();auto range=residentIndex.equal_range(a.address);for(auto i=range.first;i!=range.second;++i){const auto&r=residents_[i->second];if(samePage(r,source,a.version)){a.slot=r.slot;used[a.slot]=1;break;}}}
  uint32_t free=0,slots=1;for(size_t n=0;n<active.size();++n){auto&a=active[n];if(a.slot==gpuarea::Missing){while(free<used.size()&&used[free])++free;if(free==used.size())return fail("GPU area resident budget exceeded");a.slot=free;used[free]=1;dirty.push_back(n);}slots=std::max(slots,a.slot+1);}
  const uint32_t cols=(p.nativeWidth+127)/128,rows=(p.nativeHeight+127)/128;std::vector<uint32_t>metadata(size_t(cols)*rows,gpuarea::Missing);
  for(size_t n=0;n<p.pages.size();++n)metadata[size_t(p.pages[n].y)*cols+p.pages[n].x]=active[pageSlots[n]].slot;
  auto number=[&](double value){uint32_t bits[2];std::memcpy(bits,&value,sizeof value);metadata.push_back(bits[0]);metadata.push_back(bits[1]);};
  AreaParams params{uint32_t(p.width),uint32_t(p.height),uint32_t(p.nativeWidth),uint32_t(p.nativeHeight),cols,0,0,0,p.background,gpuarea::Missing,uint32_t(dirty.size()),0,0};
  if(metadata.size()&1)metadata.push_back(0);params.xOffset=uint32_t(metadata.size());const double sx=p.spanX/p.width,sy=p.spanY/p.height;
  for(int x=0;x<p.width;++x){number(std::clamp(x*sx,0.,double(p.nativeWidth)));number(std::clamp((x+1)*sx,0.,double(p.nativeWidth)));}params.yOffset=uint32_t(metadata.size());for(int y=0;y<p.height;++y){number(std::clamp(y*sy,0.,double(p.nativeHeight)));number(std::clamp((y+1)*sy,0.,double(p.nativeHeight)));}
  params.buildOffset=uint32_t(metadata.size());for(size_t n:dirty)metadata.push_back(active[n].slot);
  if(p.fog||p.mask){params.fogOffset=uint32_t(metadata.size());size_t words=(uint64_t(p.width)*p.height+3)/4;metadata.resize(metadata.size()+words);if(p.fog)for(size_t n=0;n<p.fog->size();++n)metadata[params.fogOffset+n/4]|=uint32_t((*p.fog)[n])<<((n&3)*8);}
  // Area resolve and ordered effects write one immutable completed output.
  // The effect shader reads both commands and alpha/fog from metadata_.
  std::vector<gpueffects::Dispatch> effects;bool waveSnapshot=false;
  if(p.effects&&!p.effects->commands.empty()){
   std::vector<uint32_t>alphas;alphas.reserve(p.effects->commands.size());
   for(const auto&c:p.effects->commands){alphas.push_back(c.indices?uint32_t(metadata.size()):gpuarea::Missing);if(c.indices){
    if(c.indices->size()>16*1024*1024||metadata.size()+(c.indices->size()+3)/4>16*1024*1024)return fail("GPU area effect material budget exceeded");
    const size_t at=metadata.size();metadata.resize(at+(c.indices->size()+3)/4);std::memcpy(metadata.data()+at,c.indices->data(),c.indices->size());
   }}
   if(!gpueffects::pack(*p.effects,alphas,params.fogOffset,metadata,effects)||!initializeEffects())return false;
   for(const auto&d:effects)waveSnapshot|=d.snapshot;
  }
  const uint64_t outputBytes=uint64_t(p.width)*p.height*4,uploadBytes=uint64_t(dirty.size())*gpuarea::PagePixels*4;
  const uint64_t outputCapacity=outputBytes*(waveSnapshot?2:1);const bool direct=gpuOut&&direct_.prepare(p.width,p.height);if(direct)destroy(readback_);
  if(!buffer(metadata_,metadata.size()*4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,true)||
   !buffer(output_,outputCapacity,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,false)||(!direct&&!buffer(readback_,outputBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true))||
   (uploadBytes&&!buffer(upload_,uploadBytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true,false,areaAtlasBytes_)))return fatal("GPU area buffer allocation failed; CPU fallback");
  std::memcpy(metadata_.mapped,metadata.data(),metadata.size()*4);std::vector<VkBufferCopy>copies[4];uint64_t next=0;
  for(size_t n:dirty){auto source=active[n].owner.lock();if(!source)return fail("GPU area page expired");std::memcpy(static_cast<char*>(upload_.mapped)+next,source->data(),gpuarea::PagePixels*4);const uint64_t destination=uint64_t(active[n].slot)*gpuarea::PagePixels*4;const unsigned bank=banked_?unsigned(destination/AtlasBytes):0;copies[bank].push_back({next,banked_?destination%AtlasBytes:destination,uint64_t(gpuarea::PagePixels)*4});next+=gpuarea::PagePixels*4;}
  if(!flush(metadata_)||(uploadBytes&&!flush(upload_)))return fatal("GPU area upload flush failed");
  if(p.mask&&!mask_.prepare(p.mask,metadata_.buffer,params.fogOffset,metadata_.size))return false;
  VkDescriptorBufferInfo infos[7]{{atlas_.buffer,0,atlas_.size},{prefix_.buffer,0,prefix_.size},{metadata_.buffer,0,metadata_.size},{output_.buffer,0,outputBytes}};for(unsigned n=0;n<3;++n){const auto&bank=rawBanks_[n].buffer!=VK_NULL_HANDLE?rawBanks_[n]:atlas_;infos[4+n]={bank.buffer,0,bank.size};}VkWriteDescriptorSet writes[7]{};for(uint32_t n=0;n<storageBindings_;++n){writes[n].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[n].dstSet=areaSet_;writes[n].dstBinding=n;writes[n].descriptorCount=1;writes[n].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[n].pBufferInfo=&infos[n];}vkUpdateDescriptorSets_(device_,storageBindings_,writes,0,nullptr);
  if(!effects.empty()){infos[1]={metadata_.buffer,0,metadata_.size};infos[3].range=outputCapacity;for(uint32_t n=0;n<storageBindings_;++n)writes[n].dstSet=effectsSet_;vkUpdateDescriptorSets_(device_,storageBindings_,writes,0,nullptr);}
  if(vkResetCommandPool_(device_,commandPool_,0)!=VK_SUCCESS||vkResetFences_(device_,1,&fence_)!=VK_SUCCESS)return fatal("GPU area command reset failed");VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;if(vkBeginCommandBuffer_(command_,&begin)!=VK_SUCCESS)return fatal("GPU area command begin failed");
  for(unsigned bank=0;bank<4;++bank)if(!copies[bank].empty())vkCmdCopyBuffer_(command_,upload_.buffer,bank?rawBanks_[bank-1].buffer:atlas_.buffer,uint32_t(copies[bank].size()),copies[bank].data());
  VkMemoryBarrier before{VK_STRUCTURE_TYPE_MEMORY_BARRIER};before.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_HOST_WRITE_BIT;before.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT|VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&before,0,nullptr,0,nullptr);
  auto barrier=[&](){VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};b.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&b,0,nullptr,0,nullptr);};
  if(p.mask)mask_.record(command_);
  vkCmdBindPipeline_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,areaPipeline_);vkCmdBindDescriptorSets_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,areaPipelineLayout_,0,1,&areaSet_,0,nullptr);
  if(!dirty.empty())for(uint32_t phase=0;phase<2;++phase){params.phase=phase;vkCmdPushConstants_(command_,areaPipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof params,&params);vkCmdDispatch_(command_,phase?5:4,(params.buildCount+7)/8,1);barrier();}
  params.phase=2;vkCmdPushConstants_(command_,areaPipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof params,&params);vkCmdDispatch_(command_,(params.width+7)/8,(params.height+7)/8,1);
  for(const auto&d:effects){
   if(d.snapshot){VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ready.srcAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&ready,0,nullptr,0,nullptr);VkBufferCopy copy{0,outputBytes,outputBytes};vkCmdCopyBuffer_(command_,output_.buffer,output_.buffer,1,&copy);ready.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,1,&ready,0,nullptr,0,nullptr);}else barrier();
   vkCmdBindPipeline_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,effectsPipeline_);vkCmdBindDescriptorSets_(command_,VK_PIPELINE_BIND_POINT_COMPUTE,areaPipelineLayout_,0,1,&effectsSet_,0,nullptr);vkCmdPushConstants_(command_,areaPipelineLayout_,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof d.params,d.params.data());vkCmdDispatch_(command_,(d.params[10]-d.params[8]+7)/8,(d.params[11]-d.params[9]+7)/8,1);
  }
  VkMemoryBarrier ready{VK_STRUCTURE_TYPE_MEMORY_BARRIER};ready.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;ready.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,1,&ready,0,nullptr,0,nullptr);if(direct)direct_.record(command_,output_.buffer);else{VkBufferCopy result{0,0,outputBytes};vkCmdCopyBuffer_(command_,output_.buffer,readback_.buffer,1,&result);}
  VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};host.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;host.dstAccessMask=VK_ACCESS_HOST_READ_BIT;vkCmdPipelineBarrier_(command_,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&host,0,nullptr,0,nullptr);if(vkEndCommandBuffer_(command_)!=VK_SUCCESS)return fatal("GPU area command completion failed");VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command_;if(direct)direct_.submission(submit);if(vkQueueSubmit_(queue_,1,&submit,fence_)!=VK_SUCCESS)return fatal("GPU area submission failed");inFlight_=true;
  const uint64_t waitBudget=AllowSoftware&&diagnostics_.softwareDevice?10000000000ull:100000000ull;VkResult waited=vkWaitForFences_(device_,1,&fence_,VK_TRUE,waitBudget);if(waited!=VK_SUCCESS){quarantined().store(true,std::memory_order_relaxed);return fatal(waited==VK_TIMEOUT?"GPU area timed out; CPU fallback":"GPU area device lost; CPU fallback");}inFlight_=false;
  if(direct){out.clear();*gpuOut=direct_.completed();}else{
  if(!readback_.coherent){VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};range.memory=readback_.memory;range.size=VK_WHOLE_SIZE;if(vkInvalidateMappedMemoryRanges_(device_,1,&range)!=VK_SUCCESS)return fatal("GPU area readback invalidation failed");}
  out.resize(size_t(p.width)*p.height);std::memcpy(out.data(),readback_.mapped,size_t(outputBytes));}
  residents_=std::move(active);if(p.mask)mask_.commit();prefixBuilds_=dirty.size();++diagnostics_.frames;diagnostics_.uploads=dirty.size()+(p.mask?mask_.uploads():0);diagnostics_.uploadedBytes=uploadBytes+metadata.size()*4+(p.mask?mask_.uploadedBytes():0);diagnostics_.dispatches=1+(dirty.empty()?0:2)+effects.size()+(p.mask?mask_.dispatches():0);diagnostics_.readbacks=direct?0:1;diagnostics_.reason.clear();diagnostics_.milliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return true;
 }
public:
 AreaResolve()=default;~AreaResolve(){releaseArea();}
 uint64_t prefixBuilds()const{return prefixBuilds_;}
 uint64_t nativeAtlasBytes()const{return areaAtlasBytes_;}bool bankedStorage()const{return banked_;}
 void reused(){prefixBuilds_=0;diagnostics_.uploads=diagnostics_.uploadedBytes=diagnostics_.dispatches=diagnostics_.readbacks=0;}
 bool render(const gpuarea::Plan&p,std::vector<uint32_t>&out,std::shared_ptr<const gpupresent::Frame>*gpuOut=nullptr){
  if(gpuOut)gpuOut->reset();
  bool ok=false;try{ok=renderArea(p,out,gpuOut);}catch(const std::bad_alloc&){fatal("GPU area host allocation failed; CPU fallback");}
  if(!ok&&disabled_&&!inFlight_){releaseArea();close();}return ok;
 }
};
}}
